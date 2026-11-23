#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>
#include <string_view>

#include "Common/MessageStream.h"
#include "Common/NameKeyGenerator.h"
#include "Common/Player.h"
#include "Common/PlayerTemplate.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "GameClient/ControlBar.h"
#include "GameClient/GadgetPushButton.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/Mouse.h"
#include "GameLogic/HeroRoster.h"
#include "GameLogic/Module/CommandSetUpgrade.h"
#include "GameLogic/Object.h"

namespace
{
GameWindow* radialButton(Int slot)
{
	AsciiString name;
	name.format("ControlBar.wnd:RadialButton%02d", slot);
	GameWindow* window = TheWindowManager->winGetWindowFromId(nullptr, TheNameKeyGenerator->nameToKey(name));
	return window && !window->winIsHidden() ? window : nullptr;
}

GameWindow* findProductionButton(std::string_view command)
{
	for (Int slot = 1; slot <= MAX_COMMANDS_PER_SET; ++slot)
	{
		GameWindow* window = radialButton(slot);
		if (!window)
			continue;
		const auto* button = static_cast<const CommandButton*>(GadgetButtonGetData(window));
		if (button && command == button->getName().str())
			return window;
	}
	return nullptr;
}

void heroButtons(const RetailTestCommands::Context& ctx)
{
	const Object* producer = ctx.requireObject();
	if (!producer)
		return;
	std::fprintf(stderr, "TEST followup-recruits frame=%u set=%s", ctx.frame, producer->getCommandSetString().str());
	for (Int slot = 1; slot <= MAX_COMMANDS_PER_SET; ++slot)
	{
		GameWindow* window = radialButton(slot);
		const ThingTemplate* hero = window ? TheControlBar->getRetailHero(window) : nullptr;
		if (hero)
			std::fprintf(stderr, " %s:%d", hero->getName().str(), TheControlBar->getRetailButtonAvailability(window));
	}
	std::fputc('\n', stderr);
}

void auditRecruitSet(const PlayerTemplate& faction, const ThingTemplate& building, const AsciiString& setName,
	const std::vector<AsciiString>& heroes)
{
	const CommandSet* set = TheRetailCommandCatalog->findCommandSet(setName);
	if (!set)
		return;
	std::fprintf(stderr, "TEST followup-recruit-audit faction=%s building=%s set=%s",
		faction.getName().str(), building.getName().str(), setName.str());
	for (Int entry = 0; entry < MAX_COMMANDS_PER_SET; ++entry)
	{
		const Int slot = HeroRoster::reviveSlotForEntry(set, entry);
		if (slot < 0 || slot >= static_cast<Int>(heroes.size()))
			continue;
		const CommandButton* button = set->getCommandButton(entry);
		std::fprintf(stderr, " %s", heroes[slot].str());
		if (BitIsSet(button->getOptions(), NEED_UPGRADE))
		{
			const char* delimiter = ":";
			for (const auto* upgrade : button->getNeededUpgrades())
			{
				std::fprintf(stderr, "%s%s", delimiter, upgrade->getUpgradeName().str());
				delimiter = button->isNeededUpgradeAny() ? "|" : "+";
			}
		}
	}
	std::fputc('\n', stderr);
}

void auditRecruitSets(const RetailTestCommands::Context&)
{
	for (Int index = 0; index < ThePlayerTemplateStore->getPlayerTemplateCount(); ++index)
	{
		const PlayerTemplate* faction = ThePlayerTemplateStore->getNthPlayerTemplate(index);
		if (!faction || !faction->isPlayableSide())
			continue;
		const auto heroes = HeroRules::rosterOrder(faction->getBuildableRingHeroesMP(), faction->getBuildableHeroesMP());
		for (const ThingTemplate* building = TheThingFactory->firstTemplate(); building;
			building = building->friend_getNextTemplate())
		{
			if (!building->isKindOf(KINDOF_STRUCTURE) || building->getDefaultOwningSide() != faction->getSide())
				continue;
			auditRecruitSet(*faction, *building, building->friend_getCommandSetString(), heroes);
			const ModuleInfo& modules = building->getBehaviorModuleInfo();
			for (Int module = 0; module < modules.getCount(); ++module)
			{
				if (modules.getNthName(module) != "CommandSetUpgrade")
					continue;
				const auto* data = static_cast<const CommandSetUpgradeModuleData*>(modules.getNthData(module));
				auditRecruitSet(*faction, *building, data->m_newCommandSet, heroes);
				auditRecruitSet(*faction, *building, data->m_newCommandSetAlt, heroes);
			}
		}
	}
}

void productionButton(const RetailTestCommands::Context& ctx)
{
	GameWindow* window = findProductionButton(ctx.argument);
	if (!window)
	{
		std::fprintf(stderr, "TEST followup frame=%u button=%s missing\n", ctx.frame, ctx.argument);
		return;
	}

	ICoord2D point;
	Int width, height;
	window->winGetScreenPosition(&point.x, &point.y);
	window->winGetSize(&width, &height);
	if (ctx.action == "followup-button-state")
	{
		Int count;
		Real progress;
		TheControlBar->getRetailProduction(window, count, progress);
		const Player* player = ctx.findPlayer();
		std::fprintf(stderr, "TEST followup frame=%u button=%s availability=%d enabled=%d count=%d progress=%.2f cash=%u x=%d y=%d width=%d height=%d\n",
			ctx.frame, ctx.argument, TheControlBar->getRetailButtonAvailability(window), window->winGetEnabled(),
			count, progress, player ? player->getMoney()->countMoney() : 0, point.x, point.y, width, height);
		return;
	}

	point.x += width / 2;
	point.y += height / 2;
	TheMouse->setPosition(point.x, point.y);
	GameMessage* move = TheMessageStream->appendMessage(GameMessage::MSG_RAW_MOUSE_POSITION);
	move->appendPixelArgument(point);
	move->appendIntegerArgument(0);
	const bool right = ctx.action == "followup-right";
	const auto down = right ? GameMessage::MSG_RAW_MOUSE_RIGHT_BUTTON_DOWN : GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_DOWN;
	const auto up = right ? GameMessage::MSG_RAW_MOUSE_RIGHT_BUTTON_UP : GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_UP;
	for (const auto type : {down, up})
	{
		GameMessage* button = TheMessageStream->appendMessage(type);
		button->appendPixelArgument(point);
		button->appendIntegerArgument(0);
		button->appendIntegerArgument(TheMouse->getMouseStatus()->time);
	}
}
}
#endif

namespace RetailTestCommands
{
void registerRetailFollowupTestCommands()
{
#if !RTS_GENERALS
	registerAction("followup-button-state", productionButton);
	registerAction("followup-left", productionButton);
	registerAction("followup-right", productionButton);
	registerAction("followup-recruit-list", heroButtons);
	registerAction("followup-recruit-audit", auditRecruitSets);
#endif
}
}
