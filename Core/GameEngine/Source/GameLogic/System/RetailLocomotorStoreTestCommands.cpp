#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/ThingTemplate.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object.h"

namespace
{
void sampleLocomotor(const RetailTestCommands::Context& ctx)
{
	const Object* object = ctx.requireObject();
	const AIUpdateInterface* ai = object ? object->getAI() : nullptr;
	const Locomotor* locomotor = ai ? ai->getCurLocomotor() : nullptr;
	if (locomotor)
	{
		const Coord3D& pos = *object->getPosition();
		std::fprintf(stderr, "TEST locomotor-store %u %s %d %.3f %.3f\n",
			ctx.frame, object->getTemplate()->getName().str(),
			locomotor->participatesInGroupFormation(), pos.x, pos.y);
	}
}

void sampleAuthoredFields(const RetailTestCommands::Context& ctx)
{
	const Object* object = ctx.requireObject();
	const AIUpdateInterface* ai = object ? object->getAI() : nullptr;
	const Locomotor* locomotor = ai ? ai->getCurLocomotor() : nullptr;
	if (locomotor)
	{
		const Real degrees = locomotor->getTurnThresholdHighSpeed() * 360.0f / RotwkLocomotorRules::FULL_TURN;
		std::fprintf(stderr, "TEST locomotor-store-fields %u %s turnHS=%.1f overlap=%.6g scales=%d smoothing=%d\n",
			ctx.frame, object->getTemplate()->getName().str(), degrees, locomotor->getMaxOverlappedHeight(),
			locomotor->scalesWalls(), locomotor->usesTerrainSmoothing());
	}
}
}
#endif

namespace RetailTestCommands
{
void registerRetailLocomotorStoreTestCommands()
{
#if !RTS_GENERALS
	registerAction("locomotor-store-sample", sampleLocomotor);
	registerAction("locomotor-store-fields", sampleAuthoredFields);
#endif
}
}
