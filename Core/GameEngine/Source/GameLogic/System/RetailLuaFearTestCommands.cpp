#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include "Common/ThingTemplate.h"
#include "GameLogic/LuaScriptEngine.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Object.h"

namespace
{
void dispatchFear(const RetailTestCommands::Context& ctx)
{
	auto* lua = LuaScriptEngine::active();
	Object* object = ctx.requireObject(true);
	if (object && ctx.action == "lua-fear-member-event")
	{
		const auto* horde = HordeContain::get(object);
		const auto* members = horde ? horde->getContainedItemsList() : nullptr;
		object = members && !members->empty() ? members->front() : nullptr;
	}
	const std::string_view argument(ctx.argument);
	const auto separator = argument.find(',');
	if (separator == std::string_view::npos)
		return;
	const std::string_view event = argument.substr(0, separator);
	const Object* source = ctx.findTarget(argument.substr(separator + 1), true);
	if (!lua || !object || !source)
		return;
	const bool ran = lua->dispatchScripted(*object, event, source);
	std::fprintf(stderr, "TEST lua fear dispatch frame=%u event=%.*s ran=%d\n", ctx.frame,
		static_cast<int>(event.size()), event.data(), ran);
}

void logFear(const RetailTestCommands::Context& ctx)
{
	const Object* object = ctx.requireObject(true);
	const auto* ai = object ? object->getAI() : nullptr;
	if (!ai)
		return;
	const auto& conditions = object->getLogicModelConditions();
	const Coord3D& position = *object->getPosition();
	const Coord3D* goal = ai->getGoalPosition();
	std::fprintf(stderr,
		"TEST lua fear state frame=%u unit=%s afraid=%d terror=%d uncontrolled=%d x=%.3f y=%.3f angle=%.3f goalx=%.3f goaly=%.3f\n",
		ctx.frame, object->getTemplate()->getName().str(), conditions.test(MODELCONDITION_EMOTION_AFRAID),
		conditions.test(MODELCONDITION_EMOTION_TERROR), conditions.test(MODELCONDITION_EMOTION_UNCONTROLLABLY_AFRAID),
		position.x, position.y, object->getOrientation(), goal->x, goal->y);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailLuaFearTestCommands()
{
#if !RTS_GENERALS
	registerAction("lua-fear-event", dispatchFear);
	registerAction("lua-fear-member-event", dispatchFear);
	registerAction("lua-fear-state", logFear);
#endif
}
}
