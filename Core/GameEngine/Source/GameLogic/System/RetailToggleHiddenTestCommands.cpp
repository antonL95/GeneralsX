#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/SpecialPower.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Object.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/WeaponSetType.h"

namespace
{
void probeToggleHidden(const RetailTestCommands::Context& ctx)
{
	const Object* object = ctx.requireObject();
	if (!object)
		return;
	const BodyModuleInterface* body = object->getBodyModule();
	const SpecialPowerTemplate* power = ctx.argument && *ctx.argument
		? TheSpecialPowerStore->findSpecialPowerTemplate(ctx.argument)
		: nullptr;
	const SpecialPowerModuleInterface* spm = power ? object->getSpecialPowerModule(power) : nullptr;
	std::fprintf(stderr, "TEST toggle-hidden frame=%u object=%s hidden=%d weapon_hidden=%d has_weapon=%d health=%.2f ready=%d ready_frame=%u percent=%.4f\n",
		ctx.frame,
		object->getTemplate()->getName().str(),
		object->testStatus(OBJECT_STATUS_HIDDEN) ? 1 : 0,
		object->getWeaponSetFlags().test(WEAPONSET_HIDDEN) ? 1 : 0,
		object->hasAnyWeapon() ? 1 : 0,
		body ? body->getHealth() : 0.0f,
		spm && spm->isReady() ? 1 : 0,
		spm ? spm->getReadyFrame() : 0u,
		spm ? spm->getPercentReady() : 0.0f);
}

void attackTarget(const RetailTestCommands::Context& ctx)
{
	Object* object = ctx.requireObject();
	Object* target = ctx.findTarget();
	if (!object || !target || !object->getAI())
		return;
	object->getAI()->aiAttackObject(target, NO_MAX_SHOTS_LIMIT, CMD_FROM_PLAYER);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailToggleHiddenTestCommands()
{
#if !RTS_GENERALS
	registerAction("toggle-hidden-probe", probeToggleHidden);
	registerAction("toggle-hidden-attack", attackTarget);
#endif
}
}
