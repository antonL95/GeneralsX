#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if RTS_ZEROHOUR
#include <cstdio>
#include <cstdlib>

#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/Team.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/RetailGate.h"
#include "GameLogic/Object.h"
#include "GameLogic/ScriptConditions.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Scripts.h"
#include "GameLogic/SidesList.h"

namespace
{
const char* const gateRefName = "ScriptGateOpsGate";
const char* const seqScriptName = "ScriptGateOpsSeqScript";
const char* const seqCounterName = "ScriptGateOpsSeqDone";

ScriptAction* actionNamed(const char* name)
{
	for (Int i = 0; i < ScriptEngine::getActionTemplateCount(); ++i)
		if (TheScriptEngine->getActionTemplate(i)->m_internalName == name)
			return newInstance(ScriptAction)(static_cast<ScriptAction::ScriptActionType>(i));
	return nullptr;
}

Bool evaluateGateReady(const char* name)
{
	Parameter* param = newInstance(Parameter)(Parameter::UNIT);
	param->friend_setString(name);
	const Bool ready = TheScriptConditions && TheScriptConditions->evaluateGateReady(param);
	deleteInstance(param);
	return ready;
}

void ensureNamedGate(Object* gate)
{
	if (!gate || gate->getName() == gateRefName)
		return;
	gate->setName(AsciiString(gateRefName));
	TheScriptEngine->addObjectToCache(gate);
}

void setNeedGate(const RetailTestCommands::Context& ctx)
{
	Object* gate = ctx.requireObject();
	if (!gate)
		return;
	ensureNamedGate(gate);

	const std::string_view spec(ctx.argument);
	const size_t colon = spec.find(':');
	if (colon == std::string_view::npos)
		return;
	Player* owner = ctx.findPlayer(spec.substr(0, colon));
	Team* team = owner ? owner->getDefaultTeam() : nullptr;
	if (!team)
		return;
	const Int enabled = std::atoi(std::string(spec.substr(colon + 1)).c_str()) != 0 ? 1 : 0;

	if (ScriptAction* action = actionNamed("TEAM_NEEDS_OPEN_GATE"))
	{
		action->getParameter(0)->friend_setString(team->getName());
		action->getParameter(1)->friend_setString(gateRefName);
		action->getParameter(2)->friend_setInt(enabled);
		TheScriptEngine->friend_executeAction(action, team);
		deleteInstance(action);
	}
}

void probeGate(const RetailTestCommands::Context& ctx)
{
	const Object* object = ctx.requireObject();
	const GateOpenAndCloseBehavior* gate = GateOpenAndCloseBehavior::resolveForControl(object);
	if (!gate)
		return;
	const Int needs = gate->getTeamOpenRequestCount();
	const Bool wants = gate->wantsOpen();
	const Bool passable = gate->isPassable();
	const Bool ready = evaluateGateReady(gateRefName);
	std::fprintf(stderr, "TEST script-gate-ops frame=%u needs=%d wantsOpen=%d passable=%d ready=%d\n",
		ctx.frame, needs, wants ? 1 : 0, passable ? 1 : 0, ready ? 1 : 0);
}

void startSequential(const RetailTestCommands::Context& ctx)
{
	Object* unit = ctx.requireObject();
	Object* gateObject = ctx.findTarget(ctx.argument, false);
	if (!unit || !gateObject || !TheSidesList || TheSidesList->getNumSides() == 0)
		return;
	ScriptList* list = TheSidesList->getSideInfo(0)->getScriptList();
	if (!list)
		return;
	Player* gateOwner = gateObject->getControllingPlayer();
	Team* gateTeam = gateOwner ? gateOwner->getDefaultTeam() : nullptr;
	if (!gateTeam)
		return;

	ensureNamedGate(gateObject);

	if (ScriptAction* resetCounter = actionNamed("SET_COUNTER"))
	{
		resetCounter->getParameter(0)->friend_setString(seqCounterName);
		resetCounter->getParameter(1)->friend_setInt(0);
		TheScriptEngine->friend_executeAction(resetCounter, nullptr);
		deleteInstance(resetCounter);
	}

	if (ScriptAction* needOpen = actionNamed("TEAM_NEEDS_OPEN_GATE"))
	{
		needOpen->getParameter(0)->friend_setString(gateTeam->getName());
		needOpen->getParameter(1)->friend_setString(gateRefName);
		needOpen->getParameter(2)->friend_setInt(1);
		TheScriptEngine->friend_executeAction(needOpen, gateTeam);
		deleteInstance(needOpen);
	}

	if (Script* existing = const_cast<Script*>(TheScriptEngine->findScriptByName(seqScriptName)))
		list->deleteScript(existing);

	ScriptAction* waitGate = actionNamed("GATE_READY");
	ScriptAction* setCounter = actionNamed("SET_COUNTER");
	ScriptAction* releaseGate = actionNamed("TEAM_NEEDS_OPEN_GATE");
	if (!waitGate || !setCounter || !releaseGate)
	{
		if (waitGate)
			deleteInstance(waitGate);
		if (setCounter)
			deleteInstance(setCounter);
		if (releaseGate)
			deleteInstance(releaseGate);
		return;
	}
	waitGate->getParameter(0)->friend_setString(gateRefName);
	setCounter->getParameter(0)->friend_setString(seqCounterName);
	setCounter->getParameter(1)->friend_setInt(1);
	releaseGate->getParameter(0)->friend_setString(gateTeam->getName());
	releaseGate->getParameter(1)->friend_setString(gateRefName);
	releaseGate->getParameter(2)->friend_setInt(0);
	waitGate->setNextAction(setCounter);
	setCounter->setNextAction(releaseGate);

	Script* script = newInstance(Script);
	script->setName(AsciiString(seqScriptName));
	script->setActive(FALSE);
	script->setSubroutine(TRUE);
	script->setAction(waitGate);
	list->addScript(script, 0);

	if (AIUpdateInterface* ai = unit->getAIUpdateInterface())
		ai->aiIdle(CMD_FROM_SCRIPT);

	SequentialScript* seqScript = newInstance(SequentialScript);
	seqScript->m_objectID = unit->getID();
	seqScript->m_scriptToExecuteSequentially = script;
	seqScript->m_timesToLoop = 0;
	TheScriptEngine->appendSequentialScript(seqScript);
	deleteInstance(seqScript);
}

void probeSequential(const RetailTestCommands::Context& ctx)
{
	const Object* object = ctx.requireObject();
	const GateOpenAndCloseBehavior* gate = GateOpenAndCloseBehavior::resolveForControl(object);
	if (!gate)
		return;
	const Int needs = gate->getTeamOpenRequestCount();
	const Bool wants = gate->wantsOpen();
	const Bool passable = gate->isPassable();
	const Bool ready = evaluateGateReady(gateRefName);
	const Int seqDone = TheScriptEngine->getCounterValue(AsciiString(seqCounterName));
	std::fprintf(stderr, "TEST script-gate-ops-seq frame=%u needs=%d wantsOpen=%d passable=%d ready=%d seqDone=%d\n",
		ctx.frame, needs, wants ? 1 : 0, passable ? 1 : 0, ready ? 1 : 0, seqDone);
}

void cleanupSequential(const RetailTestCommands::Context& ctx)
{
	if (Object* unit = ctx.findObject())
		TheScriptEngine->removeAllSequentialScripts(unit);
	if (TheSidesList && TheSidesList->getNumSides() > 0)
		if (ScriptList* list = TheSidesList->getSideInfo(0)->getScriptList())
			if (Script* existing = const_cast<Script*>(TheScriptEngine->findScriptByName(seqScriptName)))
				list->deleteScript(existing);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailGateScriptOpsTestCommands()
{
#if RTS_ZEROHOUR
	registerAction("script-gate-ops-need", setNeedGate);
	registerAction("script-gate-ops-probe", probeGate);
	registerAction("script-gate-ops-seq-start", startSequential);
	registerAction("script-gate-ops-seq-probe", probeSequential);
	registerAction("script-gate-ops-seq-cleanup", cleanupSequential);
#endif
}
}
