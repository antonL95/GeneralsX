#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>
#include <string_view>

#include "Common/ActionManager.h"
#include "Common/MessageStream.h"
#include "Common/NameKeyGenerator.h"
#include "Common/Player.h"
#include "Common/ThingTemplate.h"
#include "GameClient/ControlBar.h"
#include "GameLogic/Damage.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Object.h"
#include "GameLogic/Weapon.h"

namespace
{
void selectSingle(Object* obj)
{
	GameMessage* sel = TheMessageStream->appendMessage(GameMessage::MSG_CREATE_SELECTED_GROUP);
	sel->appendBooleanArgument(TRUE);
	sel->appendObjectIDArgument(obj->getID());
}

Int livingMemberCount(const Object* obj)
{
	const HordeContain* horde = HordeContain::get(obj);
	if (!horde)
		return obj && !obj->isEffectivelyDead() ? 1 : 0;
	Int alive = 0;
	for (const Object* member : *horde->getContainedItemsList())
	{
		if (member && !member->isEffectivelyDead())
			++alive;
	}
	return alive;
}

Bool anyMemberHasStatus(const Object* obj, ObjectStatusTypes status)
{
	const HordeContain* horde = HordeContain::get(obj);
	if (!horde)
		return obj && obj->testStatus(status);
	for (const Object* member : *horde->getContainedItemsList())
	{
		if (member && !member->isEffectivelyDead() && member->testStatus(status))
			return TRUE;
	}
	return FALSE;
}

void handlePackage31(const RetailTestCommands::Context& ctx)
{
	Object* source = ctx.requireObject();
	if (!source)
		return;
	const std::string_view action(ctx.action);
	const std::string_view argument(ctx.argument);
	const auto separator = argument.find('|');
	Object* target = separator == std::string_view::npos ? ctx.target : ctx.findTarget(argument.substr(0, separator));
	const char* label = separator == std::string_view::npos ? ctx.argument : ctx.argument + separator + 1;

	if (action == "pkg31-check-enter" && target)
	{
		const Bool playerAllowed = TheActionManager->canEnterObject(source, target, CMD_FROM_PLAYER, CHECK_CAPACITY);
		const Bool aiAllowed = TheActionManager->canEnterObject(source, target, CMD_FROM_AI, CHECK_CAPACITY);
		std::fprintf(stderr, "TEST pkg31-enter-check frame=%u label=%s player=%d ai=%d\n",
			ctx.frame, label, playerAllowed ? 1 : 0, aiAllowed ? 1 : 0);
		std::fflush(stderr);
		return;
	}

	if (action == "pkg31-msg-enter" && ctx.target)
	{
		selectSingle(source);
		GameMessage* msg = TheMessageStream->appendMessage(GameMessage::MSG_ENTER);
		msg->appendObjectIDArgument(source->getID());
		msg->appendObjectIDArgument(ctx.target->getID());
		return;
	}

	if (action == "pkg31-ai-enter" && ctx.target)
	{
		if (AIUpdateInterface* ai = source->getAI())
			ai->aiEnter(ctx.target, CMD_FROM_AI);
		return;
	}

	if (action == "pkg31-msg-evacuate")
	{
		selectSingle(source);
		TheMessageStream->appendMessage(GameMessage::MSG_EVACUATE);
		return;
	}

	if (action == "pkg31-exit-button" && ctx.target)
	{
		const CommandSet* set = TheRetailCommandCatalog->findCommandSet(source->getCommandSetString());
		const CommandButton* button = set ? set->getCommandButton(0) : nullptr;
		if (!button || button->getCommandType() != GUI_COMMAND_EXIT_CONTAINER)
			return;
		selectSingle(source);
		GameMessage* msg = TheMessageStream->appendMessage(GameMessage::MSG_EXIT);
		msg->appendObjectIDArgument(ctx.target->getID());
		std::fprintf(stderr, "TEST pkg31-exit-button frame=%u button=%s\n", ctx.frame, button->getName().str());
		return;
	}

	if (action == "pkg31-container-state")
	{
		const ContainModuleInterface* contain = source->getContain();
		const Int count = contain ? static_cast<Int>(contain->getContainCount()) : -1;
		const Int cap = contain ? contain->getContainMax() : -1;
		Int firstMembers = -1;
		if (contain && count > 0)
			firstMembers = livingMemberCount(contain->getContainedItemsList()->front());
		const AsciiString ownerName = source->getControllingPlayer()
			? TheNameKeyGenerator->keyToName(source->getControllingPlayer()->getPlayerNameKey()) : AsciiString("-");
		std::fprintf(stderr, "TEST pkg31-container frame=%u label=%s count=%d cap=%d members=%d owner=%s dead=%d\n",
			ctx.frame, label, count, cap, firstMembers, ownerName.str(), source->isEffectivelyDead() ? 1 : 0);
		std::fflush(stderr);
		return;
	}

	if (action == "pkg31-unit-state")
	{
		const Bool contained = source->getContainedBy() != nullptr;
		const Bool unsel = source->testStatus(OBJECT_STATUS_UNSELECTABLE);
		const Bool unatk = source->testStatus(OBJECT_STATUS_UNATTACKABLE);
		const Bool canatk = source->testStatus(OBJECT_STATUS_CAN_ATTACK);
		const Bool encl = source->testStatus(OBJECT_STATUS_ENCLOSED);
		const Bool memUnsel = anyMemberHasStatus(source, OBJECT_STATUS_UNSELECTABLE);
		const Bool memEncl = anyMemberHasStatus(source, OBJECT_STATUS_ENCLOSED);
		const Int members = livingMemberCount(source);
		std::fprintf(stderr, "TEST pkg31-unit frame=%u label=%s contained=%d unsel=%d unatk=%d canatk=%d encl=%d mem_unsel=%d mem_encl=%d members=%d dead=%d\n",
			ctx.frame, label, contained ? 1 : 0, unsel ? 1 : 0, unatk ? 1 : 0, canatk ? 1 : 0, encl ? 1 : 0,
			memUnsel ? 1 : 0, memEncl ? 1 : 0, members, source->isEffectivelyDead() ? 1 : 0);
		std::fflush(stderr);
		return;
	}

	if (action == "pkg31-hit-weapon" && target)
	{
		const WeaponTemplate* launch = TheWeaponStore->findWeaponTemplate(label);
		AsciiString warheadName(label);
		warheadName.concat("Warhead");
		const WeaponTemplate* warhead = TheWeaponStore->findWeaponTemplate(warheadName);
		if (!launch || !warhead)
			return;
		Weapon* weapon = TheWeaponStore->allocateNewWeapon(launch, PRIMARY_WEAPON);
		const Coord3D pos = *target->getPosition();
		weapon->loadAmmoNow(source);
		Object* projectile = weapon->forceFireWeapon(source, &pos);
		deleteInstance(weapon);
		if (!projectile)
			return;
		projectile->setPosition(&pos);
		TheWeaponStore->handleProjectileDetonation(warhead, projectile, &pos, 0, TRUE, target->getID());
		TheGameLogic->destroyObject(projectile);
		std::fprintf(stderr, "TEST pkg31-impact frame=%u weapon=%s\n", ctx.frame, label);
		return;
	}

	if (action == "pkg31-damage-half")
	{
		if (BodyModuleInterface* body = source->getBodyModule())
		{
			DamageInfo info;
			info.in.m_damageType = DAMAGE_UNRESISTABLE;
			info.in.m_deathType = DEATH_NORMAL;
			info.in.m_amount = body->getMaxHealth() * 0.5f;
			source->attemptDamage(&info);
		}
		return;
	}

	if (action == "pkg31-kill")
	{
		source->kill(DAMAGE_UNRESISTABLE, DEATH_NORMAL);
		return;
	}
}
}
#endif

namespace RetailTestCommands
{
void registerRetailPackage31TestCommands()
{
#if !RTS_GENERALS
	registerPrefix("pkg31-", handlePackage31);
#endif
}
}
