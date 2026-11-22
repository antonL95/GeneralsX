#include "PreRTS.h"

#include <cstdio>

#include "Common/Player.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/ObjectCreationList.h"
#include "GameLogic/RetailTestCommands.h"
#include "GameLogic/TerrainLogic.h"
#include "WWMath/matrix3d.h"

namespace
{
void logRetailPlacementTestObject(const Object* spawned, const char* name, UnsignedInt frame)
{
	if (!spawned)
	{
		std::fprintf(stderr, "PKG03 frame=%u list=%s spawned=none\n", frame, name);
		return;
	}

	const Coord3D& pos = *spawned->getPosition();
	const Player* owner = spawned->getControllingPlayer();
	const auto* ai = spawned->getAI();
	const auto* physics = spawned->getPhysics();
	const Coord3D velocity = physics ? *physics->getVelocity() : Coord3D{0.0f, 0.0f, 0.0f};
	std::fprintf(stderr, "PKG03 frame=%u list=%s type=%s owner=%s cp=%d upgrades=%d,%d pos=%.3f,%.3f,%.3f angle=%.3f "
		"noCollisions=%d idle=%d destroyed=%d velocity=%.3f,%.3f,%.3f\n",
		frame, name, spawned->getTemplate()->getName().str(), owner ? owner->getPlayerName().str() : "-",
		owner ? owner->getCommandPointsUsed() : 0, owner ? owner->getCompletedUpgradeMask().count() : 0,
		spawned->getObjectCompletedUpgradeMask().count(), pos.x, pos.y, pos.z, spawned->getOrientation(),
		spawned->testStatus(OBJECT_STATUS_NO_COLLISIONS), ai ? static_cast<int>(ai->isIdle()) : -1,
		spawned->isDestroyed(), velocity.x, velocity.y, velocity.z);
}

void runRetailPlacementTestCommand(Object* source, const char* name, const Coord3D* target, UnsignedInt frame)
{
	const auto* list = TheObjectCreationListStore->findObjectCreationList(name);
#if RTS_GENERALS
	const Object* spawned = ObjectCreationList::create(list, source, source->getPosition(), target);
#else
	const Object* spawned = ObjectCreationList::create(list, source, source->getPosition(), target, INVALID_ANGLE);
#endif
	logRetailPlacementTestObject(spawned, name, frame);
}

void logRetailSpawnPoints(const Object* producer, const char* prefix, UnsignedInt frame)
{
	Coord3D bones[32];
	Matrix3D transforms[32];
	Bool occupied[32]{};
	const Int count = producer->getMultiLogicalBonePosition(prefix, 32, bones, transforms, FALSE);
	Int raised = 0;
	for (Int i = 0; i < count; ++i)
	{
		producer->convertBonePosToWorldPos(&bones[i], nullptr, &bones[i], nullptr);
		const Real ground = TheTerrainLogic->getLayerHeight(bones[i].x, bones[i].y, producer->getLayer());
		if (bones[i].z > ground + 1.0f)
			++raised;
		bones[i].z = MAX(bones[i].z, ground);
	}

	Int occupants = 0;
	Int matched = 0;
	for (const Object* child = TheGameLogic->getFirstObject(); child; child = child->getNextObject())
	{
		if (child->getProducerID() != producer->getID() || !child->isDisabledByType(DISABLED_HELD) || child->isEffectivelyDead())
			continue;
		++occupants;
		const Coord3D deltaOrigin = *child->getPosition();
		for (Int i = 0; i < count; ++i)
		{
			const Coord3D delta = deltaOrigin - bones[i];
			if (!occupied[i] && delta.x * delta.x + delta.y * delta.y + delta.z * delta.z < 0.01f)
			{
				occupied[i] = TRUE;
				++matched;
				break;
			}
		}
	}
	std::fprintf(stderr, "TEST spawn-points frame=%u prefix=%s bones=%d raised=%d occupants=%d matched=%d\n",
		frame, prefix, count, raised, occupants, matched);
}
}

namespace RetailTestCommands
{
void registerRetailPlacementTestCommands()
{
	registerAction("placement-state", [](const Context& ctx) {
		logRetailPlacementTestObject(ctx.source, ctx.argument, ctx.frame);
	});
	registerAction("placement-ocl", [](const Context& ctx) {
		Object* source = ctx.requireObject();
		if (!source)
			return;
		const Coord3D target = ctx.hasPosition ? ctx.groundLocation() : Coord3D{};
		runRetailPlacementTestCommand(source, ctx.argument, ctx.hasPosition ? &target : nullptr, ctx.frame);
	});
	registerAction("placement-spawn-points", [](const Context& ctx) {
		Object* source = ctx.requireObject();
		if (!source)
			return;
		logRetailSpawnPoints(source, ctx.argument, ctx.frame);
	});
}
}
