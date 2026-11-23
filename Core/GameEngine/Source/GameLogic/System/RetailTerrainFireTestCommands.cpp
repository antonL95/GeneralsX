#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#include <algorithm>
#include <cstdio>

#include "GameLogic/Object.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/TerrainLogic.h"

namespace RetailTestCommands
{
void registerRetailTerrainFireTestCommands()
{
	registerAction("terrain-fire-state", [](const Context& ctx) {
		const Object* observer = ctx.findObject(false);
		if (!observer || !TheTerrainLogic || !ctx.hasPosition)
			return;
		const auto& fire = TheTerrainLogic->retailFire();
		const auto cell = fire.cellAt(ctx.position.x, ctx.position.y);
		const auto burning = std::count_if(fire.burning().begin(), fire.burning().end(), [&](Int index) {
			const Coord2D center = fire.center(index);
			const Real dx = center.x - ctx.position.x;
			const Real dy = center.y - ctx.position.y;
			return dx * dx + dy * dy <= 60.0f * 60.0f;
		});
		std::fprintf(stderr, "TEST terrain fire frame=%u burning=%zu fuel=%d rate=%d health=%.1f recovering=%d x=%.0f y=%.0f\n",
			ctx.frame, static_cast<size_t>(burning), cell.fuel, cell.rate, observer->getBodyModule()->getHealth(),
			static_cast<int>(cell.recovering), ctx.position.x, ctx.position.y);
	});
}
}
