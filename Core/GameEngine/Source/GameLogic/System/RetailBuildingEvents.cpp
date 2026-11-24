#include "PreRTS.h"

#if !RTS_GENERALS
#define DEFINE_WEAPONCONDITIONMAP
#include "GameLogic/WeaponSet.h"
#undef DEFINE_WEAPONCONDITIONMAP
#endif

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <locale>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "Common/File.h"
#include "Common/FileSystem.h"
#include "Common/GameState.h"
#include "Common/RetailFileLayers.h"
#include "Common/SpecialPower.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "GameClient/ControlBar.h"
#include "GameClient/Drawable.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/LogicRandomValue.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Object.h"
#include "GameLogic/RetailBuildingEvents.h"
#if !RTS_GENERALS
#include "GameLogic/Module/AttributeModifierHelper.h"
#endif

namespace
{
enum class Action { Grant, Remove, Power, Visibility, Roll };

struct Condition
{
	std::size_t roll;
	double limit;
	bool atMost;
};

struct Operation
{
	Action action;
	std::string name;
	bool hide = false;
	std::size_t roll = 0;
	std::vector<Condition> conditions;
};

using Program = std::vector<Operation>;

struct ModelConditionEvent
{
	std::string name;
	ModelConditionFlags required;
	ModelConditionFlags forbidden;
};

struct EventList
{
	std::string parent;
	std::map<std::string, std::string> handlers;
	std::map<std::string, std::string> stateHandlers;
};

std::string readData(const char* path)
{
	File* file = TheFileSystem->openFile(path, File::READ);
	if (!file)
		return {};
	const Int size = file->size();
	if (size <= 0 || size > 4 * 1024 * 1024)
	{
		file->close();
		return {};
	}
	char* buffer = file->readEntireAndClose();
	std::string result;
	if (buffer)
		result.assign(buffer, size);
	delete[] buffer;
	return result;
}

std::string attribute(const std::string& tag, const char* key)
{
	const std::regex value(std::string("(?:^|[[:space:]])") + key + R"re(\s*=\s*"([A-Za-z0-9_]+)")re");
	std::smatch match;
	return std::regex_search(tag, match, value) ? match[1].str() : std::string();
}

bool isDirectEvent(std::string_view event)
{
	return event == "OnBuildingComplete" || event == "OnDamaged" || event == "Rider8IsActive" || event == "Rider8IsNotActive" ||
		event == "UserSixIsActive" || event == "UserSixIsNotActive";
}

std::vector<ModelConditionEvent> readModelConditionEvents(const std::string& xml)
{
	static const std::regex declaration(R"(<ModelConditionEvent\s+([^>]+)>\s*<Conditions>([^<]+)</Conditions>\s*</ModelConditionEvent>)");
	std::vector<ModelConditionEvent> rules;
	for (auto it = std::sregex_iterator(xml.begin(), xml.end(), declaration); it != std::sregex_iterator(); ++it)
	{
		ModelConditionEvent event{attribute((*it)[1].str(), "Name"), {}, {}};
		if (event.name.empty())
			continue;
		std::istringstream conditions((*it)[2].str());
		std::string token;
		Bool valid = TRUE;
		while (conditions >> token)
		{
			const Int bit = ModelConditionFlags::getSingleBitFromName(token.c_str() + 1);
			if (bit < 0 || (token.front() != '+' && token.front() != '-'))
			{
				valid = FALSE;
				break;
			}
			(token.front() == '+' ? event.required : event.forbidden).set(bit);
		}
		if (valid && (event.required.any() || event.forbidden.any()))
			rules.push_back(std::move(event));
	}
	return rules;
}

std::map<std::string, EventList> readEventLists(std::string xml, std::vector<ModelConditionEvent>* modelConditions = nullptr)
{
	for (std::size_t start = xml.find("<!--"); start != std::string::npos; start = xml.find("<!--", start))
	{
		const auto end = xml.find("-->", start + 4);
		xml.erase(start, end == std::string::npos ? end : end + 3 - start);
	}
	if (modelConditions)
		*modelConditions = readModelConditionEvents(xml);
	std::map<std::string, EventList> lists;
	for (std::size_t start = 0; (start = xml.find("<EventList", start)) != std::string::npos;)
	{
		const auto tagStart = start + 10;
		const auto tagEnd = xml.find('>', tagStart);
		if (tagEnd == std::string::npos)
			break;
		start = tagEnd + 1;
		if (tagEnd - tagStart > 1024 || std::string_view(" \t\r\n").find(xml[tagStart]) == std::string_view::npos)
			continue;
		const auto last = xml.find_last_not_of(" \t\r\n", tagEnd - 1);
		if (last == std::string::npos || xml[last] == '/')
			continue;
		const auto listEnd = xml.find("</EventList>", start);
		if (listEnd == std::string::npos)
			break;
		const std::string attrs = xml.substr(tagStart, tagEnd - tagStart);
		const auto name = attribute(attrs, "Name");
		if (name.empty())
			continue;
		auto& entry = lists[name];
		entry.parent = attribute(attrs, "Inherit");
		for (auto handler = xml.find("<EventHandler", start); handler < listEnd;)
		{
			const auto fieldsStart = handler + 13;
			const auto fieldsEnd = xml.find('>', fieldsStart);
			if (fieldsEnd >= listEnd)
				break;
			handler = xml.find("<EventHandler", fieldsEnd + 1);
			if (fieldsEnd - fieldsStart > 1024)
				continue;
			const std::string fields = xml.substr(fieldsStart, fieldsEnd - fieldsStart);
			const auto event = attribute(fields, "EventName");
			if (event == "OnCreated" || isDirectEvent(event) ||
				(modelConditions && std::any_of(modelConditions->begin(), modelConditions->end(),
					[&event](const ModelConditionEvent& condition) { return condition.name == event; })))
				entry.handlers[event] = attribute(fields, "ScriptFunctionName");
			else if (event == "OnAflame" || event == "OnQuenched" || event == "ReallyDamaged" ||
				event == "User45IsActive" || event == "User45IsNotActive")
				entry.stateHandlers[event] = attribute(fields, "ScriptFunctionName");
		}
		start = listEnd + 12;
	}
	return lists;
}

bool compile(const std::vector<std::string>& lines, Program& program)
{
	static const std::regex roll(R"(local\s+(\w+)\s*=\s*GetRandomNumber\(\s*\))");
	static const std::regex branch(R"((if|elseif)\s+(\w+)\s*<=\s*([0-9]+(?:\.[0-9]+)?)\s+then)");
	static const std::regex call(R"re((ObjectGrantUpgrade|ObjectRemoveUpgrade|ObjectDoSpecialPower|ObjectHideSubObjectPermanently)\(\s*self\s*,\s*"([A-Za-z0-9_]+)"(?:\s*,\s*(true|false))?\s*\))re");
	std::map<std::string, std::size_t> rolls;
	std::vector<Condition> conditions;
	std::vector<std::size_t> levels;
	bool finished = false;
	for (const auto& raw : lines)
	{
		std::string line = raw.substr(0, raw.find("--"));
		const auto first = line.find_first_not_of(" \t\r");
		if (first == std::string::npos)
			continue;
		line = line.substr(first, line.find_last_not_of(" \t\r") + 1 - first);
		if (finished || program.size() >= 256)
			return false;
		std::smatch token;
		if (std::regex_match(line, token, roll))
		{
			const auto index = rolls.size();
			if (!conditions.empty() || !rolls.emplace(token[1].str(), index).second)
				return false;
			program.push_back({Action::Roll, {}, false, index, {}});
		}
		else if (std::regex_match(line, token, branch))
		{
			const auto variable = rolls.find(token[2].str());
			std::istringstream number(token[3].str());
			number.imbue(std::locale::classic());
			double limit;
			if (variable == rolls.end() || !(number >> limit))
				return false;
			if (token[1] == "if")
				levels.push_back(conditions.size());
			else if (levels.empty() || !conditions.back().atMost)
				return false;
			else
				conditions.back().atMost = false;
			conditions.push_back({variable->second, limit, true});
		}
		else if (line == "else")
		{
			if (levels.empty() || !conditions.back().atMost)
				return false;
			conditions.back().atMost = false;
		}
		else if (line == "end")
		{
			if (levels.empty())
				finished = true;
			else
			{
				conditions.resize(levels.back());
				levels.pop_back();
			}
		}
		else if (std::regex_match(line, token, call))
		{
			const Action action = token[1] == "ObjectGrantUpgrade" ? Action::Grant :
				token[1] == "ObjectRemoveUpgrade" ? Action::Remove :
				token[1] == "ObjectDoSpecialPower" ? Action::Power : Action::Visibility;
			if (token[3].matched != (action == Action::Visibility))
				return false;
			program.push_back({action, token[2].str(), token[3] == "true", 0, conditions});
		}
		else
			return false;
	}
	return finished;
}

struct BuildingEvents
{
	std::vector<ModelConditionEvent> modelConditions;
	std::map<std::string, EventList> lists;
	std::map<std::string, Program> programs;

	BuildingEvents() : lists(readEventLists(readData("Data\\Scripts\\ScriptEvents.xml"), &modelConditions))
	{
		std::map<std::string, std::vector<std::string>> bodies;
		std::map<std::string, bool> establishedHandlers;
		std::set<std::string> damageHandlers;
		for (const auto& [name, list] : lists)
			for (const auto& [event, handler] : list.handlers)
				if (!handler.empty())
				{
					bodies.try_emplace(handler);
					if (isDirectEvent(event))
						establishedHandlers[handler] = true;
					if (event == "OnDamaged")
						damageHandlers.insert(handler);
				}

		std::istringstream lua(readData("Data\\Scripts\\Scripts.lua"));
		const std::regex definition(R"(^\s*function\s+(\w+)\s*\(([^)]*)\).*)");
		const std::regex damageParameters(R"(^\s*self(?:\s*,\s*\w+)*\s*$)");
		std::vector<std::string>* body = nullptr;
		for (std::string line; std::getline(lua, line);)
		{
			std::smatch match;
			if (!line.empty() && line.back() == '\r')
				line.pop_back();
			if (line.size() > 1024)
			{
				if (body)
					body->push_back("<invalid>");
			}
			else if (std::regex_match(line, match, definition))
			{
				const auto found = bodies.find(match[1].str());
				const bool supportedParameters = match[2] == "self" ||
					(found != bodies.end() && damageHandlers.contains(found->first) && std::regex_match(match[2].str(), damageParameters));
				body = found != bodies.end() && supportedParameters ? &found->second : nullptr;
				if (body)
					body->clear();
			}
			else if (body)
				body->push_back(std::move(line));
		}

		for (const auto& [name, lines] : bodies)
		{
			Program program;
			if (compile(lines, program))
				programs.emplace(name, std::move(program));
			else if (establishedHandlers.contains(name))
				std::fprintf(stderr, "Retail events: unsupported callback %s\n", name.c_str());
		}
		if (std::getenv("GENERALSX_TEST_COMMANDS"))
			std::fprintf(stderr, "BPARITY building callbacks %zu/%zu\n", programs.size(), bodies.size());
	}
};

void castDamageResponse(Object& object, const SpecialPowerTemplate& requested)
{
	for (BehaviorModule** module = object.getBehaviorModules(); *module; ++module)
	{
		const auto* caster = (*module)->getSpecialPower();
		const auto* power = caster ? caster->getSpecialPowerTemplate() : nullptr;
		if (!power || power->getSpecialPowerType() != requested.getSpecialPowerType())
			continue;
		if (caster->isReady())
			object.doSpecialPower(power, COMMAND_OPTION_INTERNAL_AUTO, TRUE);
		return;
	}
}

const BuildingEvents& buildingEvents()
{
	static const BuildingEvents events;
	return events;
}

enum class DispatchMode { Direct, ModelCondition, Restore };

void dispatch(Object& object, const char* event, DispatchMode mode = DispatchMode::Direct)
{
	if (!RetailFileLayers::enabled() || !object.getAIUpdateInterface())
		return;
	const AsciiString& listName = object.getAIUpdateInterface()->getRetailLuaEventsList();
	if (listName.isEmpty())
		return;
	const auto& events = buildingEvents();
	auto list = events.lists.find(listName.str());
	for (std::size_t depth = 0; list != events.lists.end() && depth < events.lists.size(); ++depth)
	{
		const auto handler = list->second.handlers.find(event);
		if (handler == list->second.handlers.end() || handler->second.empty())
		{
			list = events.lists.find(list->second.parent);
			continue;
		}
		const auto found = events.programs.find(handler->second);
		if (found == events.programs.end())
			return;
		if (mode != DispatchMode::Direct && !std::all_of(found->second.begin(), found->second.end(), [](const Operation& operation)
		{
			return operation.action == Action::Grant || operation.action == Action::Remove;
		}))
			return;
		if (mode == DispatchMode::Restore && std::none_of(found->second.begin(), found->second.end(), [&object](const Operation& operation)
		{
			const auto* upgrade = TheUpgradeCenter->findUpgrade(operation.name.c_str());
			return upgrade && object.hasUpgrade(upgrade) != (operation.action == Action::Grant);
		}))
			return;
		const std::string_view eventName(event);
		const bool creation = eventName == "OnCreated";
		if (creation && !std::all_of(found->second.begin(), found->second.end(), [&](const Operation& operation)
		{
			if (operation.action != Action::Power)
				return operation.action == Action::Visibility || operation.action == Action::Roll;

			const auto* power = TheSpecialPowerStore->findSpecialPowerTemplate(operation.name.c_str());
			const auto* caster = object.getSpecialPowerModule(power);
			if (!caster)
				return false;

			static const NameKeyType productionBonus = NAMEKEY("ProductionSpeedBonus");
			static const NameKeyType cloudBreak = NAMEKEY("CloudBreakSpecialPower");
			for (BehaviorModule** module = object.getBehaviorModules(); *module; ++module)
			{
				const NameKeyType key = (*module)->getModuleNameKey();
				if ((key == productionBonus || key == cloudBreak) && (*module)->getSpecialPower() == caster)
					return true;
			}
			return false;
		}))
			return;
		if (eventName == "OnBuildingComplete" && std::getenv("GENERALSX_TEST_COMMANDS"))
			std::fprintf(stderr, "BPARITY complete %u %s %s %zu\n", TheGameLogic->getFrame(),
				object.getTemplate()->getName().str(), handler->second.c_str(), found->second.size());

		std::vector<double> rolls;
		UnsignedInt hidden = 0;
		UnsignedInt shown = 0;
		for (const Operation& operation : found->second)
		{
			if (!std::all_of(operation.conditions.begin(), operation.conditions.end(), [&](const Condition& condition)
			{
				return (rolls[condition.roll] <= condition.limit) == condition.atMost;
			}))
				continue;
			switch (operation.action)
			{
			case Action::Roll:
				rolls.push_back(GameLogicRandomValueReal(0.0f, 1.0f));
				break;
			case Action::Grant:
				if (const auto* upgrade = TheUpgradeCenter->findUpgrade(operation.name.c_str()))
					object.giveUpgrade(upgrade);
				break;
			case Action::Remove:
				if (const auto* upgrade = TheUpgradeCenter->findUpgrade(operation.name.c_str()))
					object.removeUpgrade(upgrade);
				break;
			case Action::Power:
				if (const auto* power = TheSpecialPowerStore->findSpecialPowerTemplate(operation.name.c_str()))
				{
					if (eventName == "OnDamaged")
						castDamageResponse(object, *power);
					else
						object.doSpecialPower(power, COMMAND_OPTION_INTERNAL_AUTO, TRUE);
				}
				break;
			case Action::Visibility:
				if (operation.hide)
					++hidden;
				else
					++shown;
				if (auto* draw = object.getDrawable())
				{
					draw->showSubObject(operation.name.c_str(), !operation.hide);
					draw->updateSubObjects();
				}
				break;
			}
			if (object.isDestroyed() || object.isEffectivelyDead())
				return;
		}
		static const bool traceCreation = std::getenv("GENERALSX_TEST_CREATION_VISIBILITY") != nullptr;
		if (creation && traceCreation && (hidden || shown))
			std::fprintf(stderr, "TEST pkg12 created frame=%u source=%s callback=%s rolls=%zu hidden=%u shown=%u\n",
				TheGameLogic->getFrame(), object.getTemplate()->getName().str(), handler->second.c_str(),
				rolls.size(), hidden, shown);
		return;
	}
}
}

void RetailBuildingEvents::created(Object& object)
{
	dispatch(object, "OnCreated");
}

void RetailBuildingEvents::complete(Object& object)
{
	dispatch(object, "OnBuildingComplete");
}

void RetailBuildingEvents::damaged(Object& object)
{
	if (!object.isEffectivelyDead() && !object.isDestroyed())
		dispatch(object, "OnDamaged");
}

void RetailBuildingEvents::riderChanged(Object& object, bool mounted)
{
	dispatch(object, mounted ? "Rider8IsActive" : "Rider8IsNotActive");
}

void RetailBuildingEvents::trollEatingChanged(Object& object, bool enabled)
{
	dispatch(object, enabled ? "UserSixIsActive" : "UserSixIsNotActive");
}

#if !RTS_GENERALS
namespace
{
void dispatchModelConditions(Object& object, const ModelConditionFlags& previous, DispatchMode mode)
{
	if (!RetailFileLayers::enabled() || !object.areModulesReady() ||
		object.isEffectivelyDead() || !object.getAIUpdateInterface() ||
		object.getAIUpdateInterface()->getRetailLuaEventsList().isEmpty())
		return;
	const ModelConditionFlags current = object.getLogicModelConditions();
	if (previous == current)
		return;
	for (const auto& event : buildingEvents().modelConditions)
	{
		if (!current.testSetAndClear(event.required, event.forbidden) ||
			previous.testSetAndClear(event.required, event.forbidden))
			continue;
		dispatch(object, event.name.c_str(), mode);
		if (object.isDestroyed() || object.isEffectivelyDead())
			return;
	}
}
}

void RetailBuildingEvents::modelConditionsChanged(Object& object, const ModelConditionFlags& previous)
{
	if (!TheGameState || !TheGameState->isInLoadGame())
		dispatchModelConditions(object, previous, DispatchMode::ModelCondition);
}

void RetailBuildingEvents::restoreHordeConditionUpgrades()
{
	for (Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
	{
		if (!object->isKindOf(KINDOF_HORDE))
			continue;
		ModelConditionFlags previous = object->getLogicModelConditions();
		for (const WeaponSetType flag : { WEAPONSET_TOGGLE_1, WEAPONSET_TOGGLE_2, WEAPONSET_TOGGLE_3 })
		{
			const auto condition = TheWeaponSetTypeToModelConditionTypeMap[flag];
			const Bool active = object->testWeaponSetFlag(flag);
			if (active)
			{
				if (!previous.test(condition))
					object->setModelConditionState(condition);
				previous.set(condition, FALSE);
			}
			else if (previous.test(condition))
				object->clearModelConditionState(condition);
		}
		if (const auto* modifiers = object->getAttributeModifierHelper())
			for (Int condition : modifiers->getAppliedModelConditions())
				if (condition >= MODELCONDITION_FIRST && condition < MODELCONDITION_COUNT)
				{
					if (!object->getLogicModelConditions().test(condition))
						object->setModelConditionState(static_cast<ModelConditionFlagType>(condition));
					previous.set(condition, FALSE);
				}
		dispatchModelConditions(*object, previous, DispatchMode::Restore);
	}
}
#endif

namespace
{
enum class StateAction { None, EndEnrage, StartEnrage, StartRampage };

StateAction findStateAction(const Object& object, const char* event)
{
	if (!RetailFileLayers::enabled() || !object.getAIUpdateInterface())
		return StateAction::None;
	const auto& lists = buildingEvents().lists;
	static const auto actions = []
	{
		std::map<std::string, StateAction> result;
		std::istringstream lua(readData("Data\\Scripts\\Scripts.lua"));
		const std::regex definition(R"(^\s*function\s+(\w+)\s*\(self\)\s*$)");
		const std::regex enrage(R"(^\s*ObjectSetEnragedState\(\s*self\s*,\s*(true|false)\s*\)\s*$)");
		const std::regex rampage(R"(^\s*ObjectEnterRampageState\(\s*self\s*\)\s*$)");
		std::string function;
		for (std::string line; std::getline(lua, line);)
		{
			line = line.substr(0, line.find("--"));
			if (line.size() > 1024)
			{
				function.clear();
				continue;
			}
			std::smatch match;
			if (std::regex_match(line, match, definition))
				function = match[1].str();
			else if (!function.empty() && std::regex_match(line, match, enrage))
				result[function] = match[1] == "true" ? StateAction::StartEnrage : StateAction::EndEnrage;
			else if (!function.empty() && std::regex_match(line, rampage))
				result[function] = StateAction::StartRampage;
			else if (line.find_first_not_of(" \t\r") != std::string::npos &&
				line.find("ObjectGrantUpgrade") == std::string::npos && line.find("ObjectRemoveUpgrade") == std::string::npos)
				function.clear();
		}
		return result;
	}();

	auto list = lists.find(object.getAIUpdateInterface()->getRetailLuaEventsList().str());
	for (std::size_t depth = 0; list != lists.end() && depth < lists.size(); ++depth)
	{
		const auto stateHandler = list->second.stateHandlers.find(event);
		const auto handler = list->second.handlers.find(event);
		if (stateHandler != list->second.stateHandlers.end() || handler != list->second.handlers.end())
		{
			const auto action = actions.find(stateHandler != list->second.stateHandlers.end() ?
				stateHandler->second : handler->second);
			return action == actions.end() ? StateAction::None : action->second;
		}
		list = lists.find(list->second.parent);
	}
	return StateAction::None;
}
}

#if !RTS_GENERALS
void RetailBuildingEvents::aflame(Object& object)
{
	auto* ai = object.getAIUpdateInterface();
	if (!ai || ai->getRetailLuaEventsList().isEmpty() || object.isEffectivelyDead())
		return;
	const auto& conditions = object.getLogicModelConditions();
	for (const auto& event : buildingEvents().modelConditions)
	{
		if (event.required.test(MODELCONDITION_AFLAME) && conditions.testSetAndClear(event.required, event.forbidden) &&
			findStateAction(object, event.name.c_str()) == StateAction::StartRampage)
		{
			ai->enterRetailRampage();
			return;
		}
	}
}

void RetailBuildingEvents::reallyDamaged(Object& object)
{
	if (findStateAction(object, "ReallyDamaged") == StateAction::StartRampage)
		object.getAIUpdateInterface()->enterRetailRampage();
}
#endif

int RetailBuildingEvents::enrageAction(const Object& object, const char* event)
{
	switch (findStateAction(object, event))
	{
		case StateAction::EndEnrage: return 0;
		case StateAction::StartEnrage: return 1;
		default: return -1;
	}
}
