#include "PreRTS.h"

#include "GameLogic/RetailCreationUpgrades.h"

#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "Common/FileSystem.h"
#include "Common/Player.h"
#include "Common/RetailFileLayers.h"
#include "Common/Upgrade.h"
#include "Common/Xfer.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object.h"

namespace
{
using Names = std::vector<std::string>;
using GrantLists = std::map<std::string, Names, std::less<>>;

struct EventList
{
	std::string parent;
	std::string onCreated;
};
using EventLists = std::map<std::string, EventList, std::less<>>;

GrantLists s_grants;

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
	char* contents = file->readEntireAndClose();
	if (!contents)
		return {};
	std::string data(contents, size);
	delete[] contents;
	return data;
}

std::string_view trim(std::string_view text)
{
	const auto first = text.find_first_not_of(" \t\r\n");
	if (first == text.npos)
		return {};
	return text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
}

std::string_view takeName(std::string_view& text)
{
	text = trim(text);
	const auto end = text.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_");
	const auto name = text.substr(0, end);
	text.remove_prefix(name.size());
	return name;
}

bool take(std::string_view& text, std::string_view prefix)
{
	text = trim(text);
	if (!text.starts_with(prefix))
		return false;
	text.remove_prefix(prefix.size());
	return true;
}

std::string_view takeQuoted(std::string_view& text)
{
	text = trim(text);
	if (text.empty() || (text.front() != '"' && text.front() != '\''))
		return {};
	const auto end = text.find(text.front(), 1);
	if (end == text.npos)
		return {};
	const auto value = text.substr(1, end - 1);
	text.remove_prefix(end + 1);
	return value;
}

std::string attribute(std::string_view attributes, std::string_view key)
{
	while (!trim(attributes).empty())
	{
		const auto name = takeName(attributes);
		if (name.empty() || !take(attributes, "="))
			break;
		const auto value = takeQuoted(attributes);
		if (name == key)
			return std::string(value);
	}
	return {};
}

EventLists readEvents(std::string_view text)
{
	EventLists lists;
	EventList* current = nullptr;
	while (!text.empty())
	{
		const auto begin = text.find('<');
		if (begin == text.npos)
			break;
		text.remove_prefix(begin + 1);
		if (text.starts_with("!--"))
		{
			const auto end = text.find("-->");
			if (end == text.npos)
				break;
			text.remove_prefix(end + 3);
			continue;
		}
		const auto end = text.find('>');
		if (end == text.npos)
			break;
		auto tag = trim(text.substr(0, end));
		text.remove_prefix(end + 1);
		if (tag == "/EventList")
			current = nullptr;
		const auto name = takeName(tag);
		if (name == "EventList")
		{
			const auto list = attribute(tag, "Name");
			const bool selfClosing = tag.ends_with('/');
			current = list.empty() ? nullptr : &lists[list];
			if (current)
				current->parent = attribute(tag, "Inherit");
			if (selfClosing)
				current = nullptr;
		}
		if (current && name == "EventHandler" && attribute(tag, "EventName") == "OnCreated")
			current->onCreated = attribute(tag, "ScriptFunctionName");
	}
	return lists;
}

GrantLists readLeadingGrants(std::string_view script)
{
	GrantLists functions;
	Names* prefix = nullptr;
	std::string parameter;
	bool longComment = false;
	while (!script.empty())
	{
		const auto end = script.find('\n');
		auto line = trim(script.substr(0, end));
		script.remove_prefix(end == script.npos ? script.size() : end + 1);
		if (longComment || line.starts_with("--[["))
		{
			longComment = line.find("]]") == line.npos;
			continue;
		}
		if (line.empty() || line.starts_with("--"))
			continue;
		const auto word = takeName(line);
		if (word == "function")
		{
			const auto name = takeName(line);
			prefix = nullptr;
			if (name.empty())
				continue;
			auto& grants = functions[std::string(name)];
			grants.clear();
			if (!take(line, "("))
				continue;
			parameter = takeName(line);
			if (!parameter.empty() && take(line, ")") && trim(line).empty())
				prefix = &grants;
			continue;
		}
		if (!prefix)
			continue;
		if (word != "ObjectGrantUpgrade" || !take(line, "(") || takeName(line) != parameter || !take(line, ","))
		{
			prefix = nullptr;
			continue;
		}
		const auto upgrade = takeQuoted(line);
		auto identifier = upgrade;
		if (upgrade.empty() || takeName(identifier) != upgrade || !take(line, ")"))
		{
			prefix = nullptr;
			continue;
		}
		line = trim(line);
		if (line.starts_with(";"))
			line = trim(line.substr(1));
		if (!line.empty() && !line.starts_with("--"))
		{
			prefix = nullptr;
			continue;
		}
		prefix->emplace_back(upgrade);
	}
	return functions;
}

const Names* resolve(const EventLists& lists, const GrantLists& functions, const EventList& root)
{
	const EventList* list = &root;
	for (size_t depth = 0; depth < lists.size(); ++depth)
	{
		if (!list->onCreated.empty())
		{
			const auto handler = functions.find(list->onCreated);
			return handler == functions.end() ? nullptr : &handler->second;
		}
		const auto parent = lists.find(list->parent);
		if (parent == lists.end())
			break;
		list = &parent->second;
	}
	return nullptr;
}
}

void RetailCreationUpgrades::load(Xfer* crc)
{
	s_grants.clear();
	if (!RetailFileLayers::enabled())
		return;
	const auto lists = readEvents(readData("Data\\Scripts\\ScriptEvents.xml"));
	const auto functions = readLeadingGrants(readData("Data\\Scripts\\Scripts.lua"));
	for (const auto& [name, list] : lists)
		if (const Names* grants = resolve(lists, functions, list); grants && !grants->empty())
			s_grants.emplace(name, *grants);
	Int count = static_cast<Int>(s_grants.size());
	crc->xferInt(&count);
	for (const auto& [eventList, grants] : s_grants)
	{
		AsciiString list(eventList.c_str());
		crc->xferAsciiString(&list);
		Int size = static_cast<Int>(grants.size());
		crc->xferInt(&size);
		for (const auto& grant : grants)
		{
			AsciiString name(grant.c_str());
			crc->xferAsciiString(&name);
		}
	}
}

void RetailCreationUpgrades::onObjectCreated(Object* object)
{
	if (!RetailFileLayers::enabled() || !object->getAI() || !TheUpgradeCenter)
		return;
	Player* owner = object->getControllingPlayer();
	if (!owner)
		return;
	const auto found = s_grants.find(object->getAI()->getRetailLuaEventList().str());
	if (found == s_grants.end())
		return;
	for (const auto& name : found->second)
	{
		const auto* upgrade = TheUpgradeCenter->findUpgrade(name.c_str());
		if (upgrade && upgrade->getUpgradeType() == UPGRADE_TYPE_PLAYER && !owner->hasUpgradeComplete(upgrade))
			owner->addUpgrade(upgrade, UPGRADE_STATUS_COMPLETE);
	}
}
