#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include "Common/MessageStream.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/SpecialPower.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "GameClient/ControlBar.h"
#include "GameClient/Drawable.h"
#include "GameClient/GadgetPushButton.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/InGameUI.h"
#include "GameLogic/AI.h"
#include "GameLogic/AttributeModifierStore.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/AttributeModifierHelper.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/LifetimeUpdate.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"

#include <cstdio>
#include <cstdlib>
#include <set>
#include <string>
#include <vector>

namespace
{
constexpr Int ACTOR_COUNT = 4;
constexpr Int CASTER = 0;
constexpr Int ALLY = 1;
constexpr Int ENEMY = 2;
constexpr Int STRUCTURE = 3;
const char* const ACTOR_NAMES[ACTOR_COUNT] = { "caster", "ally", "enemy", "structure" };

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
	ObjectID previous = INVALID_ID;
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
	std::set<std::string> persisting;
	UnsignedInt clickFrame = 0;
	UnsignedInt lastActive = 0;
};

Case s_case;
ObjectID s_sessionObjects = INVALID_ID;
UpgradeMaskType s_sessionUpgrades;
Bool s_sessionRecorded = FALSE;

void destroyObjectsAfter(ObjectID last)
{
	for (Object* object = TheGameLogic->getFirstObject(); object;)
	{
		Object* next = object->getNextObject();
		if (object->getID() > last)
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

ObjectID highestObjectId()
{
	ObjectID highest = INVALID_ID;
	for (const Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
		if (object->getID() > highest)
			highest = object->getID();
	return highest;
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
		s_sessionObjects = highestObjectId();
		s_sessionUpgrades = ctx.player->getCompletedUpgradeMask();
	}
	destroyObjectsAfter(s_sessionObjects);
	restorePlayerUpgrades(ctx.player);
	s_case.active = TRUE;
	s_case.id = ctx.ordinal;
	s_case.templateName = ctx.targetName;
	Player* opponent = ctx.findPlayer(enemyPlayer);
	const Real x = ctx.position.x;
	const Real y = ctx.position.y;
	Object* objects[ACTOR_COUNT] = {
		createActor(ctx.targetName, ctx.player, x, y),
		createActor(ally, ctx.player, x - 50.0f, y),
		createActor(enemy, opponent, x + 80.0f, y),
		createActor(structure, opponent, x + 80.0f, y + 90.0f)
	};
	for (Int i = 0; i < ACTOR_COUNT; ++i)
		if (objects[i])
		{
			s_case.actors[i].id = objects[i]->getID();
			if (i == ENEMY || i == STRUCTURE)
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

const CommandButton* findInCommandSet(const Object* caster, const char* name)
{
	const CommandSet* set = TheControlBar->findCommandSet(caster->getCommandSetString());
	for (Int slot = 0; set && slot < CommandButtonRules::RETAIL_COMMANDS_PER_SET; ++slot)
	{
		const CommandButton* button = set->getCommandButton(slot);
		if (button && button->getName().compareNoCase(name) == 0)
			return button;
	}
	return nullptr;
}

void grantRequirements(Object* caster, const CommandButton* button)
{
	Player* player = caster->getControllingPlayer();
	std::vector<const UpgradeTemplate*> upgrades = button->getNeededUpgrades();
	if (BitIsSet(button->getOptions(), NEED_UPGRADE) && button->getUpgradeTemplate())
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

void prepare(const RetailTestCommands::Context& ctx)
{
	Object* caster = TheGameLogic->findObjectByID(s_case.actors[CASTER].id);
	if (!caster)
		return;
	s_case.button = ctx.argument;
	const CommandButton* button = findInCommandSet(caster, ctx.argument);
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

void recordBaseline(Object* caster, UnsignedInt frame)
{
	for (Actor& actor : s_case.actors)
		snapshot(actor);
	s_case.conditions = conditionsOf(caster);
	s_case.weaponSet = caster->getWeaponSetFlags();
	s_case.commandSet = caster->getCommandSetString();
	s_case.previous = highestObjectId();
	s_case.clickFrame = frame;
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
		for (const Int candidate : { ENEMY, ALLY, STRUCTURE, CASTER })
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
	const Coord3D point = target ? *target->getPosition() : enemy ? *enemy->getPosition() : *caster->getPosition();
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

void click(const RetailTestCommands::Context& ctx)
{
	Object* caster = TheGameLogic->findObjectByID(s_case.actors[CASTER].id);
	if (!caster || s_case.result != "no_click")
		return;
	const CommandButton* button = findInCommandSet(caster, ctx.argument);
	if (!button)
		return;
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
	GameWindow* window = findButtonWindow(ctx.argument);
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
	}
	const CommandAvailability availability = s_case.noWindow
		? TheControlBar->queryRetailCommandAvailability(button, caster) : TheControlBar->getRetailButtonAvailability(window);
	if (availability != COMMAND_AVAILABLE && availability != COMMAND_ACTIVE)
	{
		s_case.result = availability == COMMAND_PASSIVE ? "passive" : std::string("rejected:") + availabilityName(availability);
		return;
	}
	if (!s_case.noWindow && !BitIsSet(window->winGetStatus(), WIN_STATUS_ENABLED))
	{
		s_case.result = "rejected:button_disabled";
		return;
	}
	recordBaseline(caster, ctx.frame);
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

void watchActor(Actor& actor, Bool& active, const char* label, UnsignedInt frame)
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
			const Bool running = applied.endFrame > frame ||
				frame - s_case.clickFrame < TheAttributeModifierStore->def(applied.list).durationFrames;
			if (running)
			{
				s_case.running = TRUE;
				continue;
			}
			s_case.remaining += (s_case.remaining.empty() ? "" : ",") + std::string(label) + ".mod." + list->m_name.str();
			active = TRUE;
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

void watch(const RetailTestCommands::Context& ctx)
{
	if (!s_case.clicked || s_case.result != "accepted")
		return;
	Bool active = FALSE;
	s_case.remaining.clear();
	s_case.running = FALSE;
	for (Int i = 0; i < ACTOR_COUNT; ++i)
		watchActor(s_case.actors[i], active, ACTOR_NAMES[i], ctx.frame);
	if (Object* caster = TheGameLogic->findObjectByID(s_case.actors[CASTER].id))
	{
		const ModelConditionFlags current = conditionsOf(caster);
		std::set<Int> modifierConditions;
		if (const AttributeModifierHelper* helper = caster->getAttributeModifierHelper())
			for (const Int condition : helper->getAppliedModelConditions())
				modifierConditions.insert(condition);
		for (Int bit = 0; bit < current.size(); ++bit)
		{
			if (!current.test(bit) || s_case.conditions.test(bit))
				continue;
			const std::string name = ModelConditionFlags::getNameFromSingleBit(bit);
			if (name == "SELECTED" || name == "MOVING" || name.rfind("TURN_", 0) == 0 || name == "ACCELERATE" || name == "DECELERATE" || name == "ENGAGED" || name.rfind("EMOTION_", 0) == 0)
				continue;
			s_case.animation.insert(name);
			if (modifierConditions.count(bit))
				continue;
			s_case.remaining += (s_case.remaining.empty() ? "" : ",") + std::string("anim.") + name;
			active = TRUE;
		}
		if (caster->getWeaponSetFlags() != s_case.weaponSet)
			s_case.flags.insert("weaponset");
		if (caster->getCommandSetString() != s_case.commandSet)
			s_case.flags.insert("commandset");
		if (s_case.powerTemplate)
			if (const SpecialPowerModuleInterface* module = caster->getSpecialPowerModule(s_case.powerTemplate))
				s_case.recharged = s_case.recharged || !module->isReady();
	}
	for (const Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
		if (object->getID() > s_case.previous)
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
	if (active)
		s_case.lastActive = ctx.frame;
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

void finish(const RetailTestCommands::Context& ctx)
{
	if (!s_case.active)
		return;
	std::string ended = "none";
	if (s_case.lastActive)
		ended = s_case.lastActive >= ctx.frame - 2 ? "never" : std::to_string(s_case.lastActive - s_case.clickFrame);
	if (s_case.running && ended != "never")
		ended = "running";
	for (const std::string& name : s_case.persisting)
		s_case.flags.insert("persists." + name);
	std::fprintf(stderr, "TEST ability-sweep case=%d\ttemplate=%s\tbutton=%s\ttype=%d\tpower=%s\tresult=%s\tanimation=%s\tobjects=%s\t"
		"caster=%s\tally=%s\tenemy=%s\tstructure=%s\tflags=%s\trecharge=%s\tended=%s\tremaining=%s\tno_window=%d\n",
		s_case.id, s_case.templateName.c_str(), s_case.button.c_str(), s_case.type, s_case.power.empty() ? "-" : s_case.power.c_str(),
		s_case.result.c_str(), joined(s_case.animation).c_str(), joined(s_case.objects).c_str(),
		describeActor(s_case.actors[CASTER]).c_str(), describeActor(s_case.actors[ALLY]).c_str(),
		describeActor(s_case.actors[ENEMY]).c_str(), describeActor(s_case.actors[STRUCTURE]).c_str(),
		joined(s_case.flags).c_str(), s_case.powerTemplate ? (s_case.recharged ? "yes" : "no") : "n/a", ended.c_str(),
		ended == "never" && !s_case.remaining.empty() ? s_case.remaining.c_str() : "-", s_case.noWindow ? 1 : 0);
	TheInGameUI->setGUICommand(nullptr);
	TheControlBar->closeRetailTray();
	TheInGameUI->deselectAllDrawables();
	destroyObjectsAfter(s_sessionObjects);
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
