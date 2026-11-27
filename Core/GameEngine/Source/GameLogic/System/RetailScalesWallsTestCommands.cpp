#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if RTS_ZEROHOUR
#include <cstdio>
#include "Common/MessageStream.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Object.h"

namespace
{
void moveToWall(const RetailTestCommands::Context& ctx)
{
	Object* object = ctx.requireObject(FALSE);
	if (!object || !ctx.hasPosition)
		return;
	Coord3D point = ctx.groundLocation();
	point.z = TheAI->pathfinder()->getWallHeightAt(point);
	std::fprintf(stderr, "TEST scales-walls-destination frame=%u name=%s wall=%d\n",
		ctx.frame, ctx.targetName, TheAI->pathfinder()->isPointOnWall(&point));
	ctx.selectObject(object);
	TheMessageStream->appendMessage(GameMessage::MSG_DO_MOVETO)->appendLocationArgument(point);
}

void sample(const RetailTestCommands::Context& ctx)
{
	Object* object = ctx.requireObject(FALSE);
	if (!object)
		return;
	AIUpdateInterface* ai = object->getAIUpdateInterface();
	const Locomotor* locomotor = ai ? ai->getCurLocomotor() : nullptr;
	const Real speed = locomotor ? locomotor->getMaxSpeedForCondition(object->getBodyModule()->getDamageState(), object) : 0.0f;
	const Coord3D& pos = *object->getPosition();
	std::fprintf(stderr, "TEST scales-walls frame=%u name=%s z=%.2f layer=%d x=%.2f y=%.2f speed=%.4f\n",
		ctx.frame, ctx.targetName, pos.z, object->getLayer(), pos.x, pos.y, speed);
	if (object->isKindOf(KINDOF_SCALEABLE_WALL))
		std::fprintf(stderr, "TEST scales-walls-obstacle frame=%u name=%s wall=%d\n",
			ctx.frame, ctx.targetName, TheAI->pathfinder()->isPointOnWall(&pos));
	std::fprintf(stderr, "TEST scales-walls-capability name=%s value=%d\n", ctx.targetName,
		ai && ai->getLocomotorSet().scalesWalls());
}
}
#endif

namespace RetailTestCommands
{
void registerRetailScalesWallsTestCommands()
{
#if RTS_ZEROHOUR
	registerAction("scales-walls-move", moveToWall);
	registerAction("scales-walls-sample", sample);
#endif
}
}
