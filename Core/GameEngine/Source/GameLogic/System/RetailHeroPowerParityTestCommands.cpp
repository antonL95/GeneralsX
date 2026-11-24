#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include "Common/MessageStream.h"
#include "Common/Player.h"
#include "Common/SpecialPower.h"
#include "GameClient/ControlBar.h"
#include "GameLogic/CreateAHeroSystem.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Object.h"

#include <cstdio>
#include <fstream>
#include <sstream>

namespace
{
ObjectID s_caseObjectsBefore = INVALID_ID;

void beginCase(const RetailTestCommands::Context&)
{
	s_caseObjectsBefore = INVALID_ID;
	for (const Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
		if (object->getID() > s_caseObjectsBefore)
			s_caseObjectsBefore = object->getID();
}

void clearCase(const RetailTestCommands::Context&)
{
	for (Object* object = TheGameLogic->getFirstObject(); object;)
	{
		Object* next = object->getNextObject();
		if (object->getID() > s_caseObjectsBefore)
			TheGameLogic->destroyObject(object);
		object = next;
	}
}

std::string readCaseProfile(const char* path, Int caseId)
{
	std::ifstream cases(path);
	std::string header;
	std::getline(cases, header);
	std::istringstream columns(header);
	Int profileColumn = 0;
	for (std::string column; std::getline(columns, column, '\t'); ++profileColumn)
		if (column == "profile")
			break;

	const std::string caseKey = std::to_string(caseId);
	for (std::string row; std::getline(cases, row);)
	{
		std::istringstream fields(row);
		std::string id;
		std::getline(fields, id, '\t');
		if (id != caseKey)
			continue;
		std::string profile;
		for (Int column = 0; column < profileColumn && std::getline(fields, profile, '\t'); ++column)
			if (column + 1 == profileColumn)
				return profile;
		break;
	}
	return {};
}

void setProfile(const RetailTestCommands::Context& ctx)
{
	const Player* player = ctx.player;
	if (!player)
		return;
	const Int index = player->getPlayerIndex();
	if (std::string_view(ctx.argument) == "-")
	{
		CreateAHeroSystem::resetPlayer(index);
		return;
	}

	std::istringstream fields(readCaseProfile(ctx.argument, ctx.ordinal));
	std::string side, heroClass, subClass;
	std::getline(fields, side, ',');
	std::getline(fields, heroClass, ',');
	std::getline(fields, subClass, ',');
	CreateAHeroRules::SelectionData selection;
	CreateAHeroRules::SelectionData normalized;
	const Bool defaults = CreateAHeroSystem::buildDefaultSelection(heroClass.c_str(), subClass.c_str(), selection);
	selection.heroName = "ParityCaptain";
	for (std::string button; std::getline(fields, button, ',');)
		selection.powerButtons.push_back(button);
	const Bool valid = defaults && CreateAHeroSystem::validateSelection(selection, side.c_str(), normalized);
	if (valid)
		CreateAHeroSystem::setPlayerSelection(index, normalized);
	else
		CreateAHeroSystem::resetPlayer(index);
	std::fprintf(stderr, "HERO-PARITY profile frame=%u side=%s class=%s choices=%zu valid=%d\n",
		ctx.frame, side.c_str(), heroClass.c_str(), selection.powerButtons.size(), valid);
}

void inspectButton(const RetailTestCommands::Context& ctx, Bool toggle)
{
	Object* hero = ctx.findObject(true);
	if (!hero)
	{
		std::fprintf(stderr, "HERO-PARITY missing frame=%u hero=%s name=%s rank=-1 set=-\n",
			ctx.frame, ctx.targetName, ctx.argument);
		return;
	}
	const AsciiString setName = hero->getCommandSetString();
	const CommandSet* set = TheRetailCommandCatalog->findCommandSet(setName);
	for (Int slot = 0; set && slot < CommandButtonRules::RETAIL_COMMANDS_PER_SET; ++slot)
	{
		const CommandButton* button = set->getCommandButton(slot);
		if (!button || button->getName().compareNoCase(ctx.argument) != 0)
			continue;
		const auto* power = button->getSpecialPowerTemplate();
		const auto* module = power ? hero->getSpecialPowerModule(power) : nullptr;
		std::fprintf(stderr, "HERO-PARITY button frame=%u hero=%s name=%s rank=%d slot=%d set=%s passive=%d ready=%d readyFrame=%u mounted=%d weaponToggle=%d\n",
			ctx.frame, ctx.targetName, ctx.argument, hero->getExperienceTracker()->getRank(), slot + 1, setName.str(),
			button->hasRotwkOption(CommandButtonRules::ROTWK_NONPRESSABLE),
			module ? module->isReady() : -1, module ? module->getReadyFrame() : 0,
			hero->testWeaponSetFlag(WEAPONSET_MOUNTED), hero->testWeaponSetFlag(WEAPONSET_TOGGLE_1));
		if (toggle && button->getCommandType() == GUI_COMMAND_TOGGLE_WEAPONSET)
		{
			ctx.selectObject(hero);
			GameMessage* message = TheMessageStream->appendMessage(GameMessage::MSG_RETAIL_HORDE_COMMAND);
			message->appendObjectIDArgument(hero->getID());
			message->appendIntegerArgument(slot);
			message->appendIntegerArgument(-1);
		}
		return;
	}
	std::fprintf(stderr, "HERO-PARITY missing frame=%u hero=%s name=%s rank=%d set=%s\n",
		ctx.frame, ctx.targetName, ctx.argument, hero->getExperienceTracker()->getRank(), setName.str());
}
}
#endif

namespace RetailTestCommands
{
void registerRetailHeroPowerParityTestCommands()
{
#if !RTS_GENERALS
	registerAction("hero-parity-begin", beginCase);
	registerAction("hero-parity-clear", clearCase);
	registerAction("hero-parity-profile", setProfile);
	registerAction("hero-parity-button", [](const Context& ctx) { inspectButton(ctx, FALSE); });
	registerAction("hero-parity-toggle", [](const Context& ctx) { inspectButton(ctx, TRUE); });
#endif
}
}
