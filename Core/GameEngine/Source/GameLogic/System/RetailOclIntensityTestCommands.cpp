#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/RandomValue.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/ObjectCreationList.h"
#include "GameLogic/TerrainLogic.h"

namespace
{
void traceObject(UnsignedInt frame, const Object& object)
{
	const PhysicsBehavior* physics = object.getPhysics();
	if (!physics)
		return;

	const Coord3D& pos = *object.getPosition();
	const Coord3D& velocity = *physics->getVelocity();
	const Real ground = TheTerrainLogic->getLayerHeight(pos.x, pos.y, object.getLayer());
	std::fprintf(stderr, "TEST ocl-intensity %u %s %.6f %.6f %.6f %.6f %.6f %.6f %.6f\n",
		frame, object.getTemplate()->getName().str(), pos.x, pos.y, pos.z - ground,
		velocity.x, velocity.y, velocity.z, ground);
}

void spawnFromList(const RetailTestCommands::Context& ctx)
{
	Object* source = ctx.requireObject();
	if (!source)
		return;

	const auto* list = TheObjectCreationListStore->findObjectCreationList(ctx.argument);
	const bool seeded = ctx.action == "ocl-intensity-fixture-spawn";
	const auto originalRandom = getGameLogicRandomState();
	if (seeded)
		setGameLogicRandomState({0, {0xf22d0e56, 0x883126e9, 0xc624dd2f, 0x0702c49c, 0x9e353f7d, 0x6fdf3b64}});
	const Object* object = ObjectCreationList::create(list, source, static_cast<const Object*>(nullptr));
	if (seeded)
		setGameLogicRandomState(originalRandom);
	if (object)
		traceObject(ctx.frame, *object);
}

void traceFlight(const RetailTestCommands::Context& ctx)
{
	if (const Object* object = ctx.findObject(false))
		traceObject(ctx.frame, *object);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailOclIntensityTestCommands()
{
#if !RTS_GENERALS
	registerAction("ocl-intensity-spawn", spawnFromList);
	registerAction("ocl-intensity-fixture-spawn", spawnFromList);
	registerAction("ocl-intensity-trace", traceFlight);
#endif
}
}
