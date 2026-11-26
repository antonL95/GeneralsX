#include "GameLogic/LuaState.h"

#include <cstdio>
#include <lua.hpp>

namespace
{
using Index = std::string_view::size_type;

Index longBracketEnd(std::string_view script, Index start)
{
	if (start >= script.size() || script[start] != '[')
		return start;
	Index open = start + 1;
	while (open < script.size() && script[open] == '=')
		++open;
	if (open == script.size() || script[open] != '[')
		return start;
	const Index equals = open - start - 1;
	for (Index close = script.find(']', open + 1); close != script.npos; close = script.find(']', close + 1))
	{
		Index end = close + 1;
		while (end < script.size() && script[end] == '=')
			++end;
		if (end < script.size() && script[end] == ']' && end - close - 1 == equals)
			return end + 1;
	}
	return script.size();
}

bool isIdentifierStart(char ch)
{
	return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_';
}

bool isIdentifierCharacter(char ch)
{
	return isIdentifierStart(ch) || (ch >= '0' && ch <= '9');
}

const char* stableString(lua_State* state, int index, size_t* length)
{
	if (const char* text = lua_tolstring(state, index, length))
		return text;
	const char* text = lua_isboolean(state, index)
		? (lua_toboolean(state, index) ? "true" : "false")
		: luaL_typename(state, index);
	*length = std::char_traits<char>::length(text);
	return text;
}

int toString(lua_State* state)
{
	luaL_checkany(state, 1);
	size_t length;
	const char* text = stableString(state, 1, &length);
	lua_pushlstring(state, text, length);
	return 1;
}

int print(lua_State* state)
{
	std::fputs("Lua: ", stderr);
	const int count = lua_gettop(state);
	for (int index = 1; index <= count; ++index)
	{
		if (index > 1)
			std::fputc('\t', stderr);
		size_t length;
		const char* text = stableString(state, index, &length);
		std::fwrite(text, 1, length, stderr);
	}
	std::fputc('\n', stderr);
	std::fflush(stderr);
	return 0;
}

int openBase(lua_State* state)
{
	luaopen_base(state);
	lua_newtable(state);
	lua_getfield(state, -2, "tonumber");
	lua_setfield(state, -2, "tonumber");
	lua_pushcfunction(state, toString);
	lua_setfield(state, -2, "tostring");
	lua_pushcfunction(state, print);
	lua_setfield(state, -2, "print");
	lua_rawseti(state, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
	return 0;
}

struct Call
{
	const char* name;
	LuaState::PushArguments push;
	void* data;
};

int callHandler(lua_State* state)
{
	const auto* call = static_cast<const Call*>(lua_touserdata(state, 1));
	lua_getglobal(state, call->name);
	if (!lua_isfunction(state, -1))
		return luaL_error(state, "handler is not a function");
	const int count = call->push ? call->push(state, call->data) : 0;
	lua_call(state, count, 0);
	return 0;
}
}

LuaState::LuaState() : m_state(luaL_newstate())
{
	if (!m_state)
		return;
	lua_pushcfunction(m_state, openBase);
	if (lua_pcall(m_state, 0, 0, 0) != LUA_OK)
	{
		logError("sandbox", lua_tostring(m_state, -1));
		lua_close(m_state);
		m_state = nullptr;
	}
}

LuaState::~LuaState()
{
	if (m_state)
		lua_close(m_state);
}

void LuaState::bind(const char* name, Binding binding)
{
	if (!m_state)
		return;
	lua_pushcfunction(m_state, binding);
	lua_setglobal(m_state, name);
}

bool LuaState::load(std::string_view script, const char* chunkName)
{
	if (!m_state)
		return false;
	if (!isRetailDialect(script))
	{
		logError(chunkName, "unsupported token in retail Lua script");
		return false;
	}
	if (luaL_loadbufferx(m_state, script.data(), script.size(), chunkName, "t") == LUA_OK &&
		lua_pcall(m_state, 0, 0, 0) == LUA_OK)
		return true;
	logError(chunkName, lua_tostring(m_state, -1));
	lua_pop(m_state, 1);
	return false;
}

bool LuaState::hasFunction(const std::string& name) const
{
	if (!m_state)
		return false;
	const bool found = lua_getglobal(m_state, name.c_str()) == LUA_TFUNCTION;
	lua_pop(m_state, 1);
	return found;
}

bool LuaState::call(const std::string& name, PushArguments push, void* data)
{
	if (!m_state)
		return false;
	Call call{name.c_str(), push, data};
	lua_pushcfunction(m_state, callHandler);
	lua_pushlightuserdata(m_state, &call);
	if (lua_pcall(m_state, 1, 0, 0) == LUA_OK)
		return true;
	logError(name.c_str(), lua_tostring(m_state, -1));
	lua_pop(m_state, 1);
	return false;
}

void LuaState::logError(const char* name, const char* message)
{
	if (m_loggedErrors.emplace(name).second)
	{
		std::fprintf(stderr, "Lua %s: %s\n", name, message ? message : "unknown error");
		std::fflush(stderr);
	}
}

bool LuaState::isRetailDialect(std::string_view script)
{
	for (Index index = 0; index < script.size();)
	{
		const char ch = script[index];
		const char next = index + 1 < script.size() ? script[index + 1] : '\0';
		if (ch == '\'' || ch == '"')
		{
			while (++index < script.size() && script[index] != ch)
				if (script[index] == '\\' && index + 1 < script.size())
					++index;
			if (index < script.size())
				++index;
			continue;
		}
		if (ch == '-' && next == '-')
		{
			index += 2;
			const Index end = longBracketEnd(script, index);
			index = end == index ? script.find_first_of("\r\n", index) : end;
			continue;
		}
		if (ch == '[')
		{
			const Index end = longBracketEnd(script, index);
			index = end == index ? index + 1 : end;
			continue;
		}
		if (isIdentifierStart(ch))
		{
			const Index start = index++;
			while (index < script.size() && isIdentifierCharacter(script[index]))
				++index;
			if (script.substr(start, index - start) == "goto")
				return false;
			continue;
		}
		if (ch == '&' || ch == '|' || (ch == '~' && next != '=') ||
			(ch == next && (ch == '/' || ch == '<' || ch == '>' || ch == ':')))
			return false;
		++index;
	}
	return true;
}
