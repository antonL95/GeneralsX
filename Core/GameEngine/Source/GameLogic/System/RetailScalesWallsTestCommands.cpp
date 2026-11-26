#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if RTS_ZEROHOUR
#include <cstdio>
#include "Common/MessageStream.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/Object.h"
#include "GameLogic/RetailWallMesh.h"

namespace
{
void probeWall(const RetailTestCommands::Context& ctx)
{
    const Object* object = ctx.requireObject(FALSE);
    const auto* geometry = object ? RetailWallMesh::forObject(*object) : nullptr;
    if (!geometry)
        return;
    for (const Coord3D& point : geometry->wall)
        std::fprintf(stderr, "TEST scales-walls-vertex x=%.2f y=%.2f z=%.2f\n", point.x, point.y, point.z);
}

void moveToWall(const RetailTestCommands::Context& ctx)
{
    Object* object = ctx.requireObject(FALSE);
    if (!object || !ctx.hasPosition)
        return;
    Coord3D point = ctx.groundLocation();
    point.z = TheAI->pathfinder()->getWallHeightAt(point);
    std::fprintf(stderr, "TEST scales-walls-destination frame=%u name=%s wall=%d\n",
        ctx.frame, ctx.targetName, TheAI->pathfinder()->isPointOnWall(&point));
    ctx.selectObject(object);
    TheMessageStream->appendMessage(GameMessage::MSG_DO_MOVETO)->appendLocationArgument(point);
}

void sample(const RetailTestCommands::Context& ctx)
{
    const Object* object = ctx.requireObject(FALSE);
    if (!object)
        return;
    std::fprintf(stderr, "TEST scales-walls frame=%u name=%s z=%.2f layer=%d\n", ctx.frame,
        ctx.targetName, object->getPosition()->z, object->getLayer());
}
}
#endif

namespace RetailTestCommands
{
void registerRetailScalesWallsTestCommands()
{
#if RTS_ZEROHOUR
    registerAction("scales-walls-probe", probeWall);
    registerAction("scales-walls-move", moveToWall);
    registerAction("scales-walls-sample", sample);
#endif
}
}
