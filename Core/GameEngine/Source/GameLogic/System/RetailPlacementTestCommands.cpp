#include "PreRTS.h"

#include <cstdio>

#include "Common/Player.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/ObjectCreationList.h"

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
