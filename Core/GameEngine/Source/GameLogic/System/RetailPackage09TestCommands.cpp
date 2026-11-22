#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>
#include <string_view>

#include "Common/ActionManager.h"
#include "Common/SpecialPower.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "GameClient/Drawable.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/StealthUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"


void runRetailPackage09TestCommand(std::string_view action, Object* source, Object* target, const char* argument, UnsignedInt frame)
{
	if (action == "pkg09-state")
	{
		const auto* stealth = source->getStealth();
		Int members = 0;
		Int hiddenMembers = 0;
		const auto* contain = source->getContain();
		if (contain)
			for (const auto* member : *contain->getContainedItemsList())
			{
				++members;
				hiddenMembers += member->testStatus(OBJECT_STATUS_STEALTHED);
			}
		std::fprintf(stderr, "TEST pkg09 frame=%u name=%s hidden=%d detected=%d sense=%d opacity=%.3f members=%d hiddenMembers=%d x=%.1f y=%.1f\n",
			frame, source->getTemplate()->getName().str(), source->testStatus(OBJECT_STATUS_STEALTHED),
			source->testStatus(OBJECT_STATUS_DETECTED), source->getLogicModelConditions().test(MODELCONDITION_SPECIAL_ENEMY_NEAR),
			stealth ? stealth->getFriendlyOpacity() : -1.0f, members, hiddenMembers, source->getPosition()->x, source->getPosition()->y);
	}
	else if (action == "pkg09-cast")
	{
		const auto* power = TheSpecialPowerStore->findSpecialPowerTemplate(argument);
		const Bool allowed = power && TheActionManager->canDoSpecialPower(source, power, CMD_FROM_PLAYER, 0);
		std::fprintf(stderr, "TEST pkg09 frame=%u cast=%s allowed=%d\n", frame, argument, allowed);
		if (allowed)
			source->doSpecialPower(power, 0);
	}
	else if (action == "pkg09-upgrade")
	{
		if (const auto* upgrade = TheUpgradeCenter->findUpgrade(argument))
			source->giveUpgrade(upgrade);
	}
	else if (action == "pkg09-weapon-set")
	{
		if (std::string_view(argument) == "on")
			source->setWeaponSetFlag(WEAPONSET_CLOSE_RANGE);
		else
			source->clearWeaponSetFlag(WEAPONSET_CLOSE_RANGE);
	}
	else if (action == "pkg09-protect")
		source->setTempInvulnerable(240);
	else if (action == "pkg09-disguise" && target && source->getStealth())
	{
		source->getStealth()->disguiseAsObject(target);
		std::fprintf(stderr, "TEST pkg09 frame=%u disguise=%d\n", frame, source->getStealth()->isDisguised());
	}
	else if (action == "pkg09-position")
	{
		Coord3D point;
		if (std::sscanf(argument, "%f,%f", &point.x, &point.y) == 2)
		{
			point.z = TheTerrainLogic->getGroundHeight(point.x, point.y);
			source->setPosition(&point);
		}
	}
}
#endif

namespace RetailTestCommands
{
void registerRetailPackage09TestCommands()
{
#if !RTS_GENERALS
	registerPrefix("pkg09-", [](const Context& ctx) {
		Object* source = ctx.requireObject();
		if (!source)
			return;
		runRetailPackage09TestCommand(ctx.action, source, ctx.target, ctx.argument, ctx.frame);
	});
#endif
}
}
