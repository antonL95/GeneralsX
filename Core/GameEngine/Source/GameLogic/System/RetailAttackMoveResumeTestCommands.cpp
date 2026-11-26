#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Object.h"

namespace
{
void probe(const RetailTestCommands::Context& ctx)
{
	const Object* anchor = ctx.requireObject(true);
	const HordeContain* horde = HordeContain::get(anchor);
	if (!horde || !anchor->getAI())
		return;

	const AIUpdateInterface* ai = anchor->getAI();
	const Coord3D& position = *anchor->getPosition();
	const Coord3D& goal = *ai->getGoalPosition();
	std::fprintf(stderr, "TEST attack-move-resume %u %.2f %.2f %d %d %.2f %.2f\n", ctx.frame,
		position.x, position.y, ctx.findTarget(true) != nullptr, static_cast<int>(ai->getCurrentStateID()), goal.x, goal.y);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailAttackMoveResumeTestCommands()
{
#if !RTS_GENERALS
	registerAction("attack-move-resume-probe", probe);
#endif
}
}
