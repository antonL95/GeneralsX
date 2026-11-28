#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/Team.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/PolygonTrigger.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Scripts.h"

namespace
{
const char* const fixtureUnit = "ScriptPlayerSumUnit";
const char* const fixtureUpgrade = "ScriptPlayerSumUpgrade";
const char* const fixtureArea = "ScriptPlayerSumGateArea";
const char* const anchor0Name = "ScriptPlayerSumAnchor0";
const char* const anchor1Name = "ScriptPlayerSumAnchor1";

Condition* conditionByName(const char* name)
{
	for (Int i = 0; i < Condition::NUM_ITEMS; ++i)
		if (TheScriptEngine->getConditionTemplate(i)->m_internalName == name)
			return newInstance(Condition)(static_cast<Condition::ConditionType>(i));
	return nullptr;
}

Bool evaluate(Condition* condition, Team* team)
{
	if (!condition)
		return FALSE;
	auto* clause = newInstance(OrCondition);
	clause->setFirstAndCondition(condition);
	auto* script = newInstance(Script);
	script->setOrCondition(clause);
	const Bool result = TheScriptEngine->evaluateConditions(script, team);
	deleteInstance(script);
	return result;
}

Bool evaluateAreaType(const char* conditionName, Int comparison, Int count, Team* team, Bool withUpgrade)
{
	Condition* condition = conditionByName(conditionName);
	if (!condition)
		return FALSE;
	condition->getParameter(0)->friend_setString("<This Player's Enemies>");
	condition->getParameter(1)->friend_setInt(comparison);
	condition->getParameter(2)->friend_setInt(count);
	condition->getParameter(3)->friend_setString(fixtureUnit);
	condition->getParameter(4)->friend_setString(fixtureArea);
	if (withUpgrade)
		condition->getParameter(5)->friend_setString(fixtureUpgrade);
	return evaluate(condition, team);
}

Bool evaluateAreaKind(Int comparison, Int count, Team* team)
{
	Condition* condition = conditionByName("PLAYER_HAS_COMPARISON_UNIT_KIND_IN_TRIGGER_AREA");
	if (!condition)
		return FALSE;
	condition->getParameter(0)->friend_setString("<This Player's Enemies>");
	condition->getParameter(1)->friend_setInt(comparison);
	condition->getParameter(2)->friend_setInt(count);
	condition->getParameter(3)->friend_setInt(KINDOF_INFANTRY);
	condition->getParameter(4)->friend_setString(fixtureArea);
	return evaluate(condition, team);
}

Bool evaluateObjectComparison(Int comparison, Int count, Team* team)
{
	Condition* condition = conditionByName("PLAYER_HAS_OBJECT_COMPARISON");
	if (!condition)
		return FALSE;
	condition->getParameter(0)->friend_setString("<This Player's Enemies>");
	condition->getParameter(1)->friend_setInt(comparison);
	condition->getParameter(2)->friend_setInt(count);
	condition->getParameter(3)->friend_setString(fixtureUnit);
	return evaluate(condition, team);
}

Bool evaluateBuildings(const char* side, Int count, Team* team)
{
	Condition* condition = conditionByName("PLAYER_HAS_N_OR_FEWER_BUILDINGS");
	if (!condition)
		return FALSE;
	condition->getParameter(0)->friend_setString(side);
	condition->getParameter(1)->friend_setInt(count);
	return evaluate(condition, team);
}

Bool evaluateDistanceFromObject(Int comparison, Int count, Real distance, Team* team)
{
	Condition* condition = conditionByName("PLAYER_HAS_NUMBER_UNITS_DISTANCE_FROM_OBJECT");
	if (!condition)
		return FALSE;
	condition->getParameter(0)->friend_setString("<This Player's Enemies>");
	condition->getParameter(1)->friend_setInt(comparison);
	condition->getParameter(2)->friend_setInt(count);
	condition->getParameter(3)->friend_setReal(distance);
	condition->getParameter(4)->friend_setString(anchor0Name);
	return evaluate(condition, team);
}

Bool evaluateUnitsWithUpgrade(const char* side, Int count, Team* team)
{
	Condition* condition = conditionByName("PLAYER_HAS_NUM_UNITS_WITH_UPGRADE");
	if (!condition)
		return FALSE;
	condition->getParameter(0)->friend_setString(side);
	condition->getParameter(1)->friend_setInt(count);
	condition->getParameter(2)->friend_setString(fixtureUpgrade);
	return evaluate(condition, team);
}

Bool evaluateDistanceBetween(Int comparison, Real distance, Team* team)
{
	Condition* condition = conditionByName("DISTANCE_BETWEEN_OBJ");
	if (!condition)
		return FALSE;
	condition->getParameter(0)->friend_setString(anchor0Name);
	condition->getParameter(1)->friend_setString(anchor1Name);
	condition->getParameter(2)->friend_setInt(comparison);
	condition->getParameter(3)->friend_setReal(distance);
	return evaluate(condition, team);
}

Bool evaluateBases(const char* side, Int count, Team* team)
{
	Condition* condition = conditionByName("PLAYER_HAS_N_OR_FEWER_BASES");
	if (!condition)
		return FALSE;
	condition->getParameter(0)->friend_setString(side);
	condition->getParameter(1)->friend_setInt(count);
	return evaluate(condition, team);
}

void setup(const RetailTestCommands::Context& ctx)
{
	Object* anchor0 = ctx.findTarget("Player_1:ScriptPlayerSumAnchor:0", true);
	Object* anchor1 = ctx.findTarget("Player_1:ScriptPlayerSumAnchor:1", true);
	Object* enemyUnit0 = ctx.findTarget("PlyrCivilian:ScriptPlayerSumUnit:0", true);
	const UpgradeTemplate* upgrade = TheUpgradeCenter->findUpgrade(fixtureUpgrade);
	if (!anchor0 || !anchor1 || !enemyUnit0 || !upgrade)
		return;

	anchor0->setName(anchor0Name);
	anchor1->setName(anchor1Name);
	TheScriptEngine->addObjectToCache(anchor0);
	TheScriptEngine->addObjectToCache(anchor1);
	enemyUnit0->giveUpgrade(upgrade);
}

void addArea(const RetailTestCommands::Context&)
{
	if (PolygonTrigger::findByName(fixtureArea))
		return;
	auto* area = newInstance(PolygonTrigger)(4, 79101);
	area->setTriggerName(fixtureArea);
	area->addPoint({1600, 1450, 0});
	area->addPoint({1700, 1450, 0});
	area->addPoint({1700, 1550, 0});
	area->addPoint({1600, 1550, 0});
	PolygonTrigger::addPolygonTrigger(area);
}

void upgradeCreep(const RetailTestCommands::Context& ctx)
{
	Player* creeps = ctx.findPlayer("PlyrCreeps");
	const UpgradeTemplate* upgrade = TheUpgradeCenter->findUpgrade(fixtureUpgrade);
	const PolygonTrigger* area = PolygonTrigger::findByName(fixtureArea);
	if (!creeps || !upgrade || !area)
		return;
	for (Object* obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject())
	{
		if (obj->getControllingPlayer() == creeps && !obj->isEffectivelyDead() &&
			obj->getTemplate()->getName() == fixtureUnit && obj->isInside(area))
			obj->giveUpgrade(upgrade);
	}
}

void removeArea(const RetailTestCommands::Context&)
{
	if (PolygonTrigger* area = PolygonTrigger::findByName(fixtureArea))
	{
		PolygonTrigger::removePolygonTrigger(area);
		deleteInstance(area);
	}
}

void check(const RetailTestCommands::Context& ctx)
{
	Player* player1 = ctx.findPlayer("Player_1");
	Player* civilian = ctx.findPlayer("PlyrCivilian");
	Player* creeps = ctx.findPlayer("PlyrCreeps");
	Object* anchor0 = ctx.findTarget("Player_1:ScriptPlayerSumAnchor:0", true);
	Object* anchor1 = ctx.findTarget("Player_1:ScriptPlayerSumAnchor:1", true);
	Object* baseObj = ctx.findTarget("PlyrCivilian:ScriptPlayerSumBase:0", true);
	if (!player1 || !civilian || !creeps || !anchor0 || !anchor1 || !baseObj)
		return;

	anchor0->setName(anchor0Name);
	anchor1->setName(anchor1Name);
	TheScriptEngine->addObjectToCache(anchor0);
	TheScriptEngine->addObjectToCache(anchor1);

	const Relationship prevCiv = player1->getRelationship(civilian->getDefaultTeam());
	const Relationship prevCreeps = player1->getRelationship(creeps->getDefaultTeam());
	player1->setTeamRelationship(civilian->getDefaultTeam(), ENEMIES);
	player1->setTeamRelationship(creeps->getDefaultTeam(), ENEMIES);

	PlayerMaskType enemies = 0;
	for (Int i = 0; i < ThePlayerList->getPlayerCount(); ++i)
	{
		const Player* candidate = ThePlayerList->getNthPlayer(i);
		if (candidate && candidate != player1 && candidate->getDefaultTeam() && !candidate->isPlayerObserver() &&
			player1->getRelationship(candidate->getDefaultTeam()) == ENEMIES)
			enemies |= candidate->getPlayerMask();
	}

	Team* team = player1->getDefaultTeam();
	Int totalEnemyBuildings = 0;
	for (Int i = 0; i < ThePlayerList->getPlayerCount(); ++i)
	{
		Player* candidate = ThePlayerList->getNthPlayer(i);
		if (candidate && (enemies & candidate->getPlayerMask()))
			totalEnemyBuildings += candidate->countBuildings();
	}
	const Bool bldLess = totalEnemyBuildings >= 2 &&
		!evaluateBuildings("<This Player's Enemies>", totalEnemyBuildings - 1, team) &&
		evaluateBuildings("<This Player's Enemies>", totalEnemyBuildings, team);

	Int farBaseline = 0;
	const Coord3D center = *anchor0->getPosition();
	for (const Object* obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject())
	{
		const Player* owner = obj->getControllingPlayer();
		if (!owner || !(enemies & owner->getPlayerMask()) || obj->isEffectivelyDead())
			continue;
		if (obj->isKindOf(KINDOF_HORDE))
			continue;
		if (!(obj->isKindOf(KINDOF_INFANTRY) || obj->isKindOf(KINDOF_VEHICLE) ||
			obj->isKindOf(KINDOF_MONSTER) || obj->isKindOf(KINDOF_CAVALRY) || obj->isKindOf(KINDOF_HERO)))
			continue;
		const Real dx = center.x - obj->getPosition()->x;
		const Real dy = center.y - obj->getPosition()->y;
		if (dx * dx + dy * dy >= 100.0f * 100.0f)
			++farBaseline;
	}
	const Bool distFar = farBaseline >= 3 && evaluateDistanceFromObject(Parameter::EQUAL, farBaseline, 100.0f, team);

	const Coord3D savedAnchor1 = *anchor1->getPosition();
	Coord3D elevated = savedAnchor1;
	elevated.z += 150.0f;
	anchor1->setPosition(&elevated);
	const Bool dist3d = !evaluateDistanceBetween(Parameter::LESS_THAN, 100.0f, team) &&
		evaluateDistanceBetween(Parameter::GREATER, 140.0f, team);
	anchor1->setPosition(&savedAnchor1);

	Int civBases = 0;
	for (const Object* obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject())
		if (obj->getControllingPlayer() == civilian && !obj->isEffectivelyDead() && obj->isKindOf(KINDOF_VITAL_FOR_BASE_SURVIVAL))
			++civBases;
	const Bool baseOk = civBases >= 1 && baseObj->isKindOf(KINDOF_VITAL_FOR_BASE_SURVIVAL) &&
		!evaluateBases("PlyrCivilian", civBases - 1, team) && evaluateBases("PlyrCivilian", civBases, team);

	if (PolygonTrigger::findByName(fixtureArea))
	{
		const Bool typeLt1 = evaluateAreaType("PLAYER_HAS_COMPARISON_UNIT_TYPE_IN_TRIGGER_AREA", Parameter::LESS_THAN, 1, team, FALSE);
		const Bool typeEq1 = evaluateAreaType("PLAYER_HAS_COMPARISON_UNIT_TYPE_IN_TRIGGER_AREA", Parameter::EQUAL, 1, team, FALSE);
		const Bool builtLt1 = evaluateAreaType("PLAYER_HAS_COMPARISON_UNIT_TYPE_IN_TRIGGER_AREA_COMPLETELY_BUILT", Parameter::LESS_THAN, 1, team, FALSE);
		const Bool upgAreaLt1 = evaluateAreaType("PLAYER_HAS_COMPARISON_UNIT_TYPE_IN_TRIGGER_AREA_WITH_UPGRADE", Parameter::LESS_THAN, 1, team, TRUE);
		const Bool kindLt1 = evaluateAreaKind(Parameter::LESS_THAN, 1, team);
		const Bool objGt3 = evaluateObjectComparison(Parameter::GREATER, 3, team);
		const Bool upgSingle = !evaluateUnitsWithUpgrade("<This Player's Enemies>", 2, team) &&
			evaluateUnitsWithUpgrade("<This Player's Enemies>", 1, team);
		std::fprintf(stderr,
			"TEST script-player-set-sum-area frame=%u typeLt1=%d typeEq1=%d builtLt1=%d upgAreaLt1=%d kindLt1=%d objGt3=%d bld=%d distFar=%d upgSingle=%d dist3d=%d base=%d\n",
			ctx.frame, typeLt1, typeEq1, builtLt1, upgAreaLt1, kindLt1, objGt3, bldLess, distFar, upgSingle, dist3d, baseOk);
	}
	else
	{
		const Bool objEq3 = evaluateObjectComparison(Parameter::EQUAL, 3, team);
		const Bool upgSingle = !evaluateUnitsWithUpgrade("<This Player's Enemies>", 2, team) &&
			evaluateUnitsWithUpgrade("<This Player's Enemies>", 1, team);
		std::fprintf(stderr,
			"TEST script-player-set-sum frame=%u objEq3=%d bld=%d distFar=%d upgSingle=%d dist3d=%d base=%d\n",
			ctx.frame, objEq3, bldLess, distFar, upgSingle, dist3d, baseOk);
	}

	player1->setTeamRelationship(civilian->getDefaultTeam(), prevCiv);
	player1->setTeamRelationship(creeps->getDefaultTeam(), prevCreeps);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailScriptPlayerSetSumTestCommands()
{
#if !RTS_GENERALS
	registerAction("script-sum-setup", setup);
	registerAction("script-sum-area", addArea);
	registerAction("script-sum-upg", upgradeCreep);
	registerAction("script-sum-clear", removeArea);
	registerAction("script-sum-check", check);
#endif
}
}
