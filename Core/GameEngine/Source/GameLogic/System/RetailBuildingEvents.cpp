#include "PreRTS.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <locale>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "Common/File.h"
#include "Common/FileSystem.h"
#include "Common/RetailFileLayers.h"
#include "Common/SpecialPower.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "GameClient/Drawable.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/LogicRandomValue.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/RetailBuildingEvents.h"

namespace
{
enum class Action { Grant, Power, Visibility, Roll };

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

struct EventList
{
	std::string parent;
	std::string handler;
	std::map<std::string, std::string> enrageHandlers;
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

std::map<std::string, EventList> readEventLists(std::string xml)
{
	for (std::size_t start = xml.find("<!--"); start != std::string::npos; start = xml.find("<!--", start))
	{
		const auto end = xml.find("-->", start + 4);
		xml.erase(start, end == std::string::npos ? end : end + 3 - start);
	}
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
			if (event == "OnBuildingComplete")
				entry.handler = attribute(fields, "ScriptFunctionName");
			else if (event == "OnAflame" || event == "OnQuenched" ||
				event == "User45IsActive" || event == "User45IsNotActive")
				entry.enrageHandlers[event] = attribute(fields, "ScriptFunctionName");
		}
		start = listEnd + 12;
	}
	return lists;
}

bool compile(const std::vector<std::string>& lines, Program& program)
{
	static const std::regex roll(R"(local\s+(\w+)\s*=\s*GetRandomNumber\(\s*\))");
	static const std::regex branch(R"((if|elseif)\s+(\w+)\s*<=\s*([0-9]+(?:\.[0-9]+)?)\s+then)");
	static const std::regex call(R"re((ObjectGrantUpgrade|ObjectDoSpecialPower|ObjectHideSubObjectPermanently)\(\s*self\s*,\s*"([A-Za-z0-9_]+)"(?:\s*,\s*(true|false))?\s*\))re");
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
	std::map<std::string, EventList> lists;
	std::map<std::string, Program> programs;

	BuildingEvents() : lists(readEventLists(readData("Data\\Scripts\\ScriptEvents.xml")))
	{
		std::map<std::string, std::vector<std::string>> bodies;
		for (const auto& [name, list] : lists)
			if (!list.handler.empty())
				bodies.try_emplace(list.handler);

		std::istringstream lua(readData("Data\\Scripts\\Scripts.lua"));
		const std::regex definition(R"(^\s*function\s+(\w+)\s*\(([^)]*)\).*)");
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
				body = found != bodies.end() && match[2] == "self" ? &found->second : nullptr;
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
			else
				std::fprintf(stderr, "Building completion: unsupported callback %s\n", name.c_str());
		}
		if (std::getenv("GENERALSX_TEST_COMMANDS"))
			std::fprintf(stderr, "BPARITY building callbacks %zu/%zu\n", programs.size(), bodies.size());
	}
};
}

void RetailBuildingEvents::complete(Object& object)
{
	if (!RetailFileLayers::enabled() || !object.getAIUpdateInterface())
		return;
	const AsciiString& listName = object.getAIUpdateInterface()->getRetailLuaEventsList();
	if (listName.isEmpty())
		return;
	static const BuildingEvents events;
	auto list = events.lists.find(listName.str());
	for (std::size_t depth = 0; list != events.lists.end() && depth < events.lists.size(); ++depth)
	{
		if (list->second.handler.empty())
		{
			list = events.lists.find(list->second.parent);
			continue;
		}
		const auto found = events.programs.find(list->second.handler);
		if (found == events.programs.end())
			return;
		if (std::getenv("GENERALSX_TEST_COMMANDS"))
			std::fprintf(stderr, "BPARITY complete %u %s %s %zu\n", TheGameLogic->getFrame(),
				object.getTemplate()->getName().str(), list->second.handler.c_str(), found->second.size());

		std::vector<double> rolls;
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
			case Action::Power:
				if (const auto* power = TheSpecialPowerStore->findSpecialPowerTemplate(operation.name.c_str()))
					object.doSpecialPower(power, 0, TRUE);
				break;
			case Action::Visibility:
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
		return;
	}
}

int RetailBuildingEvents::enrageAction(const Object& object, const char* event)
{
	if (!RetailFileLayers::enabled() || !object.getAIUpdateInterface())
		return -1;
	static const auto lists = readEventLists(readData("Data\\Scripts\\ScriptEvents.xml"));
	static const auto actions = []
	{
		std::map<std::string, int> result;
		std::istringstream lua(readData("Data\\Scripts\\Scripts.lua"));
		const std::regex definition(R"(^\s*function\s+(\w+)\s*\(self\)\s*$)");
		const std::regex enrage(R"(^\s*ObjectSetEnragedState\(\s*self\s*,\s*(true|false)\s*\)\s*$)");
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
				result[function] = match[1] == "true" ? 1 : 0;
			else if (line.find_first_not_of(" \t\r") != std::string::npos &&
				line.find("ObjectGrantUpgrade") == std::string::npos && line.find("ObjectRemoveUpgrade") == std::string::npos)
				function.clear();
		}
		return result;
	}();

	auto list = lists.find(object.getAIUpdateInterface()->getRetailLuaEventsList().str());
	for (std::size_t depth = 0; list != lists.end() && depth < lists.size(); ++depth)
	{
		const auto handler = list->second.enrageHandlers.find(event);
		if (handler != list->second.enrageHandlers.end())
		{
			const auto action = actions.find(handler->second);
			return action == actions.end() ? -1 : action->second;
		}
		list = lists.find(list->second.parent);
	}
	return -1;
}
