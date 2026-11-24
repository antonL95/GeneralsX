#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include "Common/ModelState.h"
#include "Common/MessageStream.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "GameClient/Drawable.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include <cstdio>

namespace
{
void runPackage28(const RetailTestCommands::Context& ctx)
{
	Object* parent = ctx.requireObject(ctx.action != "pkg28-observe");
	if (!parent)
		return;

	if (ctx.action == "pkg28-remove")
	{
		if (const UpgradeTemplate* upgrade = TheUpgradeCenter->findUpgrade(ctx.argument))
			parent->removeUpgrade(upgrade);
		return;
	}
	if (ctx.action == "pkg28-sell")
	{
		ctx.selectObject(parent);
		TheMessageStream->appendMessage(GameMessage::MSG_SELL);
		return;
	}

	const UpgradeTemplate* granted = TheUpgradeCenter->findUpgrade("Pkg28Deploy");
	const UpgradeTemplate* removed = TheUpgradeCenter->findUpgrade("Pkg28Obsolete");
	UnsignedInt live = 0;
	UnsignedInt dead = 0;
	for (const Object* child = TheGameLogic->getFirstObject(); child; child = child->getNextObject())
	{
		if (child->getProducerID() != parent->getID() || child->isDestroyed())
			continue;
		if (child->isEffectivelyDead())
			++dead;
		else
			++live;
		const Drawable* drawable = child->getDrawable();
		const Coord3D& childPosition = *child->getPosition();
		const Coord3D& parentPosition = *parent->getPosition();
		std::fprintf(stderr,
			"TEST pkg28 child %u %s %s dead=%d anim=%d offset=%.1f,%.1f,%.1f angle=%.3f opacity=%.3f weapon=%d radius=%.1f\n",
			ctx.frame, ctx.targetName, child->getTemplate()->getName().str(), child->isEffectivelyDead(),
			drawable && drawable->getModelConditionFlags().test(MODELCONDITION_DEATH_2),
			childPosition.x - parentPosition.x, childPosition.y - parentPosition.y, childPosition.z - parentPosition.z,
			child->getOrientation(), drawable ? drawable->getRetailOclFadeOpacity() : -1.0f,
			child->getWeaponInWeaponSlot(PRIMARY_WEAPON) != nullptr, child->getGeometryInfo().getMajorRadius());
	}
	std::fprintf(stderr, "TEST pkg28 parent %u %s live=%u dead=%u granted=%d obsolete=%d sold=%d\n",
		ctx.frame, ctx.targetName, live, dead, granted && parent->hasUpgrade(granted),
		removed && parent->hasUpgrade(removed), parent->testStatus(OBJECT_STATUS_SOLD));
}
}
#endif

namespace RetailTestCommands
{
void registerRetailPackage28TestCommands()
{
#if !RTS_GENERALS
	registerAction("pkg28-observe", runPackage28);
	registerAction("pkg28-remove", runPackage28);
	registerAction("pkg28-sell", runPackage28);
#endif
}
}
