#pragma once

#include <functional>
#include <string_view>

class Object;

struct LuaDispatchResult
{
	const Object& object;
	std::string_view list;
	std::string_view event;
	std::string_view handler;
	bool ok;
	std::string_view error;
};

using LuaDispatchObserver = std::function<void(const LuaDispatchResult&)>;

void observeLuaDispatch(const LuaDispatchObserver& observer, const std::function<void()>& trigger);
bool triggerLuaModelEvent(Object& object, std::string_view name);
