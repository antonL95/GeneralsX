#include "PreRTS.h"

#if !RTS_GENERALS
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

#include "Common/FileSystem.h"
#include "Common/Player.h"
#include "Common/Recorder.h"
#include "GameLogic/RetailTestCommands.h"
#include "GameNetwork/LANAPI.h"
#include "Common/PlayerTemplate.h"
#include "Common/Upgrade.h"
#include "GameClient/ControlBar.h"
#include "GameLogic/CreateAHeroRules.h"
#include "GameLogic/CreateAHeroSystem.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/GameMode.h"
#include "GameLogic/Object.h"
#include "GameNetwork/GameInfo.h"
#include "GameNetwork/LANAPICallbacks.h"
#if defined(SAGE_USE_NGMP)
#include "GameNetwork/GeneralsOnline/NGMPGame.h"
#include "GameNetwork/GeneralsOnline/OnlineServices_LobbyInterface.h"
#include "GameNetwork/GeneralsOnline/OnlineServices_Manager.h"
#include "GameNetwork/GeneralsOnline/ngmp_curl_utils.h"
#endif

static Bool checkLanStartRoster(SkirmishGameInfo& source, const AsciiString& setup)
{
	if (TheLAN)
		return FALSE;
	class Guest : public LANAPI
	{
	public:
		UnsignedInt GetLocalIP() override { return 0xFFFFFF01u; }
	} guest;
	TheLAN = &guest;
	LANGameInfo lobby;
	lobby.enterGame();
	const GameSlot oldGuest = *source.getConstSlot(1);
	const GameSlot oldHost = *source.getConstSlot(0);
	GameSlot closed;
	closed.setState(SLOT_CLOSED);
	source.setSlot(1, closed);
	const AsciiString missingGuest = GameInfoToAsciiString(&source);
	source.setSlot(1, oldGuest);
	source.setSlot(0, closed);
	const AsciiString missingHost = GameInfoToAsciiString(&source);
	source.setSlot(0, oldHost);
	const Bool rejected = ParseAsciiStringToGameInfo(&lobby, setup) &&
		!ParseGameOptionsString(&lobby, missingGuest, oldHost.getIP()) &&
		!ParseGameOptionsString(&lobby, missingHost, oldHost.getIP()) &&
		lobby.getLocalSlotNum() == 1 && lobby.getIP(0) == oldHost.getIP() &&
		lobby.getConstSlot(1)->getCreateAHeroConfig() == oldGuest.getCreateAHeroConfig();
	TheLAN = nullptr;
	return rejected;
}

#if defined(SAGE_USE_NGMP)
bool checkPackage18LobbyDetails(const AsciiString& selection)
{
	NGMP_OnlineServices_LobbyInterface lobby;
	NGMP_OnlineServicesManager manager;
	manager.m_pLobbyInterface = &lobby;
	manager.m_userId = 1;
	const std::string hero = std::string(R"JSON(,"CreateAHeroConfig":")JSON") + selection.str() + '"';
	const std::string host = R"JSON({"Owner":1,"MapName":"Before","Members":[{"UserID":1,"SlotIndex":0,"SlotState":5})JSON";
	const std::string oldGuest = R"JSON(,{"UserID":2,"SlotIndex":1,"SlotState":5)JSON";
	const std::string movedGuest = R"JSON(,{"UserID":3,"SlotIndex":2,"SlotState":5}])JSON";
	if (!manager.applyLobbyDetails(31, host + oldGuest + hero + "}]}"))
		return false;
	const auto& published = lobby.GetCurrentLobby();
	if (published.owner != 1 || published.map_name != "Before" ||
		lobby.GetRoomMemberFromIndex(1).createAHeroConfig != selection)
		return false;
	const std::string duplicate = host + oldGuest + hero + hero + "}]}";
	const std::string badCount = R"JSON({"Owner":2,"MapName":"After","NumCurrentPlayers":"wrong","Members":[{"UserID":1,"SlotIndex":0,"SlotState":5}]})JSON";
	if (manager.applyLobbyDetails(31, duplicate) || manager.applyLobbyDetails(31, badCount) ||
		manager.applyLobbyDetails(31, std::string(64 * 1024 + 1, ' ')) ||
		manager.m_hostUserId != 1 || published.owner != 1 || published.map_name != "Before" ||
		lobby.GetRoomMemberFromIndex(1).createAHeroConfig != selection)
		return false;
	auto& globalManager = NGMP_OnlineServicesManager::getInstance();
	auto* original = globalManager.m_pLobbyInterface;
	globalManager.m_pLobbyInterface = &lobby;
	NGMPGame game;
	const bool updated = manager.applyLobbyDetails(31, host + movedGuest + "}");
	game.UpdateSlotsFromCurrentLobby();
	globalManager.m_pLobbyInterface = original;
	NGMP::Internal::CurlResponse response{{}, 3};
	char chunk[] = "1234";
	return updated && !game.getConstSlot(1)->isOccupied() &&
		game.getConstSlot(1)->getCreateAHeroConfig().isEmpty() && game.getConstSlot(2)->isHuman() &&
		game.getGameSpySlot(1)->m_userID == -1 &&
		NGMP::Internal::WriteCallback(chunk, 1, 3, &response) == 3 &&
		NGMP::Internal::WriteCallback(chunk, 1, 1, &response) == 0 && response.text == "123";
}
#endif

static Bool checkReplaySetup(const AsciiString& setup)
{
	class SetupRecorder : public RecorderClass
	{
	public:
		Bool check(const AsciiString& options)
		{
			AsciiString path = getReplayDir();
			TheFileSystem->createDirectory(path);
			path.concat("pkg18-setup.rep");
			m_file = TheFileSystem->openFile(path.str(), File::WRITE | File::BINARY);
			if (!m_file)
				return FALSE;
			const AsciiString oversized(std::string(kRetailLanSetupBudget + 1, 'x').c_str());
			const Bool written = !writeReplayHeader(oversized, 7, DIFFICULTY_NORMAL, GAME_LAN, 0, 0) &&
				m_file->size() == 0 && writeReplayHeader(options, 7, DIFFICULTY_NORMAL, GAME_LAN, 0, 0);
			reset();
			ReplayHeader header;
			if (!written || !readReplayHeader(header, path, TRUE))
				return FALSE;
			Bool allHeroes = TRUE;
			for (Int i = 0; i < MAX_SLOTS; ++i)
				allHeroes &= getGameInfo()->getConstSlot(i)->getCreateAHeroConfig().isNotEmpty();
			reset();
			return allHeroes && header.gameOptions == options && header.localPlayerIndex == 7;
		}
	} recorder;
	return recorder.check(setup);
}

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
		if (!CreateAHeroSystem::decodeSelectionCompact(selected->getCreateAHeroConfig().str(), profile))
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
		const char* fixtureSelection = std::getenv("GENERALSX_TEST_PKG18_SELECTION");
		if (!fixtureSelection)
			return;
		const std::string text = fixtureSelection;
		const size_t nameStart = text.find('|', text.find('|') + 1) + 1;
		std::string leadingZero = text;
		leadingZero.insert(leadingZero.rfind('|') + 1, 1, '0');
		std::string emptyName = text;
		emptyName.erase(nameStart, text.find('|', nameStart) - nameStart);
		const std::string compact = encoded.str();
		const std::string invalid[] = { "CH0=;", "CH0=invalid;", good + good,
			std::string("CH0=") + selected->getCreateAHeroConfig().str() + ";CH1=invalid;",
			std::string("CH0=") + std::string(CreateAHeroRules::MAX_ENCODED_LENGTH + 1, 'x') + ";",
			"CH0=UnknownClass|UnknownSubclass|Bad||||0,0,0,0,0,0,0,0,0;",
			"CH0=" + leadingZero + ";", "CH0=" + emptyName + ";",
			"CH0=" + compact.substr(0, compact.find('.')) + ";",
			"CH0=~" + std::string(66, 'A') + ".Name;" };
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
		const Bool lanRoster = checkLanStartRoster(worst, retail);
		Bool ngmpLobby = TRUE;
#if defined(SAGE_USE_NGMP)
		ngmpLobby = checkPackage18LobbyDetails(selected->getCreateAHeroConfig());
#endif
		std::fprintf(stderr, "TEST pkg18 budget normal=%d full=%d retail=%d budget=%d legacyBudget=%d accepted=%d rejected=%d oversize=%d replay=%d lanRoster=%d ngmpLobby=%d\n",
			normal.getLength(), fullSetup.getLength(), retail.getLength(), kRetailLanSetupBudget, m_lanMaxOptionsLength,
			accepted, rejected, !ParseAsciiStringToGameInfo(&received, oversized), checkReplaySetup(retail), lanRoster, ngmpLobby);
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
