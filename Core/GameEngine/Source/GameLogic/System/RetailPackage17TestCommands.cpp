#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

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
	else if (action == "pkg17-members")
	{
		for (Object* member : *horde->getContainedItemsList())
		{
			if (member->isEffectivelyDead())
				continue;
			auto* ai = member->getAI();
			const Object* victim = ai ? ai->getGoalObject() : nullptr;
			HordeContain::FormationGoal goal{};
			horde->getFormationGoal(member, goal);
			const Coord3D& position = *member->getPosition();
			std::fprintf(stderr,
				"TEST pkg17 member frame=%u horde=%u id=%u backing=%d attacking=%d target=%u facingX=%.3f retreatX=%.3f x=%.3f fearX=%.1f\n",
				TheGameLogic->getFrame(), static_cast<unsigned>(source->getID()), static_cast<unsigned>(member->getID()),
				member->getLogicModelConditions().test(MODELCONDITION_BACKING_UP), ai && ai->isAttacking(),
				victim ? static_cast<unsigned>(victim->getID()) : 0, member->getUnitDirectionVector2D()->x,
				goal.backing ? goal.position.x - position.x : 0.0f, position.x, target ? target->getPosition()->x : -1.0f);
		}
	}
}
#endif

namespace RetailTestCommands
{
void registerRetailPackage17TestCommands()
{
#if !RTS_GENERALS
	registerPrefix("pkg17-", [](const Context& ctx) {
		Object* source = ctx.requireObject();
		if (!source)
			return;
		runRetailPackage17TestCommand(ctx.action, source, ctx.target);
	});
#endif
}
}
