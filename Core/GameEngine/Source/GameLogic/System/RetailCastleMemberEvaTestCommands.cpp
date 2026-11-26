#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>
#include <string_view>

#include "Common/GameAudio.h"
#include "Common/Player.h"
#include "GameClient/RetailEva.h"
#include "GameLogic/Damage.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/CastleBehavior.h"
#include "GameLogic/Module/RotwkParseOnlyModules.h"
#include "GameLogic/Object.h"

namespace
{
void unpackCastle(const RetailTestCommands::Context& ctx)
{
	Object* packed = ctx.requireObject();
	if (!packed)
		return;
	if (CastleBehavior* castle = CastleBehavior::find(packed))
	{
		castle->unpackFree();
		if (Object* keep = castle->getKeep())
			keep->setStatus(MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_NO_ATTACK));
	}
}

void linkMember(const RetailTestCommands::Context& ctx)
{
	Object* member = ctx.requireObject();
	Object* castle = ctx.findTarget(false);
	if (!member || !castle)
		return;
	const Bool isKeep = ctx.action == "castle-member-eva-link-keep";
	CastleMemberBehavior::linkToCastle(*member, castle->getID(), isKeep ? member->getID() : INVALID_ID, isKeep, TRUE);
}

void setConstruction(const RetailTestCommands::Context& ctx)
{
	Object* member = ctx.requireObject();
	if (!member)
		return;
	const std::string_view arg(ctx.argument);
	if (arg == "start")
	{
		if (TheAudio)
		{
			const Coord3D orientation = {0.0f, 1.0f, 0.0f};
			TheAudio->setListenerPosition(member->getPosition(), &orientation);
		}
		member->setModelConditionState(MODELCONDITION_ACTIVELY_BEING_CONSTRUCTED);
	}
	else if (arg == "stop")
		member->clearModelConditionState(MODELCONDITION_ACTIVELY_BEING_CONSTRUCTED);
}

void applyHit(const RetailTestCommands::Context& ctx)
{
	Object* source = ctx.requireObject();
	Object* target = ctx.findTarget(true);
	if (!source || !target || !source->getControllingPlayer() || !target->getBodyModule())
		return;

	const Bool fatal = ctx.action == "castle-member-eva-kill";
	DamageInfo damage;
	damage.shouldPlayUnderAttackEvaEvent = !fatal;
	damage.in.m_sourceID = source->getID();
	damage.in.m_sourcePlayerMask = source->getControllingPlayer()->getPlayerMask();
	damage.in.m_damageType = DAMAGE_UNRESISTABLE;
	damage.in.m_deathType = DEATH_NORMAL;
	damage.in.m_amount = fatal ? target->getBodyModule()->getMaxHealth() : 1.0f;
	damage.in.m_kill = fatal;
	target->attemptDamage(&damage);
}

void probeEva(const RetailTestCommands::Context& ctx)
{
	UnsignedInt playedFrame = 0;
	const Bool played = RetailEva::getLastPlayed(AsciiString(ctx.argument), playedFrame);
	std::fprintf(stderr, "TEST castle-member-eva eva frame=%u event=%s played=%d\n",
		ctx.frame, ctx.argument, played ? 1 : 0);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailCastleMemberEvaTestCommands()
{
#if !RTS_GENERALS
	registerAction("castle-member-eva-unpack", unpackCastle);
	registerAction("castle-member-eva-link", linkMember);
	registerAction("castle-member-eva-link-keep", linkMember);
	registerAction("castle-member-eva-construct", setConstruction);
	registerAction("castle-member-eva-damage", applyHit);
	registerAction("castle-member-eva-kill", applyHit);
	registerAction("castle-member-eva-probe", probeEva);
#endif
}
}
