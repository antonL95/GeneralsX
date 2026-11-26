#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include "Common/Player.h"
#include "Common/SpecialPower.h"
#include "GameLogic/Module/SpecialAbilityUpdate.h"
#include "GameLogic/Module/AttributeModifierHelper.h"
#include "GameLogic/Object.h"
#include <cstdio>

namespace
{
void probe(const RetailTestCommands::Context& ctx)
{
	if (ctx.action == "fortress-horn-ally")
	{
		Player* owner = ctx.findPlayer("Player_1");
		Player* ally = ctx.findPlayer("PlyrCivilian");
		if (owner && ally)
		{
			if (std::string_view(ctx.argument) == "on")
			{
				owner->setPlayerRelationship(ally, ALLIES);
				ally->setPlayerRelationship(owner, ALLIES);
			}
			else
			{
				owner->removePlayerRelationship(ally);
				ally->removePlayerRelationship(owner);
			}
		}
		return;
	}
	const Object* object = ctx.requireObject();
	if (!object)
		return;
	if (ctx.action == "fortress-horn-caster")
	{
		const auto* power = TheSpecialPowerStore->findSpecialPowerTemplate(ctx.argument);
		const auto* ability = power ? object->findSpecialAbilityUpdate(power->getSpecialPowerType()) : nullptr;
		std::fprintf(stderr, "HORN caster frame=%u player=%.*s active=%d\n", ctx.frame,
			static_cast<Int>(ctx.playerName.size()), ctx.playerName.data(), ability && ability->isActive());
		return;
	}
	const auto* helper = object->getAttributeModifierHelper();
	const Int list = AttributeModifiers::findList(AsciiString(ctx.argument));
	UnsignedInt remaining = 0;
	Bool active = FALSE;
	UnsignedInt mask = 0;
	if (helper)
	{
		mask = helper->getStack().blockedMask();
		for (const auto& entry : helper->getStack().active())
			if (entry.list == list)
			{
				active = TRUE;
				remaining = entry.endFrame > ctx.frame ? entry.endFrame - ctx.frame : 0;
			}
	}
	std::fprintf(stderr, "HORN frame=%u player=%.*s ordinal=%d list=%s active=%d remaining=%u mask=%u\n",
		ctx.frame, static_cast<Int>(ctx.playerName.size()), ctx.playerName.data(), ctx.ordinal, ctx.argument, active, remaining, mask);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailFortressHornTestCommands()
{
#if !RTS_GENERALS
	registerPrefix("fortress-horn-", probe);
#endif
}
}
