#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/Player.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Object.h"

namespace
{
void handleAodPayload(const RetailTestCommands::Context& ctx)
{
	if (ctx.action == "aod-payload-count")
	{
		const Player* player = ctx.findPlayer();
		const ThingTemplate* expected = TheThingFactory->findTemplate(ctx.argument);
		Int count = 0;
		Int contained = 0;
		for (const Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
		{
			if (object->getControllingPlayer() != player || object->isDestroyed() || object->isEffectivelyDead())
				continue;
			const Bool matches = expected
				? object->getTemplate()->isEquivalentTo(expected)
				: object->getTemplate()->getName().compare(ctx.argument) == 0;
			if (!matches)
				continue;
			++count;
			if (object->getContainedBy())
				++contained;
		}
		std::fprintf(stderr, "TEST aod-payload count frame=%u object=%s count=%d contained=%d\n",
			ctx.frame, ctx.argument, count, contained);
		return;
	}

	if (ctx.action == "aod-payload-horde")
	{
		const Object* horde = ctx.requireObject();
		if (!horde)
			return;
		const ContainModuleInterface* contain = horde->getContain();
		std::fprintf(stderr, "TEST aod-payload horde frame=%u object=%s count=%u max=%d\n",
			ctx.frame, ctx.targetName,
			contain ? contain->getContainCount() : 0u,
			contain ? contain->getContainMax() : 0);
	}
}
}
#endif

namespace RetailTestCommands
{
void registerRetailAodPayloadTestCommands()
{
#if !RTS_GENERALS
	registerPrefix("aod-payload-", handleAodPayload);
#endif
}
}
