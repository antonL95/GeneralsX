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
using LuaDispatchBefore = std::function<void(const Object&, std::string_view, std::string_view, std::string_view)>;

void observeLuaDispatch(const LuaDispatchObserver& observer, const std::function<void()>& trigger,
	const LuaDispatchBefore* before = nullptr);
bool triggerLuaModelEvent(Object& object, std::string_view name);
