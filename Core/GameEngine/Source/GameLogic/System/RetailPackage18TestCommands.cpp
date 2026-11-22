#include "PreRTS.h"

#if !RTS_GENERALS
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

#include "Common/Player.h"
#include "GameLogic/RetailTestCommands.h"
#include "GameNetwork/LANAPI.h"
#include "Common/PlayerTemplate.h"
#include "Common/Upgrade.h"
#include "GameClient/ControlBar.h"
#include "GameLogic/CreateAHeroRules.h"
#include "GameLogic/CreateAHeroSystem.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/Object.h"
#include "GameNetwork/GameInfo.h"

bool RetailTestCommands::configurePackage18Match(GameInfo& game)
{
	const char* selection = std::getenv("GENERALSX_TEST_PKG18_SELECTION");
	if (!selection || strnlen(selection, CreateAHeroRules::MAX_ENCODED_LENGTH + 1) > CreateAHeroRules::MAX_ENCODED_LENGTH)
		return false;
	const AsciiString normal = GameInfoToAsciiString(&game, 0, FALSE);
	AsciiString setup = normal;
	setup.concat("CH0=");
	setup.concat(selection);
	setup.concat(';');
	if (!ParseAsciiStringToGameInfo(&game, setup))
		return false;
	const GameSlot* slot = game.getConstSlot(0);
	if (!slot || slot->getCreateAHeroConfig().isEmpty())
		return false;
	std::fprintf(stderr, "TEST pkg18 setup accepted=1 compactLen=%d\n", slot->getCreateAHeroConfig().getLength());
	return true;
}

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

	if (action == "pkg18-setup-budget")
	{
		const GameSlot* selected = TheGameInfo ? TheGameInfo->getConstSlot(0) : nullptr;
		if (!selected || selected->getCreateAHeroConfig().isEmpty())
			return;
		CreateAHeroRules::SelectionData profile;
		if (!CreateAHeroSystem::decodeSelectionCompact(selected->getCreateAHeroConfig(), profile))
			return;
		profile.heroName.assign(CreateAHeroRules::MAX_HERO_NAME_LENGTH, 'D');
		for (const CommandButton* candidate = TheRetailCommandCatalog->getCommandButtons(); candidate;
			candidate = candidate->getNext())
		{
			if (profile.powerButtons.size() == CreateAHeroRules::MAX_POWER_COUNT)
				break;
			if (std::find(profile.powerButtons.begin(), profile.powerButtons.end(), candidate->getName().str()) == profile.powerButtons.end() &&
				CreateAHeroSystem::isPowerButtonLegalForSelection(candidate, profile, profile.powerButtons.size() + 1, profile.powerButtons))
				profile.powerButtons.emplace_back(candidate->getName().str());
		}
		const AsciiString encoded = CreateAHeroSystem::encodeSelectionCompact(profile);
		SkirmishGameInfo worst;
		worst.init();
		worst.setMap(TheGameInfo->getMap());
		worst.setMapCRC(0xFFFFFFFFu);
		worst.setMapSize(2147483647);
		worst.setSeed(-2147483647 - 1);
		for (Int i = 0; i < MAX_SLOTS; ++i)
		{
			GameSlot slot;
			slot.setState(SLOT_PLAYER, UnicodeString(L"TwelveCharsX"), 0xFFFFFF00u + i);
			slot.setPort(65535);
			slot.setPlayerTemplate(selected->getPlayerTemplate());
			slot.setStartPos(i);
			slot.setCreateAHeroConfig(CreateAHeroSystem::validateSetupSelection(encoded.str(), slot));
			worst.setSlot(i, slot);
		}
		const AsciiString fullSetup = GameInfoToAsciiString(&worst);
		const AsciiString retail = GameInfoToAsciiString(&worst, kRetailLanSetupBudget);
		SkirmishGameInfo received;
		received.init();
		Int accepted = 0;
		if (ParseAsciiStringToGameInfo(&received, retail))
			for (Int i = 0; i < MAX_SLOTS; ++i)
				accepted += received.getConstSlot(i)->getCreateAHeroConfig().isNotEmpty();
		const AsciiString normal = GameInfoToAsciiString(&worst, 0, FALSE);
		const std::string good = std::string("CH0=") + encoded.str() + ";";
		const std::string invalid[] = { "CH0=;", "CH0=invalid;", good + good,
			std::string("CH0=") + selected->getCreateAHeroConfig().str() + ";CH1=invalid;",
			std::string("CH0=") + std::string(CreateAHeroRules::MAX_ENCODED_LENGTH + 1, 'x') + ";",
			"CH0=UnknownClass|UnknownSubclass|Bad||||0,0,0,0,0,0,0,0,0;" };
		Int rejected = 0;
		for (size_t i = 0; i < std::size(invalid); ++i)
		{
			AsciiString wire = normal;
			wire.concat(invalid[i].c_str());
			const Int rejectedSlot = i == 3 ? 1 : 0;
			if (ParseAsciiStringToGameInfo(&received, wire) && received.getConstSlot(rejectedSlot)->getCreateAHeroConfig().isEmpty() &&
				(rejectedSlot == 0 || received.getConstSlot(0)->getCreateAHeroConfig() == selected->getCreateAHeroConfig()))
				++rejected;
		}
		const AsciiString oversized(std::string(kRetailLanSetupBudget + 1, 'x').c_str());
		std::fprintf(stderr, "TEST pkg18 budget normal=%d full=%d retail=%d budget=%d legacyBudget=%d accepted=%d rejected=%d oversize=%d\n",
			normal.getLength(), fullSetup.getLength(), retail.getLength(), kRetailLanSetupBudget, m_lanMaxOptionsLength,
			accepted, rejected, !ParseAsciiStringToGameInfo(&received, oversized));
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
		const CommandSet* commands = TheRetailCommandCatalog->findCommandSet(source->getCommandSetString());
		const CommandButton* power = commands ? commands->getCommandButton(1) : nullptr;
		const Color* colors = source->getCreateAHeroColors();
		AsciiString displayName;
		displayName.translate(source->getDisplayName());
		std::fprintf(stderr,
			"TEST pkg18 hero frame=%u object=%u scriptName=%s name=%s colors=%08X,%08X,%08X hasGameMode=%d hasDwarf=%d hasSub0=%d hasSub1=%d hasCheck=%d checkName=%s firstPower=%s rank=%d xp=%d\n",
			frame,
			static_cast<unsigned>(source->getID()),
			!source->getName().isEmpty() ? source->getName().str() : "-",
			displayName.str(),
			colors ? static_cast<unsigned>(colors[0]) : 0u,
			colors ? static_cast<unsigned>(colors[1]) : 0u,
			colors ? static_cast<unsigned>(colors[2]) : 0u,
			hasNamed(CreateAHeroSystem::getGameModeUpgradeName().str()),
			hasNamed("Upgrade_CreateAHero_ClassDwarf"),
			hasNamed("Upgrade_CreateAHero_SubClass_0"),
			hasNamed("Upgrade_CreateAHero_SubClass_1"),
			hasNamed(argument),
			(argument && *argument) ? argument : "-",
			power ? power->getName().str() : "-",
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
