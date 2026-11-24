#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/Player.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "GameClient/Drawable.h"
#include "GameLogic/Object.h"
#include "GameLogic/Weapon.h"

namespace
{
void handlePackage32(const RetailTestCommands::Context& ctx)
{
	const auto* upgrade = TheUpgradeCenter->findUpgrade(ctx.argument);
	if (ctx.action == "pkg32-grant-player" || ctx.action == "pkg32-player-upgrade")
	{
		Player* player = ctx.findPlayer();
		if (!player || !upgrade)
			return;
		if (ctx.action == "pkg32-grant-player")
			player->addUpgrade(upgrade, UPGRADE_STATUS_COMPLETE);
		else
			std::fprintf(stderr, "PKG32 %u %s player-upgrade %s %d\n", ctx.frame,
				ctx.playerName.data(), ctx.argument, player->hasUpgradeComplete(upgrade));
		return;
	}

	Object* object = ctx.requireObject();
	if (!object)
		return;

	const auto print = [&](const char* category, Int value) {
		std::fprintf(stderr, "PKG32 %u %s %s %s %d\n", ctx.frame,
			object->getTemplate()->getName().str(), category, ctx.argument, value);
	};
	if (ctx.action == "pkg32-model")
	{
		const Int flag = ModelConditionFlags::getSingleBitFromName(ctx.argument);
		const Drawable* drawable = object->getDrawable();
		if (flag >= 0)
		{
			print("model", object->getLogicModelConditions().test(flag));
			if (drawable)
				print("drawable", drawable->getModelConditionFlags().test(flag));
		}
	}
	else if (ctx.action == "pkg32-status")
	{
		const Int flag = ObjectStatusMaskType::getSingleBitFromName(ctx.argument);
		if (flag >= 0)
			print("status", object->testStatus(static_cast<ObjectStatusTypes>(flag)));
	}
	else if (ctx.action == "pkg32-weapon")
	{
		const Int flag = WeaponSetFlags::getSingleBitFromName(ctx.argument);
		if (flag >= 0)
			print("weapon", object->getWeaponSetFlags().test(flag));
	}
	else if (ctx.action == "pkg32-weapon-name")
	{
		const Weapon* weapon = object->getCurrentWeapon();
		print("weapon-name", weapon && weapon->getTemplate()->getName() == ctx.argument);
	}
	else if (ctx.action == "pkg32-object-upgrade" && upgrade)
		print("object-upgrade", object->hasUpgrade(upgrade));
	else if (ctx.action == "pkg32-revoke" && upgrade)
		object->removeUpgrade(upgrade);
	else if (ctx.action == "pkg32-refresh-subobjects")
		object->forceRefreshSubObjectUpgradeStatus();
}
}

#endif

namespace RetailTestCommands
{
void registerRetailPackage32TestCommands()
{
#if !RTS_GENERALS
	registerPrefix("pkg32-", handlePackage32);
#endif
}
}
