#pragma once

#include "GameLogic/LuaState.h"

#include <map>
#include <string>

class AsciiString;
class Drawable;

class ClientLuaEvents
{
public:
	static void dispatch(Drawable* drawable, const AsciiString& event);

private:
	struct Invocation;

	ClientLuaEvents();
	static Invocation* currentInvocation(lua_State* state);
	static int pushArguments(lua_State* state, void* context);
	static int ownerSide(lua_State* state);
	static int capturingSide(lua_State* state);
	static int hideSubObject(lua_State* state);

	LuaState m_state;
	std::map<std::string, std::string, std::less<>> m_handlers;
	Invocation* m_invocation = nullptr;
};
