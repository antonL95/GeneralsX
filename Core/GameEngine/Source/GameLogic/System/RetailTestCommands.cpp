#include "PreRTS.h"

#include "GameLogic/RetailTestCommands.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "Common/BuildAssistant.h"
#include "Common/GameEngine.h"
#include "Common/MessageStream.h"
#include "Common/NameKeyGenerator.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/PlayerTemplate.h"
#include "Common/RetailFileLayers.h"
#include "Common/Science.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "GameClient/ControlBar.h"
#include "GameClient/Drawable.h"
#include "GameClient/GadgetPushButton.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/InGameUI.h"
#include "GameClient/View.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/BehaviorModule.h"
#include "GameLogic/Module/UpgradeModule.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"

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
		if (fields < 5)
			continue;
		command.action = action;
		command.player = player;
		command.target = target;
		command.structure = structure;
		command.hasLocation = fields >= 8;
		command.location.set(x, y, 0.0f);
		command.angle = angle;
		s_commands.push_back(std::move(command));
	}
	std::fclose(file);
}

Object* findObject(const Command& command)
{
	std::vector<Object*> matches;
	for (Object* obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject())
	{
		const Player* owner = obj->getControllingPlayer();
		if (!owner || stricmp(obj->getTemplate()->getName().str(), command.target.c_str()) != 0)
			continue;
		if (stricmp(TheNameKeyGenerator->keyToName(owner->getPlayerNameKey()).str(), command.player.c_str()) == 0)
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
	const LegalBuildCode legal = TheBuildAssistant->isLocationLegalToBuild(&location, what, command.angle,
		BuildAssistant::TERRAIN_RESTRICTIONS | BuildAssistant::CLEAR_PATH | BuildAssistant::NO_OBJECT_OVERLAP |
			BuildAssistant::USE_QUICK_PATHFIND,
		builder, builder->getControllingPlayer());
	std::fprintf(stderr, "TEST frame %u: check %s canMake %d possible %d legal %d at %.1f %.1f cost %d money %u\n", frame,
		what->getName().str(), static_cast<int>(TheBuildAssistant->canMakeUnit(builder, what)),
		TheBuildAssistant->isPossibleToMakeUnit(builder, what) ? 1 : 0, static_cast<int>(legal), location.x, location.y,
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
	for (Int i = 0; i < MAX_COMMANDS_PER_SET; ++i)
	{
		const CommandButton* button = set->getCommandButton(i);
		if (!button)
			continue;
		const ThingTemplate* what = button->getThingTemplate();
		std::fprintf(stderr, "TEST frame %u: slot %d %s type %d object %s\n", frame, i + 1, button->getName().str(),
			static_cast<int>(button->getCommandType()), what ? what->getName().str() : "-");
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
	ai->aiRepair(target, CMD_FROM_AI);
	std::fprintf(stderr, "TEST frame %u: repair object %u by %u\n", frame, static_cast<unsigned>(target->getID()),
		static_cast<unsigned>(obj->getID()));
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

	Object* obj = findObject(command);
	if (!obj)
	{
		std::fprintf(stderr, "TEST frame %u: no %s %d of %s\n", frame, command.target.c_str(), command.ordinal,
			command.player.c_str());
		return;
	}

	if (command.action == "build")
	{
		const ThingTemplate* what = TheThingFactory->findTemplate(command.structure.c_str(), FALSE);
		if (!what)
			return;
		GameMessage* select = TheMessageStream->appendMessage(GameMessage::MSG_CREATE_SELECTED_GROUP);
		select->appendBooleanArgument(TRUE);
		select->appendObjectIDArgument(obj->getID());
		GameMessage* build = TheMessageStream->appendMessage(GameMessage::MSG_FOUNDATION_CONSTRUCT);
		build->appendIntegerArgument(what->getTemplateID());
		std::fprintf(stderr, "TEST frame %u: build %s on object %u\n", frame, command.structure.c_str(),
			static_cast<unsigned>(obj->getID()));
	}
	else if (command.action == "kill")
	{
		std::fprintf(stderr, "TEST frame %u: kill object %u (%s)\n", frame, static_cast<unsigned>(obj->getID()),
			command.target.c_str());
		obj->kill();
	}
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
	else if (command.action == "log")
	{
		logObject(obj, frame);
	}
	else if (command.action == "select")
	{
		selectObject(obj);
		std::fprintf(stderr, "TEST frame %u: select object %u (%s)\n", frame, static_cast<unsigned>(obj->getID()),
			command.target.c_str());
	}
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
		std::fprintf(stderr, "TEST frame %u: camera at %.1f %.1f\n", frame, location.x, location.y);
	}
	else if (command.action == "buttons")
	{
		logButtons(obj, command, frame);
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
