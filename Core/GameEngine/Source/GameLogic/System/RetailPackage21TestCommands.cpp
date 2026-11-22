#include "PreRTS.h"

#if !RTS_GENERALS
#include <cstdio>
#include <string_view>

#include "Common/BuildAssistant.h"
#include "Common/Player.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "GameLogic/ArmorSet.h"
#include "GameLogic/Module/CreateModule.h"
#include "GameLogic/Module/GeometryUpgrade.h"
#include "GameLogic/Object.h"
#include "GameLogic/Weapon.h"

void runRetailPackage21TestCommand(std::string_view action, Object* object, const char* argument, const Coord3D* position, UnsignedInt frame)
{
	if (action == "pkg21-build-at")
	{
		const ThingTemplate* type = TheThingFactory->findTemplate(argument, FALSE);
		if (type && position)
			TheBuildAssistant->buildObjectNow(nullptr, type, position, 0.0f, object->getControllingPlayer());
		return;
	}

	if (action == "pkg21-build-complete")
	{
		for (BehaviorModule** module = object->getBehaviorModules(); *module; ++module)
			if (CreateModuleInterface* create = (*module)->getCreate())
				create->onBuildComplete();
		return;
	}

	if (action == "pkg21-player-upgrade")
	{
		if (const auto* upgrade = TheUpgradeCenter->findUpgrade(argument))
			object->getControllingPlayer()->addUpgrade(upgrade, UPGRADE_STATUS_COMPLETE);
		return;
	}

	if (action == "pkg21-deny" || action == "pkg21-allowed")
	{
		const ThingTemplate* hero = TheThingFactory->findTemplate(argument, FALSE);
		if (!hero)
			return;
		Player* player = object->getControllingPlayer();
		if (action == "pkg21-deny")
			player->setBuildingAllowed(hero, FALSE);
		std::fprintf(stderr, "TEST pkg21 roster frame=%u player=%d hero=%s allowed=%d\n", frame,
			player->getPlayerIndex(), argument, player->allowedToBuild(hero));
		return;
	}

	const auto* upgrade = TheUpgradeCenter->findUpgrade(argument);
	const Weapon* weapon = object->getCurrentWeapon();
	const auto& condition = object->getLogicModelConditions();
	const auto& geometry = object->getGeometryInfo();
	std::vector<GeometryUpgrade::CollisionPart> parts;
	GeometryUpgrade::collectCollisionParts(*object, parts);
	std::fprintf(stderr,
		"TEST pkg21 state frame=%u object=%s upgrade=%s eligible=%d granted=%d armor=%d armor2=%d heroArmor=%d user1=%d armorModel=%d user2=%d garrison=%d trebuchet=%d weaponUpgrade=%d geometry=%.1f,%.1f,%.1f parts=%zu weapon=%s\n",
		frame, object->getTemplate()->getName().str(), argument, upgrade && object->affectedByUpgrade(upgrade),
		upgrade && object->hasUpgrade(upgrade), object->testArmorSetFlag(ARMORSET_PLAYER_UPGRADE),
		object->testArmorSetFlag(ARMORSET_PLAYER_UPGRADE_2), object->testArmorSetFlag(ARMORSET_CREATE_A_HERO_01),
		condition.test(MODELCONDITION_USER_1), condition.test(MODELCONDITION_ARMORSET_PLAYER_UPGRADE),
		condition.test(MODELCONDITION_USER_2), condition.test(MODELCONDITION_UPGRADE_GARRISON),
		condition.test(MODELCONDITION_UPGRADE_TREBUCHET), object->testWeaponSetFlag(WEAPONSET_PLAYER_UPGRADE),
		geometry.getMajorRadius(), geometry.getMinorRadius(), geometry.getMaxHeightAbovePosition(), parts.size(),
		weapon ? weapon->getTemplate()->getName().str() : "-");
}
#endif
