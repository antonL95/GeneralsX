#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/ThingTemplate.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Weapon.h"

namespace
{
void traceGiantBirdSwoop(const RetailTestCommands::Context& ctx)
{
	const Object* bird = ctx.requireObject();
	if (!bird)
		return;

	const Coord3D& pos = *bird->getPosition();
	const PhysicsBehavior* birdPhysics = bird->getPhysics();
	const Coord3D birdVel = birdPhysics ? *birdPhysics->getVelocity() : Coord3D{};
	const ContainModuleInterface* contain = bird->getContain();
	const ContainedItemsList* items = contain ? contain->getContainedItemsList() : nullptr;
	const Object* passenger = items && !items->empty() ? items->front() : nullptr;
	const Object* target = ctx.findTarget(ctx.argument, false);
	if (!target)
		target = passenger;
	const Object* carrier = target ? target->getContainedBy() : nullptr;
	const PhysicsBehavior* targetPhysics = target ? target->getPhysics() : nullptr;
	const Coord3D targetVel = targetPhysics ? *targetPhysics->getVelocity() : Coord3D{};
	const Real targetZ = target ? target->getPosition()->z - TheTerrainLogic->getGroundHeight(target->getPosition()->x, target->getPosition()->y) : 0.0f;
	UnsignedInt lastShot = 0;
	for (Int slot = PRIMARY_WEAPON; slot < WEAPONSLOT_COUNT; ++slot)
	{
		const Weapon* weapon = bird->getWeaponInWeaponSlot(static_cast<WeaponSlotType>(slot));
		if (weapon && weapon->getLastShotFrame() > lastShot)
			lastShot = weapon->getLastShotFrame();
	}

	std::fprintf(stderr,
		"TEST giant-bird-swoop frame=%u bird=%s:%d swoop=%d height=%.3f bvx=%.3f bvy=%.3f bvz=%.3f shot=%u contain=%u passenger=%s target=%s carrier=%s freefall=%d impact=%d tvx=%.3f tvy=%.3f tvz=%.3f tz=%.3f\n",
		ctx.frame, ctx.targetName, ctx.ordinal,
		bird->getLogicModelConditions().test(MODELCONDITION_SWOOPING) ? 1 : 0,
		pos.z - TheTerrainLogic->getGroundHeight(pos.x, pos.y),
		birdVel.x, birdVel.y, birdVel.z,
		lastShot,
		contain ? contain->getContainCount() : 0u,
		passenger ? passenger->getTemplate()->getName().str() : "-",
		target ? target->getTemplate()->getName().str() : "-",
		carrier ? carrier->getTemplate()->getName().str() : "-",
		target && target->getLogicModelConditions().test(MODELCONDITION_FREEFALL) ? 1 : 0,
		targetPhysics && targetPhysics->isInRetailImpact() ? 1 : 0,
		targetVel.x, targetVel.y, targetVel.z, targetZ);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailGiantBirdSwoopTestCommands()
{
#if !RTS_GENERALS
	registerAction("giant-bird-swoop-trace", traceGiantBirdSwoop);
#endif
}
}
