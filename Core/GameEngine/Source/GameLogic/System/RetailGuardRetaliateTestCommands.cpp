#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include "GameLogic/AI.h"
#include "GameLogic/AIGuardRetaliate.h"
#include "GameLogic/AIStateMachine.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Weapon.h"
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
	if (!ai)
		return;

	const Weapon* weapon = obj->getCurrentWeapon();
	const WeaponTemplate* weaponTemplate = weapon ? weapon->getTemplate() : nullptr;
	const Bool melee = weaponTemplate ? weaponTemplate->isMeleeWeapon() : FALSE;
	const Real stdRange = AIGuardRetaliateMachine::getStdGuardRange(obj);
	const Real exitRadius = TheAI->getAdjustedVisionRangeForObject(obj, AI_VISIONFACTOR_OWNERTYPE | AI_VISIONFACTOR_MOOD) + stdRange;
	std::fprintf(stderr, "TEST guard-retaliate frame=%u name=%s retaliating=%d attacking=%d melee=%d stdRange=%.2f exitRadius=%.2f chaseFrames=%u\n",
		ctx.frame,
		ctx.targetName,
		ai->getCurrentStateID() == AI_GUARD_RETALIATE,
		obj->testStatus(OBJECT_STATUS_IS_ATTACKING),
		melee,
		stdRange,
		exitRadius,
		TheAI->getAiData()->m_guardChaseUnitFrames);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailGuardRetaliateTestCommands()
{
#if !RTS_GENERALS
	registerAction("guard-retaliate-relocate", relocate);
	registerAction("guard-retaliate-probe", probe);
#endif
}
}
