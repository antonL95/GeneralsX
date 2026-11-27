#include "Precompiled/PreRTS.h"

#define DEFINE_WEAPONCONDITIONMAP
#include "GameLogic/WeaponSet.h"
#undef DEFINE_WEAPONCONDITIONMAP

#include "GameLogic/LuaScriptEngine.h"

#include "Common/File.h"
#include "Common/FileSystem.h"
#include "Common/GameState.h"
#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"
#include "GameLogic/FPUControl.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/LuaDispatchProbe.h"
#include "GameLogic/LuaEventTables.h"
#include "GameLogic/LuaLogicBindings.h"
#include "GameLogic/LuaState.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/AttributeModifierHelper.h"
#include "GameLogic/Object.h"

#include <algorithm>
#include <cstdio>
#include <lua.hpp>
#include <locale>
#include <sstream>

namespace
{
constexpr char scriptPath[] = "Data\\Scripts\\Scripts.lua";
constexpr char eventsPath[] = "Data\\Scripts\\ScriptEvents.xml";

std::string readData(const char* path)
{
	File* file = TheFileSystem->openFile(path, File::READ);
	if (!file)
		return {};
	const Int size = file->size();
	if (size <= 0)
	{
		file->close();
		return {};
	}
	const std::unique_ptr<char[]> buffer(file->readEntireAndClose());
	return buffer ? std::string(buffer.get(), size) : std::string();
}

struct ModelEvent
{
	std::string name;
	ModelConditionFlags required;
	ModelConditionFlags forbidden;
	ModelConditionFlags firingOrPreAttack;
	bool requireFiringOrPreAttack = false;

	bool matches(const ModelConditionFlags& flags) const
	{
		return flags.testSetAndClear(required, forbidden) &&
			(!firingOrPreAttack.any() || flags.testForAny(firingOrPreAttack) == requireFiringOrPreAttack);
	}
};

std::optional<ModelEvent> readModelEvent(const LuaEventTables::Event& event)
{
	ModelEvent result{};
	result.name = event.name;
	std::istringstream conditions(event.conditions);
	conditions.imbue(std::locale::classic());
	for (std::string token; conditions >> token;)
	{
		if (token.size() < 2 || (token[0] != '+' && token[0] != '-'))
			return std::nullopt;
		if (std::string_view(token).substr(1) == "FIRING_OR_PREATTACK_A")
		{
			result.firingOrPreAttack.set(MODELCONDITION_FIRING_A);
			result.firingOrPreAttack.set(MODELCONDITION_PREATTACK_A);
			result.requireFiringOrPreAttack = token[0] == '+';
			continue;
		}
		const Int bit = ModelConditionFlags::getSingleBitFromName(token.c_str() + 1);
		if (bit < 0)
			return std::nullopt;
		(token[0] == '+' ? result.required : result.forbidden).set(bit);
	}
	if (!result.required.any() && !result.forbidden.any() && !result.firingOrPreAttack.any())
		return std::nullopt;
	return result;
}

struct EventList
{
	std::string_view name;
	LuaEventTables::Handlers handlers;
	std::vector<const ModelEvent*> modelEvents;
};

struct PushArgument
{
	lua_State* state;

	void operator()(std::monostate) const { lua_pushnil(state); }
	void operator()(const Object* object) const { pushLuaObject(state, object); }
	void operator()(std::string_view text) const { lua_pushlstring(state, text.data(), text.size()); }
	void operator()(Real number) const { lua_pushnumber(state, number); }
};

struct DispatchArguments
{
	Object& object;
	LuaScriptEngine::Argument second;
	LuaScriptEngine::Argument third;
};

int pushArguments(lua_State* state, void* data)
{
	const auto& args = *static_cast<const DispatchArguments*>(data);
	pushLuaObject(state, &args.object);
	if (!std::holds_alternative<std::monostate>(args.third))
	{
		std::visit(PushArgument{state}, args.second);
		std::visit(PushArgument{state}, args.third);
		return 3;
	}
	if (!std::holds_alternative<std::monostate>(args.second))
	{
		std::visit(PushArgument{state}, args.second);
		return 2;
	}
	return 1;
}

class LogicLua final : public LuaScriptEngine
{
public:
	bool load(Xfer& crc) override;
	void reset() override;
	void clearObjects() override { m_objects.clear(); }
	void registerObject(Object& object) override;
	void onDestroyed(Object& object) override;
	bool dispatch(Object& object, std::string_view event, Argument second, Argument third) override;
	bool dispatchScripted(Object& object, std::string_view event, Argument second, Argument third) override;
	void modelConditionsChanged(Object& object, const ModelConditionFlags& previous) override;
	void restoreHordeConditions() override;
	void observe(const LuaDispatchObserver& observer, const std::function<void()>& trigger);
	bool triggerModelEvent(Object& object, std::string_view name);

private:
	const EventList* registeredList(const Object& object) const;
	const EventList* getList(const Object& object) const;
	bool invoke(Object& object, const EventList& list, std::string_view event, Argument second, Argument third);

	std::string m_source;
	std::unique_ptr<LuaState> m_state;
	std::map<std::string, EventList, std::less<>> m_lists;
	std::vector<ModelEvent> m_modelEvents;
	std::set<std::string, std::less<>> m_scriptedEvents;
	std::map<ObjectID, const EventList*> m_objects;
	const LuaDispatchObserver* m_observer = nullptr;
};

bool LogicLua::load(Xfer& crc)
{
	m_source = readData(scriptPath);
	std::string xml = readData(eventsPath);
	if (m_source.empty() || xml.empty())
		return false;
	setFPMode();
	ScopedFPUGuard fpuGuard;
	m_state = std::make_unique<LuaState>();
	if (!m_state->load(m_source, scriptPath))
		return false;
	bindLogicLua(*m_state);
	std::string error;
	auto tables = LuaEventTables::parse(xml, error);
	if (!tables)
	{
		std::fprintf(stderr, "Lua %s: %s\n", eventsPath, error.c_str());
		std::fflush(stderr);
		return false;
	}

	Int scriptSize = static_cast<Int>(m_source.size());
	Int xmlSize = static_cast<Int>(xml.size());
	crc.xferInt(&scriptSize);
	crc.xferUser(m_source.data(), scriptSize);
	crc.xferInt(&xmlSize);
	crc.xferUser(xml.data(), xmlSize);
	for (const auto& event : tables->events)
	{
		if (event.kind == LuaEventTables::Kind::Scripted)
			m_scriptedEvents.insert(event.name);
		if (event.kind != LuaEventTables::Kind::ModelCondition)
			continue;
		if (auto rule = readModelEvent(event))
			m_modelEvents.push_back(std::move(*rule));
	}

	std::set<std::string> missing;
	for (auto& [name, handlers] : tables->lists)
	{
		const auto entry = m_lists.try_emplace(name).first;
		auto& list = entry->second;
		list.name = entry->first;
		for (auto& [event, function] : handlers)
		{
			if (m_state->hasFunction(function))
				list.handlers.emplace(event, std::move(function));
			else if (missing.insert(function).second)
				std::fprintf(stderr, "Lua %s: %s is not a function\n", eventsPath, function.c_str());
		}
		for (const auto& event : m_modelEvents)
			if (list.handlers.contains(event.name))
				list.modelEvents.push_back(&event);
	}
	std::fflush(stderr);
	return true;
}

void LogicLua::reset()
{
	clearObjects();
	setFPMode();
	ScopedFPUGuard fpuGuard;
	m_state = std::make_unique<LuaState>();
	if (!m_state->load(m_source, scriptPath))
		m_state.reset();
	else
		bindLogicLua(*m_state);
}

void LogicLua::registerObject(Object& object)
{
	const auto* ai = object.getAIUpdateInterface();
	auto list = ai ? m_lists.find(ai->getRetailLuaEventList().str()) : m_lists.end();
	const Bool forced = object.getTemplate()->getRotWKObjectData().forceLuaRegistration;
	if (forced && list == m_lists.end())
		list = m_lists.find("BaseScriptFunctions");
	if (list != m_lists.end() || forced)
		m_objects.emplace(object.getID(), list != m_lists.end() ? &list->second : nullptr);
}

const EventList* LogicLua::registeredList(const Object& object) const
{
	const auto registered = m_objects.find(object.getID());
	return registered != m_objects.end() ? registered->second : nullptr;
}

const EventList* LogicLua::getList(const Object& object) const
{
	if (!m_state || object.isDestroyed() || (TheGameState && TheGameState->isInLoadGame()))
		return nullptr;
	return registeredList(object);
}

bool LogicLua::dispatch(Object& object, std::string_view event, Argument second, Argument third)
{
	const EventList* list = getList(object);
	return list && invoke(object, *list, event, second, third);
}

void LogicLua::onDestroyed(Object& object)
{
	const EventList* list = getList(object);
	m_objects.erase(object.getID());
	if (list)
		invoke(object, *list, "OnDestroyed", {}, {});
}

bool LogicLua::invoke(Object& object, const EventList& list, std::string_view event, Argument second, Argument third)
{
	const auto found = list.handlers.find(event);
	if (found == list.handlers.end())
		return false;
	setFPMode();
	ScopedFPUGuard fpuGuard;
	DispatchArguments args{object, second, third};
	std::string error;
	const bool ok = m_state->call(found->second, pushArguments, &args, m_observer ? &error : nullptr);
	if (m_observer)
		(*m_observer)({object, list.name, event, found->second, ok, error});
	return true;
}

void LogicLua::observe(const LuaDispatchObserver& observer, const std::function<void()>& trigger)
{
	struct RestoreObserver
	{
		const LuaDispatchObserver*& slot;
		const LuaDispatchObserver* previous;
		~RestoreObserver() { slot = previous; }
	} restore{m_observer, m_observer};
	m_observer = &observer;
	trigger();
}

bool LogicLua::triggerModelEvent(Object& object, std::string_view name)
{
	const auto event = std::find_if(m_modelEvents.begin(), m_modelEvents.end(),
		[name](const ModelEvent& rule) { return rule.name == name; });
	if (event == m_modelEvents.end())
		return false;

	const auto original = object.getLogicModelConditions();
	auto active = original;
	active.clearAndSet(event->forbidden, event->required);
	if (event->requireFiringOrPreAttack)
	{
		if (!active.testForAny(event->firingOrPreAttack))
			active.set(event->forbidden.test(MODELCONDITION_FIRING_A) ?
				MODELCONDITION_PREATTACK_A : MODELCONDITION_FIRING_A);
	}
	else
		active.clear(event->firingOrPreAttack);
	if (!event->matches(active))
		return false;

	if (event->matches(original))
	{
		auto inactive = original;
		if (event->required.any())
			inactive.clear(event->required);
		else if (event->forbidden.any())
			inactive.set(event->forbidden);
		else if (event->requireFiringOrPreAttack)
			inactive.clear(event->firingOrPreAttack);
		else
			inactive.set(MODELCONDITION_FIRING_A);
		object.clearAndSetModelConditionFlags(original, inactive);
	}
	if (!object.isDestroyed() && !object.isEffectivelyDead())
	{
		const auto current = object.getLogicModelConditions();
		object.clearAndSetModelConditionFlags(current, active);
	}
	return true;
}

bool LogicLua::dispatchScripted(Object& object, std::string_view event, Argument second, Argument third)
{
	return m_scriptedEvents.contains(event) && dispatch(object, event, second, third);
}

void LogicLua::modelConditionsChanged(Object& object, const ModelConditionFlags& previous)
{
	const ModelConditionFlags current = object.getLogicModelConditions();
	if (previous == current)
		return;
	const EventList* list = getList(object);
	if (!list || object.isEffectivelyDead())
		return;
	for (const ModelEvent* event : list->modelEvents)
	{
		if (!event->matches(previous) && event->matches(current))
			dispatch(object, event->name, {}, {});
		if (object.isDestroyed() || object.isEffectivelyDead())
			return;
	}
}

void LogicLua::restoreHordeConditions()
{
	setFPMode();
	ScopedFPUGuard fpuGuard;
	std::vector<LuaUpgradeChange> changes;
	LuaState restoreState;
	const bool canRestoreUpgrades = restoreState.load(m_source, scriptPath);
	if (canRestoreUpgrades)
		bindRestoringLua(restoreState, changes);

	std::vector<ObjectID> hordes;
	for (const Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
		if (object->isKindOf(KINDOF_HORDE))
			hordes.push_back(object->getID());

	for (const auto id : hordes)
	{
		Object* object = TheGameLogic->findObjectByID(id);
		if (!object || object->isEffectivelyDead() || object->isDestroyed())
			continue;
		ModelConditionFlags previous = object->getLogicModelConditions();
		for (const auto flag : { WEAPONSET_TOGGLE_1, WEAPONSET_TOGGLE_2, WEAPONSET_TOGGLE_3 })
		{
			const auto condition = TheWeaponSetTypeToModelConditionTypeMap[flag];
			if (object->testWeaponSetFlag(flag))
			{
				if (!previous.test(condition))
					object->setModelConditionState(condition);
				previous.set(condition, FALSE);
			}
			else if (previous.test(condition))
				object->clearModelConditionState(condition);
		}
		if (const auto* modifiers = object->getAttributeModifierHelper())
			for (const auto condition : modifiers->getAppliedModelConditions())
			{
				if (condition < MODELCONDITION_FIRST || condition >= MODELCONDITION_COUNT)
					continue;
				if (!object->getLogicModelConditions().test(condition))
					object->setModelConditionState(static_cast<ModelConditionFlagType>(condition));
				previous.set(condition, FALSE);
			}
		const EventList* list = registeredList(*object);
		if (!canRestoreUpgrades || !list)
			continue;
		const ModelConditionFlags current = object->getLogicModelConditions();
		for (const ModelEvent* event : list->modelEvents)
		{
			if (event->matches(previous) || !event->matches(current))
				continue;
			if (object->isEffectivelyDead() || object->isDestroyed())
				break;
			const auto handler = list->handlers.find(event->name);
			changes.clear();
			DispatchArguments args{*object, {}, {}};
			setFPMode();
			if (restoreState.call(handler->second, pushArguments, &args))
				for (const auto& change : changes)
					applyLuaUpgrade(change);
		}
	}
}
}

void observeLuaDispatch(const LuaDispatchObserver& observer, const std::function<void()>& trigger)
{
	if (auto* engine = static_cast<LogicLua*>(LuaScriptEngine::active()))
		engine->observe(observer, trigger);
}

bool triggerLuaModelEvent(Object& object, std::string_view name)
{
	auto* engine = static_cast<LogicLua*>(LuaScriptEngine::active());
	return engine && engine->triggerModelEvent(object, name);
}

std::unique_ptr<LuaScriptEngine> createLuaScriptEngine()
{
	return std::make_unique<LogicLua>();
}
