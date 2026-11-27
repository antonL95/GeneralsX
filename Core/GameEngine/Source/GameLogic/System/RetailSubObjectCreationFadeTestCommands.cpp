#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <algorithm>
#include <cstdio>

#include "Common/Upgrade.h"
#include "GameClient/Drawable.h"
#include "GameLogic/Object.h"

namespace
{
void probeFade(const RetailTestCommands::Context& ctx)
{
	const Object* object = ctx.requireObject();
	if (!object || !object->getDrawable())
		return;

	const auto& fades = object->getDrawable()->getRetailSubObjectFades();
	const auto fade = std::find_if(fades.begin(), fades.end(), [&](const auto& entry) {
		return entry.name.compareNoCase(ctx.argument) == 0;
	});
	const Bool produced = object->getProducerID() != INVALID_ID;
	if (fade == fades.end())
	{
		std::fprintf(stderr, "TEST subobject-creation-fade %u %s produced=%d none\n", ctx.frame, ctx.targetName, produced);
		return;
	}

	std::fprintf(stderr, "TEST subobject-creation-fade %u %s produced=%d show=%d start=%u delay=%.1f duration=%.1f progress=%.3f\n",
		ctx.frame, ctx.targetName, produced, fade->show, fade->startFrame, fade->delay, fade->duration, fade->progress(ctx.frame));
}

void revokeUpgrade(const RetailTestCommands::Context& ctx)
{
	Object* object = ctx.requireObject();
	const UpgradeTemplate* upgrade = TheUpgradeCenter->findUpgrade(ctx.argument);
	if (object && upgrade)
		object->removeUpgrade(upgrade);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailSubObjectCreationFadeTestCommands()
{
#if !RTS_GENERALS
	registerAction("subobject-creation-fade-probe", probeFade);
	registerAction("subobject-creation-fade-revoke", revokeUpgrade);
#endif
}
}
