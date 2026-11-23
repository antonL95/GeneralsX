#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "GameClient/Drawable.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"

namespace
{
void probeEngaged(const RetailTestCommands::Context& ctx)
{
	Object* unit = ctx.requireObject();
	if (!unit || !unit->getAI())
		return;

	const auto& flags = unit->getLogicModelConditions();
	const Bool usingWeapon = flags.test(MODELCONDITION_USING_WEAPON_A) ||
		flags.test(MODELCONDITION_USING_WEAPON_B) || flags.test(MODELCONDITION_USING_WEAPON_C);
	const Object* target = unit->getAI()->getGoalObject();
	const Drawable* draw = unit->getDrawable();
	std::fprintf(stderr, "TEST engaged frame=%u using=%d attacking=%d live-target=%d engaged=%d hit=%d draw-both=%d\n",
		ctx.frame, usingWeapon, flags.test(MODELCONDITION_ATTACKING), target && !target->isEffectivelyDead(),
		flags.test(MODELCONDITION_ENGAGED), flags.test(MODELCONDITION_HIT_REACTION),
		draw && draw->getModelConditionFlags().test(MODELCONDITION_ENGAGED) &&
		draw->getModelConditionFlags().test(MODELCONDITION_HIT_REACTION));
}

void relocateEngagedTarget(const RetailTestCommands::Context& ctx)
{
	if (Object* target = ctx.requireObject())
	{
		Coord3D position = *target->getPosition();
		if (std::sscanf(ctx.argument, "%f,%f", &position.x, &position.y) == 2)
		{
			position.z = TheTerrainLogic->getLayerHeight(position.x, position.y, target->getLayer());
			target->setPosition(&position);
		}
	}
}
}
#endif

namespace RetailTestCommands
{
void registerRetailEngagedTestCommands()
{
#if !RTS_GENERALS
	registerAction("engaged-probe", probeEngaged);
	registerAction("engaged-relocate", relocateEngagedTarget);
#endif
}
}
