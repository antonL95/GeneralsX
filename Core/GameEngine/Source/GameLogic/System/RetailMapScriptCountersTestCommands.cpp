#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if RTS_ZEROHOUR
#include "Common/INI.h"
#include "Common/Player.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/ExperienceLevelStore.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Scripts.h"

#include <cstdio>
#include <fstream>
#include <sstream>

namespace
{
const char* const lhs = "MapScriptCountersLeft";
const char* const rhs = "MapScriptCountersRight";
const char* const copied = "MapScriptCountersCopy";
const char* const unitName = "MapScriptCountersNamedUnit";
const char* const experience = "MapScriptCountersXP";
const char* const threat = "MapScriptCountersThreat";
const char* const living = "MapScriptCountersLiving";
const char* const includingDead = "MapScriptCountersIncludingDead";

Bool run(const char* name, const char* first, const char* second, const char* third, Int op = 0, Int value = 0)
{
	for (Int i = 0; i < ScriptEngine::getActionTemplateCount(); ++i)
	{
		if (TheScriptEngine->getActionTemplate(i)->m_internalName != name)
			continue;
		auto* action = newInstance(ScriptAction)(static_cast<ScriptAction::ScriptActionType>(i));
		action->getParameter(0)->friend_setString(first);
		action->getParameter(1)->friend_setString(second);
		action->getParameter(1)->friend_setInt(op);
		if (Parameter* parameter = action->getParameter(2))
		{
			parameter->friend_setString(third);
			parameter->friend_setInt(value);
			parameter->friend_setReal(static_cast<Real>(value));
		}
		TheScriptEngine->friend_executeAction(action, nullptr);
		deleteInstance(action);
		return TRUE;
	}
	return FALSE;
}

Int read(const char* name)
{
	const TCounter* counter = TheScriptEngine->getCounter(name);
	return counter ? counter->value : -1;
}

Bool compareCounters(Int comparison, const char* left, const char* right)
{
	for (Int i = 0; i < Condition::NUM_ITEMS; ++i)
	{
		if (TheScriptEngine->getConditionTemplate(i)->m_internalName != "COUNTER_COUNTER")
			continue;
		auto* condition = newInstance(Condition)(static_cast<Condition::ConditionType>(i));
		condition->getParameter(0)->friend_setString(left);
		condition->getParameter(1)->friend_setInt(comparison);
		condition->getParameter(2)->friend_setString(right);
		auto* clause = newInstance(OrCondition);
		clause->setFirstAndCondition(condition);
		auto* script = newInstance(Script);
		script->setOrCondition(clause);
		const Bool result = TheScriptEngine->evaluateConditions(script, nullptr);
		deleteInstance(script);
		return result;
	}
	return FALSE;
}

void loadLevels(const RetailTestCommands::Context& ctx)
{
	std::ifstream file(ctx.argument);
	if (!file)
		return;
	std::ostringstream data;
	data << file.rdbuf();
	INI ini;
	ini.loadText(ctx.argument, data.str().c_str(), INI_LOAD_OVERWRITE, "ExperienceLevel", ExperienceLevelStore::parseDefinition);
}

void calculate(const RetailTestCommands::Context& ctx)
{
	Object* unit = ctx.requireObject();
	const Object* enemy = ctx.findTarget("PlyrCreeps:MapScriptCountersFixtureUnit:0", true);
	if (!unit || !enemy)
		return;
	unit->setName(unitName);
	TheScriptEngine->addObjectToCache(unit);
	unit->getExperienceTracker()->advanceRetailLevel(20.0f, FALSE);
	run("SET_COUNTER", lhs, "", "", 12);
	run("SET_COUNTER", rhs, "", "", 4);
	run("COUNTER_MATH_COUNTER", lhs, "", rhs, 0);
	const Int sum = read(lhs);
	run("COUNTER_MATH_COUNTER", lhs, "", rhs, 2);
	const Int product = read(lhs);
	run("COUNTER_MATH_VALUE", lhs, "", "", 1, 4);
	const Int difference = read(lhs);
	run("COUNTER_MATH_VALUE", lhs, "", "", 3, 5);
	run("COUNTER_MATH_VALUE", lhs, "", "", 3, 0);
	run("SET_COUNTER_TO_COUNTER", copied, lhs, "");
	run("SET_UNIT_EXPERIENCE_TO_COUNTER", unitName, experience, "");
	Player* player = unit->getControllingPlayer();
	const Bool alreadyHostile = player->getRelationship(enemy->getTeam()) == ENEMIES;
	if (!alreadyHostile)
		player->setTeamRelationship(enemy->getTeam(), ENEMIES);
	run("SET_COUNTER_TO_UNIT_THREAT", threat, unitName, "", 0, 30);
	if (read(threat) != 7)
	{
		Int x, y;
		ThePartitionManager->worldToCell(enemy->getPosition()->x, enemy->getPosition()->y, &x, &y);
		PartitionCell* cell = ThePartitionManager->getCellAt(x, y);
		std::fprintf(stderr, "TEST map-script-counters threat input frame=%u raw=%u hostile=%d cell=%d offMap=%d inert=%d\n",
			ctx.frame, enemy->getTemplate()->getThreatValue(), player->getRelationship(enemy->getTeam()) == ENEMIES,
			cell != nullptr, enemy->isOffMap(), enemy->isKindOf(KINDOF_INERT));
	}
	if (!alreadyHostile)
		player->removeTeamRelationship(enemy->getTeam());
	std::fprintf(stderr, "TEST map-script-counters math frame=%u sum=%d product=%d difference=%d result=%d\n",
		ctx.frame, sum, product, difference, read(lhs));
}

void ownership(const RetailTestCommands::Context& ctx)
{
	Object* unit = ctx.requireObject();
	if (!unit)
		return;
	unit->setEffectivelyDead(TRUE);
	run("SET_PLAYER_OWNERSHIP_OF_TYPE_COUNTER", "MapScriptCountersFixtureUnit", "Player_1", living);
	run("SET_PLAYER_OWNERSHIP_OF_TYPE_COUNTER_INCLUDE_DEAD", "MapScriptCountersFixtureUnit", "<All Players>", includingDead);
	unit->setEffectivelyDead(FALSE);
}

void probe(const RetailTestCommands::Context& ctx)
{
	const Bool equal = compareCounters(Parameter::EQUAL, copied, lhs);
	const Bool greater = compareCounters(Parameter::GREATER, lhs, rhs);
	const Bool less = compareCounters(Parameter::LESS_THAN, lhs, rhs);
	std::fprintf(stderr, "TEST map-script-counters state frame=%u copy=%d xp=%d threat=%d living=%d all=%d equal=%d greater=%d less=%d\n",
		ctx.frame, read(copied), read(experience), read(threat), read(living), read(includingDead), equal, greater, less);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailMapScriptCountersTestCommands()
{
#if RTS_ZEROHOUR
	registerAction("map-script-counters-load-levels", loadLevels);
	registerAction("map-script-counters-calculate", calculate);
	registerAction("map-script-counters-ownership", ownership);
	registerAction("map-script-counters-probe", probe);
#endif
}
}
