#include "PreRTS.h"

#define DEFINE_DEATH_NAMES
#include "GameLogic/Damage.h"

#include "GameLogic/RetailTestCommands.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <string_view>
#include <vector>

#include "Common/ActionManager.h"
#include "Common/BuildAssistant.h"
#include "Common/DrawModule.h"
#include "Common/FramePacer.h"
#include "Common/GameEngine.h"
#include "Common/GameState.h"
#include "Common/GlobalData.h"
#include "Common/INI.h"
#include "Common/MapObject.h"
#include "Common/MessageStream.h"
#include "Common/Money.h"
#include "Common/NameKeyGenerator.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/PlayerTemplate.h"
#include "Common/RandomValue.h"
#include "Common/RetailFileLayers.h"
#include "Common/Science.h"
#include "Common/SpecialPower.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "Common/XferCRC.h"
#include "Common/XferLoad.h"
#include "Common/XferSave.h"
#include "GameClient/ControlBar.h"
#include "GameClient/Drawable.h"
#include "GameClient/GadgetPushButton.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/KeyDefs.h"
#include "GameClient/Image.h"
#include "GameClient/InGameUI.h"
#include "GameClient/ParticleSys.h"
#include "GameClient/TerrainVisual.h"
#include "GameClient/View.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/GhostObject.h"
#include "GameLogic/HeroRoster.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/ExperienceLevelStore.h"
#include "GameLogic/Module/BehaviorModule.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "Common/SpecialPower.h"
#include "GameLogic/Module/ProductionUpdate.h"
#include "GameLogic/Module/UpgradeModule.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/DozerAIUpdate.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Module/CrateCollide.h"
#include "GameLogic/Module/SlavedUpdate.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#if !RTS_GENERALS
#include "Common/Dict.h"
#include "Common/GameState.h"
#include "Common/Team.h"
#include "Common/WellKnownKeys.h"
#include "GameLogic/AIPlayer.h"
#include "GameLogic/ScriptActions.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Scripts.h"
#include "GameLogic/SidesList.h"
#include "GameLogic/Module/RotwkAbilityModules.h"
#include "GameLogic/Module/AttributeModifierHelper.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/RotwkParseOnlyModules.h"
#include "Common/AttributeModifierRules.h"
#endif
#include "GameLogic/Module/ProductionUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/Module/RetailGate.h"
#include "GameLogic/Module/WallHubBehavior.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Weapon.h"
#include "WWMath/wwmath.h"

struct RetailHordeSnapshotFixture
{
	static void loadPostProcess(HordeContain* contain) { contain->loadPostProcess(); }
};

#if !RTS_GENERALS
struct RetailTeamBudgetFixture
{
	static void check(Object* producer, const ThingTemplate* unit, unsigned int frame, Bool train)
	{
		Player* player = producer->getControllingPlayer();
		Bool listed = FALSE;
		for (const BuildListInfo* info = player->getBuildList(); info; info = info->getNext())
			listed |= info->getObjectID() == producer->getID();
		if (!listed)
			player->addToBuildList(producer);

		Dict teamData;
		teamData.setAsciiString(TheKey_teamUnitType1, unit->getName());
		teamData.setInt(TheKey_teamUnitMinCount1, 10);
		teamData.setInt(TheKey_teamUnitMaxCount1, 10);
		TeamPrototype* team = newInstance(TeamPrototype)(nullptr, "FortressBudgetFixture", player, FALSE, &teamData, 0);
		const Bool wasAllowedToBuild = player->getCanBuildUnits();
		AIPlayer* ai = newInstance(AIPlayer)(player);
		player->setCanBuildUnits(TRUE);
		Money* money = player->getMoney();
		const UnsignedInt original = money->countMoney();
		const auto* production = producer->getProductionUpdateInterface();
		HeroRules::Price price;
		const Bool override = production && production->getUnitPriceOverride(unit, price);
		std::fprintf(stderr, "TEST fortress-budget %u fixture=%u template=%u actual=%u status=%d,%d ready=%d queue=%u\n",
			frame, static_cast<unsigned>(producer->getID()), unit->calcCostToBuild(player),
			override ? static_cast<unsigned>(price.cost) : unit->calcCostToBuild(player),
			producer->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION), producer->testStatus(OBJECT_STATUS_SOLD),
			TheBuildAssistant->isPossibleToMakeUnit(producer, unit), production ? production->getProductionCount() : 0);
		money->withdraw(original, FALSE);
		for (UnsignedInt funds : {399u, 400u, 499u, 500u})
		{
			money->deposit(funds, FALSE, FALSE);
			Bool needsMoney = FALSE;
			const Bool allowed = ai->isPossibleToBuildTeam(team, TRUE, needsMoney);
			const Object* selected = ai->findFactory(unit, FALSE, TRUE);
			std::fprintf(stderr, "TEST fortress-budget %u funds=%u allowed=%d needsMoney=%d producer=%u fraction=%.2f\n",
				frame, funds, allowed, needsMoney, selected ? static_cast<unsigned>(selected->getID()) : 0,
				TheAI->getAiData()->m_teamResourcesToBuild);
			money->withdraw(funds, FALSE);
		}
		if (train)
		{
			money->deposit(400, FALSE, FALSE);
			WorkOrder* order = newInstance(WorkOrder);
			order->m_thing = unit;
			const Bool queued = ai->startTraining(order, FALSE, team->getName());
			std::fprintf(stderr, "TEST fortress-budget %u queued=%d producer=%u spent=%u\n", frame,
				queued, static_cast<unsigned>(order->m_factoryID), 400 - money->countMoney());
			deleteInstance(order);
			money->withdraw(money->countMoney(), FALSE);
		}
		money->deposit(original, FALSE, FALSE);
		player->setCanBuildUnits(wasAllowedToBuild);
		deleteInstance(ai);
		deleteInstance(team);
	}
};
#endif

void runRetailMordorTestCommand(std::string_view action, Object* source, Object* target, const char* argument, UnsignedInt frame);
void runRetailParticleTestCommand(std::string_view action, const Object* source, const Object* target,
	const Coord3D* location, const char* argument, UnsignedInt frame);
void runRetailPlacementTestCommand(Object* source, const char* name, const Coord3D* target, UnsignedInt frame);
void logRetailPlacementTestObject(const Object* object, const char* name, UnsignedInt frame);
void logRetailSpawnPoints(const Object* object, const char* prefix, UnsignedInt frame);
void runRetailSaveLoadPathTestCommand(std::string_view action, Object* object, UnsignedInt frame);
void runRetailSaveLoadPartitionTestCommand(std::string_view action, Object* object, const char* argument, UnsignedInt frame);
void runRetailPackage23TestCommand(std::string_view action, Player* player, Object* object, Object* target,
	const char* name, const Coord3D& point, UnsignedInt frame);
void runRetailSaveLoadWallTestCommand(std::string_view action, Player* player, Object* wall,
	const char* name, const Coord3D& position, UnsignedInt frame);
void runRetailSaveLoadBookTestCommand(Player* player, const char* templateName);
#if !RTS_GENERALS
void runRetailPackage05TestCommand(std::string_view action, Object* source, Object* target, UnsignedInt frame);
void runRetailPackage41TestCommand(std::string_view action, Object* source, UnsignedInt frame);
void runRetailPackage07TestCommand(std::string_view action, Object* source, Object* target, UnsignedInt frame);
void runRetailPackage17TestCommand(std::string_view action, Object* source, Object* target);
void runRetailPackage09TestCommand(std::string_view action, Object* source, Object* target, const char* argument, UnsignedInt frame);
void runRetailPackage10TestCommand(std::string_view action, Object* source, const char* argument, UnsignedInt frame);
void runRetailPackage50TestCommand(std::string_view action, Object* source, Object* target, const char* name, const char* argument, UnsignedInt frame);
void runRetailPackage12TestCommand(std::string_view action, Object* source, Object* target, UnsignedInt frame);
void runRetailPackage12AttachTestCommand(std::string_view action, Object* source, UnsignedInt frame);
void runRetailPackage12PickupTestCommand(std::string_view action, Object* source, const char* argument, UnsignedInt frame);
void runRetailPackage15TestCommand(std::string_view action, Object* source, UnsignedInt frame);
void runRetailPackage13TestCommand(std::string_view action, Object* source, Object* target, const char* argument, UnsignedInt frame);
void runRetailPackage16TestCommand(std::string_view action, Object* source, const char* argument, UnsignedInt frame);
void runRetailPackage19TestCommand(std::string_view action, Object* source, UnsignedInt frame);
void runRetailPackage18TestCommand(std::string_view action, Player* player, Object* source, const char* argument, UnsignedInt frame);

void runRetailPackage08TestCommand(std::string_view action, Object* source, Object* target, const char* name, const Coord3D* position, UnsignedInt frame);
void runRetailPackage11TestCommand(std::string_view action, Object* source, Object* target, const char* name, const Coord3D& inputs, UnsignedInt frame);
void runRetailPackage14TestCommand(std::string_view action, Object* object, const Object* killer, const char* argument, UnsignedInt frame);
void runRetailPackage20TestCommand(std::string_view action, Object* source, Object* target, const char* name, const Coord3D& inputs, UnsignedInt frame);
void runRetailPackage22TestCommand(std::string_view action, Object* source, Object* target, const Coord3D& inputs, UnsignedInt frame);
void runRetailPackage53TestCommand(std::string_view action, Object* source, Object* target, UnsignedInt frame);
#endif

namespace RetailTestCommands
{
namespace
{
struct Command
{
	unsigned int frame = 0;
	std::string action;
	std::string player;
	std::string target;
	int ordinal = 0;
	std::string structure;
	Coord3D location = {0.0f, 0.0f, 0.0f};
	Real angle = 0.0f;
	bool hasLocation = false;
	Bool hasPlacementPoint = FALSE;
	Real x = 0;
	Real y = 0;
};

struct MetaKey
{
	const char* name;
	GameMessage::Type type;
};

const MetaKey s_metaKeys[] = {
	{ "options", GameMessage::MSG_META_OPTIONS },
	{ "chat", GameMessage::MSG_META_CHAT_PLAYERS },
	{ "chatallies", GameMessage::MSG_META_CHAT_ALLIES },
	{ "diplomacy", GameMessage::MSG_META_DIPLOMACY },
	{ "idleworker", GameMessage::MSG_META_SELECT_NEXT_IDLE_WORKER },
	{ "nextworker", GameMessage::MSG_META_SELECT_NEXT_WORKER },
	{ "hero", GameMessage::MSG_META_SELECT_HERO },
	{ "selectall", GameMessage::MSG_META_SELECT_ALL },
	{ "matching", GameMessage::MSG_META_SELECT_MATCHING_UNITS },
	{ "nextunit", GameMessage::MSG_META_SELECT_NEXT_UNIT },
	{ "home", GameMessage::MSG_META_VIEW_COMMAND_CENTER },
	{ "radarevent", GameMessage::MSG_META_VIEW_LAST_RADAR_EVENT },
	{ "beacon", GameMessage::MSG_META_PLACE_BEACON },
	{ "stop", GameMessage::MSG_META_STOP },
	{ "scatter", GameMessage::MSG_META_SCATTER },
	{ "attackmove", GameMessage::MSG_META_TOGGLE_ATTACKMOVE },
	{ "controlbar", GameMessage::MSG_META_TOGGLE_CONTROL_BAR },
	{ "createteam", GameMessage::MSG_META_CREATE_TEAM1 },
	{ "selectteam", GameMessage::MSG_META_SELECT_TEAM1 },
};

std::vector<Command> s_commands;
std::vector<ObjectID> s_tracked;
ObjectID s_fortressWalker = INVALID_ID;
bool s_loaded = false;

void load()
{
	s_loaded = true;
	const char* path = std::getenv("GENERALSX_TEST_COMMANDS");
	if (!path || !*path)
		return;

	FILE* file = std::fopen(path, "r");
	if (!file)
		return;

	char line[512];
	while (std::fgets(line, sizeof(line), file))
	{
		Command command;
		char action[32] = {};
		char player[64] = {};
		char target[128] = {};
		char structure[384] = {};
		float x = 0.0f;
		float y = 0.0f;
		float angle = 0.0f;
		const int fields = std::sscanf(line, "%u %31s %63s %127s %d %383s %f %f %f", &command.frame, action, player, target,
			&command.ordinal, structure, &x, &y, &angle);
		if (fields < 5 || (std::string_view(action) == "place" && fields != 6 && fields < 8))
			continue;
		command.action = action;
		command.player = player;
		command.target = target;
		command.structure = structure;
		command.hasLocation = fields >= 8;
		command.location.set(x, y, 0.0f);
		command.angle = angle;
		command.hasPlacementPoint = fields >= 8;
		command.x = x;
		command.y = y;
		s_commands.push_back(std::move(command));
	}
	std::fclose(file);
}

Object* findObject(const Command& command, bool livingOnly = false)
{
	std::vector<Object*> matches;
	for (Object* obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject())
	{
		if (livingOnly && obj->isEffectivelyDead())
			continue;
		const Player* owner = obj->getControllingPlayer();
		if (!owner || stricmp(obj->getTemplate()->getName().str(), command.target.c_str()) != 0)
			continue;
		if (command.player == "*" || stricmp(TheNameKeyGenerator->keyToName(owner->getPlayerNameKey()).str(), command.player.c_str()) == 0)
			matches.push_back(obj);
	}
	std::sort(matches.begin(), matches.end(), [](const Object* a, const Object* b) { return a->getID() < b->getID(); });
	if (command.ordinal < 0 || static_cast<std::size_t>(command.ordinal) >= matches.size())
		return nullptr;
	return matches[command.ordinal];
}

Bool readPoint(const Command& command, const Object* obj, Coord3D& point)
{
	float x = 0.0f;
	float y = 0.0f;
	if (std::sscanf(command.structure.c_str(), "%f,%f", &x, &y) != 2)
		return FALSE;
	point = *obj->getPosition();
	point.x += x;
	point.y += y;
	point.z = TheTerrainLogic->getGroundHeight(point.x, point.y);
	return TRUE;
}

Object* findTarget(const Command& command, bool livingOnly);

void checkWallClaim(const Command& command, Object* source, UnsignedInt frame)
{
	const auto* hub = WallHubBehavior::find(source, OPTION_ONE);
	const auto* cap = TheThingFactory->findTemplate("MenWallHubSmall", FALSE);
	Player* player = source->getControllingPlayer();
	if (!hub || !cap || !player)
		return;

	const auto preview = hub->plan(cap, command.location);
	Int remembered = 0;
	for (const auto& piece : preview.pieces)
	{
		if (TheGhostObjectManager->collidesWithOrphanedGhost(&piece.position, piece.type->getTemplateGeometryInfo(),
			piece.facing, player->getPlayerIndex()))
			++remembered;
	}
	const Int money = player->getMoney()->countMoney();
	const Int objects = TheGameLogic->getObjectCount();
	const Bool built = hub->construct(cap, command.location, player);
	std::fprintf(stderr, "TEST wallclaim %u %s preview=%d ghosts=%d built=%d spent=%d pieces=%d\n",
		frame, command.structure.c_str(), preview.legal == LBC_OK, remembered, built,
		money - static_cast<Int>(player->getMoney()->countMoney()), static_cast<Int>(TheGameLogic->getObjectCount()) - objects);
}

void checkGateCells(const Command& command, Object* gate, UnsignedInt frame)
{
	Object* wall = findTarget(command, true);
	Command unitCommand = command;
	unitCommand.target = "GondorFighter";
	unitCommand.ordinal = 0;
	const Object* unit = findObject(unitCommand, true);
	if (!wall || !unit || !GateOpenAndCloseBehavior::find(gate))
		return;

	for (Bool gateFirst : {FALSE, TRUE})
	{
		PathfindCell cell;
		const ICoord2D location = {0, 0};
		Object* first = gateFirst ? gate : wall;
		Object* second = gateFirst ? wall : gate;
		cell.setTypeAsObstacle(first, FALSE, location);
		cell.setTypeAsObstacle(second, FALSE, location);
		cell.setTypeAsObstacle(second, FALSE, location);
		const Bool blocksWhileShared = !cell.isPassableGate(unit);
		cell.removeObstacle(first);
		const Bool surviving = cell.getType() == PathfindCell::CELL_OBSTACLE && cell.getObstacleID() == second->getID();
		const Bool friendlyPass = cell.isPassableGate(unit);
		cell.removeObstacle(second);
		std::fprintf(stderr, "TEST wallcells %u gateFirst=%d sharedBlocks=%d surviving=%d friendlyPass=%d clear=%d\n",
			frame, gateFirst, blocksWhileShared, surviving, friendlyPass, cell.getType() == PathfindCell::CELL_CLEAR);
	}
}

void checkGateFootprint(const Command& command, Object* gate, UnsignedInt frame)
{
	static std::vector<ICoord2D> wallCells;
	auto* pathfinder = TheAI->pathfinder();
	const Object* wall = findTarget(command, true);
	if (command.action == "wallgridbefore" && wall)
	{
		wallCells.clear();
		ICoord2D center;
		if (pathfinder->worldToCell(wall->getPosition(), &center))
			return;
		const Int radius = static_cast<Int>(wall->getGeometryInfo().getBoundingCircleRadius() / PATHFIND_CELL_SIZE_F) + 2;
		for (Int x = center.x - radius; x <= center.x + radius; ++x)
		{
			for (Int y = center.y - radius; y <= center.y + radius; ++y)
			{
				const auto* cell = pathfinder->getCell(LAYER_GROUND, x, y);
				if (cell && cell->getObstacleID() == wall->getID())
					wallCells.push_back({x, y});
			}
		}
		return;
	}

	Int retained = 0;
	for (const auto& position : wallCells)
	{
		const auto* cell = pathfinder->getCell(LAYER_GROUND, position.x, position.y);
		if (cell && cell->getType() == PathfindCell::CELL_OBSTACLE && cell->getObstacleID() == gate->getID())
			++retained;
	}
	std::fprintf(stderr, "TEST wallgrid %u wallCells=%zu retainedByGate=%d\n", frame, wallCells.size(), retained);
	wallCells.clear();
}

#if !RTS_GENERALS
void checkWallScriptAliases(const Command& command, Object* first, UnsignedInt frame)
{
	Object* second = findTarget(command, true);
	if (!second || !first->getBodyModule() || !second->getBodyModule())
		return;

	auto* assign = newInstance(ScriptAction)(ScriptAction::SET_UNIT_REFERENCE);
	assign->setNextAction(nullptr);
	assign->getParameter(0)->friend_setString("START_SPAWN");
	for (Object* target : {first, second})
	{
		AsciiString name;
		name.format("WallAliasFixture%u", static_cast<UnsignedInt>(target->getID()));
		target->setName(name);
		TheScriptEngine->addObjectToCache(target);
		assign->getParameter(1)->friend_setString(name);
		TheScriptEngine->friend_executeAction(assign, target->getTeam());
	}
	deleteInstance(assign);

	auto* damage = newInstance(ScriptAction)(ScriptAction::NAMED_DAMAGE);
	damage->setNextAction(nullptr);
	damage->getParameter(0)->friend_setString("START_SPAWN");
	damage->getParameter(1)->friend_setInt(1);
	for (Object* target : {first, second})
	{
		const Real firstHealth = first->getBodyModule()->getHealth();
		const Real secondHealth = second->getBodyModule()->getHealth();
		TheScriptEngine->friend_executeAction(damage, target->getTeam());
		std::fprintf(stderr, "TEST wallalias %u player=%d first=%d second=%d\n", frame,
			target->getControllingPlayer()->getPlayerIndex(), first->getBodyModule()->getHealth() < firstHealth,
			second->getBodyModule()->getHealth() < secondHealth);
	}
	deleteInstance(damage);
}
#endif

void selectGroup(Object* obj)
{
	GameMessage* select = TheMessageStream->appendMessage(GameMessage::MSG_CREATE_SELECTED_GROUP);
	select->appendBooleanArgument(TRUE);
	select->appendObjectIDArgument(obj->getID());
}

void selectObject(Object* obj)
{
	if (TheInGameUI && obj->getDrawable())
	{
		TheInGameUI->deselectAllDrawables();
		TheInGameUI->selectDrawable(obj->getDrawable());
	}
	selectGroup(obj);
}

GameWindow* commandWindow(int index)
{
	if (!TheWindowManager)
		return nullptr;
	AsciiString name;
	name.format("ControlBar.wnd:ButtonCommand%02d", index);
	return TheWindowManager->winGetWindowFromId(nullptr, TheNameKeyGenerator->nameToKey(name));
}

GameWindow* retailSlotWindow(int slot)
{
	if (!TheWindowManager || slot < 0)
		return nullptr;
	AsciiString name;
	if (slot < 6)
		name.format("ControlBar.wnd:RingButton%d", slot + 1);
	else if (slot >= MAX_COMMANDS_PER_SET + 3)
		name.format("ControlBar.wnd:RadialButton%02d", slot - MAX_COMMANDS_PER_SET - 2);
	else if (slot >= MAX_COMMANDS_PER_SET)
		name.format("ControlBar.wnd:TrayButton%d", slot - MAX_COMMANDS_PER_SET + 1);
	else
		name.format("ControlBar.wnd:SideButton%02d", slot - 5);
	return TheWindowManager->winGetWindowFromId(nullptr, TheNameKeyGenerator->nameToKey(name));
}

Player* findPlayer(const Command& command)
{
	return ThePlayerList ? ThePlayerList->findPlayerWithNameKey(TheNameKeyGenerator->nameToKey(command.player.c_str())) : nullptr;
}

const char* stateName(HeroRules::State state)
{
	switch (state)
	{
		case HeroRules::State::Revivable:
			return "revivable";
		case HeroRules::State::Lost:
			return "lost";
		default:
			return "recruitable";
	}
}

void dumpHeroes(const Command& command, unsigned int frame)
{
	const Player* player = findPlayer(command);
	if (!player)
		return;
	std::fprintf(stderr, "TEST heroes %u %s money=%u count=%d\n", frame, command.player.c_str(),
		player->getMoney()->countMoney(), HeroRoster::count(player));
	for (int i = 0; i < HeroRoster::count(player); ++i)
	{
		const ThingTemplate* hero = HeroRoster::heroAt(player, i);
		const HeroRules::Price price = HeroRoster::price(player, hero);
		const HeroRules::Price revive = HeroRoster::revivePrice(hero, HeroRoster::savedLevelAt(player, i));
		int veterancy = -1;
		for (Object* obj = TheGameLogic->getFirstObject(); obj && hero; obj = obj->getNextObject())
			if (obj->getControllingPlayer() == player && !obj->isEffectivelyDead() && hero->isEquivalentTo(obj->getTemplate()))
				veterancy = static_cast<int>(obj->getVeterancyLevel());
		std::fprintf(stderr, "TEST hero %u %s %d %s state=%s alive=%d queued=%d level=%d cost=%d frames=%u buildCost=%d buildFrames=%d revive=%d/%u veterancy=%d\n",
			frame, command.player.c_str(), i, hero ? hero->getName().str() : "-", stateName(HeroRoster::stateAt(player, i)),
			HeroRoster::isAlive(player, hero) ? 1 : 0, HeroRoster::isQueued(player, hero) ? 1 : 0,
			HeroRoster::savedLevelAt(player, i), price.cost, price.frames, hero ? hero->calcCostToBuild(player) : 0,
			hero ? hero->calcTimeToBuild(player) : 0, revive.cost, revive.frames, veterancy);
	}
}

void dumpProduction(const Command& command, unsigned int frame)
{
	const Player* player = findPlayer(command);
	for (Object* obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject())
	{
		ProductionUpdateInterface* production = obj->getControllingPlayer() == player ? obj->getProductionUpdateInterface() : nullptr;
		if (!production)
			continue;
		for (const ProductionEntry* entry = production->firstProduction(); entry; entry = production->nextProduction(entry))
		{
			const char* name = entry->getProductionType() == PRODUCTION_UNIT
				? entry->getProductionObject()->getName().str() : entry->getProductionUpgrade()->getUpgradeName().str();
			std::fprintf(stderr, "TEST production %u %s %u %s percent=%.1f exit=%d disabled=%d\n", frame, obj->getTemplate()->getName().str(),
				static_cast<unsigned>(obj->getID()), name, entry->getPercentComplete(),
				obj->getObjectExitInterface() ? 1 : 0, obj->isDisabled() ? 1 : 0);
		}
	}
}

void dumpHud(unsigned int frame)
{
	for (int slot = 0; slot < MAX_COMMANDS_PER_SET + 3 + 12; ++slot)
	{
		GameWindow* window = retailSlotWindow(slot);
		const CommandButton* button = window ? static_cast<const CommandButton*>(GadgetButtonGetData(window)) : nullptr;
		if (!window || !button || window->winIsHidden())
			continue;
		const ThingTemplate* hero = TheControlBar ? TheControlBar->getRetailHero(window) : nullptr;
		const Image* image = TheControlBar ? TheControlBar->getRetailButtonImage(window) : nullptr;
		std::fprintf(stderr, "TEST hud %u slot=%d %s type=%d %s hero=%s image=%s\n", frame, slot, button->getName().str(),
			static_cast<int>(button->getCommandType()), BitIsSet(window->winGetStatus(), WIN_STATUS_ENABLED) ? "enabled" : "disabled",
			hero ? hero->getName().str() : "-", image ? image->getName().str() : "-");
		if (hero)
		{
			Int count;
			Real progress;
			TheControlBar->getRetailProduction(window, count, progress);
			std::fprintf(stderr, "TEST hud_production %u %s count=%d progress=%.3f\n", frame, hero->getName().str(), count, progress);
			Drawable* selected = TheInGameUI->getFirstSelectedDrawable();
			if (selected)
				std::fprintf(stderr, "TEST hud_make %u slot=%d %s reason=%d\n", frame, slot, hero->getName().str(),
					static_cast<int>(TheBuildAssistant->canMakeUnit(selected->getObject(), hero)));
		}
	}
}

void clickWindow(GameWindow* button, int index, unsigned int frame)
{
	const CommandButton* commandButton = button ? static_cast<const CommandButton*>(GadgetButtonGetData(button)) : nullptr;
	if (!button || !commandButton || button->winIsHidden() || !button->winGetOwner())
	{
		std::fprintf(stderr, "TEST frame %u: slot %d empty\n", frame, index);
		return;
	}
	const Bool enabled = BitIsSet(button->winGetStatus(), WIN_STATUS_ENABLED);
	std::fprintf(stderr, "TEST frame %u: slot %d %s type %d %s\n", frame, index, commandButton->getName().str(),
		static_cast<int>(commandButton->getCommandType()), enabled ? "enabled" : "disabled");
	if (!enabled || commandButton->getCommandType() == GUI_COMMAND_SELL)
		return;
	TheWindowManager->winSendSystemMsg(button->winGetOwner(), GBM_SELECTED, (WindowMsgData)button, button->winGetWindowId());
}

void spawnObject(const Command& command, unsigned int frame)
{
	Player* player = findPlayer(command);
	const ThingTemplate* what = TheThingFactory->findTemplate(command.target.c_str(), FALSE);
	if (!player || !what)
		return;
	Object* anchor = nullptr;
	for (Object* obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject())
	{
		if (obj->getControllingPlayer() == player && obj->isKindOf(KINDOF_STRUCTURE) && (!anchor || obj->getID() < anchor->getID()))
			anchor = obj;
	}
	if (!anchor)
		return;
	Coord3D point;
	if (!readPoint(command, anchor, point))
		point = *anchor->getPosition();
	Object* obj = TheThingFactory->newObject(what, player->getDefaultTeam());
	obj->setPosition(&point);
	std::fprintf(stderr, "TEST frame %u: spawn %s id=%u for %s commandSet=%s\n", frame, command.target.c_str(),
		static_cast<unsigned>(obj->getID()), command.player.c_str(), obj->getCommandSetString().str());
}

void fortressGround(const Command& command, Object* keep, unsigned int frame)
{
	const Coord3D& origin = *keep->getPosition();
	const Real c = WWMath::CosTrig(keep->getOrientation());
	const Real s = WWMath::SinTrig(keep->getOrientation());
	Coord3D point = {origin.x + command.x * c - command.y * s, origin.y + command.x * s + command.y * c, 0.0f};
	point.z = TheTerrainLogic->getGroundHeight(point.x, point.y);
	if (command.action == "fortress-walker")
	{
		if (const ThingTemplate* unit = TheThingFactory->findTemplate(command.structure.c_str(), FALSE))
		{
			Object* walker = TheThingFactory->newObject(unit, keep->getTeam());
			walker->setPosition(&point);
			s_fortressWalker = walker->getID();
		}
		return;
	}
	Object* walker = TheGameLogic->findObjectByID(s_fortressWalker);
	if (command.action == "fortress-walk")
	{
		if (walker && walker->getAI())
			walker->getAI()->aiMoveToPosition(&point, CMD_FROM_SCRIPT);
		return;
	}
	const PathfindCell* cell = TheAI->pathfinder()->getCell(LAYER_GROUND, &point);
	const GeometryInfo probe(GEOMETRY_SPHERE, TRUE, 1.0f, 1.0f, 1.0f);
	PartitionFilterWouldCollide query(point, probe, 0.0f, TRUE);
	std::fprintf(stderr, "TEST fortress-ground %u local=%.1f,%.1f obstacle=%d hit=%d height=%.1f",
		frame, command.x, command.y, cell && cell->getType() == PathfindCell::CELL_OBSTACLE, query.allow(keep),
		keep->getGeometryInfo().getMaxHeightAbovePosition());
	if (walker)
	{
		const Real dx = walker->getPosition()->x - origin.x;
		const Real dy = walker->getPosition()->y - origin.y;
		PartitionFilterWouldCollide unitQuery(*walker->getPosition(), walker->getGeometryInfo(), walker->getOrientation(), TRUE);
		std::fprintf(stderr, " walker=%.2f,%.2f moving=%d overlaps=%d", dx * c + dy * s, dy * c - dx * s,
			walker->getAI() && walker->getAI()->isMoving(), unitQuery.allow(keep));
	}
	std::fprintf(stderr, " cell=%d small=%d structure=%d mobile=%d altitude=%.2f world=%.2f,%.2f\n",
		cell ? static_cast<Int>(cell->getType()) : -1, keep->getGeometryInfo().getIsSmall(),
		keep->isKindOf(KINDOF_STRUCTURE), keep->isMobile(), keep->getHeightAboveTerrain(), point.x, point.y);
}

void clickButton(const Command& command, unsigned int frame)
{
	const int index = std::atoi(command.structure.c_str());
	GameWindow* button = commandWindow(index);
	const CommandButton* commandButton = button ? static_cast<const CommandButton*>(GadgetButtonGetData(button)) : nullptr;
	if (!button || !commandButton || button->winIsHidden() || !button->winGetOwner())
	{
		std::fprintf(stderr, "TEST frame %u: button %d empty\n", frame, index);
		return;
	}
	const Bool enabled = BitIsSet(button->winGetStatus(), WIN_STATUS_ENABLED);
	std::fprintf(stderr, "TEST frame %u: button %d %s type %d %s\n", frame, index, commandButton->getName().str(),
		static_cast<int>(commandButton->getCommandType()), enabled ? "enabled" : "disabled");
	if (!enabled || commandButton->getCommandType() == GUI_COMMAND_SELL)
		return;
	TheWindowManager->winSendSystemMsg(button->winGetOwner(), GBM_SELECTED, (WindowMsgData)button, button->winGetWindowId());
}

void cancelInterface()
{
	if (!TheInGameUI)
		return;
	TheInGameUI->setGUICommand(nullptr);
	TheInGameUI->placeBuildAvailable(nullptr, nullptr);
}

void placeStructure(const Command& command, Object* builder, unsigned int frame)
{
	const ThingTemplate* what = TheThingFactory->findTemplate(command.structure.c_str(), FALSE);
	if (!what || !TheInGameUI || !TheTacticalView || !builder->getDrawable())
		return;
	selectObject(builder);
	TheInGameUI->placeBuildAvailable(what, builder->getDrawable());
	Coord3D anchor = *builder->getPosition();
	anchor.x += 60.0f;
	ICoord2D start;
	TheTacticalView->worldToScreen(&anchor, &start);
	ICoord2D end = start;
	end.x += 40;
	end.y += 20;
	TheInGameUI->setPlacementStart(&start);
	TheInGameUI->setPlacementEnd(&end);
	std::fprintf(stderr, "TEST frame %u: place %s anchored %d\n", frame, command.structure.c_str(),
		TheInGameUI->isPlacementAnchored() ? 1 : 0);
}

void sendKey(const Command& command, unsigned int frame)
{
	for (const MetaKey& key : s_metaKeys)
	{
		if (command.target != key.name)
			continue;
		TheMessageStream->appendMessage(key.type);
		std::fprintf(stderr, "TEST frame %u: key %s\n", frame, key.name);
		return;
	}
	std::fprintf(stderr, "TEST frame %u: unknown key %s\n", frame, command.target.c_str());
}

void orderToPoint(const Command& command, Object* obj, unsigned int frame)
{
	Coord3D point;
	if (!readPoint(command, obj, point))
		return;
	selectObject(obj);
	GameMessage* order = nullptr;
	if (command.action == "move")
		order = TheMessageStream->appendMessage(GameMessage::MSG_DO_MOVETO);
	else if (command.action == "waypoint")
		order = TheMessageStream->appendMessage(GameMessage::MSG_ADD_WAYPOINT);
	else if (command.action == "attackmove")
		order = TheMessageStream->appendMessage(GameMessage::MSG_DO_ATTACKMOVETO);
	else if (command.action == "forceattackground")
		order = TheMessageStream->appendMessage(GameMessage::MSG_DO_FORCE_ATTACK_GROUND);
	else if (command.action == "guard")
		order = TheMessageStream->appendMessage(GameMessage::MSG_DO_GUARD_POSITION);
	else if (command.action == "rally")
	{
		order = TheMessageStream->appendMessage(GameMessage::MSG_SET_RALLY_POINT);
		order->appendObjectIDArgument(obj->getID());
	}
	if (!order)
		return;
	order->appendLocationArgument(point);
	if (command.action == "guard")
		order->appendIntegerArgument(GUARDMODE_NORMAL);
	if (TheInGameUI && (command.action == "move" || command.action == "waypoint"))
		TheInGameUI->createMoveHint(order);
	else if (TheInGameUI && command.action == "attackmove")
		TheInGameUI->createAttackHint(order);
	std::fprintf(stderr, "TEST frame %u: %s object %u to (%.1f, %.1f)\n", frame, command.action.c_str(),
		static_cast<unsigned>(obj->getID()), point.x, point.y);
}

void createObject(const Command& command, unsigned int frame)
{
	const ThingTemplate* what = TheThingFactory->findTemplate(command.target.c_str(), FALSE);
	Player* player = ThePlayerList->findPlayerWithNameKey(TheNameKeyGenerator->nameToKey(command.player.c_str()));
	float x = 0.0f;
	float y = 0.0f;
	float angle = 0.0f;
	if (!what || !player || std::sscanf(command.structure.c_str(), "%f,%f,%f", &x, &y, &angle) < 2)
	{
		std::fprintf(stderr, "TEST frame %u: cannot create %s for %s\n", frame, command.target.c_str(), command.player.c_str());
		return;
	}
	Object* obj = TheThingFactory->newObject(what, player->getDefaultTeam());
	Coord3D pos;
	pos.set(x, y, TheTerrainLogic->getGroundHeight(x, y));
	obj->setPosition(&pos);
	obj->setOrientation(angle * PI / 180.0f);
	std::fprintf(stderr, "TEST frame %u: create %s for %s id=%u at (%.1f, %.1f)\n", frame, command.target.c_str(),
		command.player.c_str(), static_cast<unsigned>(obj->getID()), x, y);
}

Object* findTarget(const Command& command, bool livingOnly = false)
{
	Command target;
	char player[64] = {};
	char name[128] = {};
	if (std::sscanf(command.structure.c_str(), "%63[^:]:%127[^:]:%d", player, name, &target.ordinal) != 3)
		return nullptr;
	target.player = player;
	target.target = name;
	return findObject(target, livingOnly);
}

void attackObject(const Command& command, Object* obj, unsigned int frame)
{
	Object* target = findTarget(command);
	AIUpdateInterface* ai = obj->getAI();
	if (!target || !ai)
	{
		std::fprintf(stderr, "TEST frame %u: cannot attack %s with object %u\n", frame, command.structure.c_str(),
			static_cast<unsigned>(obj->getID()));
		return;
	}
	ai->aiAttackObject(target, NO_MAX_SHOTS_LIMIT, CMD_FROM_PLAYER);
	std::fprintf(stderr, "TEST frame %u: attack object %u (%s) by object %u (%s)\n", frame, static_cast<unsigned>(target->getID()),
		target->getTemplate()->getName().str(), static_cast<unsigned>(obj->getID()), command.target.c_str());
}

void printHealth(const Command& command, const Object* obj, unsigned int frame)
{
	const BodyModuleInterface* body = obj->getBodyModule();
	std::fprintf(stderr, "TEST health %u %s %s %d id=%u health=%.2f max=%.2f dead=%d pos=%.1f,%.1f\n", frame,
		command.player.c_str(), command.target.c_str(), command.ordinal, static_cast<unsigned>(obj->getID()),
		body ? body->getHealth() : 0.0f, body ? body->getMaxHealth() : 0.0f, obj->isEffectivelyDead() ? 1 : 0,
		obj->getPosition()->x, obj->getPosition()->y);
}

#if !RTS_GENERALS
void logModifiedObject(const char* label, const Object* obj, unsigned int frame)
{
	const BodyModuleInterface* body = obj->getBodyModule();
	const AIUpdateInterface* ai = obj->getAI();
	const Locomotor* locomotor = ai ? ai->getCurLocomotor() : nullptr;
	const Weapon* weapon = obj->getWeaponInWeaponSlot(PRIMARY_WEAPON);
	const StancesBehavior* stances = StancesBehavior::find(obj);
	AsciiString lists;
	AttributeModifiers::describe(obj, lists);
	std::fprintf(stderr, "TEST mods %u %s id=%u(%s) stance=%s health=%.1f/%.1f armorSlash=%.3f armorMagic=%.3f damage=%.3f damageAdd=%.1f range=%.1f rof=%.3f speed=%.2f vision=%.1f shroud=%.1f xp=%.3f production=%.3f recharge=%.3f armorPierce=%.3f lists=[%s]\n",
		frame, label, static_cast<unsigned>(obj->getID()), obj->getTemplate()->getName().str(),
		stances ? AttributeModifierRules::stanceNames()[stances->getStance()] : "-",
		body ? body->getHealth() : 0.0f, body ? body->getMaxHealth() : 0.0f,
		AttributeModifiers::damageTakenFactor(obj, DAMAGE_SLASH), AttributeModifiers::damageTakenFactor(obj, DAMAGE_MAGIC),
		AttributeModifiers::damageDealtFactor(obj), AttributeModifiers::value(obj, AttributeModifierRules::DAMAGE_ADD), weapon ? weapon->getAttackRange(obj) : 0.0f, AttributeModifiers::rateOfFireFactor(obj),
		locomotor ? locomotor->getMaxSpeedForCondition(BODY_PRISTINE, obj) : 0.0f, obj->getVisionRange(), obj->getShroudClearingRange(),
		AttributeModifiers::experienceFactor(obj), AttributeModifiers::productionFactor(obj), AttributeModifiers::rechargeFactor(obj),
		AttributeModifiers::damageTakenFactor(obj, DAMAGE_PIERCE), lists.str());
}

void logModifiers(Object* obj, unsigned int frame)
{
	logModifiedObject("object", obj, frame);
	ContainModuleInterface* contain = HordeContain::get(obj) ? obj->getContain() : nullptr;
	const ContainedItemsList* members = contain ? contain->getContainedItemsList() : nullptr;
	if (members && !members->empty())
		logModifiedObject("member", members->front(), frame);
}
#endif

void listObjects(const Command& command, unsigned int frame)
{
	std::vector<Object*> owned;
	for (Object* obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject())
	{
		const Player* owner = obj->getControllingPlayer();
		if (owner && stricmp(TheNameKeyGenerator->keyToName(owner->getPlayerNameKey()).str(), command.player.c_str()) == 0)
			owned.push_back(obj);
	}
	std::sort(owned.begin(), owned.end(), [](const Object* a, const Object* b) { return a->getID() < b->getID(); });
	for (const Object* obj : owned)
	{
		int ordinal = 0;
		for (const Object* other : owned)
		{
			if (other == obj)
				break;
			if (other->getTemplate()->getName() == obj->getTemplate()->getName())
				++ordinal;
		}
		std::fprintf(stderr, "TEST list %u %s %s %d id=%u pos=%.1f,%.1f structure=%d selectable=%d drawable=%d\n", frame,
			command.player.c_str(), obj->getTemplate()->getName().str(), ordinal, static_cast<unsigned>(obj->getID()),
			obj->getPosition()->x, obj->getPosition()->y, obj->isKindOf(KINDOF_STRUCTURE) ? 1 : 0,
			obj->isSelectable() ? 1 : 0, obj->getDrawable() ? 1 : 0);
	}
}

void dumpPlayer(const Command& command, unsigned int frame)
{
	if (!ThePlayerList)
		return;
	Player* player = ThePlayerList->findPlayerWithNameKey(TheNameKeyGenerator->nameToKey(command.player.c_str()));
	if (!player)
	{
		std::fprintf(stderr, "TEST player %u %s missing\n", frame, command.player.c_str());
		return;
	}
	const PlayerTemplate* pt = player->getPlayerTemplate();
	if (!pt)
	{
		std::fprintf(stderr, "TEST player %u %s no_template\n", frame, command.player.c_str());
		return;
	}
	std::string upgrades;
	if (TheUpgradeCenter)
	{
		for (const AsciiString& name : pt->getInitialUpgrades())
		{
			const UpgradeTemplate* ut = TheUpgradeCenter->findUpgrade(name);
			if (ut && player->hasUpgradeComplete(ut))
			{
				if (!upgrades.empty())
					upgrades += ',';
				upgrades += name.str();
			}
		}
	}
	std::string sciences;
	if (TheScienceStore)
	{
		for (const ScienceType sci : pt->getIntrinsicSciences(TRUE))
		{
			if (player->hasScience(sci))
			{
				if (!sciences.empty())
					sciences += ',';
				sciences += TheScienceStore->getInternalNameForScience(sci).str();
			}
		}
		for (const ScienceType sci : pt->getIntrinsicSciences(FALSE))
		{
			if (player->hasScience(sci))
			{
				const char* sciName = TheScienceStore->getInternalNameForScience(sci).str();
				if (sciences.find(sciName) == std::string::npos)
				{
					if (!sciences.empty())
						sciences += ',';
					sciences += sciName;
				}
			}
		}
	}
	std::fprintf(stderr,
		"TEST player %u %s template=%s evil=%d maxLevelMP=%d maxLevelSP=%d spellBook=%s store=%s heroes=%zu ringHeroes=%zu resModVals=%zu resModFilterObjs=%zu portrait=%s upgrades=%s sciences=%s\n",
		frame, command.player.c_str(), pt->getName().str(), pt->isEvil() ? 1 : 0, pt->getMaxLevelMP(), pt->getMaxLevelSP(),
		pt->getSpellBook(TRUE).str(), pt->getPurchaseScienceCommandSet(TRUE).str(), pt->getBuildableHeroesMP().size(),
		pt->getBuildableRingHeroesMP().size(), pt->getResourceModifierValues().size(),
		pt->getResourceModifierObjectFilter().m_includeObjects.size(), pt->getMultiSelectionPortrait().str(),
		upgrades.empty() ? "-" : upgrades.c_str(), sciences.empty() ? "-" : sciences.c_str());

	std::vector<Object*> owned;
	for (Object* obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject())
	{
		if (obj->getControllingPlayer() == player)
			owned.push_back(obj);
	}
	std::sort(owned.begin(), owned.end(), [](const Object* a, const Object* b) { return a->getID() < b->getID(); });
	for (Object* obj : owned)
	{
		int upgradedModules = 0;
		for (BehaviorModule** mod = obj->getBehaviorModules(); mod && *mod; ++mod)
		{
			const UpgradeModuleInterface* upg = (*mod)->getUpgrade();
			if (upg && upg->isAlreadyUpgraded())
				++upgradedModules;
		}
		std::fprintf(stderr, "TEST player_obj %u %s %s id=%u upgradedModules=%d commandSet=%s\n", frame,
			command.player.c_str(), obj->getTemplate()->getName().str(), static_cast<unsigned>(obj->getID()),
			upgradedModules, obj->getCommandSetString().str());
	}
}

Coord3D groundLocation(const Command& command)
{
	Coord3D location = command.location;
	location.z = TheTerrainLogic->getGroundHeight(location.x, location.y);
	return location;
}

void logObject(const Object* obj, unsigned int frame)
{
	const Coord3D* pos = obj->getPosition();
	std::fprintf(stderr, "TEST frame %u: object %u %s pos %.3f %.3f %.3f angle %.4f", frame,
		static_cast<unsigned>(obj->getID()), obj->getTemplate()->getName().str(), pos->x, pos->y, pos->z,
		obj->getOrientation());
	const AIUpdateInterface* ai = obj->getAI();
	if (ai)
	{
		const Locomotor* locomotor = ai->getCurLocomotor();
		std::fprintf(stderr, " state %d moving %d path %d set %d locomotor %s speed %.4f",
			static_cast<int>(ai->getAIStateType()), ai->isMoving() ? 1 : 0, ai->getPath() ? 1 : 0,
			static_cast<int>(ai->getCurLocomotorSetType()), locomotor ? locomotor->getTemplateName().str() : "none",
			locomotor ? locomotor->getMaxSpeedForCondition(BODY_PRISTINE) : 0.0f);
	}
	const PhysicsBehavior* physics = obj->getPhysics();
	if (physics)
		std::fprintf(stderr, " velocity %.4f", physics->getVelocityMagnitude());
	const Drawable* draw = obj->getDrawable();
	std::fprintf(stderr, " hidden %d masked %d registered %d dead %d construction %.2f\n",
		draw && draw->isDrawableEffectivelyHidden() ? 1 : 0, obj->testStatus(OBJECT_STATUS_MASKED) ? 1 : 0,
		obj->friend_getPartitionData() ? 1 : 0, obj->isEffectivelyDead() ? 1 : 0, obj->getConstructionPercent());
}

void logBuildChecks(Object* builder, const ThingTemplate* what, const Command& command, unsigned int frame)
{
	const Coord3D location = groundLocation(command);
	Int cellX = 0;
	Int cellY = 0;
	Int shroud = -1;
	if (ThePartitionManager && builder->getControllingPlayer())
	{
		ThePartitionManager->worldToCell(location.x, location.y, &cellX, &cellY);
		shroud = static_cast<Int>(ThePartitionManager->getShroudStatusForPlayer(
			builder->getControllingPlayer()->getPlayerIndex(), cellX, cellY));
	}
	const UnsignedInt basic = BuildAssistant::TERRAIN_RESTRICTIONS | BuildAssistant::CLEAR_PATH |
		BuildAssistant::NO_OBJECT_OVERLAP;
	const UnsignedInt retail = RetailFileLayers::enabled() ? BuildAssistant::IGNORE_HIDDEN_OBJECTS : 0;
	const UnsignedInt remembered = RetailFileLayers::enabled() ? BuildAssistant::BLOCK_REMEMBERED_OBJECTS : 0;
	const LegalBuildCode legal = TheBuildAssistant->isLocationLegalToBuild(&location, what, command.angle,
		basic | BuildAssistant::USE_QUICK_PATHFIND | BuildAssistant::SHROUD_REVEALED |
			BuildAssistant::IGNORE_STEALTHED | BuildAssistant::FAIL_STEALTHED_WITHOUT_FEEDBACK | retail | remembered,
		builder, nullptr);
	const LegalBuildCode order = TheBuildAssistant->isLocationLegalToBuild(&location, what, command.angle,
		basic | BuildAssistant::SHROUD_REVEALED | retail, builder, nullptr);
	const LegalBuildCode ai = TheBuildAssistant->isLocationLegalToBuild(&location, what, command.angle,
		basic, builder, builder->getControllingPlayer());
	if (TheTerrainVisual)
		TheTerrainVisual->removeAllBibs();
	const Bool ghost = TheGhostObjectManager && builder->getControllingPlayer() &&
		TheGhostObjectManager->collidesWithOrphanedGhost(&location, what->getTemplateGeometryInfo(), command.angle,
			builder->getControllingPlayer()->getPlayerIndex());
	Int cellType = -1;
	if (TheAI && TheAI->pathfinder())
	{
		const Int pCellX = REAL_TO_INT_FLOOR(location.x / PATHFIND_CELL_SIZE);
		const Int pCellY = REAL_TO_INT_FLOOR(location.y / PATHFIND_CELL_SIZE);
		PathfindCell* pCell = TheAI->pathfinder()->getCell(LAYER_GROUND, pCellX, pCellY);
		if (pCell)
			cellType = static_cast<Int>(pCell->getType());
	}
	const UnsignedInt ghosts = TheInGameUI ? TheInGameUI->getPlannedBuildGhostCount() : 0;
	std::fprintf(stderr, "TEST frame %u: check %s canMake %d possible %d legal %d order %d ai %d ghost %d shroud %d cell %d ghosts %u at %.1f %.1f cost %d money %u\n", frame,
		what->getName().str(), static_cast<int>(TheBuildAssistant->canMakeUnit(builder, what)),
		TheBuildAssistant->isPossibleToMakeUnit(builder, what) ? 1 : 0, static_cast<int>(legal), static_cast<int>(order), static_cast<int>(ai), ghost ? 1 : 0, shroud, cellType, ghosts, location.x, location.y,
		what->calcCostToBuild(builder->getControllingPlayer()), builder->getControllingPlayer()->getMoney()->countMoney());
}

void logButtons(Object* obj, const Command& command, unsigned int frame)
{
	if (!TheControlBar)
		return;
	const CommandSet* set = TheControlBar->findCommandSet(obj->getCommandSetString());
	std::fprintf(stderr, "TEST frame %u: command set %s\n", frame, obj->getCommandSetString().str());
	if (!set)
		return;
	Player* player = obj->getControllingPlayer();
	const ProductionUpdateInterface* production = obj->getProductionUpdateInterface();
	for (Int i = 0; i < CommandButtonRules::RETAIL_COMMANDS_PER_SET; ++i)
	{
		const CommandButton* button = set->getCommandButton(i);
		if (!button)
			continue;
		const ThingTemplate* what = button->getThingTemplate();
		std::fprintf(stderr, "TEST frame %u: slot %d %s type %d object %s\n", frame, i + 1, button->getName().str(),
			static_cast<int>(button->getCommandType()), what ? what->getName().str() : "-");
		const UpgradeTemplate* upgrade = button->getUpgradeTemplate();
		if (upgrade && button->getCommandType() == GUI_COMMAND_OBJECT_UPGRADE)
		{
			const Bool prereq = !BitIsSet(button->getOptions(), NEED_UPGRADE) || HeroRoster::buttonUpgradesMet(button, player, obj);
			const char* reason = !prereq ? "missing-upgrade" : obj->hasUpgrade(upgrade) ? "complete"
				: !production ? "no-producer" : production->isUpgradeInQueue(upgrade) ? "queued"
				: !obj->affectedByUpgrade(upgrade) ? "no-effect"
				: !TheUpgradeCenter->canAffordUpgrade(player, upgrade) ? "resources" : "available";
			std::fprintf(stderr, "TEST upgrade_button %u %s %s reason=%s frames=%d\n", frame, button->getName().str(),
				upgrade->getUpgradeName().str(), reason, upgrade->calcTimeToBuild(player));
		}
		if (what && button->getCommandType() == GUI_COMMAND_DOZER_CONSTRUCT && command.hasLocation)
			logBuildChecks(obj, what, command, frame);
	}
}

Player* findPlayer(const std::string& name)
{
	for (Int i = 0; i < ThePlayerList->getPlayerCount(); ++i)
	{
		Player* player = ThePlayerList->getNthPlayer(i);
		if (stricmp(TheNameKeyGenerator->keyToName(player->getPlayerNameKey()).str(), name.c_str()) == 0)
			return player;
	}
	return nullptr;
}

void capture(const Command& command, Object* obj, unsigned int frame)
{
	Player* player = findPlayer(command.structure);
	if (!player)
		return;
	obj->setTeam(player->getDefaultTeam());
	std::fprintf(stderr, "TEST frame %u: capture object %u by %s\n", frame, static_cast<unsigned>(obj->getID()),
		command.structure.c_str());
}

void repair(const Command& command, Object* obj, unsigned int frame)
{
	Command targetCommand = command;
	targetCommand.target = command.structure;
	targetCommand.ordinal = 0;
	Object* target = findObject(targetCommand);
	AIUpdateInterface* ai = obj->getAI();
	if (!target || !ai)
		return;
	std::fprintf(stderr, "TEST builder repair eligibility %u target=%u player=%d ai=%d cursor=%d\n", frame,
		static_cast<unsigned>(target->getID()), TheActionManager->canRepairObject(obj, target, CMD_FROM_PLAYER),
		TheActionManager->canRepairObject(obj, target, CMD_FROM_AI),
		TheInGameUI->canSelectedObjectsDoAction(InGameUI::ACTIONTYPE_REPAIR_OBJECT, target, InGameUI::SELECTION_ANY));
	ai->aiRepair(target, CMD_FROM_AI);
	std::fprintf(stderr, "TEST frame %u: repair object %u by %u\n", frame, static_cast<unsigned>(target->getID()),
		static_cast<unsigned>(obj->getID()));
}

void roundTripBuilder(Object* obj, unsigned int frame)
{
	auto* ai = obj->getAI();
	auto* dozer = ai ? ai->getDozerAIInterface() : nullptr;
	const char* directory = std::getenv("XDG_RUNTIME_DIR");
	if (!dozer || !directory || !obj->testStatus(OBJECT_STATUS_MASKED) || dozer->getCurrentTask() != DOZER_TASK_BUILD)
	{
		std::fprintf(stderr, "TEST builder snapshot %u skipped\n", frame);
		return;
	}

	AsciiString file;
	file.format("%s/builder-snapshot", directory);
	XferSave save;
	save.open(file);
	save.xferSnapshot(ai);
	save.close();

	const Coord3D position = *obj->getPosition();
	const auto status = obj->getStatusBits();
	Object* structure = TheGameLogic->findObjectByID(dozer->getTaskTarget(DOZER_TASK_BUILD));
	dozer->cancelTask(DOZER_TASK_BUILD);
	if (structure)
		structure->setModelConditionState(MODELCONDITION_ACTIVELY_BEING_CONSTRUCTED);
	obj->setPosition(&position);
	obj->setStatus(status);
	if (auto* draw = obj->getDrawable())
		draw->setDrawableHidden(TRUE);
	ThePartitionManager->unRegisterObject(obj);

	XferLoad load;
	load.setOptions(XO_NO_POST_PROCESSING);
	load.open(file);
	load.xferSnapshot(ai);
	load.close();
	std::remove(file.str());
	std::fprintf(stderr, "TEST builder snapshot %u restored task=%d target=%u\n", frame,
		static_cast<int>(dozer->getCurrentTask()), static_cast<unsigned>(dozer->getTaskTarget(DOZER_TASK_BUILD)));
}

void roundTripExperience(Object* obj, unsigned int frame)
{
	const char* directory = std::getenv("XDG_RUNTIME_DIR");
	if (!directory)
		return;
	AsciiString file;
	file.format("%s/experience-snapshot", directory);
	ExperienceTracker* tracker = obj->getExperienceTracker();
	XferSave save;
	save.open(file);
	save.xferSnapshot(tracker);
	save.close();

	ExperienceTracker* restored = newInstance(ExperienceTracker)(obj);
	MemoryPoolObjectHolder hold(restored);
	XferLoad load;
	load.setOptions(XO_NO_POST_PROCESSING);
	load.open(file);
	load.xferSnapshot(restored);
	load.close();
	std::remove(file.str());
	const Bool same = tracker->getRetailExperience() == restored->getRetailExperience() &&
		tracker->getRetailThreshold() == restored->getRetailThreshold() &&
		tracker->getRetailStartingRank() == restored->getRetailStartingRank() && tracker->getRank() == restored->getRank();
	std::fprintf(stderr, "TEST xp-snapshot frame=%u rank=%d xp=%.3f exact=%d\n", frame, restored->getRank(),
		restored->getRetailExperience(), same);
}

void loadHordeFixture(Object* destination, Object* source, unsigned int frame)
{
	const char* directory = std::getenv("XDG_RUNTIME_DIR");
	auto* restored = const_cast<HordeContain*>(HordeContain::get(destination));
	auto* contain = const_cast<HordeContain*>(HordeContain::get(source));
	if (!directory || !restored || restored->getContainCount() || (source && (!contain || contain == restored)))
		return;

	AsciiString file;
	file.format("%s/horde-fixture", directory);
	XferSave save;
	save.open(file);
#if !RTS_GENERALS
	AttributeModifierHelper* modifier = contain ? destination->getAttributeModifierHelper() : nullptr;
	if (modifier)
	{
		const auto owner = AttributeModifiers::ownerOf(destination, restored->getModuleTagNameKey());
		for (const AsciiString& name : contain->getFormationModifiers())
			AttributeModifiers::apply(destination, name, 0, 0, owner);
	}
#endif
	if (contain)
		save.xferSnapshot(contain);
	else
	{
		for (Int i = 0; i < 3; ++i)
		{
			XferVersion version = 1;
			save.xferVersion(&version, 1);
		}
	}
#if !RTS_GENERALS
	if (modifier)
		save.xferSnapshot(modifier);
#endif
	save.close();

	if (contain)
		while (!contain->getContainedItemsList()->empty())
			contain->removeFromContain(contain->getContainedItemsList()->front());

	const Int updateIndex = restored->friend_getIndexInLogic();
	const UnsignedInt wakeFrame = restored->friend_getNextCallFrame();
#if !RTS_GENERALS
	const Int modifierIndex = modifier ? modifier->friend_getIndexInLogic() : -1;
	const UnsignedInt modifierWake = modifier ? modifier->friend_getNextCallFrame() : FOREVER;
#endif
	XferLoad load;
	load.setOptions(XO_NO_POST_PROCESSING);
	load.open(file);
	load.xferSnapshot(restored);
#if !RTS_GENERALS
	if (modifier)
	{
		modifier->clearAll();
		load.xferSnapshot(modifier);
	}
	const Real savedArmor = AttributeModifiers::damageTakenFactor(destination, DAMAGE_SLASH);
#endif
	load.close();
	restored->friend_setIndexInLogic(updateIndex);
	restored->friend_setNextCallFrame(wakeFrame);
	RetailHordeSnapshotFixture::loadPostProcess(restored);
	std::remove(file.str());
	std::fprintf(stderr, "TEST nh-load frame=%u legacy=%d members=%u alternate=%d\n",
		frame, contain == nullptr, restored->getContainCount(), restored->isAlternateFormation());
#if !RTS_GENERALS
	if (modifier)
	{
		const UnsignedInt restoredWake = modifier->friend_getNextCallFrame();
		modifier->friend_setIndexInLogic(modifierIndex);
		modifier->friend_setNextCallFrame(modifierWake);
		modifier->sleepUntil(restoredWake);
		std::fprintf(stderr, "TEST nh-load-armor frame=%u saved=%.3f recovered=%.3f\n", frame, savedArmor,
			AttributeModifiers::damageTakenFactor(destination, DAMAGE_SLASH));
	}
#endif
}

void roundTripHeroRoster(const Player* player, unsigned int frame)
{
	const char* directory = std::getenv("XDG_RUNTIME_DIR");
	if (!directory)
		return;

	AsciiString file;
	file.format("%s/hero-roster-snapshot", directory);
	XferSave save;
	save.open(file);
	HeroRoster::xferPlayer(player, &save);
	ExperienceLevelStore::xferPendingLevels(&save);
	save.close();

	HeroRoster::reset(player->getPlayerIndex());
	ExperienceLevelStore::reset();
	XferLoad load;
	load.open(file);
	HeroRoster::xferPlayer(player, &load);
	ExperienceLevelStore::xferPendingLevels(&load);
	load.close();
	std::remove(file.str());
	std::fprintf(stderr, "TEST xp-roster-snapshot frame=%u\n", frame);
}

#if !RTS_GENERALS
void roundTripModifiers(Object* obj, Object* destination, const AsciiString& name, unsigned int frame)
{
	const char* directory = std::getenv("XDG_RUNTIME_DIR");
	auto* helper = obj->getAttributeModifierHelper();
	auto* restored = destination ? destination->getAttributeModifierHelper() : nullptr;
	const Int list = AttributeModifiers::findList(name);
	if (!directory || !helper || !restored || helper == restored || list < 0)
		return;

	using namespace AttributeModifierRules;
	const Owner first = AttributeModifiers::ownerOf(obj, TheNameKeyGenerator->nameToKey("ModuleTag_TestFirst"));
	const Owner second = AttributeModifiers::ownerOf(obj, TheNameKeyGenerator->nameToKey("ModuleTag_TestSecond"));
	const UnsignedInt mask = categoryBit(LEADERSHIP);
	helper->apply(list, 80, mask, first);
	helper->apply(list, 100, 0, second);

	AsciiString originalFile, restoredFile;
	originalFile.format("%s/modifier-snapshot", directory);
	restoredFile.format("%s/modifier-snapshot-restored", directory);
	XferSave save;
	save.open(originalFile);
	save.xferSnapshot(helper);
	save.close();

	const Int updateIndex = restored->friend_getIndexInLogic();
	const UnsignedInt oldWake = restored->friend_getNextCallFrame();
	XferLoad load;
	load.setOptions(XO_NO_POST_PROCESSING);
	load.open(originalFile);
	load.xferSnapshot(restored);
	load.close();

	save.open(restoredFile);
	save.xferSnapshot(restored);
	save.close();
	std::ifstream original(originalFile.str(), std::ios::binary);
	std::ifstream copy(restoredFile.str(), std::ios::binary);
	std::ostringstream originalBytes, restoredBytes;
	originalBytes << original.rdbuf();
	restoredBytes << copy.rdbuf();
	const bool exact = originalBytes.tellp() > 0 && originalBytes.str() == restoredBytes.str();
	std::remove(originalFile.str());
	std::remove(restoredFile.str());
	const UnsignedInt wake = restored->friend_getNextCallFrame();
	restored->friend_setIndexInLogic(updateIndex);
	restored->friend_setNextCallFrame(oldWake);
	restored->sleepUntil(wake);
	destination->getBodyModule()->setMaxHealth(obj->getBodyModule()->getMaxHealth(), SAME_CURRENTHEALTH);

	const auto& active = restored->getStack().active();
	const bool holdsRestored = active.size() == 1 && active.front().list == list &&
		active.front().endFrame == frame + 100 && active.front().antiMask == mask && active.front().holds.size() == 2 &&
		active.front().holds[0].owner == first && active.front().holds[0].endFrame == frame + 80 &&
		active.front().holds[1].owner == second && active.front().holds[1].endFrame == frame + 100;
	std::fprintf(stderr, "TEST modifier-snapshot frame=%u exact=%d holds=%d wake=%u maxBonus=%.1f damage=%.3f\n",
		frame, exact, holdsRestored, wake, restored->getTotals().value[HEALTH], restored->getTotals().damageDealtFactor());
}

void roundTripAbility(Object* obj, const SpecialPowerTemplate* power, unsigned int frame)
{
	const char* directory = std::getenv("XDG_RUNTIME_DIR");
	if (!directory || !power)
		return;
	SpecialAbilityUpdate* ability = obj->findSpecialAbilityUpdate(power->getSpecialPowerType());
	if (!ability)
		return;
	AsciiString file;
	file.format("%s/ability-snapshot", directory);
	XferSave save;
	save.open(file);
	save.xferSnapshot(ability);
	save.close();
	const Bool active = ability->isActive();
	XferLoad load;
	load.setOptions(XO_NO_POST_PROCESSING);
	load.open(file);
	load.xferSnapshot(ability);
	load.close();
	std::remove(file.str());
	std::fprintf(stderr, "TEST ability snapshot frame=%u power=%s active=%d restored=%d\n",
		frame, power->getName().str(), active, ability->isActive());
}

void checkMountPower(Object* obj, const SpecialPowerTemplate* power, unsigned int frame)
{
	if (!power)
		return;
	SpecialPowerModuleInterface* module = obj->getSpecialPowerModule(power);
	if (!module)
		return;
	const Bool allowed = TheActionManager->canDoSpecialPower(obj, power, CMD_FROM_PLAYER, 0);
	const Bool mount = obj->testWeaponSetFlag(WEAPONSET_MOUNTED);
	const Bool ready = module->isReady();
	const UnsignedInt readyFrame = module->getReadyFrame();
	if (!allowed)
		obj->doSpecialPower(power, 0, FALSE);
	std::fprintf(stderr, "TEST mount gate frame=%u power=%s mounted=%d ready=%d update=%d allowed=%d unchanged=%d\n",
		frame, power->getName().str(), mount, ready, obj->findSpecialAbilityUpdate(power->getSpecialPowerType()) != nullptr,
		allowed, readyFrame == module->getReadyFrame());
}
#endif

void logAllObjects(unsigned int frame)
{
	for (Object* obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject())
	{
		const Coord3D* pos = obj->getPosition();
		const Player* owner = obj->getControllingPlayer();
		std::fprintf(stderr, "TEST all %u %u %s %.4f %.4f %.4f %.6f %d %.4f %d\n", frame, static_cast<unsigned>(obj->getID()),
			obj->getTemplate()->getName().str(), pos->x, pos->y, pos->z, obj->getOrientation(),
			owner ? owner->getPlayerIndex() : -1, obj->getConstructionPercent(), obj->isEffectivelyDead() ? 1 : 0);
	}
}

void parseHordeTestWeapon(INI* ini)
{
	WeaponTemplate* weapon = const_cast<WeaponTemplate*>(TheWeaponStore->findWeaponTemplate(ini->getNextToken()));
	if (weapon)
		ini->initFromINI(weapon, weapon->getFieldParse());
}

void dumpAbilities(const Command& command, Object* obj, unsigned int frame)
{
	static const char* const conditions[] = { "MOUNTED", "UNPACKING", "PREPARING", "PACKING", "SPECIAL_WEAPON_ONE",
		"SPECIAL_WEAPON_TWO", "SPECIAL_WEAPON_THREE", "SPECIAL_WEAPON_FIVE", "PACKING_TYPE_1", "PACKING_TYPE_2", "USER_3" };
	std::string shown;
	if (const Drawable* draw = obj->getDrawable())
	{
		for (const char* name : conditions)
		{
			const Int bit = ModelConditionFlags::getSingleBitFromName(name);
			if (bit >= 0 && draw->getModelConditionFlags().test(bit))
				shown += std::string(shown.empty() ? "" : ",") + name;
		}
	}
	const AIUpdateInterface* ai = obj->getAI();
	const Locomotor* locomotor = ai ? ai->getCurLocomotor() : nullptr;
	const BodyModuleInterface* body = obj->getBodyModule();
	const Weapon* weapon = obj->getWeaponInWeaponSlot(PRIMARY_WEAPON);
	std::fprintf(stderr, "TEST ability %u %s %s id=%u mounted=%d locoset=%d locomotor=%s speed=%.1f weapon=%s cmdset=%s using=%d health=%.1f/%.1f pos=%.1f,%.1f cond=%s\n",
		frame, command.player.c_str(), command.target.c_str(), static_cast<unsigned>(obj->getID()),
		obj->testWeaponSetFlag(WEAPONSET_MOUNTED) ? 1 : 0, ai ? static_cast<int>(ai->getCurLocomotorSetType()) : -1,
		locomotor ? locomotor->getTemplateName().str() : "-", locomotor ? locomotor->getMaxSpeedForCondition(BODY_PRISTINE) : 0.0f,
		weapon ? weapon->getName().str() : "-", obj->getCommandSetString().str(),
		obj->testStatus(OBJECT_STATUS_IS_USING_ABILITY) ? 1 : 0, body ? body->getHealth() : 0.0f, body ? body->getMaxHealth() : 0.0f,
		obj->getPosition()->x, obj->getPosition()->y, shown.empty() ? "-" : shown.c_str());
	for (BehaviorModule** module = obj->getBehaviorModules(); *module; ++module)
	{
		SpecialPowerModuleInterface* power = (*module)->getSpecialPower();
		if (!power)
			continue;
		int automatic = -1;
#if !RTS_GENERALS
		if (const AutoAbilityBehavior* behavior = AutoAbilityBehavior::find(obj, power->getSpecialPowerTemplate()))
			automatic = behavior->isAutoActive() ? 1 : 0;
#endif
		std::fprintf(stderr, "TEST power %u %u %s ready=%d percent=%.3f readyFrame=%u auto=%d\n", frame,
			static_cast<unsigned>(obj->getID()), power->getPowerName().str(), power->isReady() ? 1 : 0, power->getPercentReady(),
			power->getReadyFrame(), automatic);
	}
}

void aimPendingCommand(const Command& command, Object* source, unsigned int frame)
{
	const CommandButton* button = TheInGameUI ? TheInGameUI->getGUICommand() : nullptr;
	if (!button || !button->getSpecialPowerTemplate())
	{
		std::fprintf(stderr, "TEST frame %u: aim without a pending special power\n", frame);
		return;
	}
	Object* target = findTarget(command);
	Coord3D point;
	if (target)
		point = *target->getPosition();
	else if (!readPoint(command, source, point))
	{
		std::fprintf(stderr, "TEST frame %u: aim %s has no target %s\n", frame, button->getName().str(), command.structure.c_str());
		return;
	}
	const Bool wantsObject = BitIsSet(button->getOptions(), COMMAND_OPTION_NEED_OBJECT_TARGET);
	if (wantsObject && (!target || !button->isValidObjectTarget(source, target)))
	{
		std::fprintf(stderr, "TEST frame %u: aim %s invalid object target %s\n", frame, button->getName().str(), command.structure.c_str());
		return;
	}
	GameMessage* msg = nullptr;
	if (wantsObject)
	{
		msg = TheMessageStream->appendMessage(GameMessage::MSG_DO_SPECIAL_POWER_AT_OBJECT);
		msg->appendIntegerArgument(button->getSpecialPowerTemplate()->getID());
		msg->appendObjectIDArgument(target->getID());
	}
	else
	{
		msg = TheMessageStream->appendMessage(GameMessage::MSG_DO_SPECIAL_POWER_AT_LOCATION);
		msg->appendIntegerArgument(button->getSpecialPowerTemplate()->getID());
		msg->appendLocationArgument(point);
#if !(RTS_GENERALS && RETAIL_COMPATIBLE_NETWORKING)
		msg->appendRealArgument(INVALID_ANGLE);
#endif
		msg->appendObjectIDArgument(target ? target->getID() : INVALID_ID);
	}
	msg->appendIntegerArgument(button->getOptions());
	msg->appendObjectIDArgument(INVALID_ID);
	TheInGameUI->setGUICommand(nullptr);
	std::fprintf(stderr, "TEST frame %u: aim %s at %s (%.1f, %.1f)\n", frame, button->getName().str(),
		target ? target->getTemplate()->getName().str() : "point", point.x, point.y);
}

void pressPowerButton(const Command& command, Object* obj, unsigned int frame)
{
	const CommandSet* set = TheControlBar ? TheControlBar->findCommandSet(obj->getCommandSetString()) : nullptr;
	const CommandButton* button = nullptr;
	for (Int i = 0; set && !button && i < CommandButtonRules::RETAIL_COMMANDS_PER_SET; ++i)
	{
		const CommandButton* candidate = set->getCommandButton(i);
		if (candidate && candidate->getSpecialPowerTemplate() &&
			stricmp(candidate->getSpecialPowerTemplate()->getName().str(), command.structure.c_str()) == 0)
			button = candidate;
	}
	if (!button)
	{
		std::fprintf(stderr, "TEST frame %u: power %s has no button\n", frame, command.structure.c_str());
		return;
	}
	selectObject(obj);
	std::fprintf(stderr, "TEST frame %u: power %s button %s\n", frame, command.structure.c_str(), button->getName().str());
	if (BitIsSet(button->getOptions(), COMMAND_OPTION_NEED_OBJECT_TARGET) || BitIsSet(button->getOptions(), NEED_TARGET_POS))
	{
		TheInGameUI->setGUICommand(button);
		return;
	}
	GameMessage* msg = TheMessageStream->appendMessage(GameMessage::MSG_DO_SPECIAL_POWER);
	msg->appendIntegerArgument(button->getSpecialPowerTemplate()->getID());
	msg->appendIntegerArgument(button->getOptions());
	msg->appendObjectIDArgument(INVALID_ID);
}

struct SweepCase
{
	ObjectID source = INVALID_ID;
	ObjectID result = INVALID_ID;
	ObjectID previousObject = INVALID_ID;
	std::vector<ObjectID> castTargets;
	std::vector<Real> health;
	std::vector<PlayerIndex> castOwners;
	const ThingTemplate* thing = nullptr;
	const UpgradeTemplate* upgrade = nullptr;
	const SpecialPowerTemplate* power = nullptr;
	UnsignedInt readyFrame = 0;
	UnsignedInt particles = 0;
	Bool plot = FALSE;
	Bool synchronousEffect = FALSE;
	Coord3D beforeMove = {};
	Bool moved = FALSE;
	std::vector<std::pair<ObjectID, NameKeyType>> pendingUpgrades;
};

std::map<Int, SweepCase> s_sweepCases;

void sweepButtonRequirements(Int id, const CommandButton* button, Object* source)
{
	if (!button)
		return;
	const Player* player = source->getControllingPlayer();
	std::fprintf(stderr, "ORDERS %d button=%s type=%d rank=%d requiredRank=%d disabled=%d neededAny=%d neededMet=%d\n",
		id, button->getName().str(), static_cast<Int>(button->getCommandType()), source->getExperienceTracker()->getRank(),
		button->getRequiredRank(), source->isDisabled(), button->isNeededUpgradeAny(),
		HeroRoster::buttonUpgradesMet(button, player, source));
	for (const UpgradeTemplate* upgrade : button->getNeededUpgrades())
	{
		const Bool playerUpgrade = upgrade->getUpgradeType() == UPGRADE_TYPE_PLAYER;
		const Bool owned = playerUpgrade ? player->hasUpgradeComplete(upgrade) : source->hasUpgrade(upgrade);
		std::fprintf(stderr, "ORDERS %d needed=%s scope=%s owned=%d\n", id,
			upgrade->getUpgradeName().str(), playerUpgrade ? "player" : "object", owned);
	}
	for (ScienceType science : button->getScienceVec())
		std::fprintf(stderr, "ORDERS %d science=%s owned=%d\n", id,
			TheScienceStore->getInternalNameForScience(science).str(), player->hasScience(science));
}

void sweepUpgradeRejection(Int id, Object* source, ProductionUpdateInterface* production,
	const UpgradeTemplate* upgrade, const CommandButton* button)
{
	Player* player = source->getControllingPlayer();
	const Bool playerUpgrade = upgrade->getUpgradeType() == UPGRADE_TYPE_PLAYER;
	const Bool affordable = TheUpgradeCenter->canAffordUpgrade(player, upgrade);
	const Bool owned = source->hasUpgrade(upgrade);
	const Bool affected = playerUpgrade || source->affectedByUpgrade(upgrade);
	const Bool queuedHere = production->isUpgradeInQueue(upgrade);
	const Bool offered = source->canProduceUpgrade(upgrade);
	const char* reason = playerUpgrade && !affordable ? "money"
		: !playerUpgrade && owned ? "object_complete"
		: !affected ? "no_upgrade_effect"
		: queuedHere ? "duplicate_queue"
		: !offered ? "upgrade_offer"
		: playerUpgrade && player->hasUpgradeComplete(upgrade) ? "player_complete"
		: playerUpgrade && player->hasUpgradeInProduction(upgrade) ? "player_in_production"
		: production->canQueueUpgrade(upgrade) == CANMAKE_QUEUE_FULL ? "queue_full" : "unexplained";
	std::fprintf(stderr, "ORDERS %d reject=%s affected=%d offered=%d affordable=%d\n", id, reason, affected, offered, affordable);
	sweepButtonRequirements(id, button, source);
}

void sweepUnitRejection(Int id, Object* source, const ThingTemplate* thing, const CommandButton* button)
{
	const Player* player = source->getControllingPlayer();
	const Int heroIndex = HeroRoster::findIndex(player, thing);
	const CommandSet* set = TheControlBar->findCommandSet(source->getCommandSetString());
	const Bool reviveOffer = HeroRoster::isReviveTemplateInSet(set, player, source, thing);
	std::fprintf(stderr, "ORDERS %d make=%d allowed=%d buildable=%d canBuild=%d limit=%d points=%d hero=%d heroAlive=%d heroQueued=%d heroState=%d reviveOffer=%d\n",
		id, static_cast<Int>(TheBuildAssistant->canMakeUnit(source, thing)), player->allowedToBuild(thing),
		static_cast<Int>(thing->getBuildable()), player->canBuild(thing), thing->getMaxSimultaneousOfType(),
		player->hasEnoughCommandPoints(thing), heroIndex, heroIndex >= 0 && HeroRoster::isAlive(player, thing),
		heroIndex >= 0 && HeroRoster::isQueued(player, thing),
		heroIndex >= 0 ? static_cast<Int>(HeroRoster::stateAt(player, heroIndex)) : -1, reviveOffer);
	for (Int i = 0; set && heroIndex >= 0 && i < CommandButtonRules::RETAIL_COMMANDS_PER_SET; ++i)
	{
		if (HeroRoster::reviveSlotForEntry(set, i) == heroIndex)
		{
			button = set->getCommandButton(i);
			break;
		}
	}
	sweepButtonRequirements(id, button, source);
}

ObjectID newestObject()
{
	ObjectID newest = INVALID_ID;
	for (const Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
		newest = std::max(newest, object->getID());
	return newest;
}

void sweepUpgradeCandidates(SweepCase& test, Object* source, const Player* player)
{
	const bool horde = HordeContain::get(source) != nullptr;
	for (Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
	{
		if (object->getControllingPlayer() != player || object->isEffectivelyDead())
			continue;
		if (test.upgrade->getUpgradeType() == UPGRADE_TYPE_OBJECT && object != source &&
			(!horde || object->getContainedBy() != source))
			continue;
		UpgradeMaskType current = object->getObjectCompletedUpgradeMask();
		current.set(player->getCompletedUpgradeMask());
		UpgradeMaskType predicted = current;
		predicted.set(test.upgrade->getUpgradeMask());
		for (BehaviorModule** module = object->getBehaviorModules(); *module; ++module)
		{
			const UpgradeModuleInterface* upgrade = (*module)->getUpgrade();
			if (upgrade && !upgrade->wouldUpgrade(current) && upgrade->wouldUpgrade(predicted))
				test.pendingUpgrades.emplace_back(object->getID(), (*module)->getModuleTagNameKey());
		}
	}
}

Int sweepActivatedUpgrades(const SweepCase& test)
{
	Int count = 0;
	for (const auto& [id, tag] : test.pendingUpgrades)
	{
		Object* object = TheGameLogic->findObjectByID(id);
		for (BehaviorModule** module = object ? object->getBehaviorModules() : nullptr; module && *module; ++module)
		{
			const UpgradeModuleInterface* upgrade = (*module)->getUpgrade();
			if ((*module)->getModuleTagNameKey() == tag && upgrade && upgrade->isAlreadyUpgraded())
				++count;
		}
	}
	return count;
}

Bool hasSweepModel(Object* object)
{
	const Drawable* draw = object->getDrawable();
	if (!draw || draw->isDrawableEffectivelyHidden())
		return FALSE;
	Coord3D position;
	Real radius;
	Matrix3D transform;
	for (const DrawModule** module = draw->getDrawModules(); module && *module; ++module)
	{
		const ObjectDrawInterface* drawInterface = (*module)->getObjectDrawInterface();
		if (drawInterface && drawInterface->clientOnly_getRenderObjInfo(&position, &radius, &transform) && radius > 0.0f)
			return TRUE;
	}
	return FALSE;
}

const CommandButton* sweepButton(Object* source, const char* name)
{
	if (!TheControlBar || !source)
		return nullptr;
	const CommandSet* set = TheControlBar->findCommandSet(source->getCommandSetString());
	for (Int slot = 0; set && slot < CommandButtonRules::RETAIL_COMMANDS_PER_SET; ++slot)
	{
		const CommandButton* button = set->getCommandButton(slot);
		if (button && stricmp(button->getName().str(), name) == 0)
			return button;
	}
	return nullptr;
}

Object* sweepSource(const Command& command, const Player* player, Bool needButton)
{
	Object* fallback = nullptr;
	Object* matching = nullptr;
	for (Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
	{
		if (object->getControllingPlayer() != player || object->isEffectivelyDead() ||
			object->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION) ||
			stricmp(object->getTemplate()->getName().str(), command.target.c_str()) != 0)
			continue;
		if (!fallback || object->getID() < fallback->getID())
			fallback = object;
		if (!needButton || !sweepButton(object, command.structure.c_str()))
			continue;
		if (object->getDrawable() && object->getDrawable()->isDrawableEffectivelyHidden())
			continue;
		if (!matching || object->getID() < matching->getID())
			matching = object;
	}
	return matching ? matching : fallback;
}

Bool sweepPlace(Object* builder, const ThingTemplate* thing, Coord3D& point)
{
	const Coord3D origin = *builder->getPosition();
	const UnsignedInt flags = BuildAssistant::TERRAIN_RESTRICTIONS | BuildAssistant::CLEAR_PATH |
		BuildAssistant::NO_OBJECT_OVERLAP | BuildAssistant::SHROUD_REVEALED | BuildAssistant::IGNORE_HIDDEN_OBJECTS;
	Region3D extent;
	TheTerrainLogic->getExtent(&extent);
	for (Int range = 120; range <= 600; range += 120)
	{
		for (Int x = -range; x <= range; x += 120)
		{
			for (Int y = -range; y <= range; y += 120)
			{
				if (std::abs(x) != range && std::abs(y) != range)
					continue;
				point.x = origin.x + x;
				point.y = origin.y + y;
				if (point.x < extent.lo.x + 80 || point.y < extent.lo.y + 80 ||
					point.x > extent.hi.x - 80 || point.y > extent.hi.y - 80)
					continue;
				point.z = TheTerrainLogic->getGroundHeight(point.x, point.y);
				if (TheBuildAssistant->isLocationLegalToBuild(&point, thing, 0, flags, builder, nullptr) == LBC_OK)
					return TRUE;
			}
		}
	}
	return FALSE;
}

#if !RTS_GENERALS
const WeaponModeSpecialPowerUpdate* sweepWeaponMode(Object* source, const SpecialPowerTemplate* power)
{
	for (BehaviorModule** module = source->getBehaviorModules(); *module; ++module)
	{
		if ((*module)->getModuleNameKey() != NAMEKEY("WeaponModeSpecialPowerUpdate"))
			continue;
		const auto* mode = static_cast<const WeaponModeSpecialPowerUpdate*>(*module);
		if (mode->isModuleForPower(power))
			return mode;
	}
	return nullptr;
}

const char* sweepPassiveSpell(Object* source, const CommandButton* button, const Player* player, std::string& evidence)
{
	const auto* module = static_cast<const SpecialPowerModule*>(source->getSpecialPowerModule(button->getSpecialPowerTemplate()));
	if (!module || module->getModuleNameKey() != NAMEKEY("PlayerUpgradeSpecialPower"))
		return "passive_upgrade_unbound";
	const auto& names = static_cast<const PlayerUpgradeSpecialPower*>(module)->getGrantedUpgradeNames();
	Bool granted = !names.empty();
	for (const AsciiString& name : names)
	{
		const UpgradeTemplate* upgrade = TheUpgradeCenter->findUpgrade(name);
		if (!upgrade || !player->hasUpgradeComplete(upgrade))
			granted = FALSE;
		else
		{
			if (!evidence.empty())
				evidence += ',';
			evidence += name.str();
		}
	}
	return granted ? "passive_upgrade_granted" : "passive_upgrade_missing";
}
#endif

Bool sweepCast(Object* source, const CommandButton* button, const Command& command, Bool spell, SweepCase& test)
{
	const SpecialPowerTemplate* power = button ? button->getSpecialPowerTemplate() : nullptr;
	if (!power || !source->getSpecialPowerModule(power))
		return FALSE;
	const UnsignedInt options = button->getOptions();
	test.power = power;
	test.readyFrame = source->getSpecialPowerModule(power)->getReadyFrame();
	test.particles = TheParticleSystemManager ? TheParticleSystemManager->getParticleSystemCount() : 0;
	Object* target = nullptr;
	Real best = 0.0f;
	Coord3D origin = *source->getPosition();
	if (spell || command.hasLocation)
	{
		origin = command.hasLocation ? command.location : Coord3D{1140.0f, 580.0f, 0.0f};
		origin.z = TheTerrainLogic->getGroundHeight(origin.x, origin.y);
	}
	for (Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
	{
		if (object->isEffectivelyDead() || !object->getBodyModule() ||
			object->getBodyModule()->getMaxHealth() <= 0.0f)
			continue;
		const Coord3D point = *object->getPosition();
		const Real dx = point.x - origin.x;
		const Real dy = point.y - origin.y;
		const Real distance = dx * dx + dy * dy;
		if (distance <= 400.0f * 400.0f)
		{
			test.castTargets.push_back(object->getID());
			test.health.push_back(object->getBodyModule()->getHealth());
			test.castOwners.push_back(object->getControllingPlayer()->getPlayerIndex());
		}
		if (BitIsSet(options, COMMAND_OPTION_NEED_OBJECT_TARGET) &&
			distance <= 450.0f * 450.0f && (!target || distance < best) &&
			((source->getRelationship(object) == ENEMIES && BitIsSet(options, NEED_TARGET_ENEMY_OBJECT)) ||
			 (source->getRelationship(object) == ALLIES && BitIsSet(options, NEED_TARGET_ALLY_OBJECT)) ||
			 (source->getRelationship(object) == NEUTRAL && BitIsSet(options, NEED_TARGET_NEUTRAL_OBJECT))) &&
			(spell ? button->canCastRetailSpell(source, object, &point) :
				TheActionManager->canDoSpecialPowerAtObject(source, object, CMD_FROM_PLAYER, power, options)))
		{
			target = object;
			best = distance;
		}
	}
	if (BitIsSet(options, COMMAND_OPTION_NEED_OBJECT_TARGET))
	{
		if (!target)
			return FALSE;
		source->doSpecialPowerAtObject(power, target, options);
	}
	else if (BitIsSet(options, NEED_TARGET_POS))
	{
		Coord3D point = origin;
		if (!spell && !command.hasLocation)
		{
			point.x += 30.0f;
			point.z = TheTerrainLogic->getGroundHeight(point.x, point.y);
		}
		const auto canCastAt = [&](const Coord3D& destination) {
			return spell ? button->canCastRetailSpell(source, nullptr, &destination) :
				TheActionManager->canDoSpecialPowerAtLocation(source, &destination, CMD_FROM_PLAYER, power, nullptr, options);
		};
		if (!canCastAt(point))
		{
			if (spell || command.hasLocation || !power->hasForbiddenRetailObjects(source, &point))
				return FALSE;
			constexpr Int directions[][2] = {{0, 1}, {-1, 0}, {0, -1}, {1, 0}, {1, 1}, {-1, 1}, {-1, -1}, {1, -1}};
			Bool found = FALSE;
			for (Int offset = 30; offset <= 120 && !found; offset += 30)
			{
				for (const auto& direction : directions)
				{
					Coord3D candidate = origin;
					candidate.x += static_cast<Real>(offset * direction[0]);
					candidate.y += static_cast<Real>(offset * direction[1]);
					candidate.z = TheTerrainLogic->getGroundHeight(candidate.x, candidate.y);
					if (!canCastAt(candidate))
						continue;
					point = candidate;
					found = TRUE;
					break;
				}
			}
			if (!found)
				return FALSE;
			std::fprintf(stderr, "SWEEP retarget %d frame=%u x=%.1f y=%.1f\n",
				command.ordinal, TheGameLogic->getFrame(), point.x, point.y);
		}
		source->doSpecialPowerAtLocation(power, &point, INVALID_ANGLE, options);
	}
	else
	{
		if (!(spell ? button->canCastRetailSpell(source, nullptr, &origin) :
			TheActionManager->canDoSpecialPower(source, power, CMD_FROM_PLAYER, options)))
			return FALSE;
		source->doSpecialPower(power, options);
	}
	return TRUE;
}

void sweepStart(const Command& command, Player* player, unsigned int frame)
{
	const Int id = command.ordinal;
	SweepCase& test = s_sweepCases[id];
	const bool hero = command.action == "sweep-hero";
	const bool spell = command.action == "sweep-spell";
	const bool buttonRequired = !hero;
	Object* source = spell ? player->getRetailSpellBook() : sweepSource(command, player, buttonRequired);
	if (!source && command.action == "sweep-power" && command.hasLocation)
		source = player->getRetailSpellBook();
	if (!source || stricmp(source->getTemplate()->getName().str(), command.target.c_str()) != 0)
	{
		std::fprintf(stderr, "SWEEP %d start frame=%u reason=no_source\n", id, frame);
		return;
	}
	test.source = source->getID();
	test.previousObject = newestObject();
	const CommandButton* button = buttonRequired ? sweepButton(source, command.structure.c_str()) : nullptr;
	if (buttonRequired && !button)
	{
		const Bool powerCase = command.action == "sweep-power" || spell;
		const CommandButton* catalogButton = powerCase && TheControlBar ? TheControlBar->findCommandButton(command.structure.c_str()) : nullptr;
		const Bool passive = !spell && catalogButton && catalogButton->hasRotwkOption(CommandButtonRules::ROTWK_NONPRESSABLE);
		std::fprintf(stderr, "SWEEP %d start frame=%u reason=%s set=%s\n", id, frame,
			passive ? "passive_display" : "no_button", source->getCommandSetString().str());
		return;
	}
	ProductionUpdateInterface* production = source->getProductionUpdateInterface();
	Bool issued = FALSE;
	const char* reason = "rejected";
	Int eligibility = -1;
	if (command.action == "sweep-power" || spell)
	{
		if (button->hasRotwkOption(CommandButtonRules::ROTWK_NONPRESSABLE))
		{
			const char* disposition = spell ? "passive_upgrade_unbound" : "passive_display";
			std::string upgrades;
#if !RTS_GENERALS
			if (spell)
				disposition = sweepPassiveSpell(source, button, player, upgrades);
#endif
			std::fprintf(stderr, "SWEEP %d start frame=%u reason=%s set=%s upgrades=%s\n",
				id, frame, disposition, source->getCommandSetString().str(), upgrades.empty() ? "-" : upgrades.c_str());
			return;
		}
#if !RTS_GENERALS
		const auto* mode = sweepWeaponMode(source, button->getSpecialPowerTemplate());
		const WeaponSlotType slot = mode ? mode->getModeWeaponSlot() : WEAPONSLOT_COUNT;
		const Bool checksSlot = slot >= PRIMARY_WEAPON && slot < WEAPONSLOT_COUNT;
		std::vector<std::pair<ObjectID, WeaponSlotType>> beforeWeapons;
		if (checksSlot)
		{
			auto recordWeapon = [&](const Object* object)
			{
				WeaponSlotType before = WEAPONSLOT_COUNT;
				object->getCurrentWeapon(&before);
				beforeWeapons.emplace_back(object->getID(), before);
			};
			recordWeapon(source);
			if (const HordeContain* horde = HordeContain::get(source))
				for (const Object* member : *horde->getContainedItemsList())
					if (!member->isEffectivelyDead())
						recordWeapon(member);
		}
#endif
		issued = sweepCast(source, button, command, spell, test);
		if (issued)
		{
			test.synchronousEffect = newestObject() > test.previousObject ||
				(TheParticleSystemManager && TheParticleSystemManager->getParticleSystemCount() > test.particles);
			for (std::size_t i = 0; !test.synchronousEffect && i < test.castTargets.size(); ++i)
			{
				const Object* target = TheGameLogic->findObjectByID(test.castTargets[i]);
				test.synchronousEffect = !target || target->getControllingPlayer()->getPlayerIndex() != test.castOwners[i] ||
					(target->getBodyModule() && target->getBodyModule()->getHealth() != test.health[i]);
			}
#if !RTS_GENERALS
			if (checksSlot)
			{
				Int eligible = 0, selected = 0, switched = 0;
				const char* weaponName = "-";
				const Bool sourceHasSlot = source->getWeaponInWeaponSlot(slot) != nullptr;
				for (const auto& [objectID, before] : beforeWeapons)
				{
					const Object* object = TheGameLogic->findObjectByID(objectID);
					if (!object || (sourceHasSlot && object != source) || !object->getWeaponInWeaponSlot(slot))
						continue;
					++eligible;
					WeaponSlotType current = WEAPONSLOT_COUNT;
					const Weapon* weapon = object->getCurrentWeapon(&current);
					if (weapon && current == slot && object->isCurWeaponLocked())
					{
						++selected;
						switched += before != current;
						weaponName = weapon->getName().str();
					}
				}
				std::fprintf(stderr, "SWEEP %d weapon frame=%u slot=%d eligible=%d selected=%d switched=%d weapon=%s\n",
					id, frame, static_cast<Int>(slot), eligible, selected, switched, weaponName);
				test.synchronousEffect = switched > 0 && selected == eligible;
				if (!test.synchronousEffect)
				{
					issued = FALSE;
					reason = "weapon_mode_not_switched";
				}
			}
			else if (mode && mode->isActive())
				test.synchronousEffect = TRUE;
#endif
		}
		if (!test.power)
		{
			const SpecialPowerTemplate* power = button->getSpecialPowerTemplate();
			if (power && power->getSpecialPowerType() == SPECIAL_INFANTRY_CAPTURE_BUILDING)
				reason = "capture_unbound";
			else if (spell && button->getCommandType() != GUI_COMMAND_SPELL_BOOK)
				reason = "not_a_spellbook_cast";
			else
				reason = "no_power_module";
		}
		else if (!issued && spell && button->getCommandType() != GUI_COMMAND_SPELL_BOOK)
			reason = "not_a_spellbook_cast";
		else if (!issued && reason == std::string_view("rejected") && !source->getSpecialPowerModule(test.power)->isReady())
			reason = "power_not_ready";
	}
	else if (command.action == "sweep-upgrade")
	{
		test.upgrade = button->getUpgradeTemplate();
		if (test.upgrade && production)
		{
			sweepUpgradeCandidates(test, source, player);
			eligibility = production->canQueueUpgrade(test.upgrade);
			issued = production->queueUpgrade(test.upgrade);
			if (!issued && std::getenv("GENERALSX_ORDER_DIAGNOSTICS"))
				sweepUpgradeRejection(id, source, production, test.upgrade, button);
			if (!issued && (source->hasUpgrade(test.upgrade) || player->hasUpgradeComplete(test.upgrade)))
				reason = "already_complete";
		}
		else
			reason = test.upgrade ? "no_production" : "no_upgrade_template";
	}
	else
	{
		test.thing = hero ? TheThingFactory->findTemplate(command.structure.c_str(), FALSE) : static_cast<const ThingTemplate*>(button->getThingTemplate());
		if (!test.thing)
			reason = "no_object_template";
		else if (command.action == "sweep-plot")
		{
			test.plot = TRUE;
			selectGroup(source);
			TheMessageStream->appendMessage(GameMessage::MSG_FOUNDATION_CONSTRUCT)->appendIntegerArgument(test.thing->getTemplateID());
			issued = TRUE;
		}
		else if (command.action == "sweep-build")
		{
			eligibility = TheBuildAssistant->canMakeUnit(source, test.thing);
			Coord3D point;
			if (sweepPlace(source, test.thing, point))
			{
				selectGroup(source);
				GameMessage* order = TheMessageStream->appendMessage(GameMessage::MSG_DOZER_CONSTRUCT);
				order->appendIntegerArgument(test.thing->getTemplateID());
				order->appendLocationArgument(point);
				order->appendRealArgument(0.0f);
				std::fprintf(stderr, "SWEEP site %d frame=%u x=%.1f y=%.1f\n", id, frame, point.x, point.y);
				issued = TRUE;
			}
			else
				reason = "no_legal_site";
		}
		else if (production)
		{
			eligibility = production->canQueueCreateUnit(test.thing);
			issued = production->queueCreateUnit(test.thing, production->requestUniqueUnitID());
			if (!issued && std::getenv("GENERALSX_ORDER_DIAGNOSTICS"))
				sweepUnitRejection(id, source, test.thing, button);
		}
		else
			reason = "no_production";
	}
	std::fprintf(stderr, "SWEEP %d start frame=%u reason=%s eligibility=%d source=%u set=%s\n", id, frame,
		issued ? "issued" : reason, eligibility, static_cast<unsigned>(test.source), source->getCommandSetString().str());
}

void sweepObserve(const Command& command, Player* player, unsigned int frame)
{
	const Int id = command.ordinal;
	auto found = s_sweepCases.find(id);
	if (found == s_sweepCases.end())
		return;
	SweepCase& test = found->second;
	Object* source = TheGameLogic->findObjectByID(test.source);
	Object* result = TheGameLogic->findObjectByID(test.result);
	if (test.result == INVALID_ID && test.thing)
	{
		for (Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
		{
			if (object->getControllingPlayer() == player && object->getID() > test.previousObject &&
				object->getTemplate() == test.thing && (!result || object->getID() < result->getID()))
				result = object;
		}
		if (result)
			test.result = result->getID();
	}
	if (command.action == "sweep-move" && result && !result->isKindOf(KINDOF_STRUCTURE))
	{
		const AIUpdateInterface* ai = result->getAI();
		test.beforeMove = *result->getPosition();
		test.moved = ai && ai->getCurLocomotor();
		if (test.moved)
		{
			Coord3D point = test.beforeMove;
			point.x += 45.0f;
			point.y += 25.0f;
			point.z = TheTerrainLogic->getGroundHeight(point.x, point.y);
			selectGroup(result);
			TheMessageStream->appendMessage(GameMessage::MSG_DO_MOVETO)->appendLocationArgument(point);
		}
	}
	else if (command.action == "sweep-kill" && result)
	{
		if (const HordeContain* horde = HordeContain::get(result))
		{
			std::vector<ObjectID> members;
			for (const Object* member : *horde->getContainedItemsList())
				members.push_back(member->getID());
			Int dead = 0;
			for (ObjectID member : members)
			{
				if (Object* object = TheGameLogic->findObjectByID(member))
				{
					if (object->getBodyModule())
						object->kill();
				}
				const Object* object = TheGameLogic->findObjectByID(member);
				if (!object || object->isEffectivelyDead())
					++dead;
			}
			std::fprintf(stderr, "SWEEP %d horde_kill frame=%u members=%zu dead_members=%d\n", id, frame, members.size(), dead);
		}
		else if (result->getBodyModule())
			result->kill();
		const Object* object = TheGameLogic->findObjectByID(test.result);
		const Bool dead = !object || object->isEffectivelyDead() || object->isDestroyed();
		std::fprintf(stderr, "SWEEP %d death frame=%u dead=%d\n", id, frame, dead);
	}
	else if (command.action == "sweep-death" && test.result != INVALID_ID)
	{
		const Bool dead = !result || result->isEffectivelyDead() || result->isDestroyed();
		std::fprintf(stderr, "SWEEP %d death frame=%u dead=%d\n", id, frame, dead);
	}
	else if (command.action == "sweep-check")
	{
		if (test.power && source)
		{
			auto* module = source->getSpecialPowerModule(test.power);
			Int damage = 0;
			Int healing = 0;
			for (std::size_t i = 0; i < test.castTargets.size(); ++i)
			{
				const Object* object = TheGameLogic->findObjectByID(test.castTargets[i]);
				if (!object || !object->getBodyModule() || object->getBodyModule()->getHealth() < test.health[i])
					++damage;
				else if (object->getBodyModule()->getHealth() > test.health[i])
					++healing;
			}
			const Int effects = TheParticleSystemManager ? static_cast<Int>(TheParticleSystemManager->getParticleSystemCount()) -
				static_cast<Int>(test.particles) : 0;
			std::fprintf(stderr, "SWEEP %d power frame=%u recharge=%d damage=%d healing=%d particles=%d newObjects=%d synchronousEffect=%d\n", id, frame,
				module && module->getReadyFrame() > test.readyFrame, damage, healing, effects,
				newestObject() > test.previousObject, test.synchronousEffect);
		}
		else if (test.upgrade)
		{
			std::fprintf(stderr, "SWEEP %d upgrade frame=%u complete=%d modules=%d candidates=%zu\n", id, frame,
				(source && source->hasUpgrade(test.upgrade)) || player->hasUpgradeComplete(test.upgrade),
				sweepActivatedUpgrades(test), test.pendingUpgrades.size());
			if (source && command.structure == "mount-command")
			{
				const auto* power = TheSpecialPowerStore->findSpecialPowerTemplate("SpecialAbilityToggleMountedTwo");
				const auto* module = power ? source->getSpecialPowerModule(power) : nullptr;
				std::fprintf(stderr, "ORDERS %d mountCommandReady=%u mounted=%d frame=%u\n", id,
					module ? module->getReadyFrame() : 0, source->testWeaponSetFlag(WEAPONSET_MOUNTED), frame);
			}
			else if (command.structure != "-")
			{
				for (const Object* replacement = TheGameLogic->getFirstObject(); replacement; replacement = replacement->getNextObject())
				{
					if (replacement->getID() <= test.previousObject || replacement->getControllingPlayer() != player ||
						stricmp(replacement->getTemplate()->getName().str(), command.structure.c_str()) != 0)
						continue;
					std::fprintf(stderr, "ORDERS %d replacement=%s oldPresent=%d frame=%u\n", id,
						command.structure.c_str(), source && !source->isDestroyed(), frame);
					break;
				}
			}
		}
		else if (result)
		{
			const HordeContain* horde = HordeContain::get(result);
			Bool model = hasSweepModel(result);
			Int count = -1;
			if (horde)
			{
				count = horde->getContainCount();
				model = count > 0;
				for (Object* member : *horde->getContainedItemsList())
					model = model && hasSweepModel(member);
			}
			const Coord3D& point = *result->getPosition();
			const Bool travelled = test.moved && (std::abs(point.x - test.beforeMove.x) > 2.0f ||
				std::abs(point.y - test.beforeMove.y) > 2.0f);
			const AIUpdateInterface* ai = result->getAI();
			const Locomotor* locomotor = ai ? ai->getCurLocomotor() : nullptr;
			const AsciiString set = result->getCommandSetString();
			std::fprintf(stderr, "SWEEP %d object frame=%u id=%u complete=%d model=%d set=%s setLoaded=%d members=%d locomotor=%s moved=%d\n",
				id, frame, static_cast<unsigned>(result->getID()), !result->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION),
				model, set.isEmpty() ? "-" : set.str(), TheControlBar && TheControlBar->findCommandSet(set), count,
				locomotor ? locomotor->getTemplateName().str() : "-", travelled);
			if (test.plot && !result->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION))
				TheGameLogic->destroyObject(result);
		}
		else if (test.thing)
		{
			const auto* production = source ? source->getProductionUpdateInterface() : nullptr;
			std::fprintf(stderr, "SWEEP %d missing frame=%u queued=%u\n", id, frame,
				production ? production->countUnitTypeInQueue(test.thing) : 0);
		}
	}
}

void sweepCommand(const Command& command, unsigned int frame)
{
	Player* player = findPlayer(command);
	if (!player)
		return;
	if (command.action == "sweep-setup")
	{
		player->getMoney()->deposit(2000000, FALSE, FALSE);
		TheWritableGlobalData->m_commandPoints.goodMP[0] = {100000, 100000};
		TheWritableGlobalData->m_commandPoints.evilMP[0] = {100000, 100000};
		TheFramePacer->enableFramesPerSecondLimit(FALSE);
		TheFramePacer->enableLogicTimeScale(FALSE);
		ThePartitionManager->revealMapForPlayerPermanently(player->getPlayerIndex());
	}
	else if (command.action == "sweep-fixture")
	{
		if (!sweepSource(command, player, FALSE))
		{
			createObject(command, frame);
			std::fprintf(stderr, "SWEEP fixture %s frame=%u\n", command.target.c_str(), frame);
		}
	}
	else if (command.action == "sweep-rank")
	{
		if (Object* object = sweepSource(command, player, FALSE))
			object->getExperienceTracker()->setMinRetailRank(10);
	}
	else if (command.action == "sweep-order-grant")
	{
		Object* object = sweepSource(command, player, FALSE);
		const UpgradeTemplate* upgrade = TheUpgradeCenter->findUpgrade(command.structure.c_str());
		if (object && upgrade && upgrade->getUpgradeType() == UPGRADE_TYPE_OBJECT && !object->hasUpgrade(upgrade))
		{
			object->giveUpgrade(upgrade);
			std::fprintf(stderr, "ORDERS fixture object=%s upgrade=%s frame=%u\n",
				command.target.c_str(), command.structure.c_str(), frame);
		}
	}
	else if (command.action == "sweep-order-remove")
	{
		if (Object* object = sweepSource(command, player, FALSE))
		{
			std::fprintf(stderr, "ORDERS fixture removed=%s frame=%u\n", command.target.c_str(), frame);
			TheGameLogic->destroyObject(object);
		}
	}
	else if (command.action == "sweep-check" || command.action == "sweep-move" ||
		command.action == "sweep-kill" || command.action == "sweep-death")
		sweepObserve(command, player, frame);
	else
		sweepStart(command, player, frame);
}

void dumpPassability(unsigned int frame)
{
	if (!TheAI || !TheAI->pathfinder())
		return;
	const ICoord2D* extent = TheAI->pathfinder()->getExtent();
	std::fprintf(stderr, "TEST passmap %u %d %d %.1f\n", frame, extent->x + 1, extent->y + 1, static_cast<Real>(PATHFIND_CELL_SIZE));
	std::string row;
	for (Int y = extent->y; y >= 0; --y)
	{
		row.clear();
		for (Int x = 0; x <= extent->x; ++x)
		{
			const PathfindCell* cell = TheAI->pathfinder()->getCell(LAYER_GROUND, x, y);
			row.push_back(cell ? static_cast<char>('0' + static_cast<int>(cell->getType())) : '?');
		}
		std::fprintf(stderr, "TEST passrow %d %s\n", y, row.c_str());
	}
}

void logTrackedObject(const Object* obj, const Object* anchor, unsigned int frame)
{
	const Coord3D* pos = obj->getPosition();
	const PhysicsBehavior* physics = obj->getPhysics();
	const AIUpdateInterface* ai = obj->getAI();
	Coord3D slot = *pos;
	if (const HordeContain* horde = anchor ? HordeContain::get(anchor) : nullptr)
		slot = horde->getMemberPosition(obj->getID(), FALSE);
	std::fprintf(stderr, "TEST pos %u %u %u %.9g %.9g %.4f %.4f %d %.3f %.3f %.2f\n", frame,
		static_cast<unsigned>(obj->getID()), static_cast<unsigned>(anchor ? anchor->getID() : INVALID_ID),
		pos->x, pos->y, obj->getOrientation(), physics ? physics->getVelocityMagnitude() : 0.0f,
		ai && ai->isMoving() ? 1 : 0, slot.x, slot.y, obj->getGeometryInfo().getBoundingCircleRadius());
}

void logTracked(unsigned int frame)
{
	for (ObjectID id : s_tracked)
	{
		const Object* obj = TheGameLogic->findObjectByID(id);
		if (!obj || obj->isEffectivelyDead())
			continue;
		logTrackedObject(obj, nullptr, frame);
		if (const HordeContain* horde = HordeContain::get(obj))
		{
			for (const Object* member : *horde->getContainedItemsList())
			{
				if (!member->isEffectivelyDead())
					logTrackedObject(member, obj, frame);
			}
		}
	}
	std::fflush(stderr);
}

void execute(const Command& command, unsigned int frame)
{
	if (command.action == "particle-fx" || command.action == "particle-systems" || command.action == "particle-water-fx" ||
		command.action == "particle-weather" || command.action == "particle-bone-sample")
	{
		const Coord3D location = command.hasLocation ? groundLocation(command) : Coord3D{};
		const auto separator = command.structure.find(',');
		const Object* target = nullptr;
		if (separator != std::string::npos)
		{
			Command targetCommand;
			targetCommand.structure = command.structure.substr(separator + 1);
			target = findTarget(targetCommand);
		}
		const std::string name = command.structure.substr(0, separator);
		runRetailParticleTestCommand(command.action, command.target != "-" ? findObject(command) : nullptr,
			target, command.hasLocation ? &location : nullptr, name.c_str(), frame);
		return;
	}
#if !RTS_GENERALS
	if (command.action == "particle-condition")
	{
		if (Object* object = findObject(command))
		{
			if (command.structure == "ring")
				object->setModelConditionState(MODELCONDITION_ONE_RING);
			else if (command.structure == "hero")
				object->setModelConditionState(MODELCONDITION_CREATE_A_HERO_02);
		}
		return;
	}
#endif
	if (command.action == "snapshot-partition-cells" || command.action == "snapshot-partition-scale")
	{
		runRetailSaveLoadPartitionTestCommand(command.action, findObject(command, true), command.structure.c_str(), frame);
		return;
	}
	if (command.action.starts_with("pkg23-"))
	{
		runRetailPackage23TestCommand(command.action, findPlayer(command), findObject(command, true),
			findTarget(command, true), command.structure.c_str(), command.location, frame);
		return;
	}
	if (command.action == "snapshot-path-state" || command.action == "snapshot-register-footprint" ||
		command.action == "snapshot-wall-state")
	{
		runRetailSaveLoadPathTestCommand(command.action, findObject(command, true), frame);
		return;
	}
	if (command.action == "snapshot-wall-reinforce" || command.action == "snapshot-wall-occupy" ||
		command.action == "snapshot-wall-occupant" || command.action == "snapshot-wall-delete")
	{
		const Coord3D position = command.hasLocation ? groundLocation(command) : Coord3D{};
		runRetailSaveLoadWallTestCommand(command.action, findPlayer(command), findObject(command, true),
			command.structure.c_str(), position, frame);
		return;
	}
	if (command.action == "snapshot-book-repair")
	{
		runRetailSaveLoadBookTestCommand(findPlayer(command), command.target.c_str());
		return;
	}
	if (command.action.starts_with("pw-") && runPowerCommand(command.action, findPlayer(command),
		command.action == "pw-rank" ? findObject(command) : nullptr, command.structure.c_str(), frame))
		return;
#if !RTS_GENERALS
	if (command.action.starts_with("pkg50-"))
	{
		runRetailPackage50TestCommand(command.action, findObject(command), command.action == "pkg50-bored" ? findTarget(command) : nullptr,
			command.target.c_str(), command.structure.c_str(), frame);
		return;
	}
#endif
	if (command.action.starts_with("sweep-"))
	{
		sweepCommand(command, frame);
		return;
	}
	if (command.action == "logall")
	{
		logAllObjects(frame);
		return;
	}
	if (command.action == "key")
	{
		sendKey(command, frame);
		return;
	}
	if (command.action == "list")
	{
		listObjects(command, frame);
		return;
	}
	if (command.action == "player")
	{
		dumpPlayer(command, frame);
		return;
	}
	if (command.action == "powerstate")
	{
		Player* player = ThePlayerList->findPlayerWithNameKey(TheNameKeyGenerator->nameToKey(command.player.c_str()));
		if (player)
		{
			const Object* book = player->getRetailSpellBook();
			std::fprintf(stderr, "TEST powerstate %u %s rank=%d skill=%d points=%d book=%s zoom=%.3f\n", frame,
				command.player.c_str(), player->getRankLevel(), player->getSkillPoints(), player->getSciencePurchasePoints(),
				book ? book->getTemplate()->getName().str() : "-", TheTacticalView ? TheTacticalView->getZoom() : 0.0f);
		}
		return;
	}
	if (command.action == "power-cast" || command.action == "power-rank" || command.action == "power-reveal" ||
		command.action == "power-book-drop" || command.action == "power-save")
	{
		Player* player = ThePlayerList->getLocalPlayer();
		if (!player)
			return;
		if (command.action == "power-cast")
		{
			GameMessage* message = TheMessageStream->appendMessage(GameMessage::MSG_DO_SPELLBOOK_SPECIAL_POWER);
			message->appendIntegerArgument(command.ordinal - 1);
			message->appendLocationArgument(groundLocation(command));
			message->appendObjectIDArgument(INVALID_ID);
		}
		else if (command.action == "power-rank")
		{
			TheGameLogic->setRankLevelLimit(command.ordinal);
			player->addSkillPoints(std::atoi(command.structure.c_str()));
			std::fprintf(stderr, "TEST power-rank %u cap=%d rank=%d skill=%d next=%d points=%d\n", frame,
				command.ordinal, player->getRankLevel(), player->getSkillPoints(), player->getSkillPointsLevelUp(),
				player->getSciencePurchasePoints());
		}
		else if (command.action == "power-reveal")
			ThePartitionManager->revealMapForPlayerPermanently(player->getPlayerIndex());
		else if (command.action == "power-book-drop")
		{
			if (Object* book = player->getRetailSpellBook())
				TheGameLogic->destroyObject(book);
		}
		else
		{
			const SaveResult result = TheGameState->saveGame(AsciiString(command.structure.c_str()),
				UnicodeString(L"Spellbook snapshot fixture"), SAVE_FILE_TYPE_NORMAL);
			std::fprintf(stderr, "TEST power-save %u %s result=%d\n", frame, result.filename.str(), result.saveCode);
		}
		return;
	}
	if (command.action == "powercrc")
	{
		std::fprintf(stderr, "TEST powercrc %u %08X\n", frame, TheGameLogic->getCRC(CRC_RECALC));
		return;
	}
#if !RTS_GENERALS
	if (command.action == "weatherstate")
	{
		std::fprintf(stderr, "TEST weather frame=%u state=%d end=%u\n", frame,
			TheGameLogic->getRetailWeather(), TheGameLogic->getRetailWeatherEndFrame());
		return;
	}
#endif
	if (command.action == "heroes")
	{
		dumpHeroes(command, frame);
		dumpProduction(command, frame);
		dumpHud(frame);
		return;
	}
	if (command.action == "xprostersnapshot")
	{
		if (const Player* player = findPlayer(command))
			roundTripHeroRoster(player, frame);
		return;
	}
	if (command.action == "xpcp")
	{
		if (const Player* player = findPlayer(command))
			std::fprintf(stderr, "TEST xp-command-points frame=%u total=%d\n", frame, player->getCommandPointsTotal());
		return;
	}
	if (command.action == "xpcrc")
	{
		std::fprintf(stderr, "TEST xp-crc frame=%u crc=%08X\n", frame, TheGameLogic->getCRC(CRC_RECALC));
		return;
	}
	if (command.action == "snapshot-crc")
	{
		auto crc = [](Snapshot* snapshot) {
			XferCRC xfer;
			xfer.open("SaveLoad");
			xfer.xferSnapshot(snapshot);
			const UnsignedInt result = xfer.getCRC();
			xfer.close();
			return result;
		};
		std::fprintf(stderr, "TEST snapshot frame=%u random=%08X partition=%08X players=%08X ai=%08X pathfinder=%08X terrain=%08X\n",
			frame, GetGameLogicRandomSeedCRC(), crc(ThePartitionManager), crc(ThePlayerList), crc(TheAI), crc(TheAI->pathfinder()), crc(TheTerrainLogic));
		for (Object* obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject())
		{
			std::fprintf(stderr, "TEST snapshot object frame=%u id=%u %s crc=%08X lookup=%d\n",
				frame, static_cast<unsigned>(obj->getID()), obj->getTemplate()->getName().str(),
				crc(obj), TheGameLogic->findObjectByID(obj->getID()) == obj);
			if (const ProductionUpdateInterface* production = obj->getProductionUpdateInterface();
				production && production->firstProduction())
			{
				static const NameKeyType key = NAMEKEY("ProductionUpdate");
				if (UpdateModule* update = obj->findUpdateModule(key))
					std::fprintf(stderr, "TEST snapshot queue frame=%u id=%u crc=%08X\n",
						frame, static_cast<unsigned>(obj->getID()), crc(update));
			}
		}
		return;
	}
#if !RTS_GENERALS
	if (command.action == "xpsavetrim")
	{
		const NameKeyType module = TheNameKeyGenerator->nameToKey("FireWeaponUpdate");
		for (Object* obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject())
		{
			if (obj->findUpdateModule(module))
				TheGameLogic->destroyObject(obj);
		}
		return;
	}
	if (command.action == "xpsave")
	{
		const SaveResult result = TheGameState->saveGame(command.structure.c_str(), UnicodeString(), SAVE_FILE_TYPE_NORMAL);
		std::fprintf(stderr, "TEST xp-save frame=%u code=%d\n", frame, result.saveCode);
		return;
	}
#endif
	if (command.action == "hud")
	{
		dumpHud(frame);
		return;
	}
	if (command.action == "slot")
	{
		clickWindow(retailSlotWindow(std::atoi(command.structure.c_str())), std::atoi(command.structure.c_str()), frame);
		return;
	}
	if (command.action == "pointer")
	{
		const Int slot = std::atoi(command.structure.c_str());
		GameWindow* window = retailSlotWindow(slot);
		if (!window || window->winIsHidden())
		{
			std::fprintf(stderr, "TEST frame %u: pointer slot %d empty\n", frame, slot);
			return;
		}
		ICoord2D point;
		Int width, height;
		window->winGetScreenPosition(&point.x, &point.y);
		window->winGetSize(&width, &height);
		point.x += width / 2;
		point.y += height / 2;
		TheMessageStream->appendMessage(GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_DOWN)->appendPixelArgument(point);
		TheMessageStream->appendMessage(GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_UP)->appendPixelArgument(point);
		std::fprintf(stderr, "TEST frame %u: pointer slot %d at %d,%d\n", frame, slot, point.x, point.y);
		return;
	}
	if (command.action == "keyw" || command.action == "escape")
	{
		GameMessage* key = TheMessageStream->appendMessage(GameMessage::MSG_RAW_KEY_UP);
		key->appendIntegerArgument(command.action == "keyw" ? KEY_W : KEY_ESC);
		key->appendIntegerArgument(KEY_STATE_UP);
		std::fprintf(stderr, "TEST frame %u: %s\n", frame, command.action.c_str());
		return;
	}
	if (command.action == "deselect")
	{
		TheInGameUI->deselectAllDrawables();
		std::fprintf(stderr, "TEST frame %u: deselect all\n", frame);
		return;
	}
	if (command.action == "cash")
	{
		Player* player = findPlayer(command);
		if (player)
		{
			player->getMoney()->deposit(static_cast<UnsignedInt>(std::atoi(command.structure.c_str())), FALSE, FALSE);
			std::fprintf(stderr, "TEST frame %u: cash %s +%s = %u\n", frame, command.player.c_str(), command.structure.c_str(),
				player->getMoney()->countMoney());
		}
		return;
	}
	if (command.action == "grant")
	{
		Player* player = findPlayer(command);
		const UpgradeTemplate* upgrade = TheUpgradeCenter ? TheUpgradeCenter->findUpgrade(command.structure.c_str()) : nullptr;
		if (player && upgrade)
		{
			player->addUpgrade(upgrade, UPGRADE_STATUS_COMPLETE);
			std::fprintf(stderr, "TEST frame %u: grant %s %s\n", frame, command.player.c_str(), command.structure.c_str());
		}
		return;
	}
	if (command.action == "science")
	{
		Player* player = findPlayer(command);
		const ScienceType science = TheScienceStore ? TheScienceStore->getScienceFromInternalName(command.structure.c_str()) : SCIENCE_INVALID;
		if (player && science != SCIENCE_INVALID)
			std::fprintf(stderr, "TEST frame %u: science %s %s granted=%d\n", frame, command.player.c_str(), command.structure.c_str(),
				player->grantScience(science) ? 1 : 0);
		return;
	}
	if (command.action == "spawn")
	{
		spawnObject(command, frame);
		return;
	}
	if (command.action == "movesel" && command.hasLocation)
	{
		GameMessage* move = TheMessageStream->appendMessage(GameMessage::MSG_DO_MOVETO);
		move->appendLocationArgument(groundLocation(command));
		std::fprintf(stderr, "TEST frame %u: movesel to %.1f %.1f\n", frame, command.location.x, command.location.y);
		return;
	}
	if (command.action == "passmap")
	{
		dumpPassability(frame);
		return;
	}
	if (command.action == "quit")
	{
		std::fprintf(stderr, "TEST frame %u: quit\n", frame);
		TheGameEngine->setQuitting(TRUE);
		return;
	}
	if (command.action == "cancel")
	{
		cancelInterface();
		std::fprintf(stderr, "TEST frame %u: cancel\n", frame);
		return;
	}
	if (command.action == "defeat")
	{
		if (Player* player = findPlayer(command.player))
		{
			std::fprintf(stderr, "TEST frame %u: defeat %s\n", frame, command.player.c_str());
			player->killPlayer();
		}
		return;
	}
	if (command.action == "create")
	{
		createObject(command, frame);
		return;
	}

#if !RTS_GENERALS
	if (command.action == "combatini" || command.action == "xptestini")
	{
		std::ifstream file(command.structure);
		if (!file)
		{
			std::fprintf(stderr, "TEST frame %u: cannot load combat INI %s\n", frame, command.structure.c_str());
			return;
		}
		std::ostringstream text;
		text << file.rdbuf();
		INI ini;
		const INILoadType mode = command.action == "xptestini" ? INI_LOAD_CREATE_OVERRIDES : INI_LOAD_OVERWRITE;
		ini.loadText(command.structure.c_str(), text.str().c_str(), mode, nullptr, nullptr);
		std::fprintf(stderr, "TEST frame %u: loaded combat INI\n", frame);
		return;
	}
#endif

	if (command.action == "hordeweapon")
	{
		AsciiString weapon;
		weapon.format("HordeTestWeapon %s\n  PrimaryDamage = 1\n  AttackRange = 18\n  DamageType = UNRESISTABLE\nEnd\n", command.target.c_str());
		INI ini;
		ini.loadText("HordeTestWeapon.ini", weapon.str(), INI_LOAD_OVERWRITE, "HordeTestWeapon", parseHordeTestWeapon);
		return;
	}

	if ((command.action == "hordespawn" || command.action == "hordespawnnear") && command.hasLocation)
	{
		Player* player = findPlayer(command.player);
		const ThingTemplate* type = TheThingFactory->findTemplate(command.target.c_str(), FALSE);
		if (player && type)
		{
			Coord3D position;
			if (command.action == "hordespawnnear")
			{
				const Object* target = findTarget(command);
				if (!target)
					return;
				position.x = target->getPosition()->x + command.location.x;
				position.y = target->getPosition()->y + command.location.y;
				position.z = TheTerrainLogic->getGroundHeight(position.x, position.y);
			}
			else
				position = groundLocation(command);
			Object* spawned = TheThingFactory->newObject(type, player->getDefaultTeam());
			spawned->setPosition(&position);
			spawned->setOrientation(command.angle);
			std::fprintf(stderr, "TEST horde spawn %u %s %u\n", frame, command.target.c_str(), static_cast<unsigned>(spawned->getID()));
		}
		return;
	}

	Object* obj = findObject(command, command.action == "xpstate");
	if (command.action == "placement-state")
	{
		logRetailPlacementTestObject(obj, command.structure.c_str(), frame);
		return;
	}
	if (!obj)
	{
		std::fprintf(stderr, "TEST frame %u: no %s %d of %s\n", frame, command.target.c_str(), command.ordinal,
			command.player.c_str());
		return;
	}

	if (command.action == "placement-ocl")
	{
		const Coord3D target = command.hasLocation ? groundLocation(command) : Coord3D{};
		runRetailPlacementTestCommand(obj, command.structure.c_str(), command.hasLocation ? &target : nullptr, frame);
		return;
	}

	if (command.action == "placement-spawn-points")
	{
		logRetailSpawnPoints(obj, command.structure.c_str(), frame);
		return;
	}

	if (command.action.starts_with("mparity-"))
	{
		runRetailMordorTestCommand(command.action, obj, findTarget(command), command.structure.c_str(), frame);
		return;
	}
#if !RTS_GENERALS
	if (command.action.starts_with("pkg08-"))
	{
		const Coord3D position = command.hasLocation ? groundLocation(command) : Coord3D{};
		runRetailPackage08TestCommand(command.action, obj, findTarget(command, true), command.structure.c_str(),
			command.hasLocation ? &position : nullptr, frame);
		return;
	}
	if (command.action.starts_with("pkg05-"))
	{
		if (command.action == "pkg05-wear-ring")
			selectObject(obj);
		runRetailPackage05TestCommand(command.action, obj, findTarget(command, true), frame);
		return;
	}
	if (command.action.starts_with("pkg07-"))
	{
		runRetailPackage07TestCommand(command.action, obj, findTarget(command, true), frame);
		return;
	}
	if (command.action.starts_with("pkg17-"))
	{
		runRetailPackage17TestCommand(command.action, obj, findTarget(command, true));
		return;
	}
	if (command.action.starts_with("pkg10-"))
	{
		runRetailPackage10TestCommand(command.action, obj, command.structure.c_str(), frame);
		return;
	}
	if (command.action.starts_with("pkg12-ai-"))
	{
		runRetailPackage12TestCommand(command.action, obj, findTarget(command, true), frame);
		return;
	}
	if (command.action == "pkg12-attach")
	{
		runRetailPackage12AttachTestCommand(command.action, obj, frame);
		return;
	}
	if (command.action.starts_with("pkg12-pickup-"))
	{
		runRetailPackage12PickupTestCommand(command.action, obj, command.structure.c_str(), frame);
		return;
	}
	if (command.action.starts_with("pkg15-"))
	{
		runRetailPackage15TestCommand(command.action, obj, frame);
		return;
	}
	if (command.action.starts_with("pkg13-"))
	{
		runRetailPackage13TestCommand(command.action, obj, findTarget(command, true), command.structure.c_str(), frame);
		return;
	}
	if (command.action.starts_with("pkg16-"))
	{
		runRetailPackage16TestCommand(command.action, obj, command.structure.c_str(), frame);
		return;
	}
	if (command.action.starts_with("pkg19-"))
	{
		runRetailPackage19TestCommand(command.action, obj, frame);
		return;
	}
	if (command.action.starts_with("pkg18-"))
	{
		runRetailPackage18TestCommand(command.action, findPlayer(command.player), obj, command.structure.c_str(), frame);
		return;
	}
	if (command.action.starts_with("pkg09-"))
	{
		runRetailPackage09TestCommand(command.action, obj, findTarget(command, true), command.structure.c_str(), frame);
		return;
	}
	if (command.action.starts_with("pkg41-"))
	{
		runRetailPackage41TestCommand(command.action, obj, frame);
		return;
	}
	if (command.action.starts_with("pkg11-"))
	{
		runRetailPackage11TestCommand(command.action, obj, findTarget(command, true), command.structure.c_str(), command.location, frame);
		return;
	}
	if (command.action.starts_with("pkg14-"))
	{
		runRetailPackage14TestCommand(command.action, obj, findTarget(command, true), command.structure.c_str(), frame);
		return;
	}
	if (command.action.starts_with("pkg20-"))
	{
		runRetailPackage20TestCommand(command.action, obj, findTarget(command, true), command.structure.c_str(), command.location, frame);
		return;
	}
	if (command.action.starts_with("pkg22-"))
	{
		runRetailPackage22TestCommand(command.action, obj, findTarget(command, true), command.location, frame);
		return;
	}
	if (command.action.starts_with("pkg53-"))
	{
		runRetailPackage53TestCommand(command.action, obj, findTarget(command, true), frame);
		return;
	}
#endif

	const Bool placeAtPoint = command.action == "place" && command.hasPlacementPoint;
	if (command.action.starts_with("bparity-") &&
		runBuildingCommand(command.action, obj, command.structure.c_str(), command.hasLocation ? &command.location : nullptr, frame))
		return;
	if (command.action == "snapshot-devastation")
	{
		const auto* power = TheSpecialPowerStore->findSpecialPowerTemplate("SpellBookDevastation");
		auto* module = power ? obj->getSpecialPowerModule(power) : nullptr;
		if (!module)
			return;
		Coord3D point = groundLocation(command);
		if (!command.hasLocation)
		{
			const MapObject* tree = nullptr;
			for (const MapObject* candidate = MapObject::getFirstMapObject(); candidate; candidate = candidate->getNext())
			{
				const ThingTemplate* type = candidate->getThingTemplate();
				if (type && type->isKindOf(KINDOF_TREE) && type->getRotWKObjectData().isHarvestable &&
					(!tree || candidate->getLocation()->x < tree->getLocation()->x))
					tree = candidate;
			}
			if (!tree)
				return;
			point = *tree->getLocation();
			point.z += TheTerrainLogic->getGroundHeight(point.x, point.y);
		}
		auto* money = obj->getControllingPlayer()->getMoney();
		const auto before = money->countMoney();
		if (command.structure == "recharge")
			module->setReadyFrame(frame);
		if (module->isReady())
			obj->doSpecialPowerAtLocation(power, &point, INVALID_ANGLE, 0);
		std::fprintf(stderr, "TEST snapshot devastation frame=%u payout=%u ready=%d\n",
			frame, money->countMoney() - before, module->isReady());
	}
	else if (command.action == "snapshot-share" || command.action == "snapshot-handoff")
	{
		const auto* horde = HordeContain::get(obj);
		if (!horde)
			return;
		std::array<Object*, 3> members{};
		for (Object* member = TheGameLogic->getFirstObject(); member; member = member->getNextObject())
		{
			const Int slot = horde->getMemberSlot(member->getID());
			if (slot >= 0 && slot < static_cast<Int>(members.size()))
				members[slot] = member;
		}
		if (!members[0] || !members[1] || !members[2] || !members[0]->getAIUpdateInterface())
			return;
		auto* pathfinder = TheAI->pathfinder();
		if (command.action == "snapshot-share")
		{
			const Coord3D point = groundLocation(command);
			for (Object* member : members)
				pathfinder->updateGoal(member, &point, LAYER_GROUND);
		}
		else
		{
			const auto goal = *members[0]->getAIUpdateInterface()->getPathfindGoalCell();
			auto* cell = pathfinder->getCell(LAYER_GROUND, goal.x, goal.y);
			if (!cell)
				return;
			const Bool primary = cell->getGoalUnit() == members[0]->getID();
			pathfinder->removeGoal(members[0]);
			const Bool first = cell->getGoalUnit() == members[1]->getID();
			pathfinder->removeGoal(members[1]);
			const Bool second = cell->getGoalUnit() == members[2]->getID();
			std::fprintf(stderr, "TEST snapshot handoff frame=%u primary=%d first=%d second=%d\n", frame, primary, first, second);
		}
	}
	else if (command.action == "snapshot-overlap" && command.hasLocation)
	{
		const Coord3D point = groundLocation(command);
		obj->setPosition(&point);
	}
	else if (command.action == "snapshot-emitter")
	{
		for (auto module = obj->getBehaviorModules(); *module; ++module)
		{
			if ((*module)->getModuleNameKey() != NAMEKEY("CritterEmitterUpdate"))
				continue;
			XferCRC xfer;
			xfer.open("SaveLoadEmitter");
			xfer.xferSnapshot(*module);
			std::fprintf(stderr, "TEST snapshot emitter frame=%u crc=%08X\n", frame, xfer.getCRC());
			xfer.close();
		}
	}
	else if (command.action == "wallblind")
	{
		const Player* player = obj->getControllingPlayer();
		for (Object* source = TheGameLogic->getFirstObject(); source; source = source->getNextObject())
		{
			if (source->getControllingPlayer() == player)
				source->setShroudClearingRange(0.0f);
		}
		ThePartitionManager->shroudMapForPlayer(player->getPlayerIndex());
	}
	else if (command.action == "wallclaim" && command.hasLocation)
		checkWallClaim(command, obj, frame);
	else if (command.action == "wallcells")
		checkGateCells(command, obj, frame);
	else if (command.action == "wallgridbefore" || command.action == "wallgridafter")
		checkGateFootprint(command, obj, frame);
#if !RTS_GENERALS
	else if (command.action == "wallalias")
		checkWallScriptAliases(command, obj, frame);
#endif
	else if ((command.action == "wallspan" || command.action == "wallpreview") && command.hasLocation)
	{
		const ThingTemplate* cap = TheThingFactory->findTemplate(command.structure.c_str(), FALSE);
		WallHubBehavior* hub = WallHubBehavior::find(obj, OPTION_ONE);
		if (!cap || !hub)
		{
			std::fprintf(stderr, "TEST wall %u source or cap unavailable\n", frame);
			return;
		}
		const Coord3D target = groundLocation(command);
		const auto plan = hub->plan(cap, target);
		std::fprintf(stderr, "TEST wall %u tiles=%zu cost=%u legal=%d affordable=%d\n", frame,
			plan.pieces.size(), plan.cost, plan.legal, plan.affordable);
		if (command.action == "wallpreview" && TheTacticalView && TheInGameUI && obj->getDrawable())
		{
			selectObject(obj);
			TheInGameUI->placeWallSpanAvailable(cap, obj->getDrawable(), OPTION_ONE);
			ICoord2D pixel;
			TheTacticalView->worldToScreen(&target, &pixel);
			TheInGameUI->setPlacementStart(&pixel);
			TheInGameUI->setPlacementEnd(&pixel);
		}
		else
		{
			GameMessage* wall = TheMessageStream->appendMessage(GameMessage::MSG_RETAIL_WALL_SPAN);
			wall->appendObjectIDArgument(obj->getID());
			wall->appendIntegerArgument(cap->getTemplateID());
			wall->appendIntegerArgument(OPTION_ONE);
			wall->appendLocationArgument(target);
		}
	}
	else if (command.action == "gatecheck")
	{
		const Object* unit = findTarget(command, true);
		const auto* gate = GateOpenAndCloseBehavior::find(obj);
		std::fprintf(stderr, "TEST gate %u unit=%s exists=%d wantsOpen=%d planning=%d passable=%d\n", frame, command.structure.c_str(),
			gate != nullptr, gate && gate->wantsOpen(), GateOpenAndCloseBehavior::canPass(obj, unit, TRUE),
			GateOpenAndCloseBehavior::canPass(obj, unit, FALSE));
	}
	else if (command.action == "hordequeue")
	{
		ProductionUpdateInterface* production = obj->getProductionUpdateInterface();
		const ThingTemplate* type = TheThingFactory->findTemplate(command.structure.c_str(), FALSE);
		Bool queued = FALSE;
		if (production && type)
			queued = production->queueCreateUnit(type, production->requestUniqueUnitID());
		std::fprintf(stderr, "TEST horde queue %u %s %d\n", frame, command.structure.c_str(), queued ? 1 : 0);
	}
	else if (command.action == "hordenoauto" && HordeContain::get(obj))
	{
		const ModuleInfo& modules = obj->getTemplate()->getBehaviorModuleInfo();
		for (Int i = 0; i < modules.getCount(); ++i)
		{
			if (stricmp(modules.getNthName(i).str(), "HordeAIUpdate") != 0)
				continue;
			const auto* moduleData = static_cast<const AIUpdateModuleData*>(modules.getNthData(i));
			auto* data = const_cast<AIUpdateModuleData*>(moduleData);
			data->m_autoAcquireEnemiesWhenIdle = 0;
			std::fprintf(stderr, "TEST horde noauto %u %u\n", frame, static_cast<unsigned>(obj->getID()));
			std::fflush(stderr);
			break;
		}
	}
	else if (command.action == "hordeproject" && TheTacticalView)
	{
		Coord3D point = command.hasLocation ? groundLocation(command) : *obj->getPosition();
		ICoord2D screen;
		TheTacticalView->worldToScreen(&point, &screen);
		std::fprintf(stderr, "TEST horde screen %u %s %d %d\n", frame, command.target.c_str(), screen.x, screen.y);
	}
	else if (command.action == "hordeattack")
	{
		const ObjectID id = static_cast<ObjectID>(std::atoi(command.structure.c_str()));
		GameMessage* attack = TheMessageStream->appendMessage(GameMessage::MSG_DO_ATTACK_OBJECT);
		attack->appendObjectIDArgument(id);
		std::fprintf(stderr, "TEST horde attack %u %u\n", frame, static_cast<unsigned>(id));
	}
	else if (command.action == "hordelog" || command.action == "hordekill")
	{
		const HordeContain* horde = HordeContain::get(obj);
		if (!horde)
			return;
		const auto& members = *horde->getContainedItemsList();
		std::fprintf(stderr, "TEST horde anchor %u %u count %u selected %d\n", frame,
			static_cast<unsigned>(obj->getID()), horde->getContainCount(),
			obj->getDrawable() && obj->getDrawable()->isSelected() ? 1 : 0);
		logObject(obj, frame);
		if (command.action == "hordelog")
		{
			std::fprintf(stderr, "TEST horde formation %u %u alternate=%d\n", frame,
				static_cast<unsigned>(obj->getID()), horde->isAlternateFormation());
		}
		for (auto it = members.begin(); it != members.end(); )
		{
			Object* member = *it++;
			AIUpdateInterface* ai = member->getAI();
			const Object* target = ai ? ai->getGoalObject() : nullptr;
			std::fprintf(stderr, "TEST horde member %u %u slot %d rankAttack %d target %u attacking %d\n", frame,
				static_cast<unsigned>(member->getID()), horde->getMemberSlot(member->getID()),
				horde->rankCanAttack(member) ? 1 : 0, target ? static_cast<unsigned>(target->getID()) : 0,
				ai && ai->isAttacking() ? 1 : 0);
			logObject(member, frame);
			if (command.action == "hordelog")
			{
				const Weapon* weapon = member->getCurrentWeapon();
				const Coord3D slot = horde->getMemberPosition(member->getID(), FALSE);
				std::fprintf(stderr, "TEST horde mode %u %u toggle=%d weapon=%s slot=%.3f,%.3f locked=%d\n", frame,
					static_cast<unsigned>(member->getID()), member->testWeaponSetFlag(WEAPONSET_TOGGLE_1),
					weapon ? weapon->getName().str() : "-", slot.x, slot.y, member->isCurWeaponLocked());
			}
			if (command.action == "hordekill")
				member->kill();
		}
	}
	else if (command.action == "rearm-sleep")
	{
		if (BannerCarrierUpdate* banner = BannerCarrierUpdate::find(obj))
		{
			banner->friend_setNextCallFrame(UPDATE_SLEEP_FOREVER);
			std::fprintf(stderr, "TEST rearm-sleep %u %s wake=%u\n", frame,
				obj->getTemplate()->getName().str(), banner->friend_getNextCallFrame());
		}
	}
	else if (command.action == "banner-wake")
	{
		if (BannerCarrierUpdate* banner = BannerCarrierUpdate::find(obj))
			std::fprintf(stderr, "TEST banner-wake %u %s wake=%u frame=%u\n", frame,
				obj->getTemplate()->getName().str(), banner->friend_getNextCallFrame(), TheGameLogic->getFrame());
	}
	else if (command.action == "build" || placeAtPoint || command.action == "queue" || command.action == "queuebutton")
	{
		const bool queue = command.action == "queue" || command.action == "queuebutton";
		const ThingTemplate* what = nullptr;
		if (command.action == "queuebutton")
		{
			const CommandSet* set = TheControlBar ? TheControlBar->findCommandSet(obj->getCommandSetString()) : nullptr;
			for (Int slot = 0; set && slot < CommandButtonRules::RETAIL_COMMANDS_PER_SET; ++slot)
			{
				const CommandButton* button = set->getCommandButton(slot);
				if (button && button->getCommandType() == GUI_COMMAND_UNIT_BUILD &&
					button->getName().compareNoCase(command.structure.c_str()) == 0)
				{
					what = button->getThingTemplate();
					break;
				}
			}
		}
		else
			what = TheThingFactory->findTemplate(command.structure.c_str(), FALSE);
		if (!what)
			return;
		ProductionUpdateInterface* production = obj->getProductionUpdateInterface();
		if (queue && !production)
			return;
		GameMessage* select = TheMessageStream->appendMessage(GameMessage::MSG_CREATE_SELECTED_GROUP);
		select->appendBooleanArgument(TRUE);
		select->appendObjectIDArgument(obj->getID());
		GameMessage::Type type = GameMessage::MSG_FOUNDATION_CONSTRUCT;
		if (placeAtPoint)
			type = GameMessage::MSG_DOZER_CONSTRUCT;
		else if (queue)
			type = GameMessage::MSG_QUEUE_UNIT_CREATE;
		GameMessage* build = TheMessageStream->appendMessage(type);
		build->appendIntegerArgument(what->getTemplateID());
		if (placeAtPoint)
		{
			const Coord3D target = {command.x, command.y, TheTerrainLogic->getGroundHeight(command.x, command.y)};
			build->appendLocationArgument(target);
			build->appendRealArgument(0.0f);
		}
		else if (queue)
		{
			build->appendIntegerArgument(production->requestUniqueUnitID());
		}
		std::fprintf(stderr, "TEST frame %u: %s %s on object %u\n", frame, command.action.c_str(), command.structure.c_str(),
			static_cast<unsigned>(obj->getID()));
	}
	else if (command.action == "research")
	{
		const auto* upgrade = TheUpgradeCenter->findUpgrade(command.structure.c_str());
		auto* production = obj->getProductionUpdateInterface();
		const Bool queued = upgrade && production && production->queueUpgrade(upgrade);
		std::fprintf(stderr, "TEST research %u %s %s queued=%d money=%u\n", frame,
			command.target.c_str(), command.structure.c_str(), queued, obj->getControllingPlayer()->getMoney()->countMoney());
	}
	else if (command.action == "produce")
	{
		const ThingTemplate* what = TheThingFactory->findTemplate(command.structure.c_str(), FALSE);
		ProductionUpdateInterface* production = obj->getProductionUpdateInterface();
		const Bool queued = what && production && production->queueCreateUnit(what, production->requestUniqueUnitID());
		std::fprintf(stderr, "TEST frame %u: produce %s on object %u queued=%d\n", frame, command.structure.c_str(),
			static_cast<unsigned>(obj->getID()), queued);
	}
	else if (command.action == "powerstate")
	{
		const auto* contain = obj->getContain();
		const auto* passengers = contain ? contain->getContainedItemsList() : nullptr;
		const auto* production = obj->getProductionUpdateInterface();
		const auto* entry = production ? production->firstProduction() : nullptr;
		const auto* weapon = obj->getWeaponInWeaponSlot(PRIMARY_WEAPON);
		const auto* draw = obj->getDrawable();
		std::fprintf(stderr, "TEST powerstate %u %s id=%u stealth=%d passenger=%s weapon=%s toggle=%d toggling=%d progress=%.3f speed=%.3f\n",
			frame, command.target.c_str(), static_cast<unsigned>(obj->getID()), obj->testStatus(OBJECT_STATUS_STEALTHED),
			passengers && !passengers->empty() ? passengers->front()->getTemplate()->getName().str() : "-",
			weapon ? weapon->getName().str() : "-", obj->testWeaponSetFlag(WEAPONSET_TOGGLE_1),
			draw && draw->getModelConditionFlags().test(MODELCONDITION_WEAPON_TOGGLING),
			entry ? entry->getPercentComplete() : -1.0f,
			obj->getControllingPlayer()->getPowerProductionSpeed(AsciiString(command.structure.c_str())));
	}
	else if (command.action == "powers")
	{
		for (BehaviorModule** module = obj->getBehaviorModules(); *module; ++module)
		{
			const SpecialPowerModuleInterface* power = (*module)->getSpecialPower();
			if (power && power->getSpecialPowerTemplate())
				std::fprintf(stderr, "TEST power %u object %u %s ready=%d percent=%.3f\n", frame,
					static_cast<unsigned>(obj->getID()), power->getSpecialPowerTemplate()->getName().str(), power->isReady(),
					power->getPercentReady());
		}
	}
	else if (command.action == "fund")
	{
		obj->getControllingPlayer()->getMoney()->setStartingCash(50000);
	}
	else if (command.action == "kill")
	{
		std::fprintf(stderr, "TEST frame %u: kill object %u (%s)\n", frame, static_cast<unsigned>(obj->getID()),
			command.target.c_str());
		obj->kill();
	}
	else if (command.action == "killby")
	{
		Object* victim = findTarget(command, true);
		if (victim && !victim->isEffectivelyDead() && victim->getBodyModule())
		{
			DamageInfo damage;
			damage.in.m_damageType = DAMAGE_UNRESISTABLE;
			damage.in.m_deathType = DEATH_NORMAL;
			damage.in.m_sourceID = obj->getID();
			damage.in.m_amount = victim->getBodyModule()->getMaxHealth();
			damage.in.m_kill = TRUE;
			victim->attemptDamage(&damage);
			std::fprintf(stderr, "TEST xp-kill frame=%u killer=%u victim=%u dead=%d\n", frame,
				static_cast<unsigned>(obj->getID()), static_cast<unsigned>(victim->getID()), victim->isEffectivelyDead());
		}
	}
	else if (command.action == "xpstate")
	{
		const auto* tracker = obj->getExperienceTracker();
		std::fprintf(stderr, "TEST xp-state frame=%u object=%u %s rank=%d max=%d xp=%.3f progress=%.3f start=%d\n", frame,
			static_cast<unsigned>(obj->getID()), obj->getTemplate()->getName().str(), tracker->getRank(),
			ExperienceLevelStore::maximumRank(obj), tracker->getRetailExperience(), tracker->getLevelProgress(),
			tracker->getRetailStartingRank());
	}
	else if (command.action == "nhload")
	{
		Object* source = command.structure == "-" ? nullptr : findTarget(command);
		if (source || command.structure == "-")
			loadHordeFixture(obj, source, frame);
	}
	else if (command.action == "nhstate")
	{
		const auto* draw = obj->getDrawable();
		const auto* slave = static_cast<const SlavedUpdate*>(obj->findUpdateModule(NAMEKEY("SlavedUpdate")));
		const auto* horde = HordeContain::get(obj);
		const auto* ai = obj->getAI();
		Bool alternateArmor = FALSE;
#if !RTS_GENERALS
		auto* body = obj->getBodyModule();
		alternateArmor = body && body->testArmorSetFlag(ARMORSET_ALTERNATE_FORMATION);
#endif
		std::fprintf(stderr, "TEST nh-state frame=%u %s id=%u toggle=%d alternateModel=%d alternateArmor=%d members=%u slaver=%u unmanned=%d moving=%d\n",
			frame, obj->getTemplate()->getName().str(), static_cast<unsigned>(obj->getID()), obj->testWeaponSetFlag(WEAPONSET_TOGGLE_1),
			draw && draw->getModelConditionFlags().test(MODELCONDITION_ALTERNATE_FORMATION),
			alternateArmor, horde ? horde->getContainCount() : 0u,
			slave ? static_cast<unsigned>(slave->getSlaverID()) : 0u, obj->isDisabledByType(DISABLED_UNMANNED), ai && ai->isMoving());
		if (slave && ai && ai->getGoalPosition())
		{
			const Coord3D* goal = ai->getGoalPosition();
			const Object* victim = ai->getCurrentVictim();
			std::fprintf(stderr, "TEST nh-slave-goal frame=%u id=%u x=%.1f y=%.1f victim=%u returning=%d\n",
				frame, static_cast<unsigned>(obj->getID()), goal->x, goal->y, victim ? static_cast<unsigned>(victim->getID()) : 0u,
				obj->testStatus(OBJECT_STATUS_LEASHED_RETURNING));
		}
#if !RTS_GENERALS
		const auto* mode = static_cast<const WeaponModeSpecialPowerUpdate*>(
			obj->findUpdateModule(NAMEKEY("WeaponModeSpecialPowerUpdate")));
		if (mode)
			std::fprintf(stderr, "TEST nh-mode frame=%u %s active=%d\n", frame, obj->getTemplate()->getName().str(), mode->isActive());
#endif
	}
	else if (command.action == "nhenslave")
	{
		auto* slave = static_cast<SlavedUpdate*>(obj->findUpdateModule(NAMEKEY("SlavedUpdate")));
		if (const Object* master = findTarget(command); slave && master)
			slave->onEnslave(master);
	}
#if !RTS_GENERALS
	else if (command.action == "nhmode")
	{
		auto* mode = static_cast<WeaponModeSpecialPowerUpdate*>(obj->findUpdateModule(NAMEKEY("WeaponModeSpecialPowerUpdate")));
		const auto* power = TheSpecialPowerStore->findSpecialPowerTemplate(command.structure.c_str());
		if (mode && power)
			std::fprintf(stderr, "TEST nh-mode-start frame=%u %s accepted=%d\n", frame, obj->getTemplate()->getName().str(),
				mode->initiateIntentToDoSpecialPower(power, nullptr, nullptr, nullptr, 0));
	}
#endif
	else if (command.action == "nhattack")
	{
		Object* target = findTarget(command);
		if (target && obj->getAI())
		{
			const auto result = obj->getAbleToAttackSpecificObject(ATTACK_NEW_TARGET, target, CMD_FROM_AI);
			std::fprintf(stderr, "TEST nh-attack frame=%u %s eligible=%d\n", frame, obj->getTemplate()->getName().str(),
				result == ATTACKRESULT_POSSIBLE || result == ATTACKRESULT_POSSIBLE_AFTER_MOVING);
			obj->getAI()->aiAttackObject(target, NO_MAX_SHOTS_LIMIT, CMD_FROM_AI);
		}
	}
	else if (command.action == "nhpickup")
	{
		if (Object* collector = findTarget(command))
			for (BehaviorModule** module = obj->getBehaviorModules(); *module; ++module)
				if (CollideModuleInterface* collide = (*module)->getCollide())
					collide->onCollide(collector, nullptr, nullptr);
	}
	else if (command.action == "xpsnapshot")
		roundTripExperience(obj, frame);
	else if (command.action == "destroy")
	{
		std::fprintf(stderr, "TEST frame %u: destroy object %u (%s)\n", frame, static_cast<unsigned>(obj->getID()),
			command.target.c_str());
		TheGameLogic->destroyObject(obj);
	}
	else if (command.action == "damage")
	{
		BodyModuleInterface* body = obj->getBodyModule();
		if (body)
			body->internalChangeHealth(-static_cast<Real>(std::atof(command.structure.c_str())));
		std::fprintf(stderr, "TEST frame %u: damage object %u to %.1f\n", frame, static_cast<unsigned>(obj->getID()),
			body ? body->getHealth() : 0.0f);
	}
	else if (command.action == "capture")
		capture(command, obj, frame);
	else if (command.action == "repair")
		repair(command, obj, frame);
	else if (command.action == "buildersnapshot")
		roundTripBuilder(obj, frame);
	else if ((command.action == "builderscriptmove" || command.action == "builderaimove") && command.hasLocation && obj->getAI())
	{
		const Coord3D location = groundLocation(command);
		obj->getAI()->aiMoveToPosition(&location, command.action == "builderscriptmove" ? CMD_FROM_SCRIPT : CMD_FROM_AI);
		std::fprintf(stderr, "TEST frame %u: %s object %u to %.1f %.1f\n", frame, command.action.c_str(),
			static_cast<unsigned>(obj->getID()), location.x, location.y);
	}
	else if (command.action == "veterancy")
	{
		if (obj->getExperienceTracker())
			obj->getExperienceTracker()->setVeterancyLevel(static_cast<VeterancyLevel>(std::atoi(command.structure.c_str())), FALSE);
		std::fprintf(stderr, "TEST frame %u: veterancy object %u = %d\n", frame, static_cast<unsigned>(obj->getID()), static_cast<int>(obj->getVeterancyLevel()));
	}
	else if (command.action == "log")
	{
		logObject(obj, frame);
	}
	else if (command.action == "flight")
	{
		const AIUpdateInterface* ai = obj->getAI();
		const Locomotor* locomotor = ai ? ai->getCurLocomotor() : nullptr;
		std::fprintf(stderr, "TEST flight frame=%u id=%u height=%.2f cutoff=%.2f airborne=%d\n", frame,
			static_cast<unsigned>(obj->getID()), obj->getHeightAboveTerrain(),
			locomotor ? locomotor->getAirborneTargetingHeight() : 0.0f, obj->isAirborneTarget() ? 1 : 0);
	}
	else if (command.action == "ability")
		dumpAbilities(command, obj, frame);
	else if (command.action == "aim")
		aimPendingCommand(command, obj, frame);
	else if (command.action == "power")
		pressPowerButton(command, obj, frame);
#if !RTS_GENERALS
	else if (command.action == "xpwaypoint")
	{
		const Waypoint* waypoint = TheTerrainLogic->getFirstWaypoint();
		if (!waypoint)
			return;
		if (obj->getName().isEmpty())
		{
			AsciiString name;
			name.format("ExperienceFixture%u", static_cast<UnsignedInt>(obj->getID()));
			obj->setName(name);
			TheScriptEngine->addObjectToCache(obj);
		}
		ScriptAction* action = newInstance(ScriptAction)(ScriptAction::NAMED_FIRE_SPECIAL_POWER_AT_WAYPOINT);
		action->getParameter(0)->friend_setString(obj->getName());
		action->getParameter(1)->friend_setString(command.structure.c_str());
		action->getParameter(2)->friend_setString(waypoint->getName());
		const UnsignedInt before = TheGameLogic->getObjectCount();
		TheScriptActions->executeAction(action);
		deleteInstance(action);
		std::fprintf(stderr, "TEST xp-waypoint frame=%u rank=%d spawned=%d\n",
			frame, obj->getExperienceTracker()->getRank(), static_cast<Int>(TheGameLogic->getObjectCount() - before));
	}
	else if (command.action == "testcontain")
	{
		Object* passenger = findTarget(command, true);
		ContainModuleInterface* contain = obj->getContain();
		if (passenger && contain && contain->isValidContainerFor(passenger, TRUE))
		{
			contain->addToContain(passenger);
			std::fprintf(stderr, "TEST containment frame=%u admitted=%d\n", frame, passenger->getContainedBy() == obj);
		}
	}
	else if (command.action == "modsnapshot")
	{
		const size_t separator = command.structure.find(',');
		if (separator != std::string::npos)
		{
			Command targetCommand = command;
			targetCommand.structure = command.structure.substr(separator + 1);
			roundTripModifiers(obj, findTarget(targetCommand, true), AsciiString(command.structure.substr(0, separator).c_str()), frame);
		}
	}
	else if (command.action == "healsource")
	{
		const Object* source = TheGameLogic->findObjectByID(obj->getSoleHealingBenefactor());
		std::fprintf(stderr, "TEST heal-source frame=%u target=%d source=%s\n", frame, command.ordinal,
			source ? source->getTemplate()->getName().str() : "none");
	}
	else if (command.action == "checkobjupgrade")
	{
		const UpgradeTemplate* upgrade = TheUpgradeCenter->findUpgrade(command.structure.c_str());
		std::fprintf(stderr, "TEST check-upgrade frame=%u found=%d\n", frame, upgrade && obj->hasUpgrade(upgrade));
	}
	else if (command.action == "firetestweapon")
	{
		const size_t separator = command.structure.find(',');
		if (separator == std::string::npos)
			return;
		Command targetCommand = command;
		targetCommand.structure = command.structure.substr(separator + 1);
		Object* target = findTarget(targetCommand, true);
		const WeaponTemplate* weapon = TheWeaponStore->findWeaponTemplate(command.structure.substr(0, separator).c_str());
		if (weapon && target)
		{
			std::fprintf(stderr, "TEST frame %u: fire test weapon %s source=%u victim=%u\n", frame, weapon->getName().str(),
				static_cast<unsigned>(obj->getID()), static_cast<unsigned>(target->getID()));
			TheWeaponStore->createAndFireTempWeapon(weapon, obj, target);
		}
	}
	else if (command.action == "mods")
		logModifiers(obj, frame);
	else if (command.action == "stance")
	{
		const Int stance = AttributeModifierRules::indexOf(AttributeModifierRules::stanceNames(), command.structure.c_str());
		selectGroup(obj);
		GameMessage* msg = TheMessageStream->appendMessage(GameMessage::MSG_SET_STANCE);
		msg->appendIntegerArgument(stance);
		std::fprintf(stderr, "TEST frame %u: stance object %u %s (%d)\n", frame, static_cast<unsigned>(obj->getID()), command.structure.c_str(), stance);
	}
	else if (command.action == "modapply")
	{
		const Bool applied = AttributeModifiers::apply(obj, AsciiString(command.structure.c_str()));
		std::fprintf(stderr, "TEST frame %u: modapply object %u %s %d\n", frame, static_cast<unsigned>(obj->getID()), command.structure.c_str(), applied ? 1 : 0);
	}
	else if (command.action == "hordeleave")
	{
		ContainModuleInterface* contain = HordeContain::get(obj) ? obj->getContain() : nullptr;
		const ContainedItemsList* members = contain ? contain->getContainedItemsList() : nullptr;
		if (members && !members->empty())
		{
			Object* member = members->front();
			logModifiedObject("leaving", member, frame);
			contain->removeFromContain(member, FALSE);
			logModifiedObject("left", member, frame);
			std::fprintf(stderr, "TEST frame %u: hordeleave member %u containedBy=%u\n", frame, static_cast<unsigned>(member->getID()),
				member->getContainedBy() ? static_cast<unsigned>(member->getContainedBy()->getID()) : 0u);
		}
	}
#endif
	else if (command.action == "objupgrade")
	{
		const UpgradeTemplate* upgrade = TheUpgradeCenter ? TheUpgradeCenter->findUpgrade(command.structure.c_str()) : nullptr;
		if (upgrade)
			obj->giveUpgrade(upgrade);
		std::fprintf(stderr, "TEST frame %u: objupgrade object %u %s %s\n", frame, static_cast<unsigned>(obj->getID()),
			command.structure.c_str(), upgrade ? "given" : "missing");
	}
#if !RTS_GENERALS
	else if (command.action == "abilitysnapshot")
		roundTripAbility(obj, TheSpecialPowerStore->findSpecialPowerTemplate(command.structure.c_str()), frame);
	else if (command.action == "mountpower")
		checkMountPower(obj, TheSpecialPowerStore->findSpecialPowerTemplate(command.structure.c_str()), frame);
	else if (command.action == "autoability")
	{
		const Int state = command.structure == "on" ? 1 : (command.structure == "off" ? -1 : 0);
		static const NameKeyType key = NAMEKEY("AutoAbilityBehavior");
		for (BehaviorModule** module = obj->getBehaviorModules(); *module; ++module)
		{
			if ((*module)->getModuleNameKey() == key)
				static_cast<AutoAbilityBehavior*>(*module)->setToggledState(state);
		}
		std::fprintf(stderr, "TEST frame %u: autoability object %u %s\n", frame, static_cast<unsigned>(obj->getID()), command.structure.c_str());
	}
#endif
	else if (command.action == "select")
	{
		selectObject(obj);
		std::fprintf(stderr, "TEST frame %u: select object %u (%s)\n", frame, static_cast<unsigned>(obj->getID()),
			command.target.c_str());
	}
	else if (command.action == "selectadd")
	{
		if (TheInGameUI && obj->getDrawable())
			TheInGameUI->selectDrawable(obj->getDrawable());
		GameMessage* select = TheMessageStream->appendMessage(GameMessage::MSG_CREATE_SELECTED_GROUP);
		select->appendBooleanArgument(FALSE);
		select->appendObjectIDArgument(obj->getID());
		std::fprintf(stderr, "TEST frame %u: selectadd object %u (%s)\n", frame, static_cast<unsigned>(obj->getID()),
			command.target.c_str());
	}
	else if (command.action == "track")
	{
		if (std::find(s_tracked.begin(), s_tracked.end(), obj->getID()) == s_tracked.end())
			s_tracked.push_back(obj->getID());
		std::fprintf(stderr, "TEST track %u %u %s\n", frame, static_cast<unsigned>(obj->getID()), command.target.c_str());
	}
	else if (command.action == "attack")
		attackObject(command, obj, frame);
	else if (command.action == "orderattack")
	{
		if (const Object* target = findTarget(command))
		{
			selectObject(obj);
			GameMessage* attack = TheMessageStream->appendMessage(GameMessage::MSG_DO_ATTACK_OBJECT);
			attack->appendObjectIDArgument(target->getID());
			std::fprintf(stderr, "TEST frame %u: queued attack on object %u\n", frame, static_cast<unsigned>(target->getID()));
		}
	}
	else if (command.action == "moveobject")
	{
		if (const Object* target = findTarget(command); target && obj->getAI())
		{
			obj->getAI()->aiMoveToPosition(target->getPosition(), CMD_FROM_SCRIPT);
			std::fprintf(stderr, "TEST moveobject frame=%u object=%u target=%u\n", frame,
				static_cast<unsigned>(obj->getID()), static_cast<unsigned>(target->getID()));
		}
	}
	else if (command.action == "killspawned")
	{
		Int count = 0;
		for (Object* unit = TheGameLogic->getFirstObject(); unit; unit = unit->getNextObject())
		{
			if (unit->getProducerID() == obj->getID() && !unit->isEffectivelyDead())
			{
				unit->kill();
				++count;
			}
		}
		std::fprintf(stderr, "TEST killspawned frame=%u producer=%u count=%d\n", frame,
			static_cast<unsigned>(obj->getID()), count);
	}
	else if (command.action == "neutralstate")
	{
		const Drawable* draw = obj->getDrawable();
		const Object* target = obj->getAI() ? obj->getAI()->getCurrentVictim() : nullptr;
		std::fprintf(stderr, "TEST neutralstate frame=%u object=%s id=%u owner=%d target=%u capture=%d captured=%d vision=%.1f shroud=%.1f x=%.1f y=%.1f level=%d money=%u cmdset=%s\n",
			frame, obj->getTemplate()->getName().str(), static_cast<unsigned>(obj->getID()),
			obj->getControllingPlayer()->getPlayerIndex(), target ? static_cast<unsigned>(target->getID()) : 0,
			draw && draw->getModelConditionFlags().test(MODELCONDITION_START_CAPTURE),
			draw && draw->getModelConditionFlags().test(MODELCONDITION_CAPTURED), obj->getVisionRange(), obj->getShroudClearingRange(),
			obj->getPosition()->x, obj->getPosition()->y, static_cast<Int>(obj->getVeterancyLevel()),
			obj->getControllingPlayer()->getMoney()->countMoney(), obj->getCommandSetString().str());
	}
	else if (command.action == "health")
		printHealth(command, obj, frame);
	else if (command.action == "button")
		clickButton(command, frame);
	else if (command.action == "place")
		placeStructure(command, obj, frame);
	else if (command.action == "move" && command.hasLocation)
	{
		const Coord3D location = groundLocation(command);
		selectGroup(obj);
		GameMessage* move = TheMessageStream->appendMessage(GameMessage::MSG_DO_MOVETO);
		move->appendLocationArgument(location);
		std::fprintf(stderr, "TEST frame %u: move object %u to %.1f %.1f\n", frame, static_cast<unsigned>(obj->getID()),
			location.x, location.y);
	}
	else if (command.action == "camera" && command.hasLocation && TheTacticalView)
	{
		const Coord3D location = groundLocation(command);
		TheTacticalView->lookAt(&location);
		if (command.angle > 0.0f)
			TheTacticalView->setZoom(command.angle);
		std::fprintf(stderr, "TEST frame %u: camera at %.1f %.1f zoom %.2f\n", frame, location.x, location.y,
			TheTacticalView->getZoom());
	}
	else if (command.action == "buttons")
	{
		logButtons(obj, command, frame);
	}
	else if (command.action == "conditions")
	{
		if (const Drawable* draw = obj->getDrawable())
			std::fprintf(stderr, "TEST conditions %u %s ivory=%d stonework=%d\n", frame, command.target.c_str(),
				draw->getModelConditionFlags().test(MODELCONDITION_UPGRADE_IVORY_TOWER),
				draw->getModelConditionFlags().test(MODELCONDITION_UPGRADE_NUMENOR_STONEWORK));
	}
	else if (command.action == "fortress-collision")
	{
		const Coord3D position = *obj->getPosition();
		const GeometryInfo probe(GEOMETRY_SPHERE, TRUE, 2.0f, 2.0f, 2.0f);
		const Coord3D tower = {position.x, position.y, position.z + 100.0f};
		const Coord3D edge = {position.x + 30.0f, position.y, tower.z};
		PartitionFilterWouldCollide towerQuery(tower, probe, 0.0f, TRUE);
		PartitionFilterWouldCollide edgeQuery(edge, probe, 0.0f, TRUE);
		std::fprintf(stderr, "TEST fortress-collision %u %s center=%d edge=%d height=%.1f\n",
			frame, command.target.c_str(), towerQuery.allow(obj), edgeQuery.allow(obj),
			obj->getGeometryInfo().getMaxHeightAbovePosition());
	}
	else if (command.hasLocation && (command.action == "fortress-ground" ||
		command.action == "fortress-walker" || command.action == "fortress-walk"))
		fortressGround(command, obj, frame);
#if !RTS_GENERALS
	else if (command.action == "fortress-budget")
	{
		if (const ThingTemplate* unit = TheThingFactory->findTemplate(command.structure.c_str(), FALSE))
			RetailTeamBudgetFixture::check(obj, unit, frame, command.hasLocation);
	}
#endif
	else if (command.action == "shroudmap")
	{
		if (ThePartitionManager && obj->getControllingPlayer())
		{
			ThePartitionManager->shroudMapForPlayer(obj->getControllingPlayer()->getPlayerIndex());
			std::fprintf(stderr, "TEST frame %u: shroudmap for player %d\n", frame,
				obj->getControllingPlayer()->getPlayerIndex());
		}
	}
	else if (command.action == "reveal" || command.action == "unreveal")
	{
		if (ThePartitionManager && obj->getControllingPlayer())
		{
			const Int playerIndex = obj->getControllingPlayer()->getPlayerIndex();
			if (command.action == "reveal")
				ThePartitionManager->revealMapForPlayerPermanently(playerIndex);
			else
				ThePartitionManager->undoRevealMapForPlayerPermanently(playerIndex);
			std::fprintf(stderr, "TEST frame %u: %s for player %d\n", frame, command.action.c_str(), playerIndex);
		}
	}
	else if (command.action == "seen")
	{
		Player* viewer = ThePlayerList ? ThePlayerList->getLocalPlayer() : nullptr;
		if (viewer)
			std::fprintf(stderr, "TEST frame %u: seen %s id=%u by %d status %d relationship %d\n", frame,
				obj->getTemplate()->getName().str(), static_cast<unsigned>(obj->getID()), viewer->getPlayerIndex(),
				static_cast<int>(obj->getShroudedStatus(viewer->getPlayerIndex())),
				static_cast<int>(viewer->getRelationship(obj->getTeam())));
	}
	else if (command.action == "findterrain")
	{
		if (TheAI && TheAI->pathfinder() && ThePartitionManager && obj->getControllingPlayer())
		{
			const Int playerIndex = obj->getControllingPlayer()->getPlayerIndex();
			Bool foundWater = FALSE;
			Bool foundCliff = FALSE;
			for (Int cy = 40; cy < 360 && (!foundWater || !foundCliff); cy += 4)
			{
				for (Int cx = 40; cx < 360 && (!foundWater || !foundCliff); cx += 4)
				{
					PathfindCell* pCell = TheAI->pathfinder()->getCell(LAYER_GROUND, cx, cy);
					if (!pCell)
						continue;
					const Real wx = cx * PATHFIND_CELL_SIZE + 5.0f;
					const Real wy = cy * PATHFIND_CELL_SIZE + 5.0f;
					Int sx = 0, sy = 0;
					ThePartitionManager->worldToCell(wx, wy, &sx, &sy);
					const Int sh = static_cast<Int>(ThePartitionManager->getShroudStatusForPlayer(playerIndex, sx, sy));
					if (sh < CELLSHROUD_FOGGED)
						continue;
					if (!foundWater && pCell->getType() == PathfindCell::CELL_WATER)
					{
						std::fprintf(stderr, "TEST frame %u: found_water %.1f %.1f shroud %d\n", frame, wx, wy, sh);
						foundWater = TRUE;
					}
					if (!foundCliff && pCell->getType() == PathfindCell::CELL_CLIFF)
					{
						std::fprintf(stderr, "TEST frame %u: found_cliff %.1f %.1f shroud %d\n", frame, wx, wy, sh);
						foundCliff = TRUE;
					}
				}
			}
		}
	}
	else if (command.action == "cancelbuild")
	{
		selectGroup(obj);
		TheMessageStream->appendMessage(GameMessage::MSG_DOZER_CANCEL_CONSTRUCT);
		std::fprintf(stderr, "TEST frame %u: cancelbuild %s id=%u money=%u ghosts=%u\n", frame,
			obj->getTemplate()->getName().str(), static_cast<unsigned>(obj->getID()),
			obj->getControllingPlayer() ? obj->getControllingPlayer()->getMoney()->countMoney() : 0,
			TheInGameUI ? TheInGameUI->getPlannedBuildGhostCount() : 0);
	}
	else if (command.action == "height" && command.hasLocation)
	{
		const Real cx = command.location.x;
		const Real cy = command.location.y;
		std::fprintf(stderr, "TEST frame %u: height at %.1f %.1f center=%.4f n=%.4f s=%.4f e=%.4f w=%.4f\n", frame, cx, cy,
			TheTerrainLogic->getGroundHeight(cx, cy),
			TheTerrainLogic->getGroundHeight(cx, cy + 40.0f),
			TheTerrainLogic->getGroundHeight(cx, cy - 40.0f),
			TheTerrainLogic->getGroundHeight(cx + 40.0f, cy),
			TheTerrainLogic->getGroundHeight(cx - 40.0f, cy));
	}
	else if (command.action == "placecheck" && command.hasLocation)
	{
		const ThingTemplate* what = TheThingFactory->findTemplate(command.structure.c_str(), FALSE);
		if (!what)
			return;
		logBuildChecks(obj, what, command, frame);
	}
	else if (command.action == "startplace")
	{
		const ThingTemplate* what = TheThingFactory->findTemplate(command.structure.c_str(), FALSE);
		if (!what || !TheInGameUI)
			return;
		TheInGameUI->placeBuildAvailable(what, obj->getDrawable());
		std::fprintf(stderr, "TEST frame %u: startplace %s by object %u ghosts %u\n", frame, command.structure.c_str(),
			static_cast<unsigned>(obj->getID()), TheInGameUI->getPlannedBuildGhostCount());
	}
	else if (command.action == "construct" && command.hasLocation)
	{
		const ThingTemplate* what = TheThingFactory->findTemplate(command.structure.c_str(), FALSE);
		if (!what)
			return;
		logBuildChecks(obj, what, command, frame);
		const Coord3D location = groundLocation(command);
		selectGroup(obj);
		GameMessage* build = TheMessageStream->appendMessage(GameMessage::MSG_DOZER_CONSTRUCT);
		build->appendIntegerArgument(what->getTemplateID());
		build->appendLocationArgument(location);
		build->appendRealArgument(command.angle);
		std::fprintf(stderr, "TEST frame %u: construct %s by object %u at %.1f %.1f\n", frame, command.structure.c_str(),
			static_cast<unsigned>(obj->getID()), location.x, location.y);
	}
	else
		orderToPoint(command, obj, frame);
}
}

bool tracesCombat()
{
	static const bool traces = RetailFileLayers::enabled() && std::getenv("GENERALSX_TEST_COMMANDS") &&
		*std::getenv("GENERALSX_TEST_COMMANDS");
	return traces;
}

void logHordeDeath(const Object* object, const DamageInfo* damage)
{
	if (!tracesCombat())
		return;
	const Object* parent = object->getContainedBy();
	if (!HordeContain::get(object) && !HordeContain::get(parent))
		return;

	const Object* source = TheGameLogic->findObjectByID(damage->in.m_sourceID);
	std::fprintf(stderr, "TEST horde death %u %u %s anchor=%u source=%u(%s) damage=%s death=%s\n",
		TheGameLogic->getFrame(), static_cast<unsigned>(object->getID()), object->getTemplate()->getName().str(),
		static_cast<unsigned>(parent ? parent->getID() : INVALID_ID), static_cast<unsigned>(damage->in.m_sourceID),
		source ? source->getTemplate()->getName().str() : "-",
		DamageTypeFlags::getNameFromSingleBit(damage->in.m_damageType), TheDeathNames[damage->in.m_deathType]);
	std::fflush(stderr);
}

void runFrame(unsigned int frame)
{
	if (!s_loaded)
		load();
	const bool retail = RetailFileLayers::enabled();
	for (const Command& command : s_commands)
	{
		if (command.frame != frame)
			continue;
		if (retail)
		{
			if (command.action != "snapshot-share" && command.action != "snapshot-overlap" &&
				command.action != "snapshot-path-queue")
				execute(command, frame);
		}
		else if (command.action == "xpcrc" || command.action == "snapshot-crc" ||
			command.action == "logall" || command.action == "quit")
			execute(command, frame);
	}
	if (!s_tracked.empty())
		logTracked(frame);
}

void saveCheckpoint(unsigned int frame)
{
	if (!s_loaded || s_commands.empty())
		return;
	for (const Command& command : s_commands)
	{
		if (command.frame != frame)
			continue;
		if (command.action == "snapshot-save")
		{
			const SaveResult result = TheGameState->saveGame(command.structure.c_str(), UnicodeString(), SAVE_FILE_TYPE_NORMAL);
			std::fprintf(stderr, "TEST snapshot-save frame=%u code=%d\n", frame, result.saveCode);
		}
		else if (RetailFileLayers::enabled() && (command.action == "snapshot-share" || command.action == "snapshot-overlap"))
			execute(command, frame);
		else if (RetailFileLayers::enabled() && command.action == "snapshot-path-queue")
			runRetailSaveLoadPathTestCommand(command.action, findObject(command, true), frame);
	}
}
}
