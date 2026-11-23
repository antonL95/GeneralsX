#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>
#include <map>
#include <string_view>

#include "Common/ThingTemplate.h"
#include "Common/ThingFactory.h"
#include "Common/Upgrade.h"
#include "GameClient/Drawable.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Object.h"

namespace
{
void removeCohort(const RetailTestCommands::Context& ctx)
{
	const HordeContain* horde = HordeContain::get(ctx.requireObject(true));
	const ThingTemplate* type = TheThingFactory->findTemplate(ctx.argument, FALSE);
	if (!horde || !type)
		return;

	Int killed = 0;
	const auto& members = *horde->getContainedItemsList();
	for (auto it = members.begin(); it != members.end(); )
	{
		Object* member = *it++;
		if (member->isEffectivelyDead() || !member->getTemplate()->isEquivalentTo(type))
			continue;
		member->kill();
		++killed;
	}
	std::fprintf(stderr, "TEST combo-casualties %u %s %s %d\n", ctx.frame, ctx.targetName, ctx.argument, killed);
}

void logHorde(const RetailTestCommands::Context& ctx)
{
	const Object* anchor = ctx.findObject(true);
	const HordeContain* horde = HordeContain::get(anchor);
	if (!horde)
	{
		std::fprintf(stderr, "TEST combo-state %u %s %d absent\n", ctx.frame, ctx.targetName, ctx.ordinal);
		return;
	}

	const UpgradeTemplate* upgrade = TheUpgradeCenter->findUpgrade(ctx.argument);
	const Coord3D& position = *anchor->getPosition();
	const Drawable* drawable = anchor->getDrawable();
	std::fprintf(stderr, "TEST combo-anchor %u %s %d id=%u pos=%.3f,%.3f,%.3f selected=%d upgraded=%d\n",
		ctx.frame, ctx.targetName, ctx.ordinal, static_cast<unsigned>(anchor->getID()),
		position.x, position.y, position.z, drawable && drawable->isSelected(), upgrade && anchor->hasUpgrade(upgrade));

	std::map<std::string_view, Int> counts;
	for (const Object* member : *horde->getContainedItemsList())
	{
		if (member->isEffectivelyDead())
			continue;
		const char* type = member->getTemplate()->getName().str();
		const Coord3D& memberPosition = *member->getPosition();
		++counts[type];
		std::fprintf(stderr, "TEST combo-member %u %s %d %s id=%u pos=%.3f,%.3f,%.3f upgraded=%d\n",
			ctx.frame, ctx.targetName, ctx.ordinal, type, static_cast<unsigned>(member->getID()),
			memberPosition.x, memberPosition.y, memberPosition.z, upgrade && member->hasUpgrade(upgrade));
	}
	for (const auto& [type, count] : counts)
		std::fprintf(stderr, "TEST combo-state %u %s %d %s %d\n", ctx.frame, ctx.targetName, ctx.ordinal, type.data(), count);
}

void logLooseMembers(const RetailTestCommands::Context& ctx)
{
	const UpgradeTemplate* upgrade = TheUpgradeCenter->findUpgrade(ctx.argument);
	for (const Object* member = TheGameLogic->getFirstObject(); member; member = member->getNextObject())
	{
		if (member->getControllingPlayer() != ctx.player || member->isEffectivelyDead() || member->isDestroyed()
			|| member->getContainedBy() || member->getTemplate()->getName().compareNoCase(ctx.targetName) != 0)
			continue;
		const Coord3D& position = *member->getPosition();
		std::fprintf(stderr, "TEST combo-loose %u %s id=%u pos=%.3f,%.3f,%.3f upgraded=%d\n", ctx.frame, ctx.targetName,
			static_cast<unsigned>(member->getID()), position.x, position.y, position.z, upgrade && member->hasUpgrade(upgrade));
	}
}
}
#endif

namespace RetailTestCommands
{
void registerRetailComboSplitTestCommands()
{
#if !RTS_GENERALS
	registerAction("combo-casualties", removeCohort);
	registerAction("combo-state", logHorde);
	registerAction("combo-loose", logLooseMembers);
#endif
}
}
