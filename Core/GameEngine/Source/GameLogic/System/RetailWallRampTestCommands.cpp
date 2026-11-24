#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if RTS_ZEROHOUR
#include <cstdio>
#include "Common/Module.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/Object.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/RetailWallMesh.h"

namespace
{
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
}
}
#endif

namespace RetailTestCommands
{
void registerRetailWallRampTestCommands()
{
#if RTS_ZEROHOUR
	registerAction("wall-ramp-probe", probeWallRamp);
#endif
}
}
