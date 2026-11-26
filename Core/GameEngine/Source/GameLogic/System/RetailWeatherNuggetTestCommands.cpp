#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <algorithm>
#include <cstdio>

#include "Common/Player.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/Damage.h"
#include "GameLogic/Module/AttributeModifierHelper.h"
#include "GameLogic/Object.h"

namespace
{
void printWeatherActor(const RetailTestCommands::Context& ctx)
{
	Object* source = ctx.requireObject();
	if (!source)
		return;
	const auto* helper = source->getAttributeModifierHelper();
	const Int list = AttributeModifiers::findList(AsciiString(ctx.argument));
	const Bool active = helper && list >= 0 && std::any_of(helper->getStack().active().begin(),
		helper->getStack().active().end(), [list](const auto& entry) { return entry.list == list; });
	std::fprintf(stderr, "WEATHERNUGGET actor frame=%u player=%.*s list=%s active=%d dealt=%.3f taken=%.3f shroud=%.1f\n",
		ctx.frame, static_cast<Int>(ctx.playerName.size()), ctx.playerName.data(), ctx.argument, active,
		AttributeModifiers::damageDealtFactor(source),
		AttributeModifiers::damageTakenFactor(source, DAMAGE_SLASH), source->getShroudClearingRange());
}

void overrideSpellBookCommands(const RetailTestCommands::Context& ctx)
{
	Player* player = ctx.findPlayer();
	Object* book = player ? player->getRetailSpellBook() : nullptr;
	if (!book)
		return;
	book->setCommandSetStringOverride(AsciiString(ctx.argument));
	std::fprintf(stderr, "WEATHERNUGGET book frame=%u template=%s set=%s\n", ctx.frame,
		book->getTemplate()->getName().str(), book->getCommandSetString().str());
}
}
#endif

namespace RetailTestCommands
{
void registerRetailWeatherNuggetTestCommands()
{
#if !RTS_GENERALS
	registerAction("weathernugget-actor", printWeatherActor);
	registerAction("weathernugget-book", overrideSpellBookCommands);
#endif
}
}
