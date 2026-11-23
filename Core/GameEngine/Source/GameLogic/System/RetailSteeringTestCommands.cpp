#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/MessageStream.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/Pathfinder/Path.h"
#include "GameLogic/Pathfinder/PathNode.h"

namespace
{
void logPathNodes(const PathNode* node, Bool optimized)
{
	for (; node; node = optimized ? node->getNextOptimized() : node->getNext())
	{
		const Coord3D& point = *node->getPosition();
		std::fprintf(stderr, " %.2f,%.2f,%d", point.x, point.y, static_cast<Int>(node->getLayer()));
	}
}

void logSteering(const RetailTestCommands::Context& ctx)
{
	Object* object = ctx.requireObject();
	AIUpdateInterface* ai = object ? object->getAI() : nullptr;
	if (!ai)
		return;

	const Coord3D& pos = *object->getPosition();
	const Coord3D& goal = *ai->getGoalPosition();
	const Locomotor* locomotor = ai->getCurLocomotor();
	const PhysicsBehavior* physics = object->getPhysics();
	Path* path = ai->getPath();
	const auto& flags = object->getLogicModelConditions();
	std::fprintf(stderr, "TEST steering %u %s pos=%.3f,%.3f facing=%.5f goal=%.3f,%.3f moving=%d velocity=%.4f loco=%s left=%d right=%d leftHS=%d rightHS=%d accel=%d decel=%d",
		ctx.frame, object->getTemplate()->getName().str(), pos.x, pos.y, object->getOrientation(), goal.x, goal.y,
		static_cast<int>(ai->isMoving()), physics ? physics->getForwardSpeed2D() : 0.0f,
		locomotor ? locomotor->getTemplateName().str() : "none",
		static_cast<int>(flags.test(MODELCONDITION_TURN_LEFT)), static_cast<int>(flags.test(MODELCONDITION_TURN_RIGHT)),
		static_cast<int>(flags.test(MODELCONDITION_TURN_LEFT_HIGH_SPEED)), static_cast<int>(flags.test(MODELCONDITION_TURN_RIGHT_HIGH_SPEED)),
		static_cast<int>(flags.test(MODELCONDITION_ACCELERATE)), static_cast<int>(flags.test(MODELCONDITION_DECELERATE)));
	const PathNode* first = path ? path->getFirstNode() : nullptr;
	std::fputs(" raw:", stderr);
	logPathNodes(first, FALSE);
	std::fputs(" optimized:", stderr);
	logPathNodes(first, TRUE);
	std::fputc('\n', stderr);
}

void stopSelection(const RetailTestCommands::Context&)
{
	TheMessageStream->appendMessage(GameMessage::MSG_DO_STOP);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailSteeringTestCommands()
{
#if !RTS_GENERALS
	registerAction("steering-path", logSteering);
	registerAction("steering-stop", stopSelection);
#endif
}
}
