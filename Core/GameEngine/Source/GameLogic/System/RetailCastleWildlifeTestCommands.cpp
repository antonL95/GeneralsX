#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/ThingTemplate.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/CastleBehavior.h"
#include "GameLogic/Object.h"
#include "WWMath/wwmath.h"

namespace
{
void handleCastleWildlife(const RetailTestCommands::Context& ctx)
{
	if (ctx.action == "castle-wildlife-unpack")
	{
		Object* source = ctx.requireObject(TRUE);
		CastleBehavior* castle = source ? CastleBehavior::find(source) : nullptr;
		const Bool unpacked = castle && castle->unpackFree();
		std::fprintf(stderr, "TEST castle-wildlife unpack frame=%u unpacked=%d\n",
			ctx.frame, unpacked ? 1 : 0);
		std::fflush(stderr);
	}
	else if (ctx.action == "castle-wildlife-status")
	{
		Object* source = ctx.requireObject(TRUE);
		if (!source)
			return;
		const AIUpdateInterface* ai = source->getAI();
		const Int inside = source->testStatus(OBJECT_STATUS_INSIDE_CASTLE) ? 1 : 0;
		const Int moving = (ai && ai->isMoving()) ? 1 : 0;
		Real distance = 0.0f;
		if (Object* camp = ctx.findTarget(FALSE))
		{
			const Coord3D* from = camp->getPosition();
			const Coord3D* to = source->getPosition();
			const Real dx = to->x - from->x;
			const Real dy = to->y - from->y;
			distance = WWMath::Sqrt(dx * dx + dy * dy);
		}
		std::fprintf(stderr, "TEST castle-wildlife status frame=%u name=%s inside=%d moving=%d dist=%.0f\n",
			ctx.frame, source->getTemplate()->getName().str(), inside, moving, distance);
		std::fflush(stderr);
	}
}
}
#endif

namespace RetailTestCommands
{
void registerRetailCastleWildlifeTestCommands()
{
#if !RTS_GENERALS
	registerPrefix("castle-wildlife-", handleCastleWildlife);
#endif
}
}
