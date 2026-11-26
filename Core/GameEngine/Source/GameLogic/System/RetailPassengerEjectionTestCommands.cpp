#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>
#include <string_view>
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/Weapon.h"

namespace
{
void handlePassengerEjection(const RetailTestCommands::Context& ctx)
{
	Object* object = ctx.requireObject();
	if (!object)
		return;
	const std::string_view action(ctx.action);
	if (action == "passenger-ejection-grab" && ctx.target)
	{
		const WeaponTemplate* data = TheWeaponStore->findWeaponTemplate("EjectionGrab");
		if (data)
		{
			Weapon* weapon = TheWeaponStore->allocateNewWeapon(data, PRIMARY_WEAPON);
			weapon->loadAmmoNow(object);
			weapon->fireWeapon(object, ctx.target);
			deleteInstance(weapon);
		}
	}
	else if (action == "passenger-ejection-turn")
		object->setOrientation(PI / 2.0f);
	else if (action == "passenger-ejection-kill")
		object->kill();
	else if (action == "passenger-ejection-sample")
	{
		const PhysicsBehavior* physics = object->getPhysics();
		const Coord3D velocity = physics ? *physics->getVelocity() : Coord3D{};
		std::fprintf(stderr, "TEST passenger-ejection frame=%u %s attached=%d dead=%d angle=%.3f velocity=%.3f,%.3f,%.3f\n",
			ctx.frame, ctx.argument, object->getContainedBy() != nullptr, object->isEffectivelyDead(),
			object->getOrientation(), velocity.x, velocity.y, velocity.z);
	}
}
}
#endif

namespace RetailTestCommands
{
void registerRetailPassengerEjectionTestCommands()
{
#if !RTS_GENERALS
	registerPrefix("passenger-ejection-", handlePassengerEjection);
#endif
}
}
