#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include "GameLogic/AIStateMachine.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"
#include <cstdio>

namespace
{
void relocate(const RetailTestCommands::Context& ctx)
{
	Object* obj = ctx.requireObject();
	if (!obj)
		return;

	Coord3D position = *obj->getPosition();
	if (std::sscanf(ctx.argument, "%f,%f", &position.x, &position.y) == 2)
	{
		position.z = TheTerrainLogic->getLayerHeight(position.x, position.y, obj->getLayer());
		obj->setPosition(&position);
	}
}

void probe(const RetailTestCommands::Context& ctx)
{
	const Object* obj = ctx.requireObject();
	const AIUpdateInterface* ai = obj ? obj->getAI() : nullptr;
	if (ai)
	{
		std::fprintf(stderr, "TEST guard-save frame=%u guard=%d attacking=%d\n", ctx.frame,
			ai->getCurrentStateID() == AI_GUARD, ai->isAttacking());
	}
}
}
#endif

namespace RetailTestCommands
{
void registerRetailGuardSaveTestCommands()
{
#if !RTS_GENERALS
	registerAction("guard-save-relocate", relocate);
	registerAction("guard-save-probe", probe);
#endif
}
}
