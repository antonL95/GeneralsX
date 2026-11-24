#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/Player.h"
#include "Common/Radar.h"
#include "GameLogic/Damage.h"
#include "GameLogic/Object.h"

namespace
{
void hitFromStealth(const RetailTestCommands::Context& ctx)
{
	Object* source = ctx.requireObject();
	Object* victim = ctx.findTarget();
	if (!source || !victim)
		return;

	source->setStatus(MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_STEALTHED));
	DamageInfo hit;
	hit.shouldPlayUnderAttackEvaEvent = TRUE;
	hit.in.m_sourceID = source->getID();
	hit.in.m_sourcePlayerMask = source->getControllingPlayer()->getPlayerMask();
	hit.in.m_damageType = DAMAGE_UNRESISTABLE;
	hit.in.m_deathType = DEATH_NORMAL;
	hit.in.m_amount = 1.0f;
	victim->attemptDamage(&hit);

	Coord3D radar;
	const Bool ping = TheRadar->getLastEventLoc(&radar) &&
		radar.x == victim->getPosition()->x && radar.y == victim->getPosition()->y;
	const auto &flags = victim->getLogicModelConditions();
	std::fprintf(stderr, "TEST engaged-damage frame=%u engaged=%d attacking=%d alert=%d undetected=%d damage=%.1f ping=%d\n",
		ctx.frame, flags.test(MODELCONDITION_ENGAGED), flags.test(MODELCONDITION_ATTACKING),
		flags.test(MODELCONDITION_EMOTION_ALERT), !source->testStatus(OBJECT_STATUS_DETECTED),
		hit.out.m_actualDamageClipped, ping);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailEngagedDamageTestCommands()
{
#if !RTS_GENERALS
	registerAction("engaged-damage-hit", hitFromStealth);
#endif
}
}
