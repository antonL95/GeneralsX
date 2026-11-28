#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include "Common/BuildAssistant.h"
#include "Common/MessageStream.h"
#include "Common/NameKeyGenerator.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/SpecialPower.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "GameClient/ControlBar.h"
#include "GameClient/Drawable.h"
#include "GameClient/GadgetPushButton.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/InGameUI.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AttributeModifierHelper.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Module/RotwkAbilityModules.h"
#include "GameLogic/Module/SpecialAbilityUpdate.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/AttributeModifierStore.h"
#include "GameLogic/Module/ProductionUpdate.h"

namespace
{
struct ParsedArg
{
	std::string primary;
	std::string secondary;
};

ParsedArg splitArg(const char* raw)
{
	ParsedArg out;
	if (!raw)
		return out;
	const std::string_view sv(raw);
	const size_t sep = sv.find('|');
	if (sep == std::string_view::npos)
	{
		out.primary = std::string(sv);
	}
	else
	{
		out.primary = std::string(sv.substr(0, sep));
		out.secondary = std::string(sv.substr(sep + 1));
	}
	return out;
}

const CommandButton* findButtonInSet(const Object* obj, const char* name)
{
	if (!obj || !name || !*name)
		return nullptr;
	const CommandSet* set = TheControlBar ? TheControlBar->findCommandSet(obj->getCommandSetString()) : nullptr;
	for (Int slot = 0; set && slot < CommandButtonRules::RETAIL_COMMANDS_PER_SET; ++slot)
	{
		const CommandButton* btn = set->getCommandButton(slot);
		if (btn && btn->getName().compareNoCase(name) == 0)
			return btn;
	}
	return nullptr;
}

GameWindow* findButtonWindow(const char* name)
{
	if (!TheWindowManager || !name || !*name)
		return nullptr;
	static const struct
	{
		const char* format;
		Int count;
	} families[] = {
		{ "ControlBar.wnd:RingButton%d", 6 },
		{ "ControlBar.wnd:SideButton%02d", MAX_COMMANDS_PER_SET },
		{ "ControlBar.wnd:TrayButton%d", 3 },
		{ "ControlBar.wnd:RadialButton%02d", 12 }
	};
	GameWindow* shown = nullptr;
	GameWindow* hidden = nullptr;
	for (const auto& family : families)
	{
		for (Int index = 1; index <= family.count; ++index)
		{
			AsciiString winName;
			winName.format(family.format, index);
			GameWindow* win = TheWindowManager->winGetWindowFromId(nullptr, TheNameKeyGenerator->nameToKey(winName));
			const CommandButton* btn = win ? static_cast<const CommandButton*>(GadgetButtonGetData(win)) : nullptr;
			if (!btn || btn->getName().compareNoCase(name) != 0)
				continue;
			if (!win->winIsHidden() && !shown)
				shown = win;
			else if (win->winIsHidden() && !hidden)
				hidden = win;
		}
	}
	return shown ? shown : hidden;
}

std::string formatConditions(const Object* obj)
{
	if (!obj || !obj->getDrawable())
		return "-";
	const ModelConditionFlags flags = obj->getDrawable()->getModelConditionFlags();
	std::vector<std::string> names;
	for (Int bit = 0; bit < flags.size(); ++bit)
	{
		if (flags.test(bit))
			names.emplace_back(ModelConditionFlags::getNameFromSingleBit(bit));
	}
	std::sort(names.begin(), names.end());
	std::string out;
	for (const std::string& n : names)
		out += (out.empty() ? "" : ",") + n;
	return out.empty() ? "-" : out;
}

std::string formatStatuses(const Object* obj)
{
	if (!obj)
		return "-";
	const ObjectStatusMaskType status = obj->getStatusBits();
	std::vector<std::string> names;
	for (Int bit = 0; bit < status.size(); ++bit)
	{
		if (status.test(bit))
			names.emplace_back(ObjectStatusMaskType::getNameFromSingleBit(bit));
	}
	std::sort(names.begin(), names.end());
	std::string out;
	for (const std::string& n : names)
		out += (out.empty() ? "" : ",") + n;
	return out.empty() ? "-" : out;
}

std::string formatModifiers(const Object* obj)
{
	if (!obj)
		return "-";
	const AttributeModifierHelper* helper = obj->getAttributeModifierHelper();
	if (!helper)
		return "-";
	std::vector<std::string> names;
	for (const auto& applied : helper->getStack().active())
	{
		if (const ModifierListTemplate* tmpl = TheAttributeModifierStore->get(applied.list))
			names.emplace_back(tmpl->m_name.str());
	}
	std::sort(names.begin(), names.end());
	names.erase(std::unique(names.begin(), names.end()), names.end());
	std::string out;
	for (const std::string& n : names)
		out += (out.empty() ? "" : ",") + n;
	return out.empty() ? "-" : out;
}

Int livingHordeMembers(const Object* obj)
{
	const HordeContain* horde = obj ? HordeContain::get(obj) : nullptr;
	if (!horde || !horde->getContainedItemsList())
		return -1;
	Int count = 0;
	for (const Object* member : *horde->getContainedItemsList())
		if (member && !member->isEffectivelyDead() && !member->isDestroyed())
			++count;
	return count;
}

Bool isPowerAbilityActive(const Object* obj, const SpecialPowerTemplate* power)
{
	if (!obj || !power)
		return FALSE;
	for (BehaviorModule* const* m = obj->getBehaviorModules(); m && *m; ++m)
	{
		if (const auto* ability = dynamic_cast<const SpecialAbilityUpdate*>(*m))
		{
			if (ability->getAuthoredAbilityData()->m_specialPowerTemplate == power && ability->isActive())
				return TRUE;
		}
		if (const auto* mode = dynamic_cast<const WeaponModeSpecialPowerUpdate*>(*m))
		{
			if (mode->isModuleForPower(power) && mode->isActive())
				return TRUE;
		}
	}
	return FALSE;
}

Object* findClosestTarget(const Object* caster, const CommandButton* button, const std::string& targetTemplate)
{
	if (!caster || !button || targetTemplate.empty())
		return nullptr;
	std::vector<Object*> candidates;
	for (Object* obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject())
	{
		if (!obj || obj->isEffectivelyDead() || obj->isDestroyed())
			continue;
		if (obj->getTemplate()->getName().compareNoCase(targetTemplate.c_str()) != 0)
			continue;
		if (!button->isValidObjectTarget(caster, obj))
			continue;
		candidates.push_back(obj);
	}
	if (candidates.empty())
		return nullptr;
	std::sort(candidates.begin(), candidates.end(), [caster](const Object* a, const Object* b) {
		const Real dax = a->getPosition()->x - caster->getPosition()->x;
		const Real day = a->getPosition()->y - caster->getPosition()->y;
		const Real dbx = b->getPosition()->x - caster->getPosition()->x;
		const Real dby = b->getPosition()->y - caster->getPosition()->y;
		const Real da = dax * dax + day * day;
		const Real db = dbx * dbx + dby * dby;
		if (da != db)
			return da < db;
		return a->getID() < b->getID();
	});
	return candidates.front();
}

void cmdSelect(const RetailTestCommands::Context& ctx)
{
	Object* obj = ctx.requireObject(true);
	if (!obj)
		return;
	ctx.selectObject(obj);
	if (TheControlBar)
	{
		TheControlBar->markUIDirty();
		TheControlBar->update();
	}
}

void cmdRank(const RetailTestCommands::Context& ctx)
{
	Object* obj = ctx.requireObject(true);
	if (!obj)
		return;
	const Int rank = std::atoi(ctx.argument);
	obj->getExperienceTracker()->setMinRetailRank(rank, FALSE);
	std::fprintf(stderr, "TEST hero-ability-rank frame=%u template=%s rank=%d\n",
		ctx.frame, obj->getTemplate()->getName().str(), obj->getExperienceTracker()->getRank());
}

void cmdUpgrade(const RetailTestCommands::Context& ctx)
{
	const UpgradeTemplate* upgrade = TheUpgradeCenter ? TheUpgradeCenter->findUpgrade(ctx.argument) : nullptr;
	if (!upgrade)
	{
		std::fprintf(stderr, "TEST hero-ability-upgrade frame=%u upgrade=%s applied=0\n", ctx.frame, ctx.argument);
		return;
	}
	Bool applied = FALSE;
	if (upgrade->getUpgradeType() == UPGRADE_TYPE_PLAYER || std::string_view(ctx.targetName) == "-")
	{
		if (ctx.player)
		{
			ctx.player->addUpgrade(upgrade, UPGRADE_STATUS_COMPLETE);
			applied = TRUE;
		}
	}
	else if (Object* obj = ctx.findObject(false))
	{
		obj->giveUpgrade(upgrade);
		applied = TRUE;
	}
	std::fprintf(stderr, "TEST hero-ability-upgrade frame=%u upgrade=%s applied=%d\n",
		ctx.frame, ctx.argument, applied ? 1 : 0);
}

void cmdButtonState(const RetailTestCommands::Context& ctx)
{
	Object* obj = ctx.requireObject(true);
	if (!obj)
		return;
	const CommandButton* setBtn = findButtonInSet(obj, ctx.argument);
	GameWindow* win = findButtonWindow(ctx.argument);
	const CommandButton* uiBtn = win ? static_cast<const CommandButton*>(GadgetButtonGetData(win)) : nullptr;
	const CommandButton* btn = uiBtn ? uiBtn : setBtn;
	CommandAvailability avail = COMMAND_HIDDEN;
	if (win && TheControlBar)
		avail = TheControlBar->getRetailButtonAvailability(win);
	else if (btn && TheControlBar)
		avail = TheControlBar->queryRetailCommandAvailability(btn, obj);
	const Bool passive = btn && btn->isRetailPassive();
	const Bool enabled = win
		? (!win->winIsHidden() && BitIsSet(win->winGetStatus(), WIN_STATUS_ENABLED))
		: (setBtn && !passive && (avail == COMMAND_AVAILABLE || avail == COMMAND_ACTIVE));
	const SpecialPowerTemplate* power = btn ? btn->getSpecialPowerTemplate() : nullptr;
	const SpecialPowerModuleInterface* spm = power ? obj->getSpecialPowerModule(power) : nullptr;
	const Int ready = spm ? (spm->isReady() ? 1 : 0) : -1;
	const UnsignedInt readyFrame = spm ? spm->getReadyFrame() : 0u;
	std::fprintf(stderr,
		"TEST hero-ability-button frame=%u template=%s rank=%d set=%s button=%s in_set=%d in_ui=%d avail=%d enabled=%d passive=%d ready=%d readyFrame=%u\n",
		ctx.frame, obj->getTemplate()->getName().str(), obj->getExperienceTracker()->getRank(),
		obj->getCommandSetString().str(), ctx.argument, setBtn ? 1 : 0, (win && !win->winIsHidden()) ? 1 : 0,
		static_cast<int>(avail), enabled ? 1 : 0, passive ? 1 : 0, ready, readyFrame);
}

void cmdPress(const RetailTestCommands::Context& ctx)
{
	Object* caster = ctx.requireObject(true);
	if (!caster)
		return;
	const ParsedArg arg = splitArg(ctx.argument);
	ctx.selectObject(caster);
	if (TheControlBar)
	{
		TheControlBar->markUIDirty();
		TheControlBar->update();
	}
	const CommandButton* setBtn = findButtonInSet(caster, arg.primary.c_str());
	GameWindow* win = findButtonWindow(arg.primary.c_str());
	if ((!win || win->winIsHidden()) && !setBtn)
	{
		std::fprintf(stderr, "TEST hero-ability-press frame=%u template=%s button=%s accepted=0 reason=no_window\n",
			ctx.frame, caster->getTemplate()->getName().str(), arg.primary.c_str());
		return;
	}
	const CommandAvailability avail = (win && !win->winIsHidden())
		? TheControlBar->getRetailButtonAvailability(win)
		: TheControlBar->queryRetailCommandAvailability(setBtn, caster);
	const Bool passive = setBtn && setBtn->isRetailPassive();
	const Bool enabled = (win && !win->winIsHidden())
		? BitIsSet(win->winGetStatus(), WIN_STATUS_ENABLED)
		: (setBtn && !passive && (avail == COMMAND_AVAILABLE || avail == COMMAND_ACTIVE));
	if (!enabled || (avail != COMMAND_AVAILABLE && avail != COMMAND_ACTIVE))
	{
		std::fprintf(stderr, "TEST hero-ability-press frame=%u template=%s button=%s avail=%d enabled=%d accepted=0 reason=unavailable\n",
			ctx.frame, caster->getTemplate()->getName().str(), arg.primary.c_str(),
			static_cast<int>(avail), enabled ? 1 : 0);
		return;
	}

	TheInGameUI->setGUICommand(nullptr);
	const CommandButton* pending = nullptr;
	if (win && !win->winIsHidden())
	{
		TheWindowManager->winSendSystemMsg(win->winGetOwner(), GBM_SELECTED, (WindowMsgData)win, win->winGetWindowId());
		pending = TheInGameUI->getGUICommand();
	}
	else if (setBtn)
	{
		const UnsignedInt targetFlags = NEED_TARGET_POS | NEED_TARGET_ALLY_OBJECT | NEED_TARGET_ENEMY_OBJECT | NEED_TARGET_NEUTRAL_OBJECT;
		if (BitIsSet(setBtn->getOptions(), targetFlags))
		{
			TheInGameUI->setGUICommand(setBtn);
			pending = setBtn;
		}
		else if (setBtn->getCommandType() == GUI_COMMAND_OBJECT_UPGRADE)
		{
			const UpgradeTemplate* upgradeT = setBtn->getUpgradeTemplate();
			ProductionUpdateInterface* pu = caster->getProductionUpdateInterface();
			if (upgradeT && pu && pu->canQueueUpgrade(upgradeT) != CANMAKE_QUEUE_FULL &&
				!caster->hasUpgrade(upgradeT) && caster->affectedByUpgrade(upgradeT) &&
				TheUpgradeCenter && TheUpgradeCenter->canAffordUpgrade(caster->getControllingPlayer(), upgradeT, TRUE))
			{
				GameMessage* msg = TheMessageStream->appendMessage(GameMessage::MSG_QUEUE_UPGRADE);
				msg->appendObjectIDArgument(caster->getID());
				msg->appendIntegerArgument(upgradeT->getUpgradeNameKey());
			}
		}
		else if (setBtn->getCommandType() == GUI_COMMAND_SPECIAL_POWER && setBtn->getSpecialPowerTemplate())
		{
			GameMessage* msg = TheMessageStream->appendMessage(GameMessage::MSG_DO_SPECIAL_POWER);
			msg->appendIntegerArgument(setBtn->getSpecialPowerTemplate()->getID());
			msg->appendIntegerArgument(setBtn->getOptions());
			msg->appendObjectIDArgument(caster->getID());
		}
	}
	if (!pending)
	{
		std::fprintf(stderr, "TEST hero-ability-press frame=%u template=%s button=%s avail=%d enabled=%d accepted=1 mode=direct\n",
			ctx.frame, caster->getTemplate()->getName().str(), arg.primary.c_str(),
			static_cast<int>(avail), enabled ? 1 : 0);
		return;
	}

	const SpecialPowerTemplate* power = pending->getSpecialPowerTemplate();
	const Bool isWeapon = pending->getCommandType() == GUI_COMMAND_FIRE_WEAPON;
	if (!arg.secondary.empty())
	{
		Object* target = findClosestTarget(caster, pending, arg.secondary);
		if (!target)
		{
			TheInGameUI->setGUICommand(nullptr);
			std::fprintf(stderr, "TEST hero-ability-press frame=%u template=%s button=%s accepted=0 reason=no_target\n",
				ctx.frame, caster->getTemplate()->getName().str(), arg.primary.c_str());
			return;
		}
		if (isWeapon)
		{
			GameMessage* msg = TheMessageStream->appendMessage(GameMessage::MSG_DO_WEAPON_AT_OBJECT);
			msg->appendIntegerArgument(pending->getWeaponSlot());
			msg->appendObjectIDArgument(target->getID());
			msg->appendIntegerArgument(pending->getMaxShotsToFire());
		}
		else if (power)
		{
			GameMessage* msg = TheMessageStream->appendMessage(GameMessage::MSG_DO_SPECIAL_POWER_AT_OBJECT);
			msg->appendIntegerArgument(power->getID());
			msg->appendObjectIDArgument(target->getID());
			msg->appendIntegerArgument(pending->getOptions());
			msg->appendObjectIDArgument(caster->getID());
		}
		TheInGameUI->setGUICommand(nullptr);
		std::fprintf(stderr, "TEST hero-ability-press frame=%u template=%s button=%s avail=%d enabled=%d accepted=1 mode=object target=%s\n",
			ctx.frame, caster->getTemplate()->getName().str(), arg.primary.c_str(),
			static_cast<int>(avail), enabled ? 1 : 0, target->getTemplate()->getName().str());
		return;
	}

	if (ctx.hasPosition)
	{
		const Coord3D pos = ctx.groundLocation();
		if (isWeapon)
		{
			GameMessage* msg = TheMessageStream->appendMessage(GameMessage::MSG_DO_WEAPON_AT_LOCATION);
			msg->appendIntegerArgument(pending->getWeaponSlot());
			msg->appendLocationArgument(pos);
			msg->appendIntegerArgument(pending->getMaxShotsToFire());
			msg->appendObjectIDArgument(caster->getID());
		}
		else if (power)
		{
			GameMessage* msg = TheMessageStream->appendMessage(GameMessage::MSG_DO_SPECIAL_POWER_AT_LOCATION);
			msg->appendIntegerArgument(power->getID());
			msg->appendLocationArgument(pos);
			msg->appendRealArgument(INVALID_ANGLE);
			msg->appendObjectIDArgument(INVALID_ID);
			msg->appendIntegerArgument(pending->getOptions());
			msg->appendObjectIDArgument(caster->getID());
		}
		TheInGameUI->setGUICommand(nullptr);
		std::fprintf(stderr, "TEST hero-ability-press frame=%u template=%s button=%s avail=%d enabled=%d accepted=1 mode=pos\n",
			ctx.frame, caster->getTemplate()->getName().str(), arg.primary.c_str(),
			static_cast<int>(avail), enabled ? 1 : 0);
		return;
	}

	TheInGameUI->setGUICommand(nullptr);
	std::fprintf(stderr, "TEST hero-ability-press frame=%u template=%s button=%s accepted=0 reason=missing_aim\n",
		ctx.frame, caster->getTemplate()->getName().str(), arg.primary.c_str());
}

void cmdProbe(const RetailTestCommands::Context& ctx)
{
	const ParsedArg arg = splitArg(ctx.argument);
	const Object* obj = ctx.findObject(false);
	if (!obj)
	{
		std::fprintf(stderr, "TEST hero-ability-probe frame=%u tag=%s template=%s alive=0\n",
			ctx.frame, arg.primary.c_str(), ctx.targetName);
		return;
	}
	const Bool alive = !obj->isEffectivelyDead() && !obj->isDestroyed();
	const BodyModuleInterface* body = obj->getBodyModule();
	const Real hp = body ? body->getHealth() : 0.0f;
	const Real maxHp = body ? body->getMaxHealth() : 0.0f;
	const Player* owner = obj->getControllingPlayer();
	const ContainModuleInterface* contain = obj->getContain();
	const Int containCount = contain ? static_cast<Int>(contain->getContainCount()) : 0;
	const Int members = livingHordeMembers(obj);
	std::fprintf(stderr,
		"TEST hero-ability-probe frame=%u tag=%s template=%s alive=%d hp=%.1f/%.1f team=%s set=%s contain=%d members=%d cond=%s status=%s mods=%s",
		ctx.frame, arg.primary.c_str(), obj->getTemplate()->getName().str(), alive ? 1 : 0,
		hp, maxHp, owner ? owner->getPlayerName().str() : "-", obj->getCommandSetString().str(),
		containCount, members, formatConditions(obj).c_str(), formatStatuses(obj).c_str(),
		formatModifiers(obj).c_str());
	if (!arg.secondary.empty())
	{
		const SpecialPowerTemplate* power = TheSpecialPowerStore
			? TheSpecialPowerStore->findSpecialPowerTemplate(arg.secondary.c_str()) : nullptr;
		const SpecialPowerModuleInterface* spm = power ? obj->getSpecialPowerModule(power) : nullptr;
		const Bool active = isPowerAbilityActive(obj, power);
		std::fprintf(stderr, " power=%s ready=%d readyFrame=%u active=%d",
			arg.secondary.c_str(), spm ? (spm->isReady() ? 1 : 0) : -1,
			spm ? spm->getReadyFrame() : 0u, active ? 1 : 0);
	}
	std::fputc('\n', stderr);
}

void cmdWorld(const RetailTestCommands::Context& ctx)
{
	const ParsedArg arg = splitArg(ctx.argument);
	const Bool prefixMatch = !arg.secondary.empty() && arg.secondary.back() == '*';
	const std::string targetPattern = prefixMatch
		? arg.secondary.substr(0, arg.secondary.size() - 1)
		: arg.secondary;
	std::vector<const Object*> matches;
	for (const Object* obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject())
	{
		const char* name = obj->getTemplate()->getName().str();
		const Bool matched = prefixMatch
			? (std::strlen(name) >= targetPattern.size() &&
				strnicmp(name, targetPattern.c_str(), targetPattern.size()) == 0)
			: (obj->getTemplate()->getName().compareNoCase(targetPattern.c_str()) == 0);
		if (matched)
			matches.push_back(obj);
	}
	std::sort(matches.begin(), matches.end(), [](const Object* a, const Object* b) { return a->getID() < b->getID(); });
	Int living = 0;
	for (const Object* obj : matches)
		if (!obj->isEffectivelyDead() && !obj->isDestroyed())
			++living;
	std::fprintf(stderr, "TEST hero-ability-world frame=%u tag=%s template=%s living=%d total=%zu\n",
		ctx.frame, arg.primary.c_str(), arg.secondary.c_str(), living, matches.size());
}
}
#endif

namespace RetailTestCommands
{
void registerRetailHeroAbilityTestCommands()
{
#if !RTS_GENERALS
	registerAction("hero-ability-select", cmdSelect);
	registerAction("hero-ability-rank", cmdRank);
	registerAction("hero-ability-upgrade", cmdUpgrade);
	registerAction("hero-ability-button", cmdButtonState);
	registerAction("hero-ability-press", cmdPress);
	registerAction("hero-ability-probe", cmdProbe);
	registerAction("hero-ability-world", cmdWorld);
#endif
}
}
