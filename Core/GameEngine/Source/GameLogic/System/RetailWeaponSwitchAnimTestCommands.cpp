#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include "Common/MessageStream.h"
#include "GameClient/ControlBar.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Object.h"
#include "GameLogic/Weapon.h"

namespace
{
void handleWeaponSwitchAnim(const RetailTestCommands::Context& ctx)
{
	Object* source = ctx.requireObject();
	if (!source)
		return;

	if (ctx.action == "weapon-switch-anim-repeat")
	{
		const Bool enabled = source->testWeaponSetFlag(WEAPONSET_TOGGLE_1);
		if (enabled)
			source->setWeaponSetFlag(WEAPONSET_TOGGLE_1);
		else
			source->clearWeaponSetFlag(WEAPONSET_TOGGLE_1);
		WeaponSetFlags flags;
		flags.set(WEAPONSET_TOGGLE_1);
		source->setWeaponSetFlags(flags, enabled);
		std::fprintf(stderr, "WEAPON-SWITCH-ANIM repeat frame=%u enabled=%d\n", ctx.frame, enabled);
		return;
	}

	if (ctx.action == "weapon-switch-anim-toggle")
	{
		const CommandSet* set = TheRetailCommandCatalog->findCommandSet(source->getCommandSetString());
		for (Int slot = 0; set && slot < CommandButtonRules::RETAIL_COMMANDS_PER_SET; ++slot)
		{
			const CommandButton* button = set->getCommandButton(slot);
			if (button && button->getCommandType() == GUI_COMMAND_TOGGLE_WEAPONSET &&
				button->getName().compareNoCase(ctx.argument) == 0)
			{
				ctx.selectObject(source);
				GameMessage* msg = TheMessageStream->appendMessage(GameMessage::MSG_RETAIL_HORDE_COMMAND);
				msg->appendObjectIDArgument(source->getID());
				msg->appendIntegerArgument(slot);
				msg->appendIntegerArgument(-1);
				return;
			}
		}
		return;
	}

	const ModelConditionFlags& conditions = source->getLogicModelConditions();
	const HordeContain* horde = HordeContain::get(source);
	const Object* weaponOwner = source;
	Int aliveMembers = 0;
	Int swappingMembers = 0;
	if (horde && horde->getContainedItemsList())
	{
		for (const Object* member : *horde->getContainedItemsList())
		{
			if (!member || member->isEffectivelyDead())
				continue;
			if (aliveMembers == 0)
				weaponOwner = member;
			++aliveMembers;
			if (member->getLogicModelConditions().test(MODELCONDITION_SWAPPING_TO_WEAPONSET_1))
				++swappingMembers;
		}
	}
	const Weapon* activeWeapon = nullptr;
	for (Int slot = PRIMARY_WEAPON; slot < WEAPONSLOT_COUNT; ++slot)
	{
		if (const Weapon* candidate = weaponOwner->getWeaponInWeaponSlot(static_cast<WeaponSlotType>(slot)))
		{
			activeWeapon = candidate;
			break;
		}
	}
	const Bool reloading = activeWeapon && activeWeapon->getStatus() == RELOADING_CLIP;
	const UnsignedInt nextShot = activeWeapon ? activeWeapon->getPossibleNextShotFrame() : 0;
	std::fprintf(stderr,
		"WEAPON-SWITCH-ANIM frame=%u actor=%s toggle1=%d swapping1=%d members=%d/%d weapon=%s reloading=%d readyAfter=%d\n",
		ctx.frame, ctx.targetName,
		conditions.test(MODELCONDITION_WEAPONSET_TOGGLE_1) ? 1 : 0,
		conditions.test(MODELCONDITION_SWAPPING_TO_WEAPONSET_1) ? 1 : 0,
		swappingMembers, aliveMembers,
		activeWeapon ? activeWeapon->getName().str() : "-",
		reloading ? 1 : 0,
		activeWeapon && nextShot > ctx.frame ? 1 : 0);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailWeaponSwitchAnimTestCommands()
{
#if !RTS_GENERALS
	registerAction("weapon-switch-anim-toggle", handleWeaponSwitchAnim);
	registerAction("weapon-switch-anim-repeat", handleWeaponSwitchAnim);
	registerAction("weapon-switch-anim-trace", handleWeaponSwitchAnim);
#endif
}
}
