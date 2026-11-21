#include "PreRTS.h"
#include "GameLogic/RetailSkirmishProbe.h"

#include "Common/BuildAssistant.h"
#include "Common/FramePacer.h"
#include "Common/GameEngine.h"
#include "Common/GlobalData.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/PlayerTemplate.h"
#include "Common/RetailFileLayers.h"
#include "Common/Science.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "GameClient/ControlBar.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/AttributeModifierHelper.h"
#include "GameLogic/Module/ProductionUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Scripts.h"
#include "GameLogic/SidesList.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/VictoryConditions.h"
#include "GameNetwork/GameInfo.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <utility>

namespace RetailSkirmishProbe
{
namespace
{
UnsignedInt frameLimit = 0;
const char* fixture = nullptr;
Bool ensnareObserved = FALSE;

Int findFaction(const char* name)
{
	for (Int i = 0; i < ThePlayerTemplateStore->getPlayerTemplateCount(); ++i)
	{
		const PlayerTemplate* faction = ThePlayerTemplateStore->getNthPlayerTemplate(i);
		if (faction->isPlayableSide() && faction->getName() == name)
			return i;
	}
	return -1;
}

[[noreturn]] void invalidConfig()
{
	std::fputs("SKIRMISH_ERROR\tExpected playable faction names, easy/medium/hard, and a positive frame limit\n", stderr);
	std::exit(2);
}

Bool isCompetitor(const Player* player)
{
	return player && player->isPlayableSide() && player->getPlayerType() == PLAYER_COMPUTER;
}

void runPorterFixture(Player& player)
{
	SidesInfo* side = TheSidesList->findSideInfo(player.getPlayerName());
	ScriptList* scripts = side ? side->getScriptList() : nullptr;
	const ThingTemplate* porter = TheThingFactory->findTemplate(player.getPlayerTemplate()->getStartingUnit(0), FALSE);
	if (!scripts || !porter || !porter->isKindOf(KINDOF_DOZER))
		return;
	for (ScriptGroup* group = scripts->getScriptGroup(); group; group = group->getNext())
	{
		if (!group->getName().startsWith("Castle - Porter Delete"))
			continue;
		for (Script* script = group->getScript(); script; script = script->getNext())
		{
			if (!script->getName().startsWith("Castle Porter - Disable buttons") || !script->getAction())
				continue;
			for (Object* keep = TheGameLogic->getFirstObject(); keep; keep = keep->getNextObject())
			{
				if (keep->getControllingPlayer() != &player || !keep->isKindOf(KINDOF_COMMANDCENTER) ||
					!keep->getProductionUpdateInterface())
					continue;
				const Bool before = player.canBuild(porter);
				const CanMakeType makeBefore = TheBuildAssistant->canMakeUnit(keep, porter);
				TheScriptEngine->friend_executeAction(script->getAction(), player.getDefaultTeam());
				std::fprintf(stderr, "SKIRMISH_PORTER_FIXTURE\t%d\t%s\t%d\t%d\t%d\t%d\n", player.getPlayerIndex(),
					porter->getName().str(), before, player.canBuild(porter), makeBefore,
					TheBuildAssistant->canMakeUnit(keep, porter));
				return;
			}
		}
	}
}

void buyEnsnare(Player& player)
{
	if (player.getPlayerTemplate()->getName() != "FactionRohan")
		return;
	const auto buy = [&player](const char* name) -> Bool
	{
		const ScienceType science = TheScienceStore->getScienceFromInternalName(name);
		if (player.hasScience(science))
			return TRUE;
		player.addSciencePurchasePoints(player.getSciencePurchaseCost(science));
		return player.attemptToPurchaseScience(science);
	};
	const Bool purchased = buy("SCIENCE_RohanRebuild") && buy("SCIENCE_CirionOath");
	std::fprintf(stderr, "SKIRMISH_ENSNARE_FIXTURE\t%d\t%d\n", player.getPlayerIndex(), purchased);
	if (!purchased)
		return;
	const ThingTemplate* hero = TheThingFactory->findTemplate("MordorGorbag", FALSE);
	Player* opponent = ThePlayerList->findPlayerWithName("Player_2");
	if (!hero || !isCompetitor(opponent) || opponent == &player)
		return;
	for (Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
	{
		if (object->getControllingPlayer() != &player || !object->isKindOf(KINDOF_COMMANDCENTER) ||
			!object->getProductionUpdateInterface())
			continue;
		Coord3D pos = *object->getPosition();
		pos.x += 115.0f;
		pos.z = TheTerrainLogic->getGroundHeight(pos.x, pos.y);
		Object* target = TheThingFactory->newObject(hero, opponent->getDefaultTeam());
		target->setPosition(&pos);
		std::fprintf(stderr, "SKIRMISH_ENSNARE_TARGET\t%u\t%s\t%a\t%a\n",
			static_cast<unsigned>(target->getID()), hero->getName().str(), pos.x, pos.y);
		return;
	}
}

void observeEnsnare(UnsignedInt frame)
{
	if (ensnareObserved)
		return;
	const Int list = AttributeModifiers::findList("EnsareHaplessCripple");
	if (list < 0)
		return;
	for (const Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
	{
		const auto* helper = object->getAttributeModifierHelper();
		if (!helper)
			continue;
		for (const auto& active : helper->getStack().active())
		{
			if (active.list != list)
				continue;
			std::fprintf(stderr, "SKIRMISH_ENSNARE_EFFECT\t%u\t%u\t%s\t%u\t%a\n", frame,
				static_cast<unsigned>(object->getID()), object->getTemplate()->getName().str(),
				active.endFrame, AttributeModifiers::speedFactor(object));
			ensnareObserved = TRUE;
			return;
		}
	}
}

void runFixture(UnsignedInt frame)
{
	if (!fixture)
		return;
	if (frame == 2)
	{
		for (Int i = 0; i < ThePlayerList->getPlayerCount(); ++i)
		{
			Player* player = ThePlayerList->getNthPlayer(i);
			if (!isCompetitor(player))
				continue;
			if (std::strcmp(fixture, "porter") == 0)
				runPorterFixture(*player);
			else
				buyEnsnare(*player);
		}
	}
	if (frame >= 2 && std::strcmp(fixture, "ensnare") == 0)
		observeEnsnare(frame);
}

void logProductionOptions(Object& object, const Player& player, UnsignedInt frame)
{
	const auto* production = object.getProductionUpdateInterface();
	const CommandSet* set = production && TheRetailCommandCatalog ?
		TheRetailCommandCatalog->findCommandSet(object.getCommandSetString()) : nullptr;
	if (!set)
		return;
	for (Int index = 0; index < CommandButtonRules::RETAIL_COMMANDS_PER_SET; ++index)
	{
		const CommandButton* button = set->getCommandButton(index);
		const ThingTemplate* unit = button ? button->getThingTemplate() : nullptr;
		if (!unit || button->getCommandType() != GUI_COMMAND_UNIT_BUILD)
			continue;
		std::fprintf(stderr, "SKIRMISH_PRODUCTION\t%u\t%d\t%s\t%s\t%d\t%u\t%d\t%d\n", frame,
			player.getPlayerIndex(), object.getTemplate()->getName().str(), unit->getName().str(),
			TheBuildAssistant->canMakeUnit(&object, unit), production->getProductionCount(),
			object.isDisabled(), unit->calcCostToBuild(&player));
	}
}

void snapshot(const Player& player, UnsignedInt frame)
{
	std::map<std::pair<std::string, std::string>, Int> counts;
	std::map<std::string, Int> upgrades;
	for (const UpgradeTemplate* upgrade = TheUpgradeCenter->firstUpgradeTemplate();
		upgrade; upgrade = upgrade->friend_getNext())
	{
		if (player.getCompletedUpgradeMask().anyIntersectionWith(upgrade->getUpgradeMask()))
			upgrades[upgrade->getUpgradeName().str()]++;
	}

	const Int playerIndex = player.getPlayerIndex();
	for (Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
	{
		if (object->getControllingPlayer() != &player || object->isEffectivelyDead() || object->isDestroyed() ||
			object->isKindOf(KINDOF_INERT) || object->isKindOf(KINDOF_BASE_FOUNDATION))
			continue;

		const char* category = nullptr;
		if (object->isKindOf(KINDOF_STRUCTURE))
			category = object->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION) ? "site" : "building";
		else if (object->isKindOf(KINDOF_HERO))
			category = "hero";
		else if (!object->getContainedBy() && (object->isKindOf(KINDOF_HORDE) || object->isKindOf(KINDOF_INFANTRY) ||
			object->isKindOf(KINDOF_VEHICLE) || object->isKindOf(KINDOF_MONSTER) || object->isKindOf(KINDOF_DOZER)))
			category = "unit";
		if (!category)
			continue;

		const char* name = object->getTemplate()->getName().str();
		counts[{category, name}]++;
		if (object->isKindOf(KINDOF_STRUCTURE) && object->getObjectCompletedUpgradeMask().any())
		{
			for (const UpgradeTemplate* upgrade = TheUpgradeCenter->firstUpgradeTemplate();
				upgrade; upgrade = upgrade->friend_getNext())
			{
				if (object->getObjectCompletedUpgradeMask().anyIntersectionWith(upgrade->getUpgradeMask()))
					upgrades[upgrade->getUpgradeName().str()]++;
			}
		}
		if (object->isKindOf(KINDOF_FS_FACTORY))
			logProductionOptions(*object, player, frame);
		const AIUpdateInterface* ai = object->getAIUpdateInterface();
		if (ai && !object->isKindOf(KINDOF_STRUCTURE))
		{
			const Coord3D& pos = *object->getPosition();
			std::fprintf(stderr, "SKIRMISH_POSITION\t%u\t%d\t%u\t%s\t%a\t%a\t%d\t%d\n", frame,
				playerIndex, static_cast<unsigned>(object->getID()), name, pos.x, pos.y, ai->isIdle(), ai->isMoving());
		}
	}
	std::fprintf(stderr, "SKIRMISH_PLAYER\t%u\t%d\t%s\t%u\t%d\n", frame, playerIndex,
		player.getPlayerTemplate()->getName().str(), player.getMoney()->countMoney(), player.getSciencePurchasePoints());
	for (const auto& [key, count] : counts)
		std::fprintf(stderr, "SKIRMISH_OBJECT\t%u\t%d\t%s\t%s\t%d\n", frame, playerIndex,
			key.first.c_str(), key.second.c_str(), count);
	for (const auto& [name, count] : upgrades)
		std::fprintf(stderr, "SKIRMISH_UPGRADE\t%u\t%d\t%s\t%d\n", frame, playerIndex, name.c_str(), count);
}
}

Bool configure(SkirmishGameInfo& game)
{
	const char* frames = std::getenv("GENERALSX_SKIRMISH_PROBE_FRAMES");
	if (!frames || !TheGlobalData->m_headless || !RetailFileLayers::enabled())
		return FALSE;

	const char* factionA = std::getenv("GENERALSX_SKIRMISH_PROBE_A");
	const char* factionB = std::getenv("GENERALSX_SKIRMISH_PROBE_B");
	const char* difficulty = std::getenv("GENERALSX_SKIRMISH_PROBE_DIFFICULTY");
	char* end = nullptr;
	const unsigned long limit = std::strtoul(frames, &end, 10);
	if (!factionA || !factionB || !difficulty || !*frames || *end || limit == 0 || limit > 54000)
		invalidConfig();

	SlotState state;
	if (std::strcmp(difficulty, "easy") == 0)
		state = SLOT_EASY_AI;
	else if (std::strcmp(difficulty, "medium") == 0)
		state = SLOT_MED_AI;
	else if (std::strcmp(difficulty, "hard") == 0)
		state = SLOT_BRUTAL_AI;
	else
		invalidConfig();

	const Int factions[] = {findFaction(factionA), findFaction(factionB)};
	if (factions[0] < 0 || factions[1] < 0)
		invalidConfig();
	for (Int i = 0; i < 2; ++i)
	{
		GameSlot ai;
		ai.setState(state);
		ai.setStartPos(i);
		ai.setColor(i);
		ai.setPlayerTemplate(factions[i]);
		game.setSlot(i, ai);
	}
	GameSlot observer;
	observer.setState(SLOT_PLAYER, UnicodeString(L"Observer"));
	observer.setColor(2);
	observer.setPlayerTemplate(PLAYERTEMPLATE_OBSERVER);
	game.setSlot(2, observer);
	game.setLocalIP(observer.getIP());

	frameLimit = static_cast<UnsignedInt>(limit);
	fixture = std::getenv("GENERALSX_SKIRMISH_PROBE_FIXTURE");
	if (fixture && std::strcmp(fixture, "porter") != 0 && std::strcmp(fixture, "ensnare") != 0)
		invalidConfig();
	TheFramePacer->enableLogicTimeScale(FALSE);
	TheFramePacer->enableFramesPerSecondLimit(FALSE);
	std::fprintf(stderr, "SKIRMISH_SETUP\t%s\t%s\t%s\t%u\n", factionA, factionB, difficulty, frameLimit);
	std::fflush(stderr);
	return TRUE;
}

Bool enabled()
{
	return frameLimit != 0;
}

void crc(UnsignedInt frame, UnsignedInt value)
{
	if (enabled())
		std::fprintf(stderr, "SKIRMISH_CRC\t%u\t%08x\n", frame, value);
}

void update(UnsignedInt frame)
{
	if (!enabled())
		return;
	runFixture(frame);
	const Bool ended = TheVictoryConditions->getEndFrame() != 0;
	if (!ended && frame < frameLimit && frame % 1800 != 0)
		return;

	Int winner = -1;
	for (Int i = 0; i < ThePlayerList->getPlayerCount(); ++i)
	{
		Player* player = ThePlayerList->getNthPlayer(i);
		if (!isCompetitor(player))
			continue;
		snapshot(*player, frame);
		if (ended && TheVictoryConditions->hasAchievedVictory(player))
			winner = i;
	}
	if (ended || frame >= frameLimit)
	{
		std::fprintf(stderr, "SKIRMISH_END\t%u\t%d\t%s\n", frame, winner, ended ? "victory" : "frame-cap");
		TheGameEngine->setQuitting(TRUE);
	}
	std::fflush(stderr);
}

void death(const Object& object, ObjectID source)
{
	if (!enabled())
		return;
	const Player* player = object.getControllingPlayer();
	if (isCompetitor(player))
		std::fprintf(stderr, "SKIRMISH_DEATH\t%u\t%d\t%u\t%s\t%u\n", TheGameLogic->getFrame(),
			player->getPlayerIndex(), static_cast<unsigned>(object.getID()), object.getTemplate()->getName().str(),
			static_cast<unsigned>(source));
}

void cast(Int player, const AsciiString& power, ObjectID source)
{
	if (enabled() && isCompetitor(ThePlayerList->getNthPlayer(player)))
		std::fprintf(stderr, "SKIRMISH_CAST\t%u\t%d\t%s\t%u\n", TheGameLogic->getFrame(),
			player, power.str(), static_cast<unsigned>(source));
}

void purchase(const Player& player, const AsciiString& science)
{
	if (enabled() && isCompetitor(&player))
		std::fprintf(stderr, "SKIRMISH_PURCHASE\t%u\t%d\t%s\n", TheGameLogic->getFrame(),
			player.getPlayerIndex(), science.str());
}

void buildPermission(const Player* player, const AsciiString& name, Bool found, Bool allowed)
{
	if (enabled())
		std::fprintf(stderr, "SKIRMISH_PERMISSION\t%u\t%d\t%s\t%d\t%d\n", TheGameLogic->getFrame(),
			player ? player->getPlayerIndex() : -1, name.str(), found, allowed);
}

void rebuild(const Object& hole, const Object& replacement)
{
	if (enabled())
		std::fprintf(stderr, "SKIRMISH_REBUILD\t%u\t%u\t%s\t%u\t%s\n", TheGameLogic->getFrame(),
			static_cast<unsigned>(hole.getID()), hole.getTemplate()->getName().str(),
			static_cast<unsigned>(replacement.getID()), replacement.getTemplate()->getName().str());
}
}
