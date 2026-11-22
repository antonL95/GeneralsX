#include "PreRTS.h"

#if !RTS_GENERALS
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

#include "Common/Player.h"
#include "Common/PlayerTemplate.h"
#include "Common/Upgrade.h"
#include "GameClient/ControlBar.h"
#include "GameLogic/CreateAHeroRules.h"
#include "GameLogic/CreateAHeroSystem.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/Object.h"
#include "GameNetwork/GameInfo.h"

void runRetailPackage18TestCommand(std::string_view action, Player* player, Object* source, const char* argument, UnsignedInt frame)
{
	if (action == "pkg18-system")
	{
		const CreateAHeroClassDef* dwarf = CreateAHeroSystem::findClassByUpgrade("Upgrade_CreateAHero_ClassDwarf");
		const CreateAHeroSubClassDef* taskmaster = CreateAHeroSystem::findSubClassByUpgrade(dwarf, "Upgrade_CreateAHero_SubClass_0");
		const CreateAHeroSubClassDef* sage = CreateAHeroSystem::findSubClassByUpgrade(dwarf, "Upgrade_CreateAHero_SubClass_1");
		std::fprintf(stderr,
			"TEST pkg18 system frame=%u loaded=%d classes=%zu blings=%zu binders=%zu revivalDisc=%d powerDisc=%d dwarfFound=%d taskmasterPts=%d taskmasterDefBlings=%zu sagePts=%d sageDefBlings=%zu\n",
			frame,
			CreateAHeroSystem::isLoaded() ? 1 : 0,
			CreateAHeroSystem::getClasses().size(),
			CreateAHeroSystem::getBlings().size(),
			CreateAHeroSystem::getBlingBinders().size(),
			CreateAHeroSystem::getHeroRevivalDiscount(),
			CreateAHeroSystem::getSpecialPowerDiscountPerLevel(),
			dwarf ? 1 : 0,
			taskmaster ? taskmaster->spendableAttributePoints : 0,
			taskmaster ? taskmaster->defaultBlingUpgrades.size() : 0u,
			sage ? sage->spendableAttributePoints : 0,
			sage ? sage->defaultBlingUpgrades.size() : 0u);
		return;
	}

	if (action == "pkg18-button")
	{
		const CommandButton* btn = (TheRetailCommandCatalog && argument && *argument)
			? TheRetailCommandCatalog->findCommandButton(argument)
			: nullptr;
		std::string allowJoined;
		if (btn)
		{
			for (const AsciiString& u : btn->getCreateAHeroUIAllowableUpgrades())
			{
				if (!allowJoined.empty())
					allowJoined.push_back(',');
				allowJoined.append(u.str());
			}
		}
		CreateAHeroRules::SelectionData dwarfSel;
		CreateAHeroSystem::buildDefaultSelection("Upgrade_CreateAHero_ClassDwarf", "Upgrade_CreateAHero_SubClass_0", dwarfSel);
		CreateAHeroRules::SelectionData wraithSel;
		CreateAHeroSystem::buildDefaultSelection("Upgrade_CreateAHero_ClassWraith", "Upgrade_CreateAHero_SubClass_0", wraithSel);
		const Bool dwarfLvl1 = CreateAHeroSystem::isPowerButtonLegalForSelection(btn, dwarfSel, 1, {});
		const Bool wraithLvl1 = CreateAHeroSystem::isPowerButtonLegalForSelection(btn, wraithSel, 1, {});
		const Bool wraithLvl3 = CreateAHeroSystem::isPowerButtonLegalForSelection(btn, wraithSel, 3, {});
		std::fprintf(stderr,
			"TEST pkg18 button frame=%u name=%s found=%d minLevel=%d cost=%d costLvl3=%d prereq=%s allowCount=%zu dwarfLvl1=%d wraithLvl1=%d wraithLvl3=%d\n",
			frame,
			argument ? argument : "-",
			btn ? 1 : 0,
			btn ? btn->getCreateAHeroUIMinimumLevel() : 0,
			btn ? btn->getCreateAHeroUICostIfSelected() : 0,
			CreateAHeroSystem::effectivePowerCost(btn, 3),
			btn && !btn->getCreateAHeroUIPrerequisiteButtonName().isEmpty() ? btn->getCreateAHeroUIPrerequisiteButtonName().str() : "-",
			btn ? btn->getCreateAHeroUIAllowableUpgrades().size() : 0u,
			dwarfLvl1 ? 1 : 0,
			wraithLvl1 ? 1 : 0,
			wraithLvl3 ? 1 : 0);
		return;
	}

	if (action == "pkg18-select")
	{
		if (!player || !argument || !*argument)
			return;
		CreateAHeroRules::SelectionData decoded;
		Bool ok = CreateAHeroRules::decodeSelection(argument, decoded);
		CreateAHeroRules::SelectionData normalized;
		const AsciiString faction = player->getPlayerTemplate() ? player->getPlayerTemplate()->getSide() : AsciiString::TheEmptyString;
		if (ok)
			ok = CreateAHeroSystem::validateSelection(decoded, faction, normalized);
		Bool wireOk = FALSE;
		if (ok)
		{
			const AsciiString encoded(CreateAHeroRules::encodeSelection(normalized).c_str());
			SkirmishGameInfo srcInfo;
			srcInfo.init();
			srcInfo.setMap("Maps/Training/Training.map");
			srcInfo.setMapCRC(1);
			srcInfo.setMapSize(1);
			srcInfo.setSeed(1);
			GameSlot slot0;
			slot0.setState(SLOT_PLAYER, UnicodeString(L"P1"), 0x01020304);
			slot0.setCreateAHeroConfig(encoded);
			srcInfo.setSlot(0, slot0);
			const AsciiString wire = GameInfoToAsciiString(&srcInfo);
			SkirmishGameInfo dstInfo;
			dstInfo.init();
			if (ParseAsciiStringToGameInfo(&dstInfo, wire))
			{
				const GameSlot* dstSlot = dstInfo.getConstSlot(0);
				if (dstSlot && dstSlot->getCreateAHeroConfig() == encoded)
				{
					wireOk = CreateAHeroSystem::setPlayerSelectionEncoded(player->getPlayerIndex(), dstSlot->getCreateAHeroConfig(), faction);
				}
			}
		}
		std::fprintf(stderr,
			"TEST pkg18 select frame=%u player=%d accepted=%d wireOk=%d class=%s subclass=%s blings=%zu attrs=%zu powers=%zu\n",
			frame,
			player->getPlayerIndex(),
			ok ? 1 : 0,
			wireOk ? 1 : 0,
			ok ? normalized.classUpgrade.c_str() : "-",
			ok ? normalized.subClassUpgrade.c_str() : "-",
			ok ? normalized.blingUpgrades.size() : 0u,
			ok ? normalized.attributeUpgrades.size() : 0u,
			ok ? normalized.powerButtons.size() : 0u);
		return;
	}

	if (action == "pkg18-hero-state")
	{
		if (!source)
			return;
		auto hasNamed = [&](const char* upgName) -> Int
		{
			if (!TheUpgradeCenter || !upgName || !*upgName)
				return 0;
			const UpgradeTemplate* upg = TheUpgradeCenter->findUpgrade(upgName);
			return (upg && source->hasUpgrade(upg)) ? 1 : 0;
		};
		const ExperienceTracker* tracker = source->getExperienceTracker();
		std::fprintf(stderr,
			"TEST pkg18 hero frame=%u object=%u hasGameMode=%d hasDwarf=%d hasSub0=%d hasSub1=%d hasCheck=%d checkName=%s rank=%d xp=%d\n",
			frame,
			static_cast<unsigned>(source->getID()),
			hasNamed(CreateAHeroSystem::getGameModeUpgradeName().str()),
			hasNamed("Upgrade_CreateAHero_ClassDwarf"),
			hasNamed("Upgrade_CreateAHero_SubClass_0"),
			hasNamed("Upgrade_CreateAHero_SubClass_1"),
			hasNamed(argument),
			(argument && *argument) ? argument : "-",
			tracker ? tracker->getRank() : 0,
			tracker ? static_cast<Int>(tracker->getRetailExperience()) : 0);
		return;
	}

	if (action == "pkg18-award-xp")
	{
		if (!source || !argument || !*argument)
			return;
		if (ExperienceTracker* tracker = source->getExperienceTracker())
			tracker->addExperiencePoints(std::atoi(argument));
		return;
	}
}
#endif
