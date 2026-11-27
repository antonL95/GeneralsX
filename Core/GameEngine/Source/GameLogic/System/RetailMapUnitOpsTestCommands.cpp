#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if RTS_ZEROHOUR
#include <cstdio>
#include "Common/Team.h"
#include "Common/INI.h"
#include "GameLogic/Damage.h"
#include "GameLogic/ExperienceLevelStore.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/RotwkModifierModules.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/PolygonTrigger.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Scripts.h"
#include "GameLogic/TerrainLogic.h"

namespace
{
Bool attack(Team* team, Object* unit, const char* waypoint)
{
	for (Int i = 0; i < ScriptEngine::getActionTemplateCount(); ++i)
	{
		if (TheScriptEngine->getActionTemplate(i)->m_internalName != "ATTACK_MOVE_TEAM_TO" ||
			ScriptEngine::findRotwkActionByRetailIndex(544) != i ||
			TheScriptEngine->getActionTemplate(i)->getNumParameters() != 2 ||
			TheScriptEngine->getActionTemplate(i)->getParameterType(0) != Parameter::TEAM ||
			TheScriptEngine->getActionTemplate(i)->getParameterType(1) != Parameter::WAYPOINT)
			continue;
		auto* action = newInstance(ScriptAction)(static_cast<ScriptAction::ScriptActionType>(i));
		action->getParameter(0)->friend_setString("<This Team>");
		action->getParameter(1)->friend_setString(waypoint);
		TheScriptEngine->friend_executeAction(action, team);
		deleteInstance(action);
		const Waypoint* destination = TheTerrainLogic->getWaypointByName(waypoint);
		const Coord3D* goal = unit->getAI()->getGoalPosition();
		return destination && goal && goal->x == destination->getLocation()->x && goal->y == destination->getLocation()->y &&
			unit->getAI()->getCurrentStateID() == AI_ATTACK_MOVE_TO;
	}
	return FALSE;
}

Bool teamHealthCondition(Team* team, Int comparison, Int percent)
{
	for (Int i = 0; i < Condition::NUM_ITEMS; ++i)
	{
		const Template* definition = TheScriptEngine->getConditionTemplate(i);
		if (definition->m_internalName != "EVAL_TEAM_HEALTH" ||
			ScriptEngine::findRotwkConditionByRetailIndex(174) != i || definition->getNumParameters() != 3 ||
			definition->getParameterType(0) != Parameter::TEAM || definition->getParameterType(1) != Parameter::COMPARISON ||
			definition->getParameterType(2) != Parameter::INT)
			continue;
		auto* condition = newInstance(Condition)(static_cast<Condition::ConditionType>(i));
		condition->getParameter(0)->friend_setString("<This Team>");
		condition->getParameter(1)->friend_setInt(comparison);
		condition->getParameter(2)->friend_setInt(percent);
		auto* clause = newInstance(OrCondition);
		clause->setFirstAndCondition(condition);
		auto* script = newInstance(Script);
		script->setOrCondition(clause);
		const Bool result = TheScriptEngine->evaluateConditions(script, team);
		deleteInstance(script);
		return result;
	}
	return FALSE;
}

Bool unitThreatCondition(const char* unit, Int comparison, Real threshold, Real radius)
{
	const Int type = ScriptEngine::findRotwkConditionByRetailIndex(115);
	if (type < 0)
		return FALSE;
	const Template* definition = TheScriptEngine->getConditionTemplate(type);
	if (definition->m_internalName != "UNIT_THREAT_LEVEL" || definition->getNumParameters() != 4 ||
		definition->getParameterType(0) != Parameter::UNIT || definition->getParameterType(1) != Parameter::COMPARISON ||
		definition->getParameterType(2) != Parameter::REAL || definition->getParameterType(3) != Parameter::REAL)
		return FALSE;
	auto* condition = newInstance(Condition)(static_cast<Condition::ConditionType>(type));
	condition->getParameter(0)->friend_setString(unit);
	condition->getParameter(1)->friend_setInt(comparison);
	condition->getParameter(2)->friend_setReal(threshold);
	condition->getParameter(3)->friend_setReal(radius);
	auto* clause = newInstance(OrCondition);
	clause->setFirstAndCondition(condition);
	auto* script = newInstance(Script);
	script->setOrCondition(clause);
	const Bool result = TheScriptEngine->evaluateConditions(script, nullptr);
	deleteInstance(script);
	return result;
}

void threatProbe(const RetailTestCommands::Context& ctx)
{
	Object* unit = ctx.requireObject();
	Object* enemy = ctx.findTarget(std::string_view("PlyrCreeps:MapScriptThreatProbe:0"), true);
	if (!unit || !enemy)
		return;
	if (unit->getName().isEmpty())
	{
		unit->setName("MapScriptThreatAnchor");
		TheScriptEngine->addObjectToCache(unit);
		enemy->getBodyModule()->setInitialHealth(50);
		enemy->getExperienceTracker()->setRetailStartingRank(2);
	}
	const Real radius = ctx.frame == 242 ? 30.0f : 1.0f;
	const Bool match = unitThreatCondition("MapScriptThreatAnchor", 2, 24.0f, radius);
	const Bool reject = !unitThreatCondition("MapScriptThreatAnchor", 2, 40.0f, 30.0f) &&
		!unitThreatCondition("MapScriptThreatMissing", 2, 0.0f, 30.0f);
	const Bool cached = unitThreatCondition("MapScriptThreatAnchor", 2, 24.0f, 1.0f);
	std::fprintf(stderr, "TEST map-unit-ops threat frame=%u match=%d reject=%d cached=%d\n", ctx.frame, match, reject, cached);
}

const Waypoint* nearestAttackPathPoint(const Coord3D& position)
{
	const Waypoint* start = TheTerrainLogic->getWaypointByName("AOR_Random Waypoint_03");
	const Waypoint* end = TheTerrainLogic->getWaypointByName("AOR_Random Waypoint_01");
	if (!start || !end || start->getPathLabel1() != "MapScriptAttackPath" ||
		end->getPathLabel1() != "MapScriptAttackPath" || start->getNumLinks() != 1 || start->getLink(0) != end)
		return nullptr;
	const auto distance = [&](const Waypoint* point) {
		const Real x = point->getLocation()->x - position.x;
		const Real y = point->getLocation()->y - position.y;
		return x * x + y * y;
	};
	return distance(start) < distance(end) ? start : end;
}

Bool hasWaypointGoal(AIUpdateInterface* ai, const Waypoint* waypoint)
{
	const Coord3D* goal = ai->getGoalPosition();
	const Coord3D* expected = waypoint->getLocation();
	return goal && goal->x == expected->x && goal->y == expected->y;
}

void namedAttackPathProbe(const RetailTestCommands::Context& ctx)
{
	Object* unit = ctx.requireObject();
	if (!unit || !unit->getAI())
		return;
	const Waypoint* point = nearestAttackPathPoint(*unit->getPosition());
	AIUpdateInterface* ai = unit->getAI();
	const Int type = ScriptEngine::findRotwkActionByRetailIndex(547);
	const Template* definition = type >= 0 ? TheScriptEngine->getActionTemplate(type) : nullptr;
	Bool missing = FALSE;
	if (definition && definition->m_internalName == "NAMED_ATTACK_FOLLOW_WAYPOINTS" &&
		definition->getNumParameters() == 2 && definition->getParameterType(0) == Parameter::UNIT &&
		definition->getParameterType(1) == Parameter::WAYPOINT_PATH)
	{
		auto* action = newInstance(ScriptAction)(static_cast<ScriptAction::ScriptActionType>(type));
		action->getParameter(0)->friend_setString(unit->getName());
		if (ctx.frame == 243)
		{
			action->getParameter(1)->friend_setString("MapScriptAttackPath");
			TheScriptEngine->friend_executeAction(action, nullptr);
		}
		action->getParameter(1)->friend_setString("MapScriptMissingPath");
		TheScriptEngine->friend_executeAction(action, nullptr);
		missing = ai->getCurrentStateID() == AI_ATTACKFOLLOW_WAYPOINT_PATH_AS_INDIVIDUALS;
		deleteInstance(action);
	}
	const Bool ordered = point && ai->getCurrentStateID() == AI_ATTACKFOLLOW_WAYPOINT_PATH_AS_INDIVIDUALS &&
		hasWaypointGoal(ai, point);
	std::fprintf(stderr, "TEST map-unit-ops named path frame=%u path=%d ordered=%d missing=%d\n",
		ctx.frame, point != nullptr, ordered, missing);
}

void guardAreaProbe(const RetailTestCommands::Context& ctx)
{
	Object* unit = ctx.requireObject();
	AIUpdateInterface* ai = unit ? unit->getAI() : nullptr;
	if (!ai)
		return;
	PolygonTrigger* area = PolygonTrigger::getFirstPolygonTrigger();
	while (area && (area->isRetired() || area->getTriggerName().isEmpty()))
		area = area->getNext();
	const Bool fromPosition = ctx.action == "map-unit-ops-guard-from";
	const Int type = ScriptEngine::findRotwkActionByRetailIndex(fromPosition ? 486 : 454);
	const Template* definition = type >= 0 ? TheScriptEngine->getActionTemplate(type) : nullptr;
	const char* actionName = fromPosition ? "UNIT_GUARD_AREA_FROM_POSITION" : "UNIT_GUARD_AREA";
	const AsciiString waypoint = "SpawnPoint_SkirmishGollum_3";
	const Waypoint* origin = fromPosition ? TheTerrainLogic->getWaypointByName(waypoint) : nullptr;
	Bool matched = FALSE;
	Bool fallback = FALSE;
	if (area && definition && definition->m_internalName == actionName &&
		definition->getNumParameters() == (fromPosition ? 3 : 2) &&
		definition->getParameterType(0) == Parameter::UNIT && definition->getParameterType(1) == Parameter::TRIGGER_AREA)
	{
		auto* action = newInstance(ScriptAction)(static_cast<ScriptAction::ScriptActionType>(type));
		action->getParameter(0)->friend_setString(unit->getName());
		action->getParameter(1)->friend_setString(area->getTriggerName());
		if (fromPosition)
			action->getParameter(2)->friend_setString(waypoint);
		TheScriptEngine->friend_executeAction(action, nullptr);
		Coord3D center;
		area->getCenterPoint(&center);
		const auto guards = [&](const Coord3D& expected) {
			const Coord3D* goal = ai->getGuardLocation();
			return ai->getCurrentStateID() == AI_GUARD && ai->getGuardTargetType() == GUARDTARGET_AREA &&
				goal->x == expected.x && goal->y == expected.y;
		};
		matched = fromPosition ? origin && guards(*origin->getLocation()) : guards(center);
		if (fromPosition)
		{
			action->getParameter(2)->friend_setString("MapScriptMissingWaypoint");
			TheScriptEngine->friend_executeAction(action, nullptr);
			fallback = guards(center);
		}
		deleteInstance(action);
	}
	if (fromPosition)
		std::fprintf(stderr, "TEST map-unit-ops guard from frame=%u area=%d matched=%d fallback=%d\n",
			ctx.frame, area != nullptr, matched, fallback);
	else
		std::fprintf(stderr, "TEST map-unit-ops guard area frame=%u area=%d matched=%d\n", ctx.frame, area != nullptr, matched);
}

void forcePlayerEmotion(const RetailTestCommands::Context& ctx)
{
	const Int type = ScriptEngine::findRotwkActionByRetailIndex(521);
	const Template* definition = type >= 0 ? TheScriptEngine->getActionTemplate(type) : nullptr;
	if (!definition || definition->m_internalName != "PLAYER_FORCE_EMOTION" || definition->getNumParameters() != 3 ||
		definition->getParameterType(0) != Parameter::SIDE || definition->getParameterType(2) != Parameter::REAL)
		return;
	auto* action = newInstance(ScriptAction)(static_cast<ScriptAction::ScriptActionType>(type));
	action->getParameter(0)->friend_setString(ctx.playerName.data());
	action->getParameter(1)->friend_setInt(EMOTION_TERROR);
	action->getParameter(2)->friend_setReal(2.0f);
	TheScriptEngine->friend_executeAction(action, nullptr);
	deleteInstance(action);
}

void playerEmotionProbe(const RetailTestCommands::Context& ctx)
{
	const Object* first = ctx.requireObject();
	const Object* second = ctx.findTarget(std::string_view("Player_1:MapScriptEmotionProbe:1"), true);
	const Object* other = ctx.findTarget(std::string_view("PlyrCreeps:MapScriptEmotionProbe:0"), true);
	const auto terror = [](const Object* unit) {
		return unit && unit->getLogicModelConditions().test(MODELCONDITION_EMOTION_TERROR);
	};
	std::fprintf(stderr, "TEST map-unit-ops player emotion frame=%u allies=%d enemy=%d\n",
		ctx.frame, terror(first) && terror(second), other ? terror(other) : -1);
}

void teamAttackPathProbe(const RetailTestCommands::Context& ctx)
{
	Object* first = ctx.requireObject();
	Object* second = ctx.findTarget();
	if (!first || !second || !first->getAI() || !second->getAI())
		return;
	const Coord3D& a = *first->getPosition();
	const Coord3D& b = *second->getPosition();
	const Coord3D center = {(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f, (a.z + b.z) * 0.5f};
	const Waypoint* point = nearestAttackPathPoint(center);
	const Int type = ScriptEngine::findRotwkActionByRetailIndex(545);
	const Template* definition = type >= 0 ? TheScriptEngine->getActionTemplate(type) : nullptr;
	Bool ordered = FALSE;
	Bool modes = FALSE;
	if (definition && definition->m_internalName == "TEAM_ATTACK_MOVE_FOLLOW_WAYPOINTS" &&
		definition->getNumParameters() == 4 && definition->getParameterType(0) == Parameter::TEAM &&
		definition->getParameterType(1) == Parameter::WAYPOINT_PATH && definition->getParameterType(2) == Parameter::BOOLEAN &&
		definition->getParameterType(3) == Parameter::BOOLEAN)
	{
		Team* original = first->getTeam();
		Team* team = newInstance(Team)(const_cast<TeamPrototype*>(original->getPrototype()), TEAM_ID_INVALID);
		first->setTemporaryTeam(team);
		second->setTemporaryTeam(team);
		auto* action = newInstance(ScriptAction)(static_cast<ScriptAction::ScriptActionType>(type));
		action->getParameter(0)->friend_setString("<This Team>");
		action->getParameter(1)->friend_setString("MapScriptAttackPath");
		const auto follows = [&](StateID state) {
			return point && first->getAI()->getCurrentStateID() == state && second->getAI()->getCurrentStateID() == state &&
				hasWaypointGoal(first->getAI(), point) && hasWaypointGoal(second->getAI(), point);
		};
		if (ctx.frame == 243)
		{
			action->getParameter(2)->friend_setInt(0);
			action->getParameter(3)->friend_setInt(0);
			TheScriptEngine->friend_executeAction(action, team);
		}
		ordered = follows(AI_ATTACKFOLLOW_WAYPOINT_PATH_AS_INDIVIDUALS);
		action->getParameter(2)->friend_setInt(1);
		action->getParameter(3)->friend_setInt(0);
		if (ctx.frame == 245)
		{
			TheScriptEngine->friend_executeAction(action, team);
			modes = follows(AI_ATTACKFOLLOW_WAYPOINT_PATH_AS_TEAM);
			action->getParameter(2)->friend_setInt(0);
			action->getParameter(3)->friend_setInt(1);
			TheScriptEngine->friend_executeAction(action, team);
			modes = modes && follows(AI_ATTACKFOLLOW_WAYPOINT_PATH_AS_TEAM);
		}
		deleteInstance(action);
		first->setTemporaryTeam(original);
		second->setTemporaryTeam(original);
		deleteInstance(team);
	}
	std::fprintf(stderr, "TEST map-unit-ops team path frame=%u path=%d ordered=%d modes=%d\n",
		ctx.frame, point != nullptr, ordered, modes);
}

void killTarget(const RetailTestCommands::Context& ctx)
{
	const Object* killer = ctx.requireObject();
	Object* victim = ctx.findTarget();
	if (!killer || !victim)
		return;
	DamageInfo damage;
	damage.in.m_sourceID = killer->getID();
	damage.in.m_damageType = DAMAGE_UNRESISTABLE;
	damage.in.m_amount = victim->getBodyModule()->getHealth();
	damage.in.m_deathType = DEATH_NORMAL;
	victim->attemptDamage(&damage);
	std::fprintf(stderr, "TEST map-unit-ops victim frame=%u dead=%d\n", ctx.frame, victim->isEffectivelyDead());
}

Bool killedTypeCondition(const char* player, Int count, const char* typeName)
{
	const Int type = ScriptEngine::findRotwkConditionByRetailIndex(129);
	if (type < 0)
		return FALSE;
	const Template* definition = TheScriptEngine->getConditionTemplate(type);
	if (definition->m_internalName != "PLAYER_HAS_KILLED_TYPE_UNITS" || definition->getNumParameters() != 3 ||
		definition->getParameterType(0) != Parameter::SIDE || definition->getParameterType(1) != Parameter::INT ||
		definition->getParameterType(2) != Parameter::OBJECT_TYPE)
		return FALSE;
	auto* condition = newInstance(Condition)(static_cast<Condition::ConditionType>(type));
	condition->getParameter(0)->friend_setString(player);
	condition->getParameter(1)->friend_setInt(count);
	condition->getParameter(2)->friend_setString(typeName);
	auto* clause = newInstance(OrCondition);
	clause->setFirstAndCondition(condition);
	auto* script = newInstance(Script);
	script->setOrCondition(clause);
	const Bool result = TheScriptEngine->evaluateConditions(script, nullptr);
	deleteInstance(script);
	return result;
}

void killCountProbe(const RetailTestCommands::Context& ctx)
{
	const Int firstCount = ctx.frame == 243 ? 1 : 2;
	const char* first = "MapScriptScoreTarget";
	const char* second = "MapScriptScoreBuilding";
	const char* list = "MapScriptScoreTypes";
	TheScriptEngine->doObjectTypeListMaintenance(list, first, TRUE);
	TheScriptEngine->doObjectTypeListMaintenance(list, second, TRUE);
	const Bool single = killedTypeCondition("Player_1", firstCount, first) &&
		!killedTypeCondition("Player_1", firstCount + 1, first);
	const Bool all = killedTypeCondition("Player_1", firstCount + 1, list) &&
		!killedTypeCondition("Player_1", firstCount + 2, list);
	const Bool separated = !killedTypeCondition("PlyrCreeps", 1, list) &&
		!killedTypeCondition("MapScriptMissingPlayer", 0, list) &&
		!killedTypeCondition("Player_1", 1, "MapScriptUnitOpsXP");
	TheScriptEngine->doObjectTypeListMaintenance(list, first, FALSE);
	TheScriptEngine->doObjectTypeListMaintenance(list, second, FALSE);
	std::fprintf(stderr, "TEST map-unit-ops kills frame=%u single=%d list=%d owner=%d\n", ctx.frame, single, all, separated);
}

void removeHordeMember(const RetailTestCommands::Context& ctx)
{
	Object* horde = ctx.requireObject();
	if (!horde || !horde->getContain())
		return;
	const ContainedItemsList* members = horde->getContain()->getContainedItemsList();
	if (members && !members->empty())
		TheGameLogic->destroyObject(*members->begin());
}

void teamHealthProbe(const RetailTestCommands::Context& ctx)
{
	Object* horde = ctx.requireObject();
	Object* unit = ctx.findTarget(std::string_view("Player_1:MapScriptVeterancyProbe:0"));
	if (!horde || !horde->getContain() || !unit)
		return;
	const ContainedItemsList* members = horde->getContain()->getContainedItemsList();
	if (!members || members->empty())
		return;
	const Int count = horde->getContain()->getContainCount();
	const Int maximum = horde->getContain()->getContainMax();
	Team* original = horde->getTeam();
	Team* team = newInstance(Team)(const_cast<TeamPrototype*>(original->getPrototype()), TEAM_ID_INVALID);
	horde->setTemporaryTeam(team);
	unit->setTemporaryTeam(team);
	const Int expected = maximum > 0 ? (count + 1) * 100 / (maximum + 1) : 0;
	const Bool partial = count > 0 && count < maximum;
	const Bool match = teamHealthCondition(team, 2, expected);
	const Bool reject = !teamHealthCondition(team, 2, expected + 1);
	horde->setTemporaryTeam(original);
	unit->setTemporaryTeam(original);
	Object* soldier = members->front();
	Team* soldierTeam = soldier->getTeam();
	soldier->setTemporaryTeam(team);
	const Bool memberExcluded = teamHealthCondition(team, 2, 0) && !teamHealthCondition(team, 2, 100);
	soldier->setTemporaryTeam(soldierTeam);
	deleteInstance(team);
	std::fprintf(stderr, "TEST map-unit-ops health frame=%u partial=%d match=%d reject=%d member-excluded=%d\n",
		ctx.frame, partial, match, reject, memberExcluded);
}

Bool veteranCondition(const char* owner, const char* type, Int comparison, Int rank)
{
	for (Int i = 0; i < Condition::NUM_ITEMS; ++i)
	{
		const Template* definition = TheScriptEngine->getConditionTemplate(i);
		if (definition->m_internalName != "PLAYER_HAS_OBJECT_OF_VETERANCY" ||
			ScriptEngine::findRotwkConditionByRetailIndex(146) != i || definition->getNumParameters() != 4 ||
			definition->getParameterType(0) != Parameter::SIDE || definition->getParameterType(1) != Parameter::OBJECT_TYPE ||
			definition->getParameterType(2) != Parameter::COMPARISON || definition->getParameterType(3) != Parameter::INT)
			continue;
		auto* condition = newInstance(Condition)(static_cast<Condition::ConditionType>(i));
		condition->getParameter(0)->friend_setString(owner);
		condition->getParameter(1)->friend_setString(type);
		condition->getParameter(2)->friend_setInt(comparison);
		condition->getParameter(3)->friend_setInt(rank);
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

void teamExperienceProbe(const RetailTestCommands::Context& ctx)
{
	Object* source = ctx.requireObject();
	Object* receiver = ctx.findTarget(std::string_view("Player_1:MapScriptUnitOpsXP:1"));
	Object* control = ctx.findTarget(std::string_view("Player_1:MapScriptUnitOpsXP:2"));
	if (!source || !receiver || !control)
		return;
	Team* original = source->getTeam();
	Team* team = newInstance(Team)(const_cast<TeamPrototype*>(original->getPrototype()), TEAM_ID_INVALID);
	source->setTemporaryTeam(team);
	control->setTemporaryTeam(team);
	const Real sourceBefore = source->getExperienceTracker()->getRetailExperience();
	const Real receiverBefore = receiver->getExperienceTracker()->getRetailExperience();
	const Real controlBefore = control->getExperienceTracker()->getRetailExperience();
	Bool registered = FALSE;
	for (Int i = 0; i < ScriptEngine::getActionTemplateCount(); ++i)
	{
		const Template* definition = TheScriptEngine->getActionTemplate(i);
		if (definition->m_internalName != "TEAM_GIVE_EXPERIENCE_POINTS" ||
			ScriptEngine::findRotwkActionByRetailIndex(356) != i || definition->getNumParameters() != 2 ||
			definition->getParameterType(0) != Parameter::TEAM || definition->getParameterType(1) != Parameter::INT)
			continue;
		auto* action = newInstance(ScriptAction)(static_cast<ScriptAction::ScriptActionType>(i));
		action->getParameter(0)->friend_setString("<This Team>");
		action->getParameter(1)->friend_setInt(10);
		TheScriptEngine->friend_executeAction(action, team);
		deleteInstance(action);
		registered = TRUE;
	}
	const Real receiverGain = receiver->getExperienceTracker()->getRetailExperience() - receiverBefore;
	const Real controlGain = control->getExperienceTracker()->getRetailExperience() - controlBefore;
	const Bool sink = source->getExperienceTracker()->getRetailExperience() == sourceBefore && receiverGain > 0.0f;
	const Bool members = registered && controlGain > 0.0f && controlGain == 2.0f * receiverGain;
	source->setTemporaryTeam(original);
	control->setTemporaryTeam(original);
	deleteInstance(team);
	std::fprintf(stderr, "TEST map-unit-ops team-xp frame=%u sink=%d members=%d\n", ctx.frame, sink, members);
}

void veterancyProbe(const RetailTestCommands::Context& ctx)
{
	Object* unit = ctx.requireObject();
	if (!unit)
		return;
	INI ini;
	ini.loadText("map-unit-ops-veterancy", R"(
ExperienceLevel MapScriptVeterancyFirst
    TargetNames = MapScriptVeterancyProbe
    RequiredExperience = 1
    Rank = 1
End
ExperienceLevel MapScriptVeterancySecond
    TargetNames = MapScriptVeterancyProbe
    RequiredExperience = 20
    Rank = 2
End
)", INI_LOAD_OVERWRITE, "ExperienceLevel", ExperienceLevelStore::parseDefinition);
	unit->getExperienceTracker()->setTrainable(TRUE);
	unit->getExperienceTracker()->setMinRetailRank(2);
	const Bool match = veteranCondition("Player_1", "MapScriptVeterancyProbe", 2, 2) &&
		veteranCondition("Player_1", "MapScriptVeterancyProbe", 3, 2);
	const Bool reject = !veteranCondition("Player_1", "MapScriptVeterancyProbe", 0, 2) &&
		!veteranCondition("Player_1", "MapScriptVeterancyMissing", 2, 2) &&
		!veteranCondition("PlyrCreeps", "MapScriptVeterancyProbe", 2, 2);
	std::fprintf(stderr, "TEST map-unit-ops veterancy frame=%u match=%d reject=%d\n", ctx.frame, match, reject);
}

void experienceProbe(const RetailTestCommands::Context& ctx)
{
	Object* source = ctx.requireObject();
	Object* receiver = ctx.findTarget(std::string_view("Player_1:MapScriptUnitOpsXP:1"));
	Object* control = ctx.findTarget(std::string_view("Player_1:MapScriptUnitOpsXP:2"));
	if (!source || !receiver || !control)
		return;
	INI ini;
	ini.loadText("map-unit-ops-levels", R"(
ExperienceLevel MapScriptUnitOpsXPLevel
    TargetNames = MapScriptUnitOpsXP
    RequiredExperience = 1
    Rank = 1
End
)", INI_LOAD_OVERWRITE, "ExperienceLevel", ExperienceLevelStore::parseDefinition);
	if (source->getName().isEmpty())
	{
		source->setName("MapScriptUnitOpsXPSource");
		TheScriptEngine->addObjectToCache(source);
	}
	auto* from = source->getExperienceTracker();
	auto* to = receiver->getExperienceTracker();
	auto* comparison = control->getExperienceTracker();
	from->setTrainable(TRUE);
	to->setTrainable(TRUE);
	comparison->setTrainable(TRUE);
	from->initializeRetailLevel();
	to->initializeRetailLevel();
	comparison->initializeRetailLevel();
	from->setExperienceScalar(0.5f);
	from->setExperienceSink(receiver->getID());
	const Real before = to->getRetailExperience();
	const Real sourceBefore = from->getRetailExperience();
	const Real comparisonBefore = comparison->getRetailExperience();
	comparison->addRetailExperience(5.0f, TRUE);
	const Real expected = comparison->getRetailExperience() - comparisonBefore;
	comparison->addRetailExperience(2.0f, TRUE);
	const Real expectedLoss = comparison->getRetailExperience() - comparisonBefore - expected;
	Bool registered = FALSE;
	Real gain = 0.0f;
	for (Int i = 0; i < ScriptEngine::getActionTemplateCount(); ++i)
	{
		const Template* definition = TheScriptEngine->getActionTemplate(i);
		if (definition->m_internalName != "UNIT_GIVE_EXPERIENCE_POINTS" ||
			ScriptEngine::findRotwkActionByRetailIndex(359) != i || definition->getNumParameters() != 2 ||
			definition->getParameterType(0) != Parameter::UNIT || definition->getParameterType(1) != Parameter::INT)
			continue;
		auto* action = newInstance(ScriptAction)(static_cast<ScriptAction::ScriptActionType>(i));
		action->getParameter(0)->friend_setString("MapScriptUnitOpsXPSource");
		action->getParameter(1)->friend_setInt(10);
		TheScriptEngine->friend_executeAction(action, nullptr);
		gain = to->getRetailExperience() - before;
		action->getParameter(1)->friend_setInt(-4);
		TheScriptEngine->friend_executeAction(action, nullptr);
		deleteInstance(action);
		registered = TRUE;
	}
	const Real loss = before + gain - to->getRetailExperience();
	std::fprintf(stderr, "TEST map-unit-ops xp frame=%u sink=%d amount=%d negative=%d\n", ctx.frame,
		registered && from->getRetailExperience() == sourceBefore && gain > 0.0f, gain == expected && expected > 0.0f,
		loss == expectedLoss && expectedLoss > 0.0f);
}

void setExperienceProbe(const RetailTestCommands::Context& ctx)
{
	Object* source = ctx.requireObject();
	Object* receiver = ctx.findTarget();
	if (!source || !receiver)
		return;
	INI ini;
	ini.loadText("map-unit-ops-absolute-xp-levels", R"(
ExperienceLevel MapScriptAbsoluteXPFirst
    TargetNames = MapScriptAbsoluteXPProbe
    RequiredExperience = 1
    Rank = 1
End
ExperienceLevel MapScriptAbsoluteXPSecond
    TargetNames = MapScriptAbsoluteXPProbe
    RequiredExperience = 20
    Rank = 2
End
)", INI_LOAD_OVERWRITE, "ExperienceLevel", ExperienceLevelStore::parseDefinition);
	auto* from = source->getExperienceTracker();
	auto* to = receiver->getExperienceTracker();
	const Bool first = ctx.frame == 242;
	if (first)
	{
		source->setName("MapScriptAbsoluteXPSource");
		TheScriptEngine->addObjectToCache(source);
		from->initializeRetailLevel();
		to->initializeRetailLevel();
		from->setExperienceScalar(0.25f);
		to->setExperienceScalar(0.5f);
		from->setExperienceSink(receiver->getID());
	}
	const Bool baseline = to->getRetailExperience() == (first ? 1.0f : 35.0f) &&
		to->getRank() == (first ? 1 : 2);
	const Real sourceExperience = from->getRetailExperience();
	const Int points = first ? 35 : 2;
	Bool amount = FALSE;
	Bool missing = FALSE;
	const Int type = ScriptEngine::findRotwkActionByRetailIndex(360);
	const Template* definition = type >= 0 ? TheScriptEngine->getActionTemplate(type) : nullptr;
	if (definition && definition->m_internalName == "UNIT_SET_EXPERIENCE_POINTS" &&
		definition->getNumParameters() == 2 && definition->getParameterType(0) == Parameter::UNIT &&
		definition->getParameterType(1) == Parameter::INT)
	{
		auto* action = newInstance(ScriptAction)(static_cast<ScriptAction::ScriptActionType>(type));
		action->getParameter(0)->friend_setString("MapScriptAbsoluteXPSource");
		action->getParameter(1)->friend_setInt(points);
		TheScriptEngine->friend_executeAction(action, nullptr);
		amount = to->getRetailExperience() == static_cast<Real>(points) && to->getRank() == 2 &&
			from->getRetailExperience() == sourceExperience;
		action->getParameter(0)->friend_setString("MapScriptAbsoluteXPMissing");
		action->getParameter(1)->friend_setInt(0);
		TheScriptEngine->friend_executeAction(action, nullptr);
		missing = to->getRetailExperience() == static_cast<Real>(points);
		deleteInstance(action);
	}
	std::fprintf(stderr, "TEST map-unit-ops set xp frame=%u baseline=%d amount=%d missing=%d\n",
		ctx.frame, baseline, amount, missing);
}

void teleportProbe(const RetailTestCommands::Context& ctx)
{
	Object* unit = ctx.requireObject();
	if (!unit)
		return;
	if (unit->getName().isEmpty())
	{
		unit->setName("MapScriptUnitOpsNamed");
		TheScriptEngine->addObjectToCache(unit);
	}
	const Coord3D origin = *unit->getPosition();
	const Waypoint* destination = TheTerrainLogic->getWaypointByName("SpawnPoint_SkirmishGollum_3");
	Bool moved = FALSE;
	Bool missing = FALSE;
	for (Int i = 0; i < ScriptEngine::getActionTemplateCount(); ++i)
	{
		const Template* definition = TheScriptEngine->getActionTemplate(i);
		if (definition->m_internalName != "UNIT_TELEPORT_TO_WAYPOINT" ||
			ScriptEngine::findRotwkActionByRetailIndex(502) != i || definition->getNumParameters() != 2 ||
			definition->getParameterType(0) != Parameter::UNIT || definition->getParameterType(1) != Parameter::WAYPOINT)
			continue;
		auto* action = newInstance(ScriptAction)(static_cast<ScriptAction::ScriptActionType>(i));
		action->getParameter(0)->friend_setString("MapScriptUnitOpsNamed");
		action->getParameter(1)->friend_setString("MapUnitOpsMissingWaypoint");
		TheScriptEngine->friend_executeAction(action, nullptr);
		missing = unit->getPosition()->x == origin.x && unit->getPosition()->y == origin.y;
		action->getParameter(0)->friend_setString("MapUnitOpsMissingUnit");
		action->getParameter(1)->friend_setString("SpawnPoint_SkirmishGollum_3");
		TheScriptEngine->friend_executeAction(action, nullptr);
		missing = missing && unit->getPosition()->x == origin.x && unit->getPosition()->y == origin.y;
		action->getParameter(0)->friend_setString("MapScriptUnitOpsNamed");
		TheScriptEngine->friend_executeAction(action, nullptr);
		deleteInstance(action);
		const Coord3D* position = unit->getPosition();
		moved = destination && position->x == destination->getLocation()->x && position->y == destination->getLocation()->y;
		unit->setPosition(&origin);
	}
	std::fprintf(stderr, "TEST map-unit-ops teleport frame=%u moved=%d missing=%d\n", ctx.frame, moved, missing);
}

void teamTeleportProbe(const RetailTestCommands::Context& ctx)
{
	Object* first = ctx.requireObject();
	Object* second = ctx.findTarget();
	if (!first || !second)
		return;
	const Coord3D firstOrigin = *first->getPosition();
	const Coord3D secondOrigin = *second->getPosition();
	const char* waypoint = "SpawnPoint_SkirmishGollum_3";
	const auto at = [](const Object* unit, const Coord3D& point) {
		return unit->getPosition()->x == point.x && unit->getPosition()->y == point.y;
	};
	const Waypoint* destination = TheTerrainLogic->getWaypointByName(waypoint);
	Bool moved = FALSE;
	Bool missing = FALSE;
	const Int type = ScriptEngine::findRotwkActionByRetailIndex(503);
	const Template* definition = type >= 0 ? TheScriptEngine->getActionTemplate(type) : nullptr;
	if (definition && destination && definition->m_internalName == "TEAM_TELEPORT_TO_WAYPOINT" &&
		definition->getNumParameters() == 2 && definition->getParameterType(0) == Parameter::TEAM &&
		definition->getParameterType(1) == Parameter::WAYPOINT)
	{
		Team* firstTeam = first->getTeam();
		Team* secondTeam = second->getTeam();
		Team* team = newInstance(Team)(const_cast<TeamPrototype*>(firstTeam->getPrototype()), TEAM_ID_INVALID);
		first->setTemporaryTeam(team);
		second->setTemporaryTeam(team);
		auto* action = newInstance(ScriptAction)(static_cast<ScriptAction::ScriptActionType>(type));
		action->getParameter(0)->friend_setString("<This Team>");
		action->getParameter(1)->friend_setString("MapUnitOpsMissingWaypoint");
		TheScriptEngine->friend_executeAction(action, team);
		missing = at(first, firstOrigin) && at(second, secondOrigin);
		action->getParameter(0)->friend_setString("MapUnitOpsMissingTeam");
		action->getParameter(1)->friend_setString(waypoint);
		TheScriptEngine->friend_executeAction(action, team);
		missing = missing && at(first, firstOrigin) && at(second, secondOrigin);
		if (ctx.frame == 242)
		{
			action->getParameter(0)->friend_setString("<This Team>");
			TheScriptEngine->friend_executeAction(action, team);
		}
		moved = at(first, *destination->getLocation()) && at(second, *destination->getLocation());
		deleteInstance(action);
		first->setTemporaryTeam(firstTeam);
		second->setTemporaryTeam(secondTeam);
		deleteInstance(team);
	}
	std::fprintf(stderr, "TEST map-unit-ops team teleport frame=%u moved=%d missing=%d\n", ctx.frame, moved, missing);
}

void probe(const RetailTestCommands::Context& ctx)
{
	Object* unit = ctx.requireObject();
	if (!unit || !unit->getAI())
		return;
	Team* original = unit->getTeam();
	Team* team = newInstance(Team)(const_cast<TeamPrototype*>(original->getPrototype()), TEAM_ID_INVALID);
	unit->setTemporaryTeam(team);
	const Bool ordered = attack(team, unit, "SpawnPoint_SkirmishGollum_3");
	const Bool missing = !attack(team, unit, "MapUnitOpsMissingWaypoint") &&
		unit->getAI()->getCurrentStateID() == AI_ATTACK_MOVE_TO;
	unit->setTemporaryTeam(original);
	deleteInstance(team);
	std::fprintf(stderr, "TEST map-unit-ops frame=%u attack=%d missing=%d\n", ctx.frame, ordered, missing);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailMapUnitOpsTestCommands()
{
#if RTS_ZEROHOUR
	registerAction("map-unit-ops-attack", probe);
	registerAction("map-unit-ops-teleport", teleportProbe);
	registerAction("map-unit-ops-team-teleport", teamTeleportProbe);
	registerAction("map-unit-ops-xp", experienceProbe);
	registerAction("map-unit-ops-set-xp", setExperienceProbe);
	registerAction("map-unit-ops-veterancy", veterancyProbe);
	registerAction("map-unit-ops-team-xp", teamExperienceProbe);
	registerAction("map-unit-ops-horde-remove", removeHordeMember);
	registerAction("map-unit-ops-health", teamHealthProbe);
	registerAction("map-unit-ops-threat", threatProbe);
	registerAction("map-unit-ops-named-path", namedAttackPathProbe);
	registerAction("map-unit-ops-guard-area", guardAreaProbe);
	registerAction("map-unit-ops-guard-from", guardAreaProbe);
	registerAction("map-unit-ops-force-emotion", forcePlayerEmotion);
	registerAction("map-unit-ops-emotion-probe", playerEmotionProbe);
	registerAction("map-unit-ops-team-path", teamAttackPathProbe);
	registerAction("map-unit-ops-kill", killTarget);
	registerAction("map-unit-ops-kill-count", killCountProbe);
#endif
}
}
