#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include "Common/MapObject.h"
#include "Common/MessageStream.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/SpecialPower.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "GameClient/CommandXlat.h"
#include "GameClient/ControlBar.h"
#include "GameClient/Drawable.h"
#include "GameClient/GadgetPushButton.h"
#include "GameClient/GameClient.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/InGameUI.h"
#include "GameClient/Keyboard.h"
#include "GameLogic/AI.h"
#include "GameLogic/AttributeModifierStore.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/AttributeModifierHelper.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/DoCommandUpgrade.h"
#include "GameLogic/Module/LifetimeUpdate.h"
#include "GameLogic/Module/RotwkAbilityModules.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Module/SpecialAbilityUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"

#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace
{
constexpr Int ACTOR_COUNT = 6;
constexpr Int CASTER = 0;
constexpr Int ALLY = 1;
constexpr Int ENEMY = 2;
constexpr Int STRUCTURE = 3;
constexpr Int HERO = 4;
constexpr Int CAPTURE = 5;

class IdleKeyboard final : public Keyboard
{
	Bool getCapsState() override { return FALSE; }
	void getKey(KeyboardIO* key) override { key->key = KEY_NONE; }
};

struct Actor
{
	ObjectID id = INVALID_ID;
	std::set<std::string> modifiers;
	ObjectStatusMaskType status;
	Real health = 0.0f;
	Real maxHealth = 0.0f;
	Coord3D position = { 0.0f, 0.0f, 0.0f };
	const Player* owner = nullptr;
	std::set<std::string> newModifiers;
	std::set<std::string> newStatus;
	Real lowestHealth = 0.0f;
	Real highestHealth = 0.0f;
	Bool died = FALSE;
	Bool moved = FALSE;
	Bool converted = FALSE;
};

struct AbilityState
{
	Bool observed = FALSE;
	Bool active = FALSE;
	UnsignedInt start = 0;
	UnsignedInt elapsed = 0;
	double bound = 0;
};

struct Case
{
	Bool active = FALSE;
	Bool clicked = FALSE;
	Int id = 0;
	std::string templateName;
	std::string button;
	std::string power;
	std::string result = "no_click";
	Int type = 0;
	Actor actors[ACTOR_COUNT];
	Bool noWindow = FALSE;
	std::set<ObjectID> previous;
	const ThingTemplate* templateAtClick = nullptr;
	ModelConditionFlags conditions;
	WeaponSetFlags weaponSet;
	AsciiString commandSet;
	std::set<std::string> animation;
	std::set<std::string> objects;
	std::set<ObjectID> lingering;
	std::set<std::string> flags;
	std::string remaining;
	const SpecialPowerTemplate* powerTemplate = nullptr;
	Bool recharged = FALSE;
	Bool running = FALSE;
	AbilityState abilityStates[2];
	std::set<std::string> persisting;
};

Case s_case;
std::set<ObjectID> s_sessionObjects;
UpgradeMaskType s_sessionUpgrades;
Bool s_sessionRecorded = FALSE;

void destroyObjectsOutside(const std::set<ObjectID>& kept)
{
	for (Object* object = TheGameLogic->getFirstObject(); object;)
	{
		Object* next = object->getNextObject();
		if (!kept.count(object->getID()))
			TheGameLogic->destroyObject(object);
		object = next;
	}
}

void restorePlayerUpgrades(Player* player)
{
	std::vector<const UpgradeTemplate*> added;
	for (const UpgradeTemplate* upgrade = TheUpgradeCenter->firstUpgradeTemplate(); upgrade; upgrade = upgrade->friend_getNext())
		if (upgrade->getUpgradeType() == UPGRADE_TYPE_PLAYER && player->getCompletedUpgradeMask().testForAny(upgrade->getUpgradeMask()) &&
			!s_sessionUpgrades.testForAny(upgrade->getUpgradeMask()))
			added.push_back(upgrade);
	for (const UpgradeTemplate* upgrade : added)
		player->removeUpgrade(upgrade);
}

std::string joined(const std::set<std::string>& values)
{
	std::string result;
	for (const std::string& value : values)
		result += (result.empty() ? "" : ",") + value;
	return result.empty() ? "-" : result;
}

std::set<ObjectID> currentObjectIds()
{
	std::set<ObjectID> ids;
	for (const Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
		ids.insert(object->getID());
	return ids;
}

std::set<std::string> modifierNames(const Object* object)
{
	std::set<std::string> names;
	if (const AttributeModifierHelper* helper = object->getAttributeModifierHelper())
		for (const auto& active : helper->getStack().active())
			if (const ModifierListTemplate* list = TheAttributeModifierStore->get(active.list))
				names.insert(list->m_name.str());
	return names;
}

ModelConditionFlags conditionsOf(const Object* object)
{
	return object && object->getDrawable() ? object->getDrawable()->getModelConditionFlags() : ModelConditionFlags();
}

Object* createActor(const char* templateName, Player* player, Real x, Real y)
{
	const ThingTemplate* thing = TheThingFactory->findTemplate(templateName, FALSE);
	if (!thing || !player)
		return nullptr;
	Object* object = TheThingFactory->newObject(thing, player->getDefaultTeam());
	Coord3D position;
	position.set(x, y, TheTerrainLogic->getGroundHeight(x, y));
	object->setPosition(&position);
	return object;
}

void setup(const RetailTestCommands::Context& ctx)
{
	s_case = Case();
	char ally[128] = {};
	char enemy[128] = {};
	char structure[128] = {};
	char enemyPlayer[64] = {};
	if (std::sscanf(ctx.argument, "%127[^,],%127[^,],%127[^,],%63s", ally, enemy, structure, enemyPlayer) != 4 || !ctx.player)
		return;
	if (!s_sessionRecorded)
	{
		s_sessionRecorded = TRUE;
		s_sessionObjects = currentObjectIds();
		s_sessionUpgrades = ctx.player->getCompletedUpgradeMask();
	}
	destroyObjectsOutside(s_sessionObjects);
	restorePlayerUpgrades(ctx.player);
	s_case.active = TRUE;
	s_case.id = ctx.ordinal;
	s_case.templateName = ctx.targetName;
	Player* opponent = ctx.findPlayer(enemyPlayer);
	Player* neutral = ThePlayerList->getNeutralPlayer();
	const Real x = ctx.position.x;
	const Real y = ctx.position.y;
	Object* objects[ACTOR_COUNT] = {
		createActor(ctx.targetName, ctx.player, x, y),
		createActor(ally, ctx.player, x - 50.0f, y),
		createActor(enemy, opponent, x + 80.0f, y),
		createActor(structure, opponent, x + 80.0f, y + 90.0f),
		createActor("AbilitySweepHero", opponent, x + 90.0f, y - 35.0f),
		createActor("AbilitySweepCapturable", neutral, x + 55.0f, y + 55.0f)
	};
	for (Int i = 0; i < ACTOR_COUNT; ++i)
		if (objects[i])
		{
			s_case.actors[i].id = objects[i]->getID();
			if (i == ENEMY || i == STRUCTURE || i == HERO)
				objects[i]->setStatus(MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_NO_ATTACK_FROM_AI));
		}
	if (Object* caster = objects[CASTER])
	{
		caster->getExperienceTracker()->setMinRetailRank(10);
		if (AIUpdateInterface* ai = caster->getAI())
			ai->setAttitude(ATTITUDE_PASSIVE);
	}
	else
		s_case.result = "rejected:cannot_create";
	if (Object* friendly = objects[ALLY])
	{
		DamageInfo damage;
		damage.in.m_damageType = DAMAGE_UNRESISTABLE;
		damage.in.m_amount = friendly->getBodyModule()->getMaxHealth() * 0.5f;
		friendly->attemptDamage(&damage);
	}
}

const CommandButton* findInCommandSet(const Object* caster, const char* name, Int* outSlot = nullptr)
{
	const CommandSet* set = TheControlBar->findCommandSet(caster->getCommandSetString());
	for (Int slot = 0; set && slot < CommandButtonRules::RETAIL_COMMANDS_PER_SET; ++slot)
	{
		const CommandButton* button = set->getCommandButton(slot);
		if (button && button->getName().compareNoCase(name) == 0)
		{
			if (outSlot)
				*outSlot = slot;
			return button;
		}
	}
	return nullptr;
}

void grantRequirements(Object* caster, const CommandButton* button)
{
	Player* player = caster->getControllingPlayer();
	std::vector<const UpgradeTemplate*> upgrades = button->getNeededUpgrades();
	if (BitIsSet(button->getOptions(), NEED_UPGRADE) && button->getUpgradeTemplate() &&
		button->getCommandType() != GUI_COMMAND_OBJECT_UPGRADE && button->getCommandType() != GUI_COMMAND_PLAYER_UPGRADE)
		upgrades.push_back(button->getUpgradeTemplate());
	for (const UpgradeTemplate* upgrade : upgrades)
	{
		if (!upgrade)
			continue;
		if (upgrade->getUpgradeType() == UPGRADE_TYPE_PLAYER)
			player->addUpgrade(upgrade, UPGRADE_STATUS_COMPLETE);
		else if (!caster->hasUpgrade(upgrade))
			caster->giveUpgrade(upgrade);
	}
	const SpecialPowerTemplate* power = button->getSpecialPowerTemplate();
	if (power && power->getRequiredScience() != SCIENCE_INVALID)
		player->grantScience(power->getRequiredScience());
}

void grantCommandSetUpgrades(Object* caster)
{
	const CommandSet* set = TheControlBar->findCommandSet(caster->getCommandSetString());
	for (Int slot = 0; set && slot < CommandButtonRules::RETAIL_COMMANDS_PER_SET; ++slot)
	{
		const CommandButton* button = set->getCommandButton(slot);
		if (button && button->getCommandType() == GUI_COMMAND_OBJECT_UPGRADE && button->getUpgradeTemplate())
		{
			grantRequirements(caster, button);
			if (!caster->hasUpgrade(button->getUpgradeTemplate()))
				caster->giveUpgrade(button->getUpgradeTemplate());
		}
	}
}

const CommandButton* findSamePower(const Object* caster, const char* name)
{
	const CommandButton* wanted = TheControlBar->findCommandButton(name);
	const CommandSet* set = TheControlBar->findCommandSet(caster->getCommandSetString());
	if (!wanted || !wanted->getSpecialPowerTemplate())
		return nullptr;
	for (Int slot = 0; set && slot < CommandButtonRules::RETAIL_COMMANDS_PER_SET; ++slot)
	{
		const CommandButton* button = set->getCommandButton(slot);
		if (button && button->getSpecialPowerTemplate() == wanted->getSpecialPowerTemplate())
			return button;
	}
	return nullptr;
}

void prepare(const RetailTestCommands::Context& ctx)
{
	Object* caster = TheGameLogic->findObjectByID(s_case.actors[CASTER].id);
	if (!caster)
		return;
	s_case.button = ctx.argument;
	const CommandButton* button = findInCommandSet(caster, ctx.argument);
	if (!button && (button = findSamePower(caster, ctx.argument)))
	{
		s_case.button = button->getName().str();
		s_case.flags.insert(std::string("level_button.") + s_case.button);
	}
	if (!button)
	{
		grantCommandSetUpgrades(caster);
		button = findInCommandSet(caster, ctx.argument);
	}
	if (!button)
	{
		s_case.result = "rejected:not_in_command_set";
		return;
	}
	grantRequirements(caster, button);
	const CommandButton* effective = button->resolveRetailToggleButton(caster);
	if (effective && effective != button)
		grantRequirements(caster, effective);
	TheInGameUI->deselectAllDrawables();
	ctx.selectObject(caster);
}

template <class Visitor>
void forEachButtonWindow(Visitor visit)
{
	static const struct { const char* format; Int count; } families[] = {
		{ "ControlBar.wnd:RingButton%d", 6 }, { "ControlBar.wnd:SideButton%02d", MAX_COMMANDS_PER_SET },
		{ "ControlBar.wnd:TrayButton%d", 3 }, { "ControlBar.wnd:RadialButton%02d", 12 }
	};
	for (const auto& family : families)
		for (Int index = 1; index <= family.count; ++index)
		{
			AsciiString windowName;
			windowName.format(family.format, index);
			GameWindow* window = TheWindowManager->winGetWindowFromId(nullptr, TheNameKeyGenerator->nameToKey(windowName));
			if (const CommandButton* button = window ? static_cast<const CommandButton*>(GadgetButtonGetData(window)) : nullptr)
				visit(window, button);
		}
}

GameWindow* findButtonWindow(const char* name)
{
	GameWindow* shown = nullptr;
	GameWindow* hidden = nullptr;
	forEachButtonWindow([&](GameWindow* window, const CommandButton* button) {
		if (button->getName().compareNoCase(name) != 0)
			return;
		if (window->winIsHidden())
			hidden = window;
		else if (!shown)
			shown = window;
	});
	return shown ? shown : hidden;
}

const char* availabilityName(CommandAvailability availability)
{
	switch (availability)
	{
		case COMMAND_RESTRICTED: return "restricted";
		case COMMAND_HIDDEN: return "hidden";
		case COMMAND_NOT_READY: return "not_ready";
		case COMMAND_CANT_AFFORD: return "cant_afford";
		case COMMAND_PASSIVE: return "passive";
		default: return "other";
	}
}

void snapshot(Actor& actor)
{
	const Object* object = TheGameLogic->findObjectByID(actor.id);
	if (!object)
		return;
	actor.modifiers = modifierNames(object);
	actor.status = object->getStatusBits();
	actor.health = object->getBodyModule()->getHealth();
	actor.maxHealth = object->getBodyModule()->getMaxHealth();
	actor.lowestHealth = actor.highestHealth = actor.health;
	actor.position = *object->getPosition();
	actor.owner = object->getControllingPlayer();
}

std::string aim(Object* caster, const CommandButton* pending);

std::string issueWithoutWindow(Object* caster, const CommandButton* button)
{
	if (button->getCommandType() == GUI_COMMAND_SPECIAL_POWER_TOGGLE)
		button = button->resolveRetailToggleButton(caster);
	if (BitIsSet(button->getOptions(), COMMAND_OPTION_NEED_TARGET))
		return aim(caster, button);
	const SpecialPowerTemplate* power = button->getSpecialPowerTemplate();
	switch (button->getCommandType())
	{
		case GUI_COMMAND_SPECIAL_POWER:
		case GUI_COMMAND_SPECIAL_POWER_TOGGLE:
		{
			if (!power)
				return "rejected:no_power";
			GameMessage* message = TheMessageStream->appendMessage(GameMessage::MSG_DO_SPECIAL_POWER);
			message->appendIntegerArgument(power->getID());
			message->appendIntegerArgument(button->getOptions());
			message->appendObjectIDArgument(button->getCommandType() == GUI_COMMAND_SPECIAL_POWER_TOGGLE ? caster->getID() : INVALID_ID);
			return "accepted";
		}
		case GUI_COMMAND_FIRE_WEAPON:
		{
			GameMessage* message = TheMessageStream->appendMessage(GameMessage::MSG_DO_WEAPON);
			message->appendIntegerArgument(button->getWeaponSlot());
			message->appendIntegerArgument(button->getMaxShotsToFire());
			return "accepted";
		}
		case GUI_COMMAND_SWITCH_WEAPON:
			TheMessageStream->appendMessage(GameMessage::MSG_SWITCH_WEAPONS)->appendIntegerArgument(button->getWeaponSlot());
			return "accepted";
		case GUI_COMMAND_TOGGLE_WEAPONSET:
		case GUI_COMMAND_ONE_RING:
		case GUI_COMMAND_WAKE_AUTO_PICKUP:
		{
			const CommandSet* set = TheControlBar->findCommandSet(caster->getCommandSetString());
			for (Int slot = 0; set && slot < CommandButtonRules::RETAIL_COMMANDS_PER_SET; ++slot)
				if (set->getCommandButton(slot) == button)
				{
					GameMessage* message = TheMessageStream->appendMessage(GameMessage::MSG_RETAIL_HORDE_COMMAND);
					message->appendObjectIDArgument(caster->getID());
					message->appendIntegerArgument(slot);
					message->appendIntegerArgument(-1);
					return "accepted";
				}
			return "rejected:not_in_command_set";
		}
		default:
			return "rejected:unsupported_command";
	}
}

void recordBaseline(Object* caster)
{
	for (Actor& actor : s_case.actors)
		snapshot(actor);
	s_case.conditions = conditionsOf(caster);
	s_case.weaponSet = caster->getWeaponSetFlags();
	s_case.templateAtClick = caster->getTemplate();
	s_case.commandSet = caster->getCommandSetString();
	s_case.previous = currentObjectIds();
	s_case.clicked = TRUE;
}

std::string aim(Object* caster, const CommandButton* pending)
{
	const UnsignedInt options = pending->getOptions();
	const SpecialPowerTemplate* power = pending->getSpecialPowerTemplate();
	const Bool weapon = pending->getCommandType() == GUI_COMMAND_FIRE_WEAPON;
	if (!power && !weapon)
		return "rejected:unsupported_target_command";
	Object* target = nullptr;
	if (BitIsSet(options, COMMAND_OPTION_NEED_OBJECT_TARGET))
		for (const Int candidate : { ENEMY, HERO, ALLY, STRUCTURE, CASTER })
		{
			Object* object = TheGameLogic->findObjectByID(s_case.actors[candidate].id);
			if (object && pending->isValidObjectTarget(caster, object) &&
				(!power || CommandButton::isRetailPowerObjectTargetAllowed(caster, power, object)))
			{
				target = object;
				break;
			}
		}
	if (!target && !BitIsSet(options, NEED_TARGET_POS))
		return "rejected:no_valid_target";
	const Object* enemy = TheGameLogic->findObjectByID(s_case.actors[ENEMY].id);
	const Object* filtered = nullptr;
	if (!target && power && power->hasRetailFlag(SpecialPowerTemplate::RETAIL_NEEDS_OBJECT_FILTER))
		for (const Int candidate : { ENEMY, STRUCTURE, HERO, CAPTURE, ALLY })
			if (const Object* object = TheGameLogic->findObjectByID(s_case.actors[candidate].id);
				object && power->getObjectFilter().matchesObject(caster, object))
			{
				filtered = object;
				break;
			}
	const Coord3D point = target ? *target->getPosition() : filtered ? *filtered->getPosition() :
		enemy ? *enemy->getPosition() : *caster->getPosition();
	GameMessage* message = nullptr;
	if (weapon)
	{
		message = TheMessageStream->appendMessage(target ? GameMessage::MSG_DO_WEAPON_AT_OBJECT : GameMessage::MSG_DO_WEAPON_AT_LOCATION);
		message->appendIntegerArgument(pending->getWeaponSlot());
		if (target)
			message->appendObjectIDArgument(target->getID());
		else
			message->appendLocationArgument(point);
		message->appendIntegerArgument(pending->getMaxShotsToFire());
		if (!target)
			message->appendObjectIDArgument(INVALID_ID);
		return "accepted";
	}
	if (target)
	{
		message = TheMessageStream->appendMessage(GameMessage::MSG_DO_SPECIAL_POWER_AT_OBJECT);
		message->appendIntegerArgument(power->getID());
		message->appendObjectIDArgument(target->getID());
	}
	else
	{
		message = TheMessageStream->appendMessage(GameMessage::MSG_DO_SPECIAL_POWER_AT_LOCATION);
		message->appendIntegerArgument(power->getID());
		message->appendLocationArgument(point);
		message->appendRealArgument(INVALID_ANGLE);
		message->appendObjectIDArgument(INVALID_ID);
	}
	message->appendIntegerArgument(options);
	message->appendObjectIDArgument(INVALID_ID);
	return "accepted";
}

Bool triggerDoCommandUpgrade(Object* caster, const CommandButton* button)
{
	Player* player = caster->getControllingPlayer();
	const ModuleInfo& modules = caster->getTemplate()->getBehaviorModuleInfo();
	for (Int i = 0; i < modules.getCount(); ++i)
	{
		if (modules.getNthName(i) != "DoCommandUpgrade")
			continue;
		const auto* data = static_cast<const DoCommandUpgradeModuleData*>(modules.getNthData(i));
		if (data->m_grantCommand.compareNoCase(button->getName()) != 0)
			continue;
		const auto& mux = data->m_upgradeMuxData;
		Bool granted = FALSE;
		for (const AsciiString& name : mux.m_activationUpgradeNames.empty() ? mux.m_triggerUpgradeNames : mux.m_activationUpgradeNames)
		{
			const UpgradeTemplate* upgrade = TheUpgradeCenter->findUpgrade(name);
			if (!upgrade)
				continue;
			if (upgrade->getUpgradeType() == UPGRADE_TYPE_PLAYER)
				player->addUpgrade(upgrade, UPGRADE_STATUS_COMPLETE);
			else if (!caster->hasUpgrade(upgrade))
				caster->giveUpgrade(upgrade);
			granted = TRUE;
		}
		if (granted)
			return TRUE;
	}
	return FALSE;
}

std::string triggerHiddenSlot(Object* caster, const CommandButton* button)
{
	if (triggerDoCommandUpgrade(caster, button))
	{
		s_case.flags.insert("trigger.docommandupgrade");
		return "accepted";
	}
	const SpecialPowerTemplate* power = button->getSpecialPowerTemplate();
	const SpecialPowerType spType = power ? power->getSpecialPowerType() : SPECIAL_INVALID;
	if (spType == SPECIAL_INFANTRY_CAPTURE_BUILDING || spType == SPECIAL_BLACKLOTUS_CAPTURE_BUILDING)
	{
		Object* target = TheGameLogic->findObjectByID(s_case.actors[CAPTURE].id);
		if (!target || !target->getDrawable())
			return "rejected:no_capture_target";
		s_case.flags.insert("trigger.context");
		TheInGameUI->setGUICommand(nullptr);
		IdleKeyboard idleKeyboard;
		Keyboard* keyboard = std::exchange(TheKeyboard, TheKeyboard ? TheKeyboard : &idleKeyboard);
		const auto order = TheGameClient->evaluateContextCommand(target->getDrawable(), target->getPosition(),
			CommandTranslator::DO_COMMAND);
		TheKeyboard = keyboard;
		return order == GameMessage::MSG_DO_SPECIAL_POWER_AT_OBJECT ? "accepted" : "rejected:context_capture_failed";
	}
	if (spType == SPECIAL_GRAB_PASSENGER && BitIsSet(button->getOptions(), ALLOW_SHRUBBERY_TARGET))
	{
		const MapObject* closest = nullptr;
		Real best = 900.0f * 900.0f;
		for (const MapObject* map = MapObject::getFirstMapObject(); map; map = map->getNext())
		{
			const ThingTemplate* type = map->getThingTemplate();
			const Coord3D* pos = map->getLocation();
			if (!type || !type->isKindOf(KINDOF_TREE) || !type->isKindOf(KINDOF_CLUB) ||
				!type->getRotWKObjectData().isGrabbable || !TheTerrainLogic->isNearMapTree(*pos, 0.0f))
				continue;
			const Real dx = pos->x - caster->getPosition()->x;
			const Real dy = pos->y - caster->getPosition()->y;
			const Real distance = dx * dx + dy * dy;
			if (distance < best)
			{
				best = distance;
				closest = map;
			}
		}
		if (!closest)
			return "rejected:no_grabbable_tree";
		Coord3D pos = *closest->getLocation();
		pos.z += TheTerrainLogic->getGroundHeight(pos.x, pos.y);
		s_case.flags.insert("trigger.context");
		TheInGameUI->setGUICommand(nullptr);
		IdleKeyboard idleKeyboard;
		Keyboard* keyboard = std::exchange(TheKeyboard, TheKeyboard ? TheKeyboard : &idleKeyboard);
		const auto order = TheGameClient->evaluateContextCommand(nullptr, &pos, CommandTranslator::DO_COMMAND);
		TheKeyboard = keyboard;
		return order == GameMessage::MSG_DO_SPECIAL_POWER_AT_OBJECT ? "accepted" : "rejected:context_grab_tree_failed";
	}
	return "hidden_by_design";
}

void describeNotReady(Object* caster)
{
	s_case.flags.insert("rank." + std::to_string(caster->getExperienceTracker()->getRank()));
	if (s_case.powerTemplate)
		if (const SpecialPowerModuleInterface* module = caster->getSpecialPowerModule(s_case.powerTemplate))
			s_case.flags.insert(module->isReady() ? "power_ready" : "power_paused");
	for (const UpgradeTemplate* upgrade = TheUpgradeCenter->firstUpgradeTemplate(); upgrade; upgrade = upgrade->friend_getNext())
		if (upgrade->getUpgradeType() != UPGRADE_TYPE_PLAYER && caster->hasUpgrade(upgrade))
			s_case.flags.insert(std::string("has.") + upgrade->getUpgradeName().str());
}

void click(const RetailTestCommands::Context& ctx)
{
	Object* caster = TheGameLogic->findObjectByID(s_case.actors[CASTER].id);
	if (!caster || s_case.result != "no_click")
		return;
	Int slot = -1;
	const CommandButton* button = findInCommandSet(caster, s_case.button.c_str(), &slot);
	if (!button)
		return;
	for (const Int idx : { ENEMY, STRUCTURE, HERO })
		if (Object* target = TheGameLogic->findObjectByID(s_case.actors[idx].id))
			target->clearStatus(MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_NO_ATTACK_FROM_AI));
	s_case.type = button->getCommandType();
	const CommandButton* effective = button->resolveRetailToggleButton(caster);
	s_case.powerTemplate = effective ? effective->getSpecialPowerTemplate() : nullptr;
	if (s_case.powerTemplate)
	{
		s_case.power = s_case.powerTemplate->getName().str();
		if (SpecialPowerModuleInterface* module = caster->getSpecialPowerModule(s_case.powerTemplate))
		{
			if (!module->isReady())
				module->setReadyFrame(ctx.frame);
		}
		else
			s_case.flags.insert("no_power_module");
	}
	GameWindow* window = findButtonWindow(s_case.button.c_str());
	if (!window || window->winIsHidden())
	{
		const Bool mounted = caster->testWeaponSetFlag(WEAPONSET_MOUNTED);
		if (!button->isShowButton() || !button->isModelConditionAllowed(caster) ||
			(mounted && button->hasRotwkOption(CommandButtonRules::ROTWK_UNMOUNTED_ONLY)) ||
			(!mounted && button->hasRotwkOption(CommandButtonRules::ROTWK_MOUNTED_ONLY)))
		{
			s_case.result = "not_shown";
			return;
		}
		s_case.noWindow = TRUE;
		if (slot >= 6 && button->isInPalantir())
		{
			recordBaseline(caster);
			s_case.result = triggerHiddenSlot(caster, button);
			if (s_case.result != "accepted")
				s_case.clicked = FALSE;
			return;
		}
	}
	const CommandAvailability availability = s_case.noWindow
		? TheControlBar->queryRetailCommandAvailability(button, caster) : TheControlBar->getRetailButtonAvailability(window);
	if (availability != COMMAND_AVAILABLE && availability != COMMAND_ACTIVE)
	{
		if (availability == COMMAND_NOT_READY)
			describeNotReady(caster);
		s_case.result = availability == COMMAND_PASSIVE ? "passive" : std::string("rejected:") + availabilityName(availability);
		return;
	}
	if (!s_case.noWindow && !BitIsSet(window->winGetStatus(), WIN_STATUS_ENABLED))
	{
		s_case.result = "rejected:button_disabled";
		return;
	}
	recordBaseline(caster);
	TheInGameUI->setGUICommand(nullptr);
	if (s_case.noWindow)
	{
		s_case.result = issueWithoutWindow(caster, button);
		return;
	}
	TheWindowManager->winSendSystemMsg(window->winGetOwner(), GBM_SELECTED, (WindowMsgData)window, window->winGetWindowId());
	s_case.result = "accepted";
	if (const CommandButton* pending = TheInGameUI->getGUICommand())
	{
		s_case.result = aim(caster, pending);
		TheInGameUI->setGUICommand(nullptr);
	}
}

void watchActor(Actor& actor, UnsignedInt frame)
{
	const Object* object = TheGameLogic->findObjectByID(actor.id);
	if (!object || object->isEffectivelyDead())
	{
		actor.died = actor.id != INVALID_ID;
		return;
	}
	if (const AttributeModifierHelper* helper = object->getAttributeModifierHelper())
		for (const auto& applied : helper->getStack().active())
		{
			const ModifierListTemplate* list = TheAttributeModifierStore->get(applied.list);
			if (!list || actor.modifiers.count(list->m_name.str()))
				continue;
			actor.newModifiers.insert(list->m_name.str());
			if (!applied.endFrame || applied.endFrame > frame)
				s_case.running = TRUE;
		}
	const ObjectStatusMaskType status = object->getStatusBits();
	for (Int bit = 0; bit < status.size(); ++bit)
		if (status.test(bit) && !actor.status.test(bit))
			actor.newStatus.insert(ObjectStatusMaskType::getNameFromSingleBit(bit));
	const Real health = object->getBodyModule()->getHealth();
	actor.lowestHealth = min(actor.lowestHealth, health);
	actor.highestHealth = max(actor.highestHealth, health);
	const Real dx = object->getPosition()->x - actor.position.x;
	const Real dy = object->getPosition()->y - actor.position.y;
	actor.moved = actor.moved || dx * dx + dy * dy > 400.0f;
	actor.converted = actor.converted || object->getControllingPlayer() != actor.owner;
}

void watchAbilityState(AbilityState& state, Bool active, double bound, UnsignedInt frame)
{
	if (active && !state.active)
		state.start = frame;
	state.observed = TRUE;
	state.active = active;
	state.bound = bound;
	state.elapsed = active ? frame - state.start : 0;
	s_case.running = s_case.running || active;
}

void watch(const RetailTestCommands::Context& ctx)
{
	if (ctx.frame % 60 == 0)
		std::fprintf(stderr, "TEST ability-heartbeat frame=%u case=%d\n", ctx.frame, s_case.id);
	if (!s_case.clicked || s_case.result != "accepted")
		return;
	s_case.remaining.clear();
	s_case.running = FALSE;
	Bool abilityFound[2] = { FALSE, FALSE };
	for (Int i = 0; i < ACTOR_COUNT; ++i)
		watchActor(s_case.actors[i], ctx.frame);
	if (Object* caster = TheGameLogic->findObjectByID(s_case.actors[CASTER].id))
	{
		const ModelConditionFlags current = conditionsOf(caster);
		for (Int bit = 0; bit < current.size(); ++bit)
		{
			if (!current.test(bit) || s_case.conditions.test(bit))
				continue;
			const std::string name = ModelConditionFlags::getNameFromSingleBit(bit);
			if (name == "SELECTED" || name == "MOVING" || name.rfind("TURN_", 0) == 0 || name == "ACCELERATE" || name == "DECELERATE" || name == "ENGAGED" || name.rfind("EMOTION_", 0) == 0)
				continue;
			s_case.animation.insert(name);
		}
		if (caster->getTemplate() != s_case.templateAtClick)
			s_case.flags.insert(std::string("replaced.") + caster->getTemplate()->getName().str());
		if (caster->getWeaponSetFlags() != s_case.weaponSet)
			s_case.flags.insert("weaponset");
		if (caster->getCommandSetString() != s_case.commandSet)
			s_case.flags.insert("commandset");
		if (s_case.powerTemplate)
		{
			if (const SpecialPowerModuleInterface* module = caster->getSpecialPowerModule(s_case.powerTemplate))
				s_case.recharged = s_case.recharged || !module->isReady();
			if (const SpecialAbilityUpdate* ability = caster->findSpecialAbilityUpdate(s_case.powerTemplate->getSpecialPowerType()))
			{
				const SpecialAbilityUpdateModuleData* data = ability->getAuthoredAbilityData();
				const UnsignedInt duration = std::max({ data->m_effectDuration, data->m_attributeModifierDuration,
					data->m_customAnimAndDuration.m_duration, data->m_grabPassengerAnimAndDuration.m_duration });
				const Real variation = data->m_packUnpackVariationFactor < 0 ? -data->m_packUnpackVariationFactor : data->m_packUnpackVariationFactor;
				const double bound = static_cast<double>(duration) + data->m_preparationFrames + data->m_persistentPrepFrames +
					(static_cast<double>(data->m_packTime) + data->m_unpackTime) * (1.0 + variation);
				watchAbilityState(s_case.abilityStates[0], ability->isActive(), bound, ctx.frame);
				abilityFound[0] = TRUE;
			}
			for (BehaviorModule** module = caster->getBehaviorModules(); module && *module; ++module)
				if (const auto* mode = dynamic_cast<const WeaponModeSpecialPowerUpdate*>(*module))
					if (mode->isModuleForPower(s_case.powerTemplate))
					{
						watchAbilityState(s_case.abilityStates[1], mode->isActive(), mode->getAuthoredModeDuration(), ctx.frame);
						abilityFound[1] = TRUE;
						break;
					}
		}
	}
	for (Int i = 0; i < 2; ++i)
		if (!abilityFound[i])
			s_case.abilityStates[i].active = FALSE;
	for (const Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
		if (!s_case.previous.count(object->getID()))
		{
			s_case.objects.insert(object->getTemplate()->getName().str());
			if (!object->isKindOf(KINDOF_PROJECTILE) && !object->findUpdateModule(NAMEKEY("LifetimeUpdate")))
				s_case.lingering.insert(object->getID());
		}
	s_case.persisting.clear();
	for (const ObjectID id : s_case.lingering)
		if (const Object* object = TheGameLogic->findObjectByID(id))
			if (!object->isEffectivelyDead())
				s_case.persisting.insert(object->getTemplate()->getName().str());
}

std::string describeActor(const Actor& actor)
{
	std::string result;
	if (!actor.newModifiers.empty())
		result += "mods:" + joined(actor.newModifiers) + ";";
	if (!actor.newStatus.empty())
		result += "status:" + joined(actor.newStatus) + ";";
	const Real threshold = actor.maxHealth * 0.05f;
	if (actor.health - actor.lowestHealth > threshold)
		result += "damaged;";
	if (actor.highestHealth - actor.health > threshold)
		result += "healed;";
	if (actor.died)
		result += "died;";
	if (actor.moved)
		result += "moved;";
	if (actor.converted)
		result += "converted;";
	return result.empty() ? "-" : result;
}

std::string combineActorDescriptions(const std::string& first, const std::string& second)
{
	if (second == "-")
		return first;
	if (first == "-")
		return second;
	return first + second;
}

void finish(const RetailTestCommands::Context& ctx)
{
	if (!s_case.active)
		return;
	std::string ended = "unbounded";
	Bool observed = FALSE;
	Bool unbounded = FALSE;
	Bool running = FALSE;
	Bool overrun = FALSE;
	for (Int i = 0; i < 2; ++i)
	{
		const AbilityState& state = s_case.abilityStates[i];
		if (!state.observed)
			continue;
		observed = TRUE;
		unbounded = unbounded || state.bound <= 0;
		running = running || state.active;
		overrun = overrun || (state.active && state.bound > 0 && state.elapsed > state.bound);
		s_case.remaining += (s_case.remaining.empty() ? "" : ",") + std::string(i == 0 ? "ability." : "power.") +
			(state.active ? "active" : "inactive") + ":bound=" + std::to_string(state.bound) + ":elapsed=" + std::to_string(state.elapsed);
	}
	if (overrun)
		ended = "never";
	else if (observed && !unbounded)
		ended = running ? "running" : "ended";
	for (const std::string& name : s_case.persisting)
		s_case.flags.insert("persists." + name);
	if (const Object* caster = TheGameLogic->findObjectByID(s_case.actors[CASTER].id))
	{
		if (s_case.templateAtClick && caster->getTemplate() != s_case.templateAtClick)
		{
			const ContainModuleInterface* contain = caster->getContain();
			s_case.flags.insert("final_members." + std::to_string(contain ? contain->getContainCount() : 0));
		}
	}
	else if (s_case.clicked)
		s_case.flags.insert("caster_gone");
	const std::string enemyDesc = combineActorDescriptions(describeActor(s_case.actors[ENEMY]), describeActor(s_case.actors[HERO]));
	const std::string structureDesc = combineActorDescriptions(describeActor(s_case.actors[STRUCTURE]), describeActor(s_case.actors[CAPTURE]));
	std::fprintf(stderr, "TEST ability-sweep case=%d\ttemplate=%s\tbutton=%s\ttype=%d\tpower=%s\tresult=%s\tanimation=%s\tobjects=%s\t"
		"caster=%s\tally=%s\tenemy=%s\tstructure=%s\tflags=%s\trecharge=%s\tended=%s\tremaining=%s\tno_window=%d\n",
		s_case.id, s_case.templateName.c_str(), s_case.button.c_str(), s_case.type, s_case.power.empty() ? "-" : s_case.power.c_str(),
		s_case.result.c_str(), joined(s_case.animation).c_str(), joined(s_case.objects).c_str(),
		describeActor(s_case.actors[CASTER]).c_str(), describeActor(s_case.actors[ALLY]).c_str(),
		enemyDesc.c_str(), structureDesc.c_str(),
		joined(s_case.flags).c_str(), !s_case.powerTemplate ? "n/a" : !s_case.powerTemplate->getReloadTime() ? "n/a:zero_reload" : s_case.recharged ? "yes" : s_case.running ? "pending" : "no", ended.c_str(),
		s_case.remaining.empty() ? "-" : s_case.remaining.c_str(), s_case.noWindow ? 1 : 0);
	TheInGameUI->setGUICommand(nullptr);
	TheControlBar->closeRetailTray();
	TheInGameUI->deselectAllDrawables();
	destroyObjectsOutside(s_sessionObjects);
	if (ctx.player)
		restorePlayerUpgrades(ctx.player);
	s_case = Case();
}
}
#endif

namespace RetailTestCommands
{
void registerRetailAbilitySweepTestCommands()
{
#if !RTS_GENERALS
	registerAction("ability-setup", setup);
	registerAction("ability-prepare", prepare);
	registerAction("ability-click", click);
	registerAction("ability-watch", watch);
	registerAction("ability-end", finish);
#endif
}
}
