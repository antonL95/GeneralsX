#include "PreRTS.h"

#if !RTS_GENERALS
#include <cstdio>
#include <string_view>

#include "Common/ActionManager.h"
#include "Common/MessageStream.h"
#include "Common/Player.h"
#include "Common/SpecialPower.h"
#include "Common/Upgrade.h"
#include "GameClient/ControlBar.h"
#include "GameLogic/Module/RotwkAbilityModules.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"

void runRetailPackage05TestCommand(std::string_view action, Object* source, Object* target, UnsignedInt frame)
{
	if (action == "pkg05-ringstate")
	{
		const auto* playerUpgrade = TheUpgradeCenter->findUpgrade("Upgrade_RingHero");
		const auto* objectUpgrade = TheUpgradeCenter->findUpgrade("Upgrade_FortressRingHero");
		std::fprintf(stderr, "TEST ringstate frame=%u player=%d citadel=%d carrier=%d\n", frame,
			source->getControllingPlayer()->hasUpgradeComplete(playerUpgrade), source->hasUpgrade(objectUpgrade),
			target && target->testStatus(OBJECT_STATUS_HOLDING_THE_RING));
	}
	else if (action == "pkg05-brawl" && target && TheRetailCommandCatalog)
	{
		const CommandSet* set = TheRetailCommandCatalog->findCommandSet(source->getCommandSetString());
		for (Int slot = 0; set && slot < CommandButtonRules::RETAIL_COMMANDS_PER_SET; ++slot)
		{
			const CommandButton* button = set->getCommandButton(slot);
			if (!button || button->getCommandType() != GUI_COMMAND_BLOODTHIRSTY)
				continue;
			GameMessage* message = TheMessageStream->appendMessage(GameMessage::MSG_RETAIL_BLOODTHIRSTY);
			message->appendObjectIDArgument(source->getID());
			message->appendObjectIDArgument(target->getID());
			message->appendIntegerArgument(slot);
			break;
		}
	}
	else if (action == "pkg05-speech" || action == "pkg05-mount-speech")
	{
		if (action == "pkg05-mount-speech")
			ToggleMountedSpecialAbilityUpdate::setMounted(source, TRUE);
		const auto* power = TheSpecialPowerStore->findSpecialPowerTemplate("SpecialAbilityRousingSpeech");
		const Bool allowed = power && TheActionManager->canDoSpecialPower(source, power, CMD_FROM_PLAYER, 0);
		std::fprintf(stderr, "TEST speech frame=%u mounted=%d allowed=%d\n", frame,
			source->testWeaponSetFlag(WEAPONSET_MOUNTED), allowed);
		if (allowed)
			source->doSpecialPower(power, 0);
	}
	else if (action == "pkg05-ready-curse")
	{
		const auto* power = TheSpecialPowerStore->findSpecialPowerTemplate("SpecialAbilityCurseEnemy");
		if (auto* module = power ? source->getSpecialPowerModule(power) : nullptr)
			module->setReadyFrame(TheGameLogic->getFrame());
	}
	else if (action == "pkg05-curse" && target)
	{
		const auto* power = TheSpecialPowerStore->findSpecialPowerTemplate("SpecialAbilityCurseEnemy");
		const Bool allowed = power && TheActionManager->canDoSpecialPowerAtObject(source, target, CMD_FROM_PLAYER, power, 0);
		std::fprintf(stderr, "TEST curse frame=%u allowed=%d\n", frame, allowed);
		if (allowed)
			source->doSpecialPowerAtObject(power, target, 0);
	}
	else if (action == "pkg05-curse-area" && target)
	{
		const auto* power = TheSpecialPowerStore->findSpecialPowerTemplate("SpecialAbilityCurseEnemy");
		const Coord3D point = *target->getPosition();
		const Bool allowed = power && TheActionManager->canDoSpecialPowerAtLocation(source, &point, CMD_FROM_PLAYER, power, target, NEED_TARGET_POS);
		std::fprintf(stderr, "TEST curse-area frame=%u allowed=%d\n", frame, allowed);
		if (allowed)
			source->doSpecialPowerAtLocation(power, &point, 0.0f, NEED_TARGET_POS);
	}
}
#endif
