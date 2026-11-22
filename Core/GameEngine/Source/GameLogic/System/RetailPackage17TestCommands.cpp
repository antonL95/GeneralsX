#include "PreRTS.h"

#if !RTS_GENERALS
#include <cstdio>
#include <string_view>

#include "Common/ThingTemplate.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Object.h"
#include "GameLogic/Weapon.h"

void runRetailPackage17TestCommand(std::string_view action, Object* source, Object* target)
{
	const auto* horde = HordeContain::get(source);
	if (!horde)
		return;
	if (action == "pkg17-fire")
	{
		const auto* defenders = HordeContain::get(target);
		const auto* weapon = TheWeaponStore->findWeaponTemplate("Pkg17Flank");
		if (!defenders || !weapon)
			return;
		Object* attacker = horde->findNearestLivingMember(*target->getPosition());
		Object* defender = defenders->findNearestLivingMember(*source->getPosition());
		if (attacker && defender)
			TheWeaponStore->createAndFireTempWeapon(weapon, attacker, defender);
	}
	else if (action == "pkg17-state")
	{
		Int living = 0;
		Int backing = 0;
		Int attacking = 0;
		for (Object* member : *horde->getContainedItemsList())
		{
			if (member->isEffectivelyDead())
				continue;
			++living;
			backing += member->getLogicModelConditions().test(MODELCONDITION_BACKING_UP);
			const auto* ai = member->getAI();
			attacking += ai && ai->isAttacking();
		}
		std::fprintf(stderr, "TEST pkg17 formation frame=%u id=%u name=%s living=%d backing=%d attacking=%d x=%.2f y=%.2f facing=%.3f\n",
			TheGameLogic->getFrame(), static_cast<unsigned>(source->getID()), source->getTemplate()->getName().str(),
			living, backing, attacking, source->getPosition()->x, source->getPosition()->y, source->getOrientation());
	}
}
#endif
