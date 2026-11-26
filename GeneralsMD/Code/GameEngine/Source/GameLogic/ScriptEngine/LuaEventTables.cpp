#include "GameLogic/LuaEventTables.h"

#include <expat.h>
#include <limits>
#include <memory>
#include <set>
#include <type_traits>

namespace
{
using Kind = LuaEventTables::Kind;

struct List
{
	std::string parent;
	LuaEventTables::Handlers handlers;
};

struct Reader
{
	XML_Parser parser;
	std::size_t depth = 0;
	bool validRoot = false;
	bool doctype = false;
	bool inEvents = false;
	bool inConditions = false;
	List* list = nullptr;
	LuaEventTables::Event* event = nullptr;
	std::vector<LuaEventTables::Event> events;
	std::map<std::string, List, std::less<>> lists;
};

std::optional<Kind> eventKind(std::string_view tag)
{
	if (tag == "InternalEvent")
		return Kind::Internal;
	if (tag == "ScriptedEvent")
		return Kind::Scripted;
	if (tag == "ModelConditionEvent")
		return Kind::ModelCondition;
	if (tag == "ObjectStatusEvent")
		return Kind::ObjectStatus;
	return std::nullopt;
}

std::string attribute(const XML_Char** attributes, std::string_view name)
{
	for (; *attributes; attributes += 2)
		if (attributes[0] == name)
			return attributes[1];
	return {};
}

void XMLCALL startElement(void* userData, const XML_Char* name, const XML_Char** attributes)
{
	auto& reader = *static_cast<Reader*>(userData);
	const std::string_view tag(name);
	switch (++reader.depth)
	{
		case 1:
			reader.validRoot = tag == "SageLuaScriptSection";
			if (!reader.validRoot)
				XML_StopParser(reader.parser, XML_FALSE);
			break;
		case 2:
			reader.inEvents = tag == "Events";
			if (tag == "EventList")
			{
				auto listName = attribute(attributes, "Name");
				if (!listName.empty())
				{
					reader.list = &reader.lists[std::move(listName)];
					reader.list->parent = attribute(attributes, "Inherit");
				}
			}
			break;
		case 3:
			if (reader.inEvents)
			{
				const auto kind = eventKind(tag);
				auto eventName = attribute(attributes, "Name");
				if (kind && !eventName.empty())
					reader.event = &reader.events.emplace_back(LuaEventTables::Event{std::move(eventName), *kind, {}});
			}
			else if (reader.list && tag == "EventHandler")
			{
				auto eventName = attribute(attributes, "EventName");
				auto functionName = attribute(attributes, "ScriptFunctionName");
				if (!eventName.empty() && !functionName.empty())
					reader.list->handlers[std::move(eventName)] = std::move(functionName);
			}
			break;
		case 4:
			reader.inConditions = reader.event && tag == "Conditions";
			break;
	}
}

void XMLCALL endElement(void* userData, const XML_Char*)
{
	auto& reader = *static_cast<Reader*>(userData);
	switch (reader.depth--)
	{
		case 2:
			reader.list = nullptr;
			reader.inEvents = false;
			break;
		case 3:
			reader.event = nullptr;
			break;
		case 4:
			reader.inConditions = false;
			break;
	}
}

void XMLCALL characterData(void* userData, const XML_Char* text, int size)
{
	auto& reader = *static_cast<Reader*>(userData);
	if (reader.inConditions && reader.depth == 4)
		reader.event->conditions.append(text, static_cast<std::size_t>(size));
}

void XMLCALL startDoctype(void* userData, const XML_Char*, const XML_Char*, const XML_Char*, int)
{
	auto& reader = *static_cast<Reader*>(userData);
	reader.doctype = true;
	XML_StopParser(reader.parser, XML_FALSE);
}

LuaEventTables resolve(Reader& reader)
{
	LuaEventTables tables;
	tables.events.reserve(reader.events.size());
	std::set<std::string_view> eventNames;
	for (auto& event : reader.events)
	{
		tables.events.push_back(std::move(event));
		if (!eventNames.insert(tables.events.back().name).second)
			tables.events.pop_back();
	}
	for (const auto& [name, list] : reader.lists)
	{
		auto& handlers = tables.lists[name];
		std::set<const List*> visited;
		for (const List* entry = &list; entry && visited.insert(entry).second;)
		{
			for (const auto& [event, function] : entry->handlers)
				if (eventNames.contains(event))
					handlers.try_emplace(event, function);
			const auto parent = reader.lists.find(entry->parent);
			entry = parent == reader.lists.end() ? nullptr : &parent->second;
		}
	}
	return tables;
}
}

std::optional<LuaEventTables> LuaEventTables::parse(std::string_view source, std::string& error)
{
	error.clear();
	if (source.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
	{
		error = "XML exceeds the parser input size limit";
		return std::nullopt;
	}
	std::unique_ptr<std::remove_pointer_t<XML_Parser>, decltype(&XML_ParserFree)> parser(
		XML_ParserCreate(nullptr), XML_ParserFree);
	if (!parser)
	{
		error = "unable to allocate XML parser";
		return std::nullopt;
	}
	Reader reader{};
	reader.parser = parser.get();
	XML_SetUserData(parser.get(), &reader);
	XML_SetElementHandler(parser.get(), startElement, endElement);
	XML_SetCharacterDataHandler(parser.get(), characterData);
	XML_SetStartDoctypeDeclHandler(parser.get(), startDoctype);
	if (XML_Parse(parser.get(), source.data(), static_cast<int>(source.size()), XML_TRUE) != XML_STATUS_OK)
	{
		if (reader.doctype)
			error = "DOCTYPE is unsupported";
		else if (!reader.validRoot && reader.depth)
			error = "expected SageLuaScriptSection root";
		else
			error = "XML line " + std::to_string(XML_GetCurrentLineNumber(parser.get())) + ": " +
				XML_ErrorString(XML_GetErrorCode(parser.get()));
		return std::nullopt;
	}
	return resolve(reader);
}
