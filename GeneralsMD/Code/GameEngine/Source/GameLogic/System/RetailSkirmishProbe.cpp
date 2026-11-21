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
#include "GameLogic/Damage.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/AttributeModifierHelper.h"
#include "GameLogic/Module/HordeAIUpdate.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Module/ProductionUpdate.h"
#include "GameLogic/Module/RetailGate.h"
#include "GameLogic/Object.h"
#include "GameLogic/Pathfinder/Path.h"
#include "GameLogic/Pathfinder/PathNode.h"
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
Bool active = FALSE;

namespace
{
UnsignedInt frameLimit = 0;
const char* fixture = nullptr;
Bool ensnareObserved = FALSE;
ObjectID plannerFixtureHorde = INVALID_ID;
ObjectID plannerFixtureTarget = INVALID_ID;
ObjectID plannerFixtureGate = INVALID_ID;
ObjectID plannerFixtureGuard = INVALID_ID;

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

void runPlannerFixture(UnsignedInt frame, const char* unitName, const char* targetName, Bool capture)
{
	if (frame != 2 && (!capture || (frame != 60 && frame != 62)))
		return;
	Player* attacker = ThePlayerList->findPlayerWithName("Player_1");
	Player* defender = ThePlayerList->findPlayerWithName("Player_2");
	if (!isCompetitor(attacker) || !isCompetitor(defender))
		return;
	if (frame == 2)
	{
		const ThingTemplate* squad = TheThingFactory->findTemplate(unitName, FALSE);
		const ThingTemplate* farm = TheThingFactory->findTemplate(targetName, FALSE);
		const Waypoint* start = TheTerrainLogic->getWaypointByName("Player_1_Start");
		if (!squad || !farm || !start)
			return;
		Coord3D position = *start->getLocation();
		position.x += capture ? 260.0f : 420.0f;
		position.z = TheTerrainLogic->getGroundHeight(position.x, position.y);
		Object* target = TheThingFactory->newObject(farm, defender->getDefaultTeam());
		target->setPosition(&position);
		plannerFixtureTarget = target->getID();
		position.y += std::strcmp(unitName, "RohanBatteringRam") == 0 ? 200.0f : 150.0f;
		position.z = TheTerrainLogic->getGroundHeight(position.x, position.y);
		Object* squadObject = TheThingFactory->newObject(squad, attacker->getDefaultTeam());
		squadObject->setPosition(&position);
		plannerFixtureHorde = squadObject->getID();
		auto* horde = static_cast<HordeAIUpdate*>(squadObject->findUpdateModule(NAMEKEY("HordeAIUpdate")));
		const Bool accepted = horde ? horde->issuePlannerOrder(target, *target->getPosition())
			: squadObject->getAI()->issueRetailPlannerAttack(target);
		std::fprintf(stderr, "SKIRMISH_PLANNER_FIXTURE\tissue\t%u\t%u\t%d\n",
			static_cast<unsigned>(plannerFixtureHorde), static_cast<unsigned>(plannerFixtureTarget), accepted);
	}
	else if (frame == 60 || frame == 62)
	{
		Object* target = TheGameLogic->findObjectByID(plannerFixtureTarget);
		Object* squad = TheGameLogic->findObjectByID(plannerFixtureHorde);
		if (!target || !squad)
			return;
		const auto* horde = static_cast<const HordeAIUpdate*>(squad->findUpdateModule(NAMEKEY("HordeAIUpdate")));
		const auto* contain = HordeContain::get(squad);
		if (!horde || !contain)
			return;
		Int attackers = 0;
		for (Object* member : *contain->getContainedItemsList())
			if (member->getAI() && member->getAI()->getGoalObject() == target)
				++attackers;
		std::fprintf(stderr, "SKIRMISH_PLANNER_FIXTURE\t%s\t%u\t%d\t%d\t%d\n",
			frame == 60 ? "before" : "after", frame, target->getControllingPlayer()->getPlayerIndex(),
			horde->hasAttackOrder(), attackers);
		if (frame == 60)
			target->setTeam(attacker->getDefaultTeam());
	}
}

void runPlannerGateFixture(UnsignedInt frame)
{
	if (frame != 2 && frame != 4 && frame != 170 && frame != 360 && frame != 450 && frame != 900)
		return;
	Player* attacker = ThePlayerList->findPlayerWithName("Player_1");
	Player* defender = ThePlayerList->findPlayerWithName("Player_2");
	if (!isCompetitor(attacker) || !isCompetitor(defender))
		return;
	if (frame == 2 || frame == 4)
	{
		const Waypoint* start = TheTerrainLogic->getWaypointByName("Player_1_Start");
		const ThingTemplate* wall = TheThingFactory->findTemplate("MenWallSegmentSmall", FALSE);
		const ThingTemplate* gate = TheThingFactory->findTemplate("MenWallGateSmall", FALSE);
		const ThingTemplate* horde = TheThingFactory->findTemplate("DwarvenGuardianHorde", FALSE);
		const ThingTemplate* farm = TheThingFactory->findTemplate("HaradwaithBazaar_ForAI", FALSE);
		const ThingTemplate* spotter = TheThingFactory->findTemplate("DwarvenPorter", FALSE);
		if (!start || !wall || !gate || !horde || !farm || !spotter)
			return;
		attacker->setCanBuildBase(FALSE);
		Team* neutral = ThePlayerList->getNeutralPlayer()->getDefaultTeam();
		const Coord3D center = *start->getLocation() + Coord3D{-180.0f, 360.0f, 0.0f};
		const auto place = [&](const ThingTemplate* type, Team* team, Real x, Real y) {
			Coord3D position = center + Coord3D{x, y, 0.0f};
			position.z = TheTerrainLogic->getGroundHeight(position.x, position.y);
			Object* object = TheThingFactory->newObject(type, team);
			object->setPosition(&position);
			return object;
		};
		if (frame == 4)
		{
			for (Real offset : {-34.0f, 0.0f, 34.0f})
				plannerFixtureGuard = place(horde, attacker->getDefaultTeam(), 10.0f, offset)->getID();
			std::fprintf(stderr, "SKIRMISH_GATE_FIXTURE\tsetup\t%u\t%u\t%u\n",
				static_cast<UnsignedInt>(plannerFixtureGate), static_cast<UnsignedInt>(plannerFixtureTarget),
				static_cast<UnsignedInt>(plannerFixtureGuard));
			return;
		}
		const auto footprint = [](Object* object, Real halfX, Real halfY, Real angle) {
			auto* pathfinder = TheAI->pathfinder();
			pathfinder->removeObjectFromPathfindMap(object);
			object->setOrientation(angle);
			object->setGeometryInfo(GeometryInfo(GEOMETRY_BOX, FALSE, 45.0f, halfX, halfY));
			pathfinder->addObjectToPathfindMap(object);
		};
		footprint(place(wall, neutral, -100.0f, 0.0f), 12.0f, 112.0f, 0.0f);
		footprint(place(wall, neutral, 0.0f, -100.0f), 112.0f, 12.0f, 0.0f);
		footprint(place(wall, neutral, 0.0f, 100.0f), 112.0f, 12.0f, 0.0f);
		for (Real side : {-1.0f, 1.0f})
			footprint(place(wall, neutral, 100.0f, side * 74.0f), 12.0f, 38.0f, 0.0f);
		Object* gateObject = place(gate, neutral, 100.0f, 0.0f);
		footprint(gateObject, 40.0f, 12.0f, PI / 2.0f);
		plannerFixtureGate = gateObject->getID();
		if (auto* behavior = GateOpenAndCloseBehavior::find(gateObject))
			behavior->request(FALSE);
		Object* target = place(farm, defender->getDefaultTeam(), 193.0f, 0.0f);
		plannerFixtureTarget = target->getID();
		place(spotter, attacker->getDefaultTeam(), 215.0f, 50.0f);
		return;
	}
	Object* gate = TheGameLogic->findObjectByID(plannerFixtureGate);
	Object* unit = TheGameLogic->findObjectByID(plannerFixtureGuard);
	const auto* horde = unit ? static_cast<const HordeAIUpdate*>(unit->findUpdateModule(NAMEKEY("HordeAIUpdate"))) : nullptr;
	if (!gate || !horde)
		return;
	std::fprintf(stderr, "SKIRMISH_GATE_FIXTURE\t%u\t%d\t%u\t%u\t%d\t%a\t%a\n", frame,
		GateOpenAndCloseBehavior::canPass(gate, unit, FALSE), TheAI->pathfinder()->getRetailRouteRevision(),
		horde->blockedPlannerRevision(63), horde->hasAttackOrder(),
		unit->getPosition()->x, gate->getPosition()->x);
	if (frame == 360)
	{
		gate->setTeam(attacker->getDefaultTeam());
		if (auto* behavior = GateOpenAndCloseBehavior::find(gate))
			behavior->request(TRUE);
	}
}

void runFixture(UnsignedInt frame)
{
	if (!fixture)
		return;
	if (std::strcmp(fixture, "planner-gate") == 0)
	{
		runPlannerGateFixture(frame);
		return;
	}
	if (std::strcmp(fixture, "planner-capture") == 0)
	{
		runPlannerFixture(frame, "RohanArcherNewHorde", "MordorSlaughterHouse_ForAI", TRUE);
		return;
	}
	if (std::strcmp(fixture, "planner-siege") == 0)
	{
		runPlannerFixture(frame, "RohanBatteringRam", "MordorSlaughterHouse_ForAI", FALSE);
		return;
	}
	if (std::strcmp(fixture, "planner-harad") == 0)
	{
		runPlannerFixture(frame, "HaradTribalAxemenHorde_AI", "DwarvenMineShaft_ForAI", FALSE);
		return;
	}
	if (std::strcmp(fixture, "human-lair") == 0)
	{
		if (frame != 2)
			return;
		Player* human = ThePlayerList->getLocalPlayer();
		if (!human || human->getPlayerType() != PLAYER_HUMAN || !human->isPlayableSide())
			return;
		for (Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
		{
			if (object->getTemplate()->getName() != "MoriarGoblinLair")
				continue;
			object->setTeam(human->getDefaultTeam());
			std::fprintf(stderr, "SKIRMISH_HUMAN_LAIR\t%u\t%d\t%u\n", frame,
				human->getPlayerIndex(), static_cast<unsigned>(object->getID()));
			object->kill(DAMAGE_UNRESISTABLE, DEATH_NORMAL);
			break;
		}
		return;
	}
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
		AIUpdateInterface* ai = object->getAIUpdateInterface();
		if (ai && !object->isKindOf(KINDOF_STRUCTURE))
		{
			const Coord3D& pos = *object->getPosition();
			std::fprintf(stderr, "SKIRMISH_POSITION\t%u\t%d\t%u\t%s\t%a\t%a\t%d\t%d\n", frame,
				playerIndex, static_cast<unsigned>(object->getID()), name, pos.x, pos.y, ai->isIdle(), ai->isMoving());
			if (object->isKindOf(KINDOF_HORDE) && ai->getModuleNameKey() == NAMEKEY("HordeAIUpdate"))
			{
				const auto* horde = static_cast<const HordeAIUpdate*>(ai);
				const Coord3D& goal = *ai->getGoalPosition();
				Path* path = ai->getPath();
				const Coord3D* end = path && path->getLastNode() ? path->getLastNode()->getPosition() : nullptr;
				std::fprintf(stderr, "SKIRMISH_PATH\t%u\t%d\t%u\t%d\t%d\t%d\t%d\t%a\t%a\t%d\t%a\t%a\n",
					frame, playerIndex, static_cast<unsigned>(object->getID()), ai->getCurrentStateID(),
					ai->isWaitingForPath(), ai->isBlockedAndStuck(), horde->hasAttackOrder(), goal.x, goal.y,
					end != nullptr, end ? end->x : 0.0f, end ? end->y : 0.0f);
			}
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
	fixture = std::getenv("GENERALSX_SKIRMISH_PROBE_FIXTURE");
	const Bool humanLair = fixture && std::strcmp(fixture, "human-lair") == 0;
	if (fixture && !humanLair && std::strcmp(fixture, "porter") != 0 &&
		std::strcmp(fixture, "ensnare") != 0 && std::strcmp(fixture, "planner-capture") != 0 &&
		std::strcmp(fixture, "planner-siege") != 0 && std::strcmp(fixture, "planner-harad") != 0 &&
		std::strcmp(fixture, "planner-gate") != 0)
		invalidConfig();
	for (Int i = 0; i < 2; ++i)
	{
		GameSlot slot;
		if (humanLair && i == 0)
		{
			slot.setState(SLOT_PLAYER, UnicodeString(L"Player"));
			game.setLocalIP(slot.getIP());
		}
		else
			slot.setState(state);
		slot.setStartPos(i);
		slot.setColor(i);
		slot.setPlayerTemplate(factions[i]);
		game.setSlot(i, slot);
	}
	if (!humanLair)
	{
		GameSlot observer;
		observer.setState(SLOT_PLAYER, UnicodeString(L"Observer"));
		observer.setColor(2);
		observer.setPlayerTemplate(PLAYERTEMPLATE_OBSERVER);
		game.setSlot(2, observer);
		game.setLocalIP(observer.getIP());
	}

	frameLimit = static_cast<UnsignedInt>(limit);
	active = TRUE;
	TheFramePacer->enableLogicTimeScale(FALSE);
	TheFramePacer->enableFramesPerSecondLimit(FALSE);
	std::fprintf(stderr, "SKIRMISH_SETUP\t%s\t%s\t%s\t%u\n", factionA, factionB, difficulty, frameLimit);
	std::fflush(stderr);
	return TRUE;
}

void crc(UnsignedInt frame, UnsignedInt value)
{
	std::fprintf(stderr, "SKIRMISH_CRC\t%u\t%08x\n", frame, value);
}

void update(UnsignedInt frame)
{
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
	const Player* player = object.getControllingPlayer();
	if (isCompetitor(player))
		std::fprintf(stderr, "SKIRMISH_DEATH\t%u\t%d\t%u\t%s\t%u\n", TheGameLogic->getFrame(),
			player->getPlayerIndex(), static_cast<unsigned>(object.getID()), object.getTemplate()->getName().str(),
			static_cast<unsigned>(source));
	if (!isCompetitor(player) || !object.isKindOf(KINDOF_STRUCTURE) || object.isKindOf(KINDOF_BASE_FOUNDATION) ||
		object.isKindOf(KINDOF_INERT))
		return;
	const Object* attacker = TheGameLogic->findObjectByID(source);
	const Player* killer = attacker ? attacker->getControllingPlayer() : nullptr;
	if (isCompetitor(killer) && killer->getRelationship(object.getTeam()) == ENEMIES)
		std::fprintf(stderr, "SKIRMISH_STRUCTURE_KILL\t%u\t%d\t%d\t%u\t%s\n", TheGameLogic->getFrame(),
			killer->getPlayerIndex(), player->getPlayerIndex(), static_cast<unsigned>(object.getID()),
			object.getTemplate()->getName().str());
}

void structureDamage(const Object& object, const DamageInfo& damage)
{
	const Player* owner = object.getControllingPlayer();
	const Object* source = TheGameLogic->findObjectByID(damage.in.m_sourceID);
	const Player* attacker = source ? source->getControllingPlayer() : nullptr;
	if (damage.in.m_amount <= 0.0f || !isCompetitor(owner) || !isCompetitor(attacker) ||
		attacker->getRelationship(object.getTeam()) != ENEMIES)
		return;
	std::fprintf(stderr, "SKIRMISH_STRUCTURE_DAMAGE\t%u\t%d\t%d\t%u\t%s\t%s\t%d\t%a\t%a\n",
		TheGameLogic->getFrame(), attacker->getPlayerIndex(), owner->getPlayerIndex(),
		static_cast<unsigned>(object.getID()), object.getTemplate()->getName().str(),
		source->getTemplate()->getName().str(), damage.in.m_damageType, damage.in.m_amount, damage.out.m_actualDamageDealt);
}

void cast(Int player, const AsciiString& power, ObjectID source)
{
	if (isCompetitor(ThePlayerList->getNthPlayer(player)))
		std::fprintf(stderr, "SKIRMISH_CAST\t%u\t%d\t%s\t%u\n", TheGameLogic->getFrame(),
			player, power.str(), static_cast<unsigned>(source));
}

void purchase(const Player& player, const AsciiString& science)
{
	if (isCompetitor(&player))
		std::fprintf(stderr, "SKIRMISH_PURCHASE\t%u\t%d\t%s\n", TheGameLogic->getFrame(),
			player.getPlayerIndex(), science.str());
}

void buildPermission(const Player* player, const AsciiString& name, Bool found, Bool allowed)
{
	std::fprintf(stderr, "SKIRMISH_PERMISSION\t%u\t%d\t%s\t%d\t%d\n", TheGameLogic->getFrame(),
		player ? player->getPlayerIndex() : -1, name.str(), found, allowed);
}

void rebuild(const Object& hole, const Object& replacement)
{
	std::fprintf(stderr, "SKIRMISH_REBUILD\t%u\t%u\t%s\t%u\t%s\t%d\n", TheGameLogic->getFrame(),
		static_cast<unsigned>(hole.getID()), hole.getTemplate()->getName().str(),
		static_cast<unsigned>(replacement.getID()), replacement.getTemplate()->getName().str(),
		replacement.getControllingPlayer()->getPlayerIndex());
}
}
