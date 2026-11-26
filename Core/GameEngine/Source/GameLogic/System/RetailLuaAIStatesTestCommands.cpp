#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include "GameLogic/LuaLogicBindings.h"
#include "GameLogic/LuaScriptEngine.h"
#include "GameLogic/LuaState.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Object.h"

namespace
{
constexpr auto probes = R"(
function ProbeAlert(self)
    ObjectEnterAlertState(self)
end

function ProbeFear(self, other)
    ObjectEnterFearState(self, other, false)
end

function ProbeDescription(self)
    print(ObjectDescription(self))
end
)";

struct ProbeArguments
{
	const Object* self;
	const Object* other;
};

int pushProbeArguments(lua_State* state, void* data)
{
	const auto& args = *static_cast<const ProbeArguments*>(data);
	pushLuaObject(state, args.self);
	pushLuaObject(state, args.other);
	return 2;
}

void runProbe(const RetailTestCommands::Context& ctx)
{
	const Object* object = ctx.requireObject(true);
	if (!object)
		return;

	const char* function = ctx.argument;
	const Object* threat = nullptr;
	if (ctx.action == "lua-ai-fear-probe")
	{
		function = "ProbeFear";
		threat = ctx.findTarget(ctx.argument, true);
		if (!threat)
			return;
	}

	LuaState lua;
	bindLogicLua(lua);
	ProbeArguments args{object, threat};
	const bool ok = lua.load(probes, "lua-ai-states") && lua.call(function, pushProbeArguments, &args);
	const auto& conditions = object->getLogicModelConditions();
	std::fprintf(stderr, "TEST lua ai frame=%u probe=%s ok=%d alert=%d fear=%d\n", ctx.frame, function, ok,
		conditions.test(MODELCONDITION_EMOTION_ALERT), conditions.test(MODELCONDITION_EMOTION_AFRAID));
}

Object* firstMember(Object& horde)
{
	const auto* contain = horde.getContain();
	const auto* members = contain ? contain->getContainedItemsList() : nullptr;
	return members && !members->empty() ? members->front() : nullptr;
}

void dispatchChant(const RetailTestCommands::Context& ctx)
{
	Object* horde = ctx.requireObject(true);
	auto* lua = LuaScriptEngine::active();
	if (horde && lua)
		if (Object* member = firstMember(*horde))
			lua->dispatchScripted(*member, ctx.argument);
}

void logChant(const RetailTestCommands::Context& ctx)
{
	Object* horde = ctx.requireObject(true);
	if (horde)
		if (const Object* member = firstMember(*horde))
			std::fprintf(stderr, "TEST lua chant frame=%u member=%d horde=%d\n", ctx.frame,
				member->testStatus(OBJECT_STATUS_CHANTING), horde->testStatus(OBJECT_STATUS_CHANTING));
}
}
#endif

namespace RetailTestCommands
{
void registerRetailLuaAIStatesTestCommands()
{
#if !RTS_GENERALS
	registerAction("lua-ai-probe", runProbe);
	registerAction("lua-ai-fear-probe", runProbe);
	registerAction("lua-ai-chant-event", dispatchChant);
	registerAction("lua-ai-chant", logChant);
#endif
}
}
