#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>
#include "Common/Player.h"
#include "Common/SpecialPower.h"
#include "Common/ThingTemplate.h"
#include "GameClient/ControlBar.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Object.h"

namespace
{
void weatherButtons(const RetailTestCommands::Context& ctx)
{
	Player* player = ctx.findPlayer();
	Object* book = player ? player->getRetailSpellBook() : nullptr;
	if (!book)
		return;
	if (ctx.action == "weather-buttons-setup")
	{
		book->setCommandSetStringOverride("WeatherButtonsCommands");
		player->addSciencePurchasePoints(100);
		return;
	}
	const CommandSet* set = TheControlBar->findCommandSet(book->getCommandSetString());
	const CommandButton* button = set ? set->getCommandButton(ctx.ordinal - 1) : nullptr;
	if (!button)
		return;
	if (ctx.action == "weather-buttons-cast")
	{
		TheControlBar->clickRetailSpellButton(button);
		return;
	}
	const SpecialPowerTemplate* power = button->getSpecialPowerTemplate();
	const auto* module = power ? book->getSpecialPowerModule(power) : nullptr;
	const Object* trigger = nullptr;
	for (const Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
	{
		if (object->getControllingPlayer() == player && !object->isDestroyed() &&
			object->getTemplate()->getName() == ctx.targetName)
		{
			trigger = object;
			break;
		}
	}
	const CommandButton* internal = TheRetailCommandCatalog->findCommandButton(ctx.argument);
	const auto* fx = trigger && internal ? trigger->getSpecialPowerModule(internal->getSpecialPowerTemplate()) : nullptr;
	std::fprintf(stderr, "WEATHERBUTTON frame=%u slot=%d unlocked=%d valid=%d ready=%d percent=%.3f internal=%d accepted=%d\n",
		ctx.frame, ctx.ordinal, power && power->isUnlockedFor(player),
		TheControlBar->targetRetailSpell(button, nullptr, nullptr, FALSE), button->isReady(book),
		module ? module->getPercentReady() : -1.0f, fx != nullptr, fx && fx->wasLastCastAccepted());
}
}
#endif

namespace RetailTestCommands
{
void registerRetailWeatherButtonsTestCommands()
{
#if !RTS_GENERALS
	registerPrefix("weather-buttons-", weatherButtons);
#endif
}
}
