#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include "Common/File.h"
#include "Common/FileSystem.h"
#include "Common/Player.h"
#include "Common/SpecialPower.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "GameClient/Drawable.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/LuaDispatchProbe.h"
#include "GameLogic/LuaEventTables.h"
#include "GameLogic/LuaScriptEngine.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Object.h"
#include "../../../../GameEngineDevice/Include/W3DDevice/GameClient/Module/W3DModelDraw.h"

#include <algorithm>
#include <map>
#include <optional>
#include <string>

namespace
{
using Event = LuaEventTables::Event;
using Kind = LuaEventTables::Kind;
constexpr size_t batchSize = 32;

const char* eventKind(Kind kind)
{
	switch (kind)
	{
		case Kind::Internal: return "internal";
		case Kind::Scripted: return "scripted";
		case Kind::ModelCondition: return "model";
		case Kind::ObjectStatus: return "status";
	}
	return "unknown";
}

std::optional<LuaEventTables> readEventTables()
{
	File* input = TheFileSystem->openFile("Data\\Scripts\\ScriptEvents.xml", File::READ);
	if (!input)
		return {};
	const Int size = input->size();
	if (size <= 0)
	{
		input->close();
		return {};
	}
	const std::unique_ptr<char[]> data(input->readEntireAndClose());
	if (!data)
		return {};
	std::string error;
	auto tables = LuaEventTables::parse(std::string_view(data.get(), size), error);
	if (!tables)
		std::fprintf(stderr, "TEST lua bulk cannot read event tables: %s\n", error.c_str());
	return tables;
}

using Templates = std::map<std::string_view, const ThingTemplate*, std::less<>>;

Templates templatesByEventList(const LuaEventTables& tables)
{
	Templates result;
	for (const ThingTemplate* type = TheThingFactory->firstTemplate(); type; type = type->friend_getNextTemplate())
	{
		if (type->isKindOf(KINDOF_DRAWABLE_ONLY))
			continue;
		const auto& modules = type->getBehaviorModuleInfo();
		std::string_view list;
		for (size_t index = 0; index < modules.getCount(); ++index)
		{
			if (const auto* data = dynamic_cast<const AIUpdateModuleData*>(modules.getNthData(index));
				data && data->m_luaEventsList.isNotEmpty())
			{
				list = data->m_luaEventsList.str();
				break;
			}
		}
		if (!tables.lists.contains(list) && type->getRotWKObjectData().forceLuaRegistration)
			list = "BaseScriptFunctions";
		if (!tables.lists.contains(list))
			continue;
		auto [entry, inserted] = result.try_emplace(list, type);
		if (!inserted && std::string_view(type->getName().str()) < entry->second->getName().str())
			entry->second = type;
	}
	return result;
}

struct ProbeResult
{
	const char* status = "unreachable";
	std::string reason = "registered handler did not run";
	const char* effect = "";
};

const Object* eventInstigator(const Object& recipient)
{
	const Object* instigator = nullptr;
	for (const Object* candidate = TheGameLogic->getFirstObject(); candidate; candidate = candidate->getNextObject())
		if (candidate != &recipient && !candidate->isDestroyed() && !candidate->isEffectivelyDead() &&
			recipient.getRelationship(candidate) == ENEMIES && (!instigator || candidate->getID() < instigator->getID()))
			instigator = candidate;
	return instigator;
}

void fireEvent(Object& object, const Event& event, ProbeResult& result)
{
	auto* lua = LuaScriptEngine::active();
	const auto& name = event.name;
	if (event.kind == Kind::ModelCondition)
	{
		if (!triggerLuaModelEvent(object, name))
			result.reason = "model condition unavailable";
		return;
	}
	if (event.kind == Kind::Scripted)
	{
		lua->dispatchScripted(object, name, eventInstigator(object));
		return;
	}
	if (name == "OnDestroyed")
		TheGameLogic->destroyObject(&object);
	else if (name == "OnDamaged" || name == "OnSlaughtered")
		lua->dispatch(object, name, eventInstigator(object));
	else if (name == "OnUnitEntered" || name == "OnUnitExited")
		lua->dispatch(object, name, std::string_view("LuaBulkProbe"));
	else if (name == "OnBuildVariation")
		lua->dispatch(object, name, Real{1});
	else if (name == "BeScary")
		lua->dispatch(object, name, {}, Real{100});
	else if (name == "OnBuildingComplete" || name == "OnAflame" || name == "OnQuenched")
		lua->dispatch(object, name);
	else
		result.reason = "no logic trigger in probe";
}

bool hasUpgrade(const Object& object, const char* name)
{
	const auto* upgrade = TheUpgradeCenter->findUpgrade(name);
	if (!upgrade)
		return false;
	const Player* owner = object.getControllingPlayer();
	return upgrade->getUpgradeType() == UPGRADE_TYPE_PLAYER ?
		owner && owner->hasUpgradeComplete(upgrade) : object.hasUpgrade(upgrade);
}

bool pairedVisibility(const Object& object, const char* first, const char* second)
{
	const auto* drawable = object.getDrawable();
	if (!drawable)
		return false;
	for (auto module = drawable->getDrawModules(); *module; ++module)
	{
		const auto* model = dynamic_cast<const W3DModelDraw*>(*module);
		Bool firstHidden = FALSE;
		Bool secondHidden = FALSE;
		if (model && model->getSubObjectHidden(first, firstHidden) && model->getSubObjectHidden(second, secondHidden))
			return firstHidden != secondHidden;
	}
	return false;
}

const SpecialPowerModuleInterface* getLibraryPower(const Object& object)
{
	const auto* power = TheSpecialPowerStore->findSpecialPowerTemplate("SpecialAbilityLibraryDisabled");
	return power ? object.getSpecialPowerModule(power) : nullptr;
}

const char* checkEffect(Object& object, std::string_view handler, bool powerWasReady)
{
	bool verified;
	if (handler == "OnFaramirCreated")
		verified = hasUpgrade(object, "Upgrade_AmrothKnights") && hasUpgrade(object, "Upgrade_MiniHordeLvl2") &&
			pairedVisibility(object, "BODY", "BODYDUSTER") && pairedVisibility(object, "HOOD", "HOODDOWN");
	else if (handler == "OnMountedBodyguard")
		verified = hasUpgrade(object, "Upgrade_MountedBodyGuard") && !hasUpgrade(object, "Upgrade_DismountedBodyGuard");
	else if (handler == "TDArcherTowerUpgrade01")
		verified = hasUpgrade(object, "Upgrade_ElvenSilverthornArrows");
	else if (handler == "OnNecromancerGetTheRing")
		verified = hasUpgrade(object, "Upgrade_NecromancerRingHero");
	else if (handler == "OnRetreat")
		verified = hasUpgrade(object, "Upgrade_AIRetreatON") && !hasUpgrade(object, "Upgrade_AIRetreatOFF");
	else if (handler == "OnLibraryIdle" || handler == "OnTwinsDeathEggCreated")
	{
		const auto* power = getLibraryPower(object);
		verified = power && (handler == "OnLibraryIdle" ? powerWasReady && !power->isReady() :
			power->getPercentReady() < 1.0f);
	}
	else if (handler == "DragonStrikeDragonCreated")
	{
		verified = false;
		if (auto* ai = object.getAIUpdateInterface())
		{
			ai->aiIdle(CMD_FROM_SCRIPT);
			const bool acceptedScript = ai->getLastCommandSource() == CMD_FROM_SCRIPT;
			ai->aiIdle(CMD_FROM_PLAYER);
			verified = acceptedScript && ai->getLastCommandSource() == CMD_FROM_SCRIPT;
		}
	}
	else
		return "";
	return verified ? "effect verified" : "effect missing";
}

ProbeResult probe(const RetailTestCommands::Context& ctx, const ThingTemplate* type,
	std::string_view list, const Event& event, std::string_view handler)
{
	ProbeResult result;
	if (event.name == "OnGenericEvent")
	{
		result.reason = "generic event outside the logic sweep";
		return result;
	}
	if (!type)
	{
		result.reason = "no loaded template uses this list";
		return result;
	}
	if (type->isKindOf(KINDOF_DRAWABLE_ONLY))
	{
		result.reason = "template can only instantiate a drawable";
		return result;
	}
	Object* object = nullptr;
	const bool checkEffects = std::string_view(ctx.argument) != "sweep";
	bool powerWasReady = false;
	std::map<ObjectID, ProbeResult> observed;
	observeLuaDispatch([&](const LuaDispatchResult& call)
	{
		if (call.object.getTemplate() == type && call.list == list && call.event == event.name && call.handler == handler)
		{
			auto& row = observed[call.object.getID()];
			if (std::string_view(row.status) != "error")
				row = {call.ok ? "ran" : "error", std::string(call.error), ""};
		}
	}, [&]
	{
		object = TheThingFactory->newObjectFromExactTemplate(type, ctx.player->getDefaultTeam(), OBJECT_STATUS_MASK_NONE);
		if (!object || object->isDestroyed() || event.name == "OnCreated")
			return;
		const Coord3D point = ctx.groundLocation();
		object->setPosition(&point);
		if (checkEffects && handler == "OnMountedBodyguard")
			if (const auto* upgrade = TheUpgradeCenter->findUpgrade("Upgrade_DismountedBodyGuard"))
				object->giveUpgrade(upgrade);
		if (checkEffects && handler == "OnLibraryIdle")
			if (const auto* power = getLibraryPower(*object))
				powerWasReady = power->isReady();
		fireEvent(*object, event, result);
	});
	if (!object)
		return result;
	if (const auto hit = observed.find(object->getID()); hit != observed.end())
		result = std::move(hit->second);
	if (checkEffects && std::string_view(result.status) == "ran" && !object->isDestroyed())
		result.effect = checkEffect(*object, handler, powerWasReady);
	if (!object->isDestroyed())
		TheGameLogic->destroyObject(object);
	return result;
}

void record(const RetailTestCommands::Context& ctx, const ThingTemplate* type,
	const std::string& list, const Event& event, const std::string& handler, bool sweep)
{
	auto row = probe(ctx, type, list, event, handler);
	std::replace_if(row.reason.begin(), row.reason.end(), [](char value)
	{
		return value == '\t' || value == '\r' || value == '\n';
	}, ' ');
	std::fprintf(stderr, "LUA_BULK\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n", list.c_str(), event.name.c_str(),
		handler.c_str(), eventKind(event.kind), type ? type->getName().str() : "", row.status, row.reason.c_str(), row.effect, sweep ? "sweep" : "sample");
}

void runBulk(const RetailTestCommands::Context& ctx)
{
	if (!ctx.player || !ctx.hasPosition || !LuaScriptEngine::active() || ctx.ordinal < 0)
		return;
	const auto tables = readEventTables();
	if (!tables)
		return;
	const Templates templates = templatesByEventList(*tables);
	std::map<std::string_view, const Event*, std::less<>> events;
	for (const auto& event : tables->events)
		events.emplace(event.name, &event);

	const std::string_view selector(ctx.argument);
	const bool sweep = selector == "sweep";
	const size_t delimiter = selector.find(':');
	const size_t first = static_cast<size_t>(ctx.ordinal) * batchSize;
	size_t index = 0;
	for (const auto& [list, handlers] : tables->lists)
	{
		if (!sweep && (delimiter == std::string_view::npos || selector.substr(0, delimiter) != list))
			continue;
		const auto chosen = templates.find(list);
		const ThingTemplate* type = chosen == templates.end() ? nullptr : chosen->second;
		if (!sweep && std::string_view(ctx.targetName) != "-")
			type = TheThingFactory->findTemplate(ctx.targetName, FALSE);
		for (const auto& [event, handler] : handlers)
		{
			if (sweep)
			{
				if (index >= first + batchSize)
					return;
				if (index++ < first)
					continue;
			}
			else if (selector.substr(delimiter + 1) != event)
				continue;
			record(ctx, type, list, *events.at(event), handler, sweep);
		}
	}
	if (sweep && first <= index && index < first + batchSize)
		std::fprintf(stderr, "TEST lua bulk sweep pairs=%zu\n", index);
}

void logModelCondition(const RetailTestCommands::Context& ctx)
{
	const Object* object = ctx.requireObject(true);
	const Int flag = ModelConditionFlags::getSingleBitFromName(ctx.argument);
	if (object && flag >= 0)
		std::fprintf(stderr, "TEST lua bulk condition frame=%u player=%.*s set=%d\n", ctx.frame,
			static_cast<int>(ctx.playerName.size()), ctx.playerName.data(), object->getLogicModelConditions().test(flag));
}
}
#endif

namespace RetailTestCommands
{
void registerRetailLuaBulkTestCommands()
{
#if !RTS_GENERALS
	registerAction("lua-bulk", runBulk);
	registerAction("lua-bulk-model-condition", logModelCondition);
#endif
}
}
