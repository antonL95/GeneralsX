#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <array>
#include <cmath>
#include <cstdio>
#include <span>
#include <string_view>

#include "Common/GameCommon.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/SlavedUpdate.h"
#include "GameLogic/Object.h"

namespace
{
struct SpawnSlot
{
	Vector3 offset;
	Real degrees;
};

const SpawnSlot fellbeastSlots[] = {
	{ Vector3(0.0f, 0.0f, 200.0f), 240.0f },
};

const SpawnSlot garrisonSlots[] = {
	{ Vector3(31.282f, 16.346f, 88.636f), 45.0f },
	{ Vector3(31.282f, -22.011f, 88.636f), -45.0f },
	{ Vector3(-32.976f, -22.011f, 88.636f), -135.0f },
	{ Vector3(-32.976f, 16.346f, 88.636f), 135.0f },
	{ Vector3(-20.025f, 29.297f, 88.636f), 135.0f },
	{ Vector3(18.331f, 29.297f, 88.636f), 45.0f },
	{ Vector3(-20.0f, -34.962f, 88.636f), -135.0f },
	{ Vector3(18.331f, -34.962f, 88.636f), -45.0f },
};

void observeSpawns(const RetailTestCommands::Context& ctx)
{
	const Object* source = ctx.requireObject();
	if (!source)
		return;

	const Bool mordor = std::string_view(ctx.targetName) == "MordorFortressCitadel";
	const std::span<const SpawnSlot> slots = mordor ? std::span<const SpawnSlot>(fellbeastSlots) : std::span<const SpawnSlot>(garrisonSlots);
	const AsciiString childName(mordor ? "MordorIdleFellbeast" : "MirkwoodPrisonGuard_Slaved");
	const Real heading = source->getOrientation();
	std::array<ObjectID, std::size(garrisonSlots)> ids{};
	Int count = 0;
	Int matched = 0;
	Real flightAngleDelta = 0.0f;
	Real flightDistanceSquared = 0.0f;

	for (const Object* child = TheGameLogic->getFirstObject(); child; child = child->getNextObject())
	{
		if (child->getProducerID() != source->getID() || child->isEffectivelyDead() ||
			child->getTemplate()->getName() != childName)
			continue;

		++count;
		const auto* slave = static_cast<const SlavedUpdate*>(child->findUpdateModule(NAMEKEY("SlavedUpdate")));
		const AIUpdateInterface* ai = child->getAIUpdateInterface();
		const Coord3D& position = *child->getPosition();
		for (size_t index = 0; index < slots.size(); ++index)
		{
			Vector3 world;
			Matrix3D::Transform_Vector(*source->getTransformMatrix(), slots[index].offset, &world);
			const Real dx = position.x - world.X;
			const Real dy = position.y - world.Y;
			const Real distanceSquared = dx * dx + dy * dy;
			if ((!mordor && distanceSquared > 1.0f) || ids[index] != INVALID_ID)
				continue;

			const Real expected = heading + slots[index].degrees * (PI / 180.0f);
			const Real angleDelta = stdAngleDiff(child->getOrientation(), expected);
			if (mordor)
			{
				flightAngleDelta = angleDelta;
				flightDistanceSquared = distanceSquared;
			}
			if ((mordor || std::fabs(angleDelta) < 0.001f) &&
				slave && slave->getSlaverID() == source->getID() && ai && !ai->getCurrentVictim() && !ai->isMoving())
			{
				ids[index] = child->getID();
				++matched;
			}
			break;
		}
	}
	std::fprintf(stderr, "SPAWN_ANGLE %u %s %d %d %d ", ctx.frame, ctx.targetName, count, matched, std::fabs(heading) > 0.1f);
	for (size_t index = 0; index < slots.size(); ++index)
		std::fprintf(stderr, "%s%u", index ? "," : "", static_cast<UnsignedInt>(ids[index]));
	std::fprintf(stderr, " %.6f %.6f heading=%.5f\n", flightAngleDelta, flightDistanceSquared, heading);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailSpawnAngleTestCommands()
{
#if !RTS_GENERALS
	registerAction("spawn-angle-state", observeSpawns);
#endif
}
}
