#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if RTS_ZEROHOUR
#include <cstdio>

#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/Team.h"
#include "GameLogic/Object.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Scripts.h"

namespace
{
void arm(const RetailTestCommands::Context& ctx)
{
	Object* unit = ctx.requireObject();
	if (!unit)
		return;

	Team* team = unit->getTeam();
	auto* setCounter = newInstance(ScriptAction)(ScriptAction::SET_COUNTER);
	setCounter->getParameter(0)->friend_setString("SkirmishOpcodeFixtureCounter");
	setCounter->getParameter(1)->friend_setInt(7);
	auto* random = newInstance(ScriptAction)(ScriptAction::SET_COUNTER_TO_CLIENT_RANDOM_VALUE);
	random->getParameter(0)->friend_setString("SkirmishOpcodeFixtureCounter");
	random->getParameter(1)->friend_setInt(42);
	random->getParameter(2)->friend_setInt(42);
	auto* reference = newInstance(ScriptAction)(ScriptAction::TEAM_SET_PLAYERS_NEAREST_UNIT_OF_TYPE_TO_REFERENCE);
	reference->getParameter(0)->friend_setString("SkirmishOpcodeFixtureUnit");
	reference->getParameter(1)->friend_setString("<This Team>");
	reference->getParameter(2)->friend_setString("SkirmishOpcodeFixtureRef");
	auto* timed = newInstance(ScriptAction)(ScriptAction::UNIT_SET_MODELCONDITION_FOR_DURATION);
	timed->getParameter(0)->friend_setString("SkirmishOpcodeFixtureRef");
	timed->getParameter(1)->friend_setString("USER_2");
	timed->getParameter(2)->friend_setReal(2.0f);
	timed->getParameter(3)->friend_setReal(0.0f);

	setCounter->setNextAction(random);
	random->setNextAction(reference);
	reference->setNextAction(timed);
	TheScriptEngine->friend_executeAction(setCounter, team);
	deleteInstance(setCounter);
}

Bool evaluate(Script& script, OrCondition& clause, Condition& condition, Team* team)
{
	clause.setFirstAndCondition(&condition);
	script.setOrCondition(&clause);
	const Bool result = TheScriptEngine->evaluateConditions(&script, team);
	script.setOrCondition(nullptr);
	clause.setFirstAndCondition(nullptr);
	return result;
}

void check(const RetailTestCommands::Context& ctx)
{
	Object* unit = ctx.requireObject();
	if (!unit)
		return;

	Team* team = unit->getTeam();
	const TCounter* counter = TheScriptEngine->getCounter("SkirmishOpcodeFixtureCounter");
	Script* script = newInstance(Script);
	OrCondition* clause = newInstance(OrCondition);
	Condition* reference = newInstance(Condition)(Condition::NAMED_NOT_DESTROYED);
	reference->getParameter(0)->friend_setString("SkirmishOpcodeFixtureRef");
	const Bool found = evaluate(*script, *clause, *reference, team);
	deleteInstance(reference);

	Condition* distance = newInstance(Condition)(Condition::PLAYER_HAS_NUMBER_UNITS_DISTANCE_FROM_OBJECT);
	distance->getParameter(0)->friend_setString("PlyrCreeps");
	distance->getParameter(1)->friend_setInt(Parameter::GREATER_EQUAL);
	distance->getParameter(2)->friend_setInt(1);
	distance->getParameter(3)->friend_setReal(35.0f);
	distance->getParameter(4)->friend_setString("SkirmishOpcodeFixtureRef");
	const Bool distancePass = evaluate(*script, *clause, *distance, team);
	distance->getParameter(3)->friend_setReal(10000.0f);
	const Bool distant = evaluate(*script, *clause, *distance, team);
	distance->getParameter(4)->friend_setString("SkirmishOpcodeFixtureMissing");
	const Bool missing = evaluate(*script, *clause, *distance, team);
	deleteInstance(distance);

	Condition* teams = newInstance(Condition)(Condition::DISTANCE_BETWEEN_TEAM);
	teams->getParameter(0)->friend_setString("<This Team>");
	const Player* creeps = ThePlayerList->findPlayerWithName("PlyrCreeps");
	teams->getParameter(1)->friend_setString(creeps ? creeps->getDefaultTeam()->getName() : AsciiString());
	teams->getParameter(2)->friend_setInt(Parameter::GREATER);
	teams->getParameter(3)->friend_setReal(0.0f);
	const Bool teamPass = evaluate(*script, *clause, *teams, team);
	deleteInstance(teams);

	Condition* model = newInstance(Condition)(Condition::PLAYER_HAS_NUMBER_OBJECTS_WITH_MODELCONDITION);
	model->getParameter(0)->friend_setString("<All Players>");
	model->getParameter(1)->friend_setString("USER_2");
	model->getParameter(2)->friend_setInt(Parameter::GREATER_EQUAL);
	model->getParameter(3)->friend_setInt(1);
	const Bool modelPass = unit->getLogicModelConditions().test(MODELCONDITION_USER_2) && evaluate(*script, *clause, *model, team);
	deleteInstance(model);
	deleteInstance(clause);
	deleteInstance(script);

	std::fprintf(stderr, "TEST skirmish-opcode frame=%u counter=%d reference=%d distance=%d distant=%d missing=%d team=%d model=%d\n",
		ctx.frame, counter ? counter->value : -1, found, distancePass, distant, missing, teamPass, modelPass);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailSkirmishOpcodeTestCommands()
{
#if RTS_ZEROHOUR
	registerAction("skirmish-opcode-arm", arm);
	registerAction("skirmish-opcode-check", check);
#endif
}
}
