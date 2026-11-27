#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "GameClient/Drawable.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object.h"

namespace
{
void traceNonDirtyTransform(const RetailTestCommands::Context& ctx)
{
	const Object* bird = ctx.requireObject();
	if (!bird)
		return;
	const AIUpdateInterface* ai = bird->getAI();
	const Locomotor* locomotor = ai ? ai->getCurLocomotor() : nullptr;
	const Coord3D& pos = *bird->getPosition();
	const ICoord3D& triggerPos = bird->getTriggerAreaPosition();
	const Drawable* drawable = bird->getDrawable();
	const Coord3D drawPos = drawable ? *drawable->getPosition() : Coord3D{};
	std::fprintf(stderr,
		"TEST non-dirty-transform frame=%u bird=%s x=%.3f y=%.3f z=%.3f ix=%d iy=%d dx=%.3f dy=%.3f dz=%.3f non_dirty=%d\n",
		ctx.frame, ctx.targetName, pos.x, pos.y, pos.z, triggerPos.x, triggerPos.y,
		drawPos.x, drawPos.y, drawPos.z, locomotor && locomotor->getNonDirtyTransform() ? 1 : 0);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailNonDirtyTransformTestCommands()
{
#if !RTS_GENERALS
	registerAction("non-dirty-transform-trace", traceNonDirtyTransform);
#endif
}
}
