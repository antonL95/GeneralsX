#include "PreRTS.h"

#define DEFINE_DEATH_NAMES
#include "GameLogic/Damage.h"

#include "GameLogic/RetailTestCommands.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

#include "Common/ActionManager.h"
#include "Common/BuildAssistant.h"
#include "Common/GameEngine.h"
#include "Common/INI.h"
#include "Common/MessageStream.h"
#include "Common/Money.h"
#include "Common/NameKeyGenerator.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/PlayerTemplate.h"
#include "Common/RetailFileLayers.h"
#include "Common/Science.h"
#include "Common/SpecialPower.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
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
#include "GameLogic/Module/SpecialPowerModule.h"
#if !RTS_GENERALS
#include "GameLogic/Module/RotwkAbilityModules.h"
#endif
#include "GameLogic/Module/ProductionUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Weapon.h"

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
		char structure[128] = {};
		float x = 0.0f;
		float y = 0.0f;
		float angle = 0.0f;
		const int fields = std::sscanf(line, "%u %31s %63s %127s %d %127s %f %f %f", &command.frame, action, player, target,
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
	for (int slot = 0; slot < MAX_COMMANDS_PER_SET + 3; ++slot)
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

void execute(const Command& command, unsigned int frame)
{
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
	if (command.action == "heroes")
	{
		dumpHeroes(command, frame);
		dumpProduction(command, frame);
		dumpHud(frame);
		return;
	}
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

	Object* obj = findObject(command);
	if (!obj)
	{
		std::fprintf(stderr, "TEST frame %u: no %s %d of %s\n", frame, command.target.c_str(), command.ordinal,
			command.player.c_str());
		return;
	}

	const Bool placeAtPoint = command.action == "place" && command.hasPlacementPoint;
	if (command.action == "hordequeue")
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
				std::fprintf(stderr, "TEST horde mode %u %u toggle=%d weapon=%s slot=%.3f,%.3f\n", frame,
					static_cast<unsigned>(member->getID()), member->testWeaponSetFlag(WEAPONSET_TOGGLE_1),
					weapon ? weapon->getName().str() : "-", slot.x, slot.y);
			}
			if (command.action == "hordekill")
				member->kill();
		}
	}
	else if (command.action == "build" || placeAtPoint || command.action == "queue")
	{
		const ThingTemplate* what = TheThingFactory->findTemplate(command.structure.c_str(), FALSE);
		if (!what)
			return;
		ProductionUpdateInterface* production = obj->getProductionUpdateInterface();
		if (command.action == "queue" && !production)
			return;
		GameMessage* select = TheMessageStream->appendMessage(GameMessage::MSG_CREATE_SELECTED_GROUP);
		select->appendBooleanArgument(TRUE);
		select->appendObjectIDArgument(obj->getID());
		GameMessage::Type type = GameMessage::MSG_FOUNDATION_CONSTRUCT;
		if (placeAtPoint)
			type = GameMessage::MSG_DOZER_CONSTRUCT;
		else if (command.action == "queue")
			type = GameMessage::MSG_QUEUE_UNIT_CREATE;
		GameMessage* build = TheMessageStream->appendMessage(type);
		build->appendIntegerArgument(what->getTemplateID());
		if (placeAtPoint)
		{
			const Coord3D target = {command.x, command.y, TheTerrainLogic->getGroundHeight(command.x, command.y)};
			build->appendLocationArgument(target);
			build->appendRealArgument(0.0f);
		}
		else if (command.action == "queue")
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
		std::fprintf(stderr, "TEST xp-state frame=%u object=%u %s rank=%d max=%d xp=%.3f progress=%.3f\n", frame,
			static_cast<unsigned>(obj->getID()), obj->getTemplate()->getName().str(), tracker->getRank(),
			ExperienceLevelStore::maximumRank(obj), tracker->getRetailExperience(), tracker->getLevelProgress());
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
	else if (command.action == "ability")
		dumpAbilities(command, obj, frame);
	else if (command.action == "aim")
		aimPendingCommand(command, obj, frame);
	else if (command.action == "power")
		pressPowerButton(command, obj, frame);
	else if (command.action == "objupgrade")
	{
		const UpgradeTemplate* upgrade = TheUpgradeCenter ? TheUpgradeCenter->findUpgrade(command.structure.c_str()) : nullptr;
		if (upgrade)
			obj->giveUpgrade(upgrade);
		std::fprintf(stderr, "TEST frame %u: objupgrade object %u %s %s\n", frame, static_cast<unsigned>(obj->getID()),
			command.structure.c_str(), upgrade ? "given" : "missing");
	}
#if !RTS_GENERALS
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
		std::fprintf(stderr, "TEST neutralstate frame=%u object=%s id=%u owner=%d target=%u capture=%d captured=%d vision=%.1f shroud=%.1f x=%.1f y=%.1f level=%d money=%u\n",
			frame, obj->getTemplate()->getName().str(), static_cast<unsigned>(obj->getID()),
			obj->getControllingPlayer()->getPlayerIndex(), target ? static_cast<unsigned>(target->getID()) : 0,
			draw && draw->getModelConditionFlags().test(MODELCONDITION_START_CAPTURE),
			draw && draw->getModelConditionFlags().test(MODELCONDITION_CAPTURED), obj->getVisionRange(), obj->getShroudClearingRange(),
			obj->getPosition()->x, obj->getPosition()->y, static_cast<Int>(obj->getVeterancyLevel()),
			obj->getControllingPlayer()->getMoney()->countMoney());
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
	if (!RetailFileLayers::enabled())
		return;
	if (!s_loaded)
		load();
	for (const Command& command : s_commands)
	{
		if (command.frame == frame)
			execute(command, frame);
	}
}
}
