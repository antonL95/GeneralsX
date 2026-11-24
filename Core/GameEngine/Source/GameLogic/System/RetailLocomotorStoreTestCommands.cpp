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
}
#endif

namespace RetailTestCommands
{
void registerRetailLocomotorStoreTestCommands()
{
#if !RTS_GENERALS
	registerAction("locomotor-store-sample", sampleLocomotor);
#endif
}
}
