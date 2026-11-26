#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include "GameClient/View.h"
#include "GameLogic/LuaLogicBindings.h"
#include "GameLogic/LuaState.h"
#include "GameLogic/Object.h"
#include "GameLogic/ScriptEngine.h"

namespace
{
constexpr auto probes = R"(
function Begin()
    ExecuteAction("CAMERA_LETTERBOX_BEGIN")
end

function End()
    ExecuteAction("CAMERA_LETTERBOX_END")
end

function Kill(self)
    ExecuteAction("NAMED_KILL", self)
end
)";

int pushTarget(lua_State* state, void* data)
{
	pushLuaObject(state, static_cast<Object*>(data));
	return 1;
}

void runAction(const RetailTestCommands::Context& ctx)
{
	Object* target = ctx.findObject(true);
	LuaState lua;
	bindLogicLua(lua);
	const bool ok = lua.load(probes, "lua-actions") && lua.call(ctx.argument, pushTarget, target);
	std::fprintf(stderr, "TEST lua action frame=%u probe=%s ok=%d letterbox=%d dead=%d\n", ctx.frame,
		ctx.argument, ok, TheScriptEngine->isLetterBoxActive(), target && target->isEffectivelyDead());
}

void logCamera(const RetailTestCommands::Context& ctx)
{
	if (TheTacticalView)
		std::fprintf(stderr, "TEST lua action camera frame=%u clip=%.2f letterbox=%d\n",
			ctx.frame, TheTacticalView->getClipDepthMultiplier(), TheScriptEngine->isLetterBoxActive());
}
}
#endif

namespace RetailTestCommands
{
void registerRetailLuaActionsTestCommands()
{
#if !RTS_GENERALS
	registerAction("lua-action-probe", runAction);
	registerAction("lua-action-camera", logCamera);
#endif
}
}
