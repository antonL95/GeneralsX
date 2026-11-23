#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/ThingTemplate.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Weapon.h"

namespace
{
void traceEagle(const RetailTestCommands::Context& ctx)
{
	const Object* bird = ctx.requireObject();
	if (!bird)
		return;

	const Coord3D& pos = *bird->getPosition();
	const Object* victim = bird->getAI() ? bird->getAI()->getCurrentVictim() : nullptr;
	const Object* target = ctx.findTarget(ctx.argument, false);
	if (!target)
		target = victim;
	const Object* carrier = target ? target->getContainedBy() : nullptr;
	const PhysicsBehavior* physics = target ? target->getPhysics() : nullptr;
	const Coord3D velocity = physics ? *physics->getVelocity() : Coord3D{};
	WeaponSlotType slot = PRIMARY_WEAPON;
	const Weapon* weapon = bird->getCurrentWeapon(&slot);
	std::fprintf(stderr, "TEST eagle frame=%u bird=%s:%d pos=%.3f,%.3f,%.3f height=%.3f swoop=%d slot=%d weapon=%s moving-fire=%d in-range=%d victim=%s target=%s hp=%.3f carrier=%s velocity=%.3f,%.3f,%.3f",
		ctx.frame, ctx.targetName, ctx.ordinal, pos.x, pos.y, pos.z,
		pos.z - TheTerrainLogic->getGroundHeight(pos.x, pos.y),
		bird->getLogicModelConditions().test(MODELCONDITION_SWOOPING), static_cast<Int>(slot),
		weapon ? weapon->getName().str() : "-", weapon && weapon->canFireInCurrentMovement(bird),
		weapon && victim && weapon->isWithinAttackRange(bird, victim), victim ? victim->getTemplate()->getName().str() : "-",
		target ? target->getTemplate()->getName().str() : "-", target && target->getBodyModule() ? target->getBodyModule()->getHealth() : 0.0f,
		carrier ? carrier->getTemplate()->getName().str() : "-", velocity.x, velocity.y, velocity.z);
	for (Int i = PRIMARY_WEAPON; i < WEAPONSLOT_COUNT; ++i)
	{
		const Weapon* candidate = bird->getWeaponInWeaponSlot(static_cast<WeaponSlotType>(i));
		if (candidate)
			std::fprintf(stderr, " fire%d=%u", i, candidate->getLastShotFrame());
	}
	std::fputc('\n', stderr);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailEagleAttackTestCommands()
{
#if !RTS_GENERALS
	registerAction("eagle-trace", traceEagle);
#endif
}
}
