#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include "GameLogic/LuaDispatchProbe.h"

namespace
{
void reenterModelEvent(const RetailTestCommands::Context& ctx)
{
	if (Object* object = ctx.requireObject(true))
		triggerLuaModelEvent(*object, ctx.argument);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailLuaBulkEdgeTestCommands()
{
#if !RTS_GENERALS
	registerAction("lua-bulk-reenter-model", reenterModelEvent);
#endif
}
}
