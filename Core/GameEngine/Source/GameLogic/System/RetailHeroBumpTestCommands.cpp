#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/ThingTemplate.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/Pathfinder/Path.h"
#include "GameLogic/Pathfinder/PathNode.h"

namespace
{
void probeHeroBump(const RetailTestCommands::Context& ctx)
{
	Object* object = ctx.requireObject();
	AIUpdateInterface* ai = object ? object->getAI() : nullptr;
	if (!ai)
		return;

	const Coord3D& pos = *object->getPosition();
	const Coord3D& goal = *ai->getGoalPosition();
	const Locomotor* locomotor = ai->getCurLocomotor();
	Path* path = ai->getPath();
	const PathNode* first = path ? path->getFirstNode() : nullptr;
	const PathNode* last = path ? path->getLastNode() : nullptr;
	const PathNode* next = first ? first->getNextOptimized() : nullptr;
	const Coord3D* step = next ? next->getPosition() : nullptr;
	const Coord3D* end = last ? last->getPosition() : nullptr;
	std::fprintf(stderr, "TEST hero-bump %u %s %d %s id=%u pos=%.3f,%.3f goal=%.3f,%.3f end=%.3f,%.3f state=%d blocked=%d stuck=%d waiting=%d through=%d loco=%s next=%.3f,%.3f yield=%d\n",
		ctx.frame, object->getTemplate()->getName().str(), ctx.ordinal,
		ctx.argument, static_cast<unsigned>(object->getID()),
		pos.x, pos.y, goal.x, goal.y, end ? end->x : 0.0f, end ? end->y : 0.0f,
		static_cast<int>(ai->getCurrentStateID()), ai->getNumFramesBlocked(),
		static_cast<int>(ai->isBlockedAndStuck()), static_cast<int>(ai->isWaitingForPath()),
		static_cast<int>(ai->canPathThroughUnits()), locomotor ? locomotor->getTemplateName().str() : "none",
		step ? step->x : 0.0f, step ? step->y : 0.0f,
		static_cast<int>(ctx.target && ai->isMovingAwayFrom(ctx.target)));
}
}
#endif

namespace RetailTestCommands
{
void registerRetailHeroBumpTestCommands()
{
#if !RTS_GENERALS
	registerAction("hero-bump-probe", probeHeroBump);
#endif
}
}
