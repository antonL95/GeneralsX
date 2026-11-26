#pragma once

#include "Common/GameType.h"

#include <vector>

class LuaState;
class UpgradeTemplate;

struct LuaUpgradeChange
{
	ObjectID object;
	const UpgradeTemplate* upgrade;
	bool grant;
};

void bindLogicLua(LuaState& state);
void bindRestoringLua(LuaState& state, std::vector<LuaUpgradeChange>& changes);
void applyLuaUpgrade(const LuaUpgradeChange& change);
