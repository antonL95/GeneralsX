#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/ThingTemplate.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Object.h"
#include "GameLogic/Weapon.h"

namespace
{
void probeReach(const RetailTestCommands::Context& ctx)
{
	if (ctx.action == "pkg61-resolve")
	{
		if (const WeaponTemplate* weapon = TheWeaponStore->findWeaponTemplate(AsciiString(ctx.argument)))
			const_cast<WeaponTemplate*>(weapon)->postProcessLoad();
		return;
	}

	Object* source = ctx.requireObject();
	if (!source)
		return;

	const Weapon* weapon = source->getWeaponInWeaponSlot(PRIMARY_WEAPON);
	if (ctx.action == "pkg61-state")
	{
		const auto* contain = source->getContain();
		std::fprintf(stderr, "TEST pkg61 state frame=%u source=%s layer=%d swoop=%d cargo=%d last=%u\n",
			ctx.frame, source->getTemplate()->getName().str(), static_cast<Int>(source->getLayer()),
			source->getLogicModelConditions().test(MODELCONDITION_SWOOPING),
			contain ? contain->getContainCount() : 0, weapon ? weapon->getLastShotFrame() : 0);
		return;
	}

	Object* target = ctx.findTarget();
	if (!target)
		return;

	if (ctx.action == "pkg61-elevation")
	{
		Coord3D position = *source->getPosition();
		position.z = target->getPosition()->z + ctx.position.x;
		source->setPosition(&position);
	}
	else if (ctx.action == "pkg61-wall-layer")
	{
		Coord3D position = *target->getPosition();
		position.z += ctx.position.x;
		target->setPosition(&position);
		target->setDestinationLayer(LAYER_WALL);
		target->setLayer(LAYER_WALL);
	}
	else if (ctx.action == "pkg61-range" && weapon)
	{
		std::fprintf(stderr, "TEST pkg61 range frame=%u weapon=%s height=%.1f range=%.2f eligible=%d\n",
			ctx.frame, weapon->getName().str(), source->getPosition()->z - target->getPosition()->z,
			weapon->getAttackRange(source, target->getPosition()), weapon->isWithinAttackRange(source, target));
	}
}
}
#endif

namespace RetailTestCommands
{
void registerRetailPackage61TestCommands()
{
#if !RTS_GENERALS
	registerPrefix("pkg61-", probeReach);
#endif
}
}
