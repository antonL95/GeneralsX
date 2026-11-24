#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/ThingTemplate.h"
#include "GameClient/Drawable.h"
#include "GameLogic/Damage.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Object.h"

namespace
{
void probeWeaponFXTrigger(const RetailTestCommands::Context& ctx)
{
	const Object* building = ctx.requireObject(true);
	if (!building)
		return;

	Int debris = 0;
	for (const Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
	{
		if (object->getProducerID() == building->getID() &&
			object->getTemplate()->getName().startsWith("BuildingDamageBits"))
			++debris;
	}

	std::fprintf(stderr, "TEST weapon-fx-impact frame=%u case=%s damaged=%d debris=%d\n",
		ctx.frame, ctx.argument, building->getBodyModule()->getHealth() < building->getBodyModule()->getMaxHealth(), debris);
}

void probeImpactDrawable(const RetailTestCommands::Context& ctx)
{
	const Object* object = ctx.requireObject(true);
	const Drawable* drawable = object ? object->getDrawable() : nullptr;
	if (!drawable)
		return;

	std::fprintf(stderr, "TEST impact-drawable frame=%u case=%s trigger=%s\n",
		ctx.frame, ctx.argument, RetailDamageFXTriggerNames[drawable->getRetailImpactFXTrigger()]);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailWeaponFXTriggerTestCommands()
{
#if !RTS_GENERALS
	registerAction("weapon-fx-impact", probeWeaponFXTrigger);
	registerAction("weapon-fx-drawable", probeImpactDrawable);
#endif
}
}
