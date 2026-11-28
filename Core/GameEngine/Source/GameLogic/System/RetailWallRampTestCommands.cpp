#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if RTS_ZEROHOUR
#include <cstdio>
#include "Common/MessageStream.h"
#include "Common/Module.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "GameLogic/Object.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Pathfinder/Path.h"
#include "GameLogic/RetailWallMesh.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Weapon.h"

namespace
{
std::vector<ICoord2D> footprintBefore;

void probeFootprint(const RetailTestCommands::Context& ctx)
{
	Object* object = ctx.requireObject(FALSE);
	if (!object)
		return;
	ICoord2D center;
	TheAI->pathfinder()->worldToCell(object->getPosition(), &center);
	std::vector<ICoord2D> cells;
	for (Int x = center.x - 20; x <= center.x + 20; ++x)
		for (Int y = center.y - 20; y <= center.y + 20; ++y)
		{
			const PathfindCell* cell = TheAI->pathfinder()->getCell(LAYER_GROUND, x, y);
			if (cell && cell->isSoleObstacle(object->getID()))
				cells.push_back({x, y});
		}
	if (ctx.frame == 181)
		footprintBefore = cells;
	else if (ctx.frame == 186)
	{
		Bool different = cells.size() != footprintBefore.size();
		for (size_t i = 0; !different && i < cells.size(); ++i)
			different = cells[i].x != footprintBefore[i].x || cells[i].y != footprintBefore[i].y;
		std::fprintf(stderr, "TEST wall-ramp-footprint upgraded different=%d\n", different);
	}
	else
	{
		Bool equal = cells.size() == footprintBefore.size();
		for (size_t i = 0; equal && i < cells.size(); ++i)
			equal = cells[i].x == footprintBefore[i].x && cells[i].y == footprintBefore[i].y;
		std::fprintf(stderr, "TEST wall-ramp-footprint reset equal=%d\n", equal);
	}
}

std::vector<ICoord2D> groundBefore;

void probeGround(const RetailTestCommands::Context& ctx)
{
	Object* object = ctx.requireObject(FALSE);
	if (!object)
		return;
	ICoord2D center;
	TheAI->pathfinder()->worldToCell(object->getPosition(), &center);
	std::vector<ICoord2D> cells;
	for (Int x = center.x - 30; x <= center.x + 30; ++x)
		for (Int y = center.y - 30; y <= center.y + 30; ++y)
		{
			const PathfindCell* cell = TheAI->pathfinder()->getCell(LAYER_GROUND, x, y);
			if (cell && cell->getType() == PathfindCell::CELL_BRIDGE_IMPASSABLE)
				cells.push_back({x, y});
		}
	if (ctx.frame == 181)
	{
		groundBefore = cells;
		std::fprintf(stderr, "TEST wall-ramp-ground frame=%u cells=%u\n", ctx.frame, static_cast<unsigned>(cells.size()));
		return;
	}
	Bool equal = cells.size() == groundBefore.size();
	for (size_t i = 0; equal && i < cells.size(); ++i)
		equal = cells[i].x == groundBefore[i].x && cells[i].y == groundBefore[i].y;
	std::fprintf(stderr, "TEST wall-ramp-ground frame=%u cells=%u same=%d\n", ctx.frame,
		static_cast<unsigned>(cells.size()), equal ? 1 : 0);
}

void probePinched(const RetailTestCommands::Context& ctx)
{
	Object* object = ctx.requireObject(FALSE);
	if (!object)
		return;
	ICoord2D center;
	TheAI->pathfinder()->worldToCell(object->getPosition(), &center);
	const ICoord2D sides[] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
	Int own = 0, stale = 0;
	for (Int x = center.x - 20; x <= center.x + 20; ++x)
		for (Int y = center.y - 20; y <= center.y + 20; ++y)
		{
			const PathfindCell* cell = TheAI->pathfinder()->getCell(LAYER_GROUND, x, y);
			if (!cell || !cell->isSoleObstacle(object->getID()))
				continue;
			++own;
			if (cell->getPinched())
				++stale;
			for (const ICoord2D& side : sides)
			{
				const PathfindCell* next = TheAI->pathfinder()->getCell(LAYER_GROUND, x + side.x, y + side.y);
				if (next && next->getType() == PathfindCell::CELL_CLEAR && !next->getPinched())
					++stale;
			}
		}
	std::fprintf(stderr, "TEST wall-ramp-pinched frame=%u name=%s own=%d stale=%d\n",
		ctx.frame, object->getTemplate()->getName().str(), own, stale);
}

void revokeUpgrade(const RetailTestCommands::Context& ctx)
{
	Object* object = ctx.requireObject(FALSE);
	const auto* upgrade = TheUpgradeCenter->findUpgrade(ctx.argument);
	if (object && upgrade)
		object->removeUpgrade(upgrade);
}

void fireWeaponAt(const RetailTestCommands::Context& ctx)
{
	const Object* source = ctx.requireObject(FALSE);
	const WeaponTemplate* weapon = TheWeaponStore->findWeaponTemplate(ctx.argument);
	const Bool fired = source && weapon && ctx.hasPosition;
	if (fired)
		TheWeaponStore->createAndFireTempWeapon(weapon, source, &ctx.position, TRUE);
	std::fprintf(stderr, "TEST wall-ramp-weapon frame=%u fired=%d\n", ctx.frame, fired);
}

void probeUnit(const RetailTestCommands::Context& ctx)
{
	Object* object = ctx.requireObject(FALSE);
	if (!object)
		return;
	const Coord3D& position = *object->getPosition();
	std::fprintf(stderr, "TEST wall-ramp-unit frame=%u name=%s x=%.2f y=%.2f z=%.2f layer=%d\n",
		ctx.frame, ctx.targetName, position.x, position.y, position.z, object->getLayer());
	if (AIUpdateInterface* ai = object->getAIUpdateInterface())
	{
		const Coord3D* goal = ai->getGoalPosition();
		if (goal)
			std::fprintf(stderr, "TEST wall-ramp-goal frame=%u x=%.2f y=%.2f z=%.2f\n", ctx.frame, goal->x, goal->y, goal->z);
		if (Path* path = ai->getPath())
			for (const PathNode* node = path->getFirstNode(); node; node = node->getNext())
				std::fprintf(stderr, "TEST wall-ramp-node frame=%u x=%.2f y=%.2f z=%.2f layer=%d\n",
					ctx.frame, node->getPosition()->x, node->getPosition()->y, node->getPosition()->z, node->getLayer());
	}
}

void moveToWall(const RetailTestCommands::Context& ctx)
{
	Object* object = ctx.requireObject(FALSE);
	if (!object || !ctx.hasPosition)
		return;
	Coord3D point = ctx.groundLocation();
	point.z = TheAI->pathfinder()->getWallHeightAt(point);
	std::fprintf(stderr, "TEST wall-ramp-destination frame=%u x=%.2f y=%.2f z=%.2f wall=%d layer=%d\n",
		ctx.frame, point.x, point.y, point.z, TheAI->pathfinder()->isPointOnWall(&point),
		TheTerrainLogic->getLayerForDestination(&point));
	if (const AIUpdateInterface* ai = object->getAIUpdateInterface())
		std::fprintf(stderr, "TEST wall-ramp-path frame=%u possible=%d\n", ctx.frame,
			TheAI->pathfinder()->clientSafeQuickDoesPathExist(ai->getLocomotorSet(), object->getPosition(), &point));
	ctx.selectObject(object);
	TheMessageStream->appendMessage(GameMessage::MSG_DO_MOVETO)->appendLocationArgument(point);
}

void probeWallRamp(const RetailTestCommands::Context& ctx)
{
	const Object* object = ctx.requireObject();
	if (!object)
		return;
	const ModuleInfo& modules = object->getTemplate()->getDrawModuleInfo();
	AsciiString model, wall, ramp1, ramp2;
	for (Int i = 0; i < modules.getCount(); ++i)
		if (modules.getNthData(i)->getWallRampMeshes(model, wall, ramp1, ramp2))
			break;
	RetailWallMesh::Mesh a, b, c;
	const Bool foundWall = RetailWallMesh::load(model, wall, a);
	const Bool foundFirst = RetailWallMesh::load(model, ramp1, b);
	const Bool foundSecond = RetailWallMesh::load(model, ramp2, c);
	std::fprintf(stderr, "TEST wall-ramp frame=%u wall=%d ramp1=%d ramp2=%d vertices=%zu,%zu,%zu faces=%zu,%zu,%zu heights=%.2f,%.2f,%.2f\n",
		ctx.frame, foundWall, foundFirst, foundSecond, a.vertices.size(), b.vertices.size(), c.vertices.size(),
		a.faces.size(), b.faces.size(), c.faces.size(),
		foundWall ? a.vertices[0].z : 0.0f, foundFirst ? b.vertices[0].z : 0.0f,
		foundSecond ? c.vertices[0].z : 0.0f);
	std::vector<Coord3D> points;
	RetailWallMesh::Ramp first{}, second{};
	const Bool hasPolygon = RetailWallMesh::polygon(a, points);
	const Bool hasFirst = RetailWallMesh::ramp(b, first);
	const Bool hasSecond = RetailWallMesh::ramp(c, second);
	std::fprintf(stderr, "TEST wall-ramp-shape frame=%u polygon=%zu ramp1=%d ramp2=%d\n",
		ctx.frame, hasPolygon ? points.size() : 0, hasFirst, hasSecond);
	Coord3D onWall = *object->getPosition();
	onWall.y += 86.0f;
	Coord3D offWall = *object->getPosition();
	offWall.y += 50.0f;
	std::fprintf(stderr, "TEST wall-ramp-cell frame=%u inside=%d outside=%d height=%.2f\n", ctx.frame,
		TheAI->pathfinder()->isPointOnWall(&onWall), TheAI->pathfinder()->isPointOnWall(&offWall),
		TheAI->pathfinder()->getWallHeightAt(onWall) - object->getPosition()->z);
	const auto* geometry = RetailWallMesh::forObject(*object);
	Bool linked[2] = {FALSE, FALSE};
	for (Int i = 0; geometry && i < 2; ++i)
	{
		if (!geometry->hasRamp[i])
			continue;
		const auto& ramp = geometry->ramps[i];
		Coord3D point = {(ramp.from.x + ramp.to.x) * 0.5f, (ramp.from.y + ramp.to.y) * 0.5f,
			(ramp.from.z + ramp.to.z) * 0.5f};
		const Bridge* bridge = TheTerrainLogic->findBridgeAt(&point);
		linked[i] = bridge && bridge->peekBridgeInfo()->bridgeObjectID == object->getID();
		if (bridge)
		{
			ICoord2D lo, hi;
			Coord3D a = {bridge->getBounds()->lo.x, bridge->getBounds()->lo.y, 0.0f};
			Coord3D b = {bridge->getBounds()->hi.x, bridge->getBounds()->hi.y, 0.0f};
			TheAI->pathfinder()->worldToCell(&a, &lo);
			TheAI->pathfinder()->worldToCell(&b, &hi);
			Int ground = 0, wall = 0;
			for (Int x = lo.x - 1; x <= hi.x + 1; ++x)
				for (Int y = lo.y - 1; y <= hi.y + 1; ++y)
				{
					const PathfindCell* cell = TheAI->pathfinder()->getCell(bridge->getLayer(), x, y);
					if (!cell || cell->getLayer() != bridge->getLayer())
						continue;
					ground += cell->getConnectLayer() == LAYER_GROUND;
					wall += cell->getConnectLayer() == LAYER_WALL;
				}
			std::fprintf(stderr, "TEST wall-ramp-connections index=%d ground=%d wall=%d\n", i, ground, wall);
		}
	}
	std::fprintf(stderr, "TEST wall-ramp-links frame=%u first=%d second=%d\n", ctx.frame, linked[0], linked[1]);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailWallRampTestCommands()
{
#if RTS_ZEROHOUR
	registerAction("wall-ramp-probe", probeWallRamp);
	registerAction("wall-ramp-move", moveToWall);
	registerAction("wall-ramp-unit", probeUnit);
	registerAction("wall-ramp-footprint", probeFootprint);
	registerAction("wall-ramp-ground", probeGround);
	registerAction("wall-ramp-revoke", revokeUpgrade);
	registerAction("wall-ramp-pinched", probePinched);
	registerAction("wall-ramp-weapon", fireWeaponAt);
#endif
}
}
