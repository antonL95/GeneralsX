#include "PreRTS.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/SpecialPower.h"
#include "Common/ThingFactory.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/RotwkAbilityModules.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"

void runRetailAbilityTimingTestCommand(Player* player, const char* hero, const char* powerName,
	const Coord3D& location, Int targetMode, UnsignedInt frame)
{
	const auto* tmpl = TheThingFactory->findTemplate(hero, FALSE);
	const auto* power = TheSpecialPowerStore->findSpecialPowerTemplate(powerName);
	if (!player || !tmpl || !power)
	{
		std::fprintf(stderr, "TEST ability-probe frame=%u hero=%s power=%s id=0 accepted=0\n", frame, hero, powerName);
		return;
	}
	const auto place = [](Object* object, Coord3D position)
	{
		position.z = TheTerrainLogic->getGroundHeight(position.x, position.y);
		object->setPosition(&position);
		object->setOrientation(0.0f);
		object->getBodyModule()->setIndestructible(TRUE);
	};
	Object* caster = TheThingFactory->newObject(tmpl, player->getDefaultTeam());
	place(caster, location);
	Coord3D point = *caster->getPosition();
	point.x += 30.0f;
	point.z = TheTerrainLogic->getGroundHeight(point.x, point.y);

	Object* target = nullptr;
	if (targetMode >= 2)
	{
		Player* owner = targetMode == 3 ? player : ThePlayerList->findPlayerWithNameKey(NAMEKEY("PlyrCreeps"));
		const auto* dummy = TheThingFactory->findTemplate("GondorFighter", FALSE);
		if (owner && dummy)
		{
			target = TheThingFactory->newObject(dummy, owner->getDefaultTeam());
			place(target, point);
		}
	}
	Bool accepted = FALSE;
	if (caster->getSpecialPowerModule(power))
	{
		for (BehaviorModule** module = caster->getBehaviorModules(); *module; ++module)
		{
			auto* ability = dynamic_cast<WeaponFireSpecialAbilityUpdate*>(*module);
			if (ability && ability->initiateIntentToDoSpecialPower(power, target, targetMode == 1 ? &point : nullptr, nullptr, 0))
			{
				accepted = TRUE;
				break;
			}
		}
	}
	std::fprintf(stderr, "TEST ability-probe frame=%u hero=%s power=%s id=%u accepted=%d\n",
		frame, hero, powerName, static_cast<UnsignedInt>(caster->getID()), accepted);
}
#endif
