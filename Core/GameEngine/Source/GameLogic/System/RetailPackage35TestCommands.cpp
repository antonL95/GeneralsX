#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <algorithm>
#include <cstdio>

#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/SpecialPower.h"
#include "Common/ThingTemplate.h"
#include "GameClient/ControlBar.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AttributeModifierHelper.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"

namespace
{
void handlePackage35(const RetailTestCommands::Context& ctx)
{
	if (ctx.action == "pkg35-count" || ctx.action == "pkg35-present")
	{
		const Player* player = ctx.playerName == "neutral" ? ThePlayerList->getNeutralPlayer() : ctx.findPlayer();
		const Bool includeInactive = ctx.action == "pkg35-present";
		Int count = 0;
		for (const Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
			if (object->getControllingPlayer() == player && object->getTemplate()->getName() == ctx.argument &&
				!object->isDestroyed() && (includeInactive || !object->isEffectivelyDead()))
				++count;
		std::fprintf(stderr, "PKG35 count frame=%u player=%.*s type=%s count=%d\n",
			ctx.frame, static_cast<Int>(ctx.playerName.size()), ctx.playerName.data(), ctx.argument, count);
		return;
	}
	if (ctx.action == "pkg35-taint")
	{
		const ObjectID owner = TheTerrainLogic->retailTaintOwner(ctx.position.x, ctx.position.y);
		const Object* patch = ctx.findTarget(false);
		const Bool match = patch ? owner == patch->getID() : owner == INVALID_ID && std::string_view(ctx.argument) == "-";
		std::fprintf(stderr, "PKG35 taint frame=%u label=%s at=%.0f,%.0f match=%d\n",
			ctx.frame, ctx.argument, ctx.position.x, ctx.position.y, match);
		return;
	}

	Object* source = ctx.requireObject();
	if (!source)
		return;

	if (ctx.action == "pkg35-cast")
	{
		const auto* power = TheSpecialPowerStore->findSpecialPowerTemplate(ctx.argument);
		if (!power)
			return;
		if (ctx.hasPosition)
		{
			const Coord3D position = ctx.groundLocation();
			source->doSpecialPowerAtLocation(power, &position, 0, COMMAND_OPTION_INTERNAL_AUTO, TRUE);
		}
		else
			source->doSpecialPower(power, COMMAND_OPTION_INTERNAL_AUTO, TRUE);
	}
	else if (ctx.action == "pkg35-mods")
	{
		const auto* helper = source->getAttributeModifierHelper();
		const Int list = AttributeModifiers::findList(AsciiString(ctx.argument));
		const Bool active = helper && list >= 0 && std::any_of(helper->getStack().active().begin(),
			helper->getStack().active().end(), [list](const auto& entry) { return entry.list == list; });
		std::fprintf(stderr, "PKG35 mods frame=%u ordinal=%d list=%s active=%d damage=%.3f\n",
			ctx.frame, ctx.ordinal, ctx.argument, active, AttributeModifiers::damageDealtFactor(source));
	}
}
}
#endif

namespace RetailTestCommands
{
void registerRetailPackage35TestCommands()
{
#if !RTS_GENERALS
	registerPrefix("pkg35-", handlePackage35);
#endif
}
}
