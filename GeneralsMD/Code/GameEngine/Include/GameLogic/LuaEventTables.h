#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct LuaEventTables
{
	enum class Kind { Internal, Scripted, ModelCondition, ObjectStatus };

	struct Event
	{
		std::string name;
		Kind kind;
		std::string conditions;
	};

	using Handlers = std::map<std::string, std::string, std::less<>>;

	std::vector<Event> events;
	std::map<std::string, Handlers, std::less<>> lists;

	static std::optional<LuaEventTables> parse(std::string_view source, std::string& error);
};
