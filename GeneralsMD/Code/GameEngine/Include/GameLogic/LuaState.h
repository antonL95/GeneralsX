#pragma once

#include <set>
#include <string>
#include <string_view>

struct lua_State;

class LuaState
{
public:
	using Binding = int (*)(lua_State*);
	using PushArguments = int (*)(lua_State*, void*);

	LuaState();
	~LuaState();

	LuaState(const LuaState&) = delete;
	LuaState& operator=(const LuaState&) = delete;

	void bind(const char* name, Binding binding);
	bool load(std::string_view script, const char* chunkName);
	bool hasFunction(const std::string& name) const;
	bool call(const std::string& name, PushArguments push = nullptr, void* data = nullptr);

	static bool isRetailDialect(std::string_view script);

private:
	void logError(const char* name, const char* message);

	lua_State* m_state;
	std::set<std::string> m_loggedErrors;
};
