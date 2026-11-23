#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if RTS_ZEROHOUR
#include <cstdio>
#include <cstdlib>

#include "Common/ActionManager.h"
#include "Common/MessageStream.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/Radar.h"
#include "Common/SpecialPower.h"
#include "Common/Team.h"
#include "GameClient/ControlBar.h"
#include "GameLogic/Damage.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Object.h"
#include "GameLogic/ScriptActions.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Scripts.h"

namespace
{
void neutralizeObjective(const RetailTestCommands::Context& ctx)
{
	if (Object* object = ctx.requireObject())
		object->setTeam(ThePlayerList->getNeutralPlayer()->getDefaultTeam());
}

void captureObjective(const RetailTestCommands::Context& ctx)
{
	Object* actor = ctx.requireObject();
	Object* flag = ctx.findTarget();
	const CommandButton* button = TheRetailCommandCatalog->findCommandButton("Command_CaptureBuilding");
	if (!actor || !flag || !button)
		return;
	const Bool allowed = TheActionManager->canCaptureBuilding(actor, flag, CMD_FROM_PLAYER);
	std::fprintf(stderr, "TEST radar capture order frame=%u actor=%u flag=%u allowed=%d\n",
		ctx.frame, static_cast<unsigned>(actor->getID()), static_cast<unsigned>(flag->getID()), allowed);
	GameMessage* order = TheMessageStream->appendMessage(GameMessage::MSG_DO_SPECIAL_POWER_AT_OBJECT);
	order->appendIntegerArgument(button->getSpecialPowerTemplate()->getID());
	order->appendObjectIDArgument(flag->getID());
	order->appendIntegerArgument(button->getOptions());
	order->appendObjectIDArgument(actor->getID());
}

void damageUnit(const RetailTestCommands::Context& ctx)
{
	Object* source = ctx.requireObject();
	Object* target = ctx.findTarget();
	if (!source || !target)
		return;

	const Bool fatal = ctx.action == "radar-fall";
	DamageInfo damage;
	damage.shouldPlayUnderAttackEvaEvent = !fatal;
	damage.in.m_damageType = DAMAGE_UNRESISTABLE;
	damage.in.m_deathType = DEATH_NORMAL;
	damage.in.m_sourceID = source->getID();
	damage.in.m_sourcePlayerMask = source->getControllingPlayer()->getPlayerMask();
	damage.in.m_amount = fatal ? target->getBodyModule()->getMaxHealth() : 1.0f;
	damage.in.m_kill = fatal;
	target->attemptDamage(&damage);
	std::fprintf(stderr, "TEST radar damage frame=%u damage=%.1f dead=%d\n", ctx.frame,
		damage.out.m_actualDamageDealt, target->isEffectivelyDead());
}

void scriptRadarEvent(const RetailTestCommands::Context& ctx)
{
	const Coord3D position = ctx.groundLocation();
	auto* action = newInstance(ScriptAction)(ScriptAction::RADAR_CREATE_EVENT);
	action->getParameter(0)->friend_setCoord3D(&position);
	action->getParameter(1)->friend_setInt(std::atoi(ctx.argument));
	TheScriptActions->executeAction(action);
	deleteInstance(action);
}

void namedRadarEvent(const RetailTestCommands::Context& ctx)
{
	Object* source = ctx.requireObject();
	if (!source)
		return;
	const Bool team = ctx.action == "radar-script-team";
	if (!team && source->getName().isEmpty())
	{
		source->setName("RadarTestNamedObject");
		TheScriptEngine->addObjectToCache(source);
	}
	auto* action = newInstance(ScriptAction)(team ? ScriptAction::TEAM_CREATE_RADAR_EVENT : ScriptAction::OBJECT_CREATE_RADAR_EVENT);
	action->getParameter(0)->friend_setString(team ? source->getTeam()->getName() : source->getName());
	action->getParameter(1)->friend_setInt(std::atoi(ctx.argument));
	TheScriptActions->executeAction(action);
	deleteInstance(action);

	if (!team)
	{
		Coord3D position;
		const Bool remembered = TheRadar->getLastEventLoc(&position) &&
			position.x == source->getPosition()->x && position.y == source->getPosition()->y;
		std::fprintf(stderr, "TEST radar named object frame=%u remembered=%d\n", ctx.frame, remembered);
	}
}
}
#endif

namespace RetailTestCommands
{
void registerRetailRadarTestCommands()
{
#if RTS_ZEROHOUR
	registerAction("radar-neutralize", neutralizeObjective);
	registerAction("radar-capture", captureObjective);
	registerAction("radar-hit", damageUnit);
	registerAction("radar-fall", damageUnit);
	registerAction("radar-script", scriptRadarEvent);
	registerAction("radar-script-object", namedRadarEvent);
	registerAction("radar-script-team", namedRadarEvent);
#endif
}
}
