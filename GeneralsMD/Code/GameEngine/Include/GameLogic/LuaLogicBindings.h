#pragma once

#include "Common/GameType.h"

#include <vector>

class LuaState;
class Object;
class UpgradeTemplate;
struct lua_State;

struct LuaUpgradeChange
{
	ObjectID object;
	const UpgradeTemplate* upgrade;
	bool grant;
};

void bindLogicLua(LuaState& state);
void bindRestoringLua(LuaState& state, std::vector<LuaUpgradeChange>& changes);
void pushLuaObject(lua_State* state, const Object* object);
void applyLuaUpgrade(const LuaUpgradeChange& change);
