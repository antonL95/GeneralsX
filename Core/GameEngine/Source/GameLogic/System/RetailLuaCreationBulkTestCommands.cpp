#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include "Common/Errors.h"
#include "Common/Player.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "GameClient/Drawable.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/LuaDispatchProbe.h"
#include "GameLogic/LuaScriptEngine.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/FoundationAIUpdate.h"
#include "GameLogic/Module/SubObjectsUpgrade.h"
#include "GameLogic/Object.h"
#include "../../../../GameEngineDevice/Include/W3DDevice/GameClient/Module/W3DModelDraw.h"

#include <algorithm>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace
{
using Keys = std::vector<std::pair<std::string, std::string>>;

struct Handler
{
	std::string name;
	Keys keys;
};

using Handlers = std::map<std::string, Handler, std::less<>>;
using Manifest = std::map<std::string, Handlers, std::less<>>;

enum CreationPath
{
	DirectCreation,
	BuildVariationOne,
	BuildVariationTwo,
	BuildingCompletion,
};

Manifest readManifest(const char* path)
{
	std::ifstream stream(path);
	Manifest result;
	for (std::string row; std::getline(stream, row);)
	{
		std::string_view fields[5];
		std::string_view remaining(row);
		for (auto& field : fields)
		{
			const auto tab = remaining.find('\t');
			field = remaining.substr(0, tab);
			remaining.remove_prefix(tab == std::string_view::npos ? remaining.size() : tab + 1);
		}
		const auto& [list, event, handler, kind, name] = fields;
		if (list.empty() || event.empty() || handler.empty())
			return {};
		auto& entry = result[std::string(list)][std::string(event)];
		if (!entry.name.empty() && entry.name != handler)
			return {};
		entry.name = handler;
		if (!kind.empty() && !name.empty())
			entry.keys.emplace_back(kind, name);
	}
	return result;
}

std::string_view eventList(const ThingTemplate& type, const Manifest& manifest)
{
	const auto& modules = type.getBehaviorModuleInfo();
	for (size_t index = 0; index < modules.getCount(); ++index)
	{
		if (const auto* data = dynamic_cast<const AIUpdateModuleData*>(modules.getNthData(index));
			data && data->m_luaEventsList.isNotEmpty())
			return data->m_luaEventsList.str();
	}
	if (type.getRotWKObjectData().forceLuaRegistration && manifest.contains("BaseScriptFunctions"))
		return "BaseScriptFunctions";
	return {};
}

void recordAuthoredVisibility(const ThingTemplate& type, const Handlers& handlers)
{
	std::set<std::string_view> upgrades;
	for (const auto& [event, handler] : handlers)
		for (const auto& [kind, name] : handler.keys)
			if (kind == "upgrade")
				upgrades.insert(name);
	if (upgrades.empty())
		return;

	const auto& modules = type.getBehaviorModuleInfo();
	for (size_t index = 0; index < modules.getCount(); ++index)
	{
		const auto* data = dynamic_cast<const SubObjectsUpgradeModuleData*>(modules.getNthData(index));
		if (!data)
			continue;
		const auto& mux = data->m_upgradeMuxData;
		if (mux.m_activationUpgradeNames.size() != 1 || !mux.m_removalUpgradeNames.empty())
			continue;
		const auto& trigger = mux.m_activationUpgradeNames.front();
		if (!upgrades.contains(trigger.str()))
			continue;
		const auto* upgrade = TheUpgradeCenter->findUpgrade(trigger);
		if (!upgrade || upgrade->getUpgradeType() != UPGRADE_TYPE_OBJECT)
			continue;
		const auto record = [&](const AsciiString& subobject, char hidden)
		{
			std::fprintf(stderr, "LUA_CREATION_RULE\t%s\t%s\t%s\t%c", type.getName().str(),
				trigger.str(), subobject.str(), hidden);
			for (const auto& conflict : mux.m_conflictingUpgradeNames)
				std::fprintf(stderr, "\t%s", conflict.str());
			std::fputc('\n', stderr);
		};
		for (const auto& subobject : data->m_showSubObjectNames)
			record(subobject, '0');
		for (const auto& subobject : data->m_hideSubObjectNames)
			record(subobject, '1');
	}
}

char mergeState(char first, char next)
{
	return first == '?' ? next : first == next ? first : 'x';
}

char subObjectState(const Object& object, const AsciiString& name, bool permanent)
{
	const Drawable* drawable = object.getDrawable();
	char value = '?';
	if (drawable)
		for (auto module = drawable->getDrawModules(); *module; ++module)
		{
			const auto* model = dynamic_cast<const W3DModelDraw*>(*module);
			if (!model)
				continue;
			Bool hidden = FALSE;
			const Bool known = permanent ? model->getSubObjectHidden(name, hidden) :
				model->getTemporarySubObjectHidden(name, hidden);
			if (known)
				value = mergeState(value, hidden ? '1' : '0');
		}
	return value;
}

char readState(const Object& object, const std::string& kind, const std::string& name)
{
	if (kind == "hide" || kind == "temporary")
		return subObjectState(object, name.c_str(), kind == "hide");
	if (kind == "upgrade")
	{
		const auto* upgrade = TheUpgradeCenter->findUpgrade(name.c_str());
		if (!upgrade)
			return '-';
		const bool owned = upgrade->getUpgradeType() == UPGRADE_TYPE_PLAYER ?
			object.getControllingPlayer()->hasUpgradeComplete(upgrade) : object.hasUpgrade(upgrade);
		return owned ? '1' : '0';
	}
	if (kind == "condition")
	{
		const auto bit = ModelConditionFlags::getSingleBitFromName(name.c_str());
		return bit < 0 ? '?' : object.getLogicModelConditions().test(bit) ? '1' : '0';
	}
	if (kind == "geometry")
	{
		char value = '?';
		const auto& data = object.getTemplate()->getRotWKObjectData();
		const auto sample = [&](const ThingTemplate::GeometryDetails& geometry)
		{
			if (geometry.name.compareNoCase(name.c_str()) == 0)
				value = mergeState(value, object.getLuaGeometryActive(geometry.name, geometry.active) ? '1' : '0');
		};
		sample(data.primaryGeometry);
		for (const auto& geometry : data.additionalGeometries)
			sample(geometry.details);
		return value == '?' ? '-' : value;
	}
	return '?';
}

struct Call
{
	std::string event;
	const Handler* handler;
	std::string before;
	std::string after;
	std::string error;
	const char* status = "incomplete";
};

std::string readStates(const Object& object, const Handler& handler)
{
	std::string state;
	state.reserve(handler.keys.size());
	for (const auto& [kind, name] : handler.keys)
		state.push_back(readState(object, kind, name));
	return state;
}

void runTrial(const RetailTestCommands::Context& ctx, const ThingTemplate& type, const std::string& list,
	const Handlers& handlers, CreationPath path, bool sample)
{
	Object* const previousHead = TheGameLogic->getFirstObject();
	const char* status = "not created";
	std::string reason;
	Object* object = nullptr;
	std::map<ObjectID, std::vector<Call>> calls;
	const LuaDispatchBefore before = [&](const Object& actor, std::string_view callbackList, std::string_view event,
		std::string_view handler)
	{
		const auto found = handlers.find(event);
		if (actor.getTemplate() != &type || callbackList != list ||
			found == handlers.end() || found->second.name != handler)
			return;
		calls[actor.getID()].push_back({std::string(event), &found->second, readStates(actor, found->second), {}, {}});
	};
	const LuaDispatchObserver after = [&](const LuaDispatchResult& result)
	{
		const auto observed = calls.find(result.object.getID());
		if (observed == calls.end())
			return;
		for (auto call = observed->second.rbegin(); call != observed->second.rend(); ++call)
			if (call->event == result.event && std::string_view(call->status) == "incomplete")
			{
				call->status = result.ok ? "ran" : "error";
				call->error = result.error;
				call->after = readStates(result.object, *call->handler);
				break;
			}
	};
	observeLuaDispatch(after, [&]
	{
		if (type.isKindOf(KINDOF_DRAWABLE_ONLY))
		{
			reason = "drawable only";
			return;
		}
		const Coord3D point = ctx.groundLocation();
		try
		{
			if (path != DirectCreation)
			{
				const char* plots[] = {nullptr, "LuaCreationBulkPlotOne", "LuaCreationBulkPlotTwo", "LuaCreationBulkPlot"};
				const auto* plotType = TheThingFactory->findTemplate(plots[path], FALSE);
				if (!plotType)
				{
					reason = "missing foundation fixture";
					return;
				}
				Object* plot = TheThingFactory->newObjectFromExactTemplate(plotType, ctx.player->getDefaultTeam(), OBJECT_STATUS_MASK_NONE);
				plot->setPosition(&point);
				if (auto* foundation = FoundationAIUpdate::find(*plot))
					object = foundation->construct(&type, ctx.player, TRUE);
			}
			else
			{
				object = TheThingFactory->newObjectFromExactTemplate(&type, ctx.player->getDefaultTeam(), OBJECT_STATUS_MASK_NONE);
				if (object && !object->isDestroyed())
					object->setPosition(&point);
			}
		}
		catch (ErrorCode error)
		{
			reason = "factory error " + std::to_string(static_cast<unsigned>(error));
		}
	}, &before);
	if (object && object->getTemplate() == &type)
		status = "created";
	else if (reason.empty())
		reason = object ? "construction selected another variation" : "construction rejected template";
	const char* name = type.getName().str();
	std::fprintf(stderr, "LUA_CREATION_TEMPLATE\t%s\t%u\t%s\t%d\t%d\t%s\t%s\n", name,
		static_cast<unsigned>(type.getTemplateID()), list.c_str(), static_cast<int>(path),
		object ? static_cast<int>(object->getID()) : 0, status, reason.c_str());
	if (object && object->getTemplate() == &type)
	{
		if (const auto found = calls.find(object->getID()); found != calls.end())
			for (auto& call : found->second)
			{
				std::replace_if(call.error.begin(), call.error.end(),
					[](char ch) { return ch == '\t' || ch == '\r' || ch == '\n'; }, ' ');
				std::fprintf(stderr, "LUA_CREATION_CALL\t%s\t%d\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n",
					name, static_cast<int>(path), list.c_str(), call.event.c_str(), call.handler->name.c_str(),
					call.status, call.error.c_str(), call.before.c_str(), call.after.c_str());
				if (sample && std::string_view(call.status) == "ran")
					std::fprintf(stderr, "TEST lua creation sample template=%s trial=%d event=%s state=%s\n",
						name, static_cast<int>(path), call.event.c_str(), call.after.c_str());
			}
	}
	for (Object* spawned = TheGameLogic->getFirstObject(); spawned && spawned != previousHead;)
	{
		Object* next = spawned->getNextObject();
		if (!spawned->isDestroyed())
			TheGameLogic->destroyObject(spawned);
		spawned = next;
	}
}

void runBulk(const RetailTestCommands::Context& ctx)
{
	if (!ctx.player || !ctx.hasPosition || !LuaScriptEngine::active() || ctx.ordinal < 0)
		return;
	constexpr std::string_view snapshotPrefix = "snapshot:";
	const bool snapshot = std::string_view(ctx.argument).starts_with(snapshotPrefix);
	const Manifest manifest = readManifest(ctx.argument + (snapshot ? snapshotPrefix.size() : 0));
	if (manifest.empty())
	{
		std::fprintf(stderr, "TEST lua creation unavailable manifest: %s\n", ctx.argument);
		return;
	}
	if (snapshot)
	{
		if (const Object* object = ctx.requireObject(true))
		{
			const auto list = manifest.find(eventList(*object->getTemplate(), manifest));
			if (list != manifest.end())
				if (const auto handler = list->second.find("OnCreated"); handler != list->second.end())
					std::fprintf(stderr, "TEST lua creation saved template=%s frame=%u state=%s\n",
						ctx.targetName, ctx.frame, readStates(*object, handler->second).c_str());
		}
		return;
	}
	const bool sweep = std::string_view(ctx.targetName) == "-";
	std::vector<const ThingTemplate*> templates;
	if (sweep)
	{
		for (const auto* type = TheThingFactory->firstTemplate(); type; type = type->friend_getNextTemplate())
			if (manifest.contains(eventList(*type, manifest)))
				templates.push_back(type);
		std::sort(templates.begin(), templates.end(), [](const ThingTemplate* left, const ThingTemplate* right)
		{
			return left->getTemplateID() < right->getTemplateID();
		});
	}
	else if (const auto* type = TheThingFactory->findTemplate(ctx.targetName, FALSE))
		templates.push_back(type);

	constexpr size_t batchSize = 16;
	const size_t first = sweep ? static_cast<size_t>(ctx.ordinal) * batchSize : 0;
	const size_t end = std::min(first + batchSize, templates.size());
	for (size_t index = first; index < end; ++index)
	{
		const auto& type = *templates[index];
		const auto list = manifest.find(eventList(type, manifest));
		if (list == manifest.end())
			continue;
		if (sweep)
			recordAuthoredVisibility(type, list->second);
		const bool structure = type.isKindOf(KINDOF_STRUCTURE);
		const bool variation = structure && list->second.contains("OnBuildVariation");
		const bool completion = structure && list->second.contains("OnBuildingComplete");
		if (sweep || !(variation || completion))
			runTrial(ctx, type, list->first, list->second, DirectCreation, !sweep);
		if (variation)
		{
			runTrial(ctx, type, list->first, list->second, BuildVariationOne, !sweep);
			runTrial(ctx, type, list->first, list->second, BuildVariationTwo, !sweep);
		}
		else if (completion)
			runTrial(ctx, type, list->first, list->second, BuildingCompletion, !sweep);
	}
	if (sweep && first < templates.size() && end == templates.size())
		std::fprintf(stderr, "LUA_CREATION_COMPLETE\t%zu\n", templates.size());
}
}
#endif

namespace RetailTestCommands
{
void registerRetailLuaCreationBulkTestCommands()
{
#if !RTS_GENERALS
	registerAction("lua-creation-bulk", runBulk);
#endif
}
}
