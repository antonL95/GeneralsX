#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "GameClient/Drawable.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Object.h"

namespace
{
void logOpacity(UnsignedInt frame, Int horde, Int member, const Object& object)
{
	const Drawable* drawable = object.getDrawable();
	const Coord3D& pos = *object.getPosition();
	std::fprintf(stderr, "TEST ocl-fade %u %d %d %.6f %.3f %.3f %.3f %.6f\n",
		frame, horde, member, drawable ? drawable->getEffectiveOpacity() : -1.0f,
		pos.x, pos.y, pos.z, object.getOrientation());
}

void traceOclFade(const RetailTestCommands::Context& ctx)
{
	auto probe = ctx;
	for (probe.ordinal = 0; const Object* anchor = probe.findObject(true); ++probe.ordinal)
	{
		logOpacity(ctx.frame, probe.ordinal, -1, *anchor);
		if (const HordeContain* horde = HordeContain::get(anchor))
		{
			Int member = 0;
			for (const Object* rider : *horde->getContainedItemsList())
				if (rider && !rider->isEffectivelyDead())
					logOpacity(ctx.frame, probe.ordinal, member++, *rider);
		}
	}
}
}
#endif

namespace RetailTestCommands
{
void registerRetailOclFadeTestCommands()
{
#if !RTS_GENERALS
	registerAction("ocl-fade-trace", traceOclFade);
#endif
}
}
