#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if RTS_ZEROHOUR
#include <cmath>
#include <cstdio>
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/Object.h"
#include "GameLogic/RetailWallMesh.h"
#include "GameLogic/TerrainLogic.h"

namespace
{
Region2D wallBounds(const RetailWallMesh::Geometry& geometry)
{
	Region2D bounds;
	bounds.lo = bounds.hi = geometry.wall.front().asCoord2D();
	for (const Coord3D& point : geometry.wall)
		bounds.uniteWith(point.asCoord2D());
	return bounds;
}

void handleWallHeight(const RetailTestCommands::Context& ctx)
{
	Object* left = ctx.requireObject();
	Object* right = ctx.findTarget(FALSE);
	if (!left || !right)
		return;
	const auto* leftGeometry = RetailWallMesh::forObject(*left);
	const auto* rightGeometry = RetailWallMesh::forObject(*right);
	if (!leftGeometry || !rightGeometry || leftGeometry->wall.empty() || rightGeometry->wall.empty())
		return;
	const Region2D leftBounds = wallBounds(*leftGeometry);
	const Region2D rightBounds = wallBounds(*rightGeometry);
	if (ctx.action == "wall-height-stack")
	{
		Coord3D position = *right->getPosition();
		position.x += leftBounds.hi.x - rightBounds.lo.x - PATHFIND_CELL_SIZE_F;
		position.z = left->getPosition()->z + ctx.angle;
		right->setPosition(&position);
		return;
	}
	if (ctx.action == "wall-height-level")
	{
		Coord3D position = *right->getPosition();
		position.z = left->getPosition()->z;
		right->setPosition(&position);
		return;
	}

	const Real y = (leftBounds.lo.y + leftBounds.hi.y) * 0.5f;
	const Coord3D from = {(leftBounds.lo.x + leftBounds.hi.x) * 0.5f, y, leftGeometry->wallHeight};
	const Coord3D to = {(rightBounds.lo.x + rightBounds.hi.x) * 0.5f, y, rightGeometry->wallHeight};
	const Real leftHeight = TheTerrainLogic->getLayerHeight(from.x, from.y, LAYER_WALL);
	const Real rightHeight = TheTerrainLogic->getLayerHeight(to.x, to.y, LAYER_WALL);
	const Bool leftMatches = std::fabs(leftHeight - from.z) <= 0.01f;
	const Bool rightMatches = std::fabs(rightHeight - to.z) <= 0.01f;
	const Bool sameHeight = std::fabs(rightHeight - leftHeight) <= 0.01f;
	const Bool crosses = TheAI->pathfinder()->isLinePassable(nullptr, LOCOMOTORSURFACE_GROUND, LAYER_WALL,
		from, to, FALSE, TRUE);
	std::fprintf(stderr, "TEST wall-height frame=%u left=%d right=%d same=%d crosses=%d\n",
		ctx.frame, leftMatches, rightMatches, sameHeight, crosses);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailWallHeightTestCommands()
{
#if RTS_ZEROHOUR
	registerPrefix("wall-height-", handleWallHeight);
#endif
}
}
