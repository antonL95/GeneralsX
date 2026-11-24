#include "PreRTS.h"

#include <cstdio>

#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/RunOffMapBehavior.h"
#include "GameLogic/Object.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
namespace
{
void igniteBeast(const RetailTestCommands::Context& ctx)
{
	if (Object* beast = ctx.requireObject())
	{
		DamageInfo damage;
		damage.in.m_amount = 1700.0f;
		damage.in.m_damageType = DAMAGE_FLAME;
		damage.in.m_deathType = DEATH_BURNED;
		beast->attemptDamage(&damage);
	}
}

void inspectBeast(const RetailTestCommands::Context& ctx)
{
	const Object* beast = ctx.requireObject();
	if (!beast)
		return;
	const auto* contain = beast->getContain();
	const auto* runOff = RunOffMapBehavior::find(beast);
	const Coord3D& position = *beast->getPosition();
	const auto* ai = beast->getAI();
	std::fprintf(stderr, "TEST beast-rampage frame=%u case=%s aflame=%d rampage=%d running=%d passengers=%u enraged-locomotor=%d position=%.1f,%.1f\n",
		ctx.frame, ctx.argument, beast->testStatus(OBJECT_STATUS_AFLAME), beast->testStatus(OBJECT_STATUS_RAMPAGING),
		runOff && runOff->isRunning(), contain ? contain->getContainCount() : 0,
		ai && ai->getCurLocomotorSetType() == LOCOMOTORSET_ENRAGED, position.x, position.y);
}

void seatRider(const RetailTestCommands::Context& ctx)
{
	Object* rider = ctx.requireObject();
	Object* beast = ctx.findTarget();
	if (!rider || !beast)
		return;
	rider->setStatus(MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_NO_ATTACK));
	if (auto* contain = beast->getContain(); contain && contain->isValidContainerFor(rider, TRUE))
		contain->addToContain(rider);
	std::fprintf(stderr, "TEST beast-rampage seat frame=%u rider=%d admitted=%d\n", ctx.frame, ctx.ordinal,
		rider->getContainedBy() == beast);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailBeastRampageTestCommands()
{
#if !RTS_GENERALS
	registerAction("beast-rampage-fire", igniteBeast);
	registerAction("beast-rampage-state", inspectBeast);
	registerAction("beast-rampage-seat", seatRider);
#endif
}
}
