#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if RTS_ZEROHOUR
#include <cstdio>
#include <string_view>

#include "Common/Money.h"
#include "Common/Player.h"
#include "Common/ThingTemplate.h"
#include "GameClient/Drawable.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/CastleBehavior.h"
#include "GameLogic/Module/FoundationAIUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/PolygonTrigger.h"
#include "GameLogic/ScriptConditions.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Scripts.h"

namespace
{
void unpack(const RetailTestCommands::Context& ctx)
{
	Object* object = ctx.requireObject();
	if (!object)
		return;

	const AsciiString name("CastlePerimeterFixturePacked");
	object->setName(name);
	TheScriptEngine->addObjectToCache(object);
	auto* action = newInstance(ScriptAction)(ScriptAction::NAMED_BASE_UNPACK_FREE);
	action->getParameter(0)->friend_setString(name);
	action->getParameter(1)->friend_setString("CastlePerimeterFixtureKeep");
	TheScriptEngine->friend_executeAction(action, object->getTeam());
	deleteInstance(action);
	TheScriptEngine->setRetailUnitReference("CastlePerimeterFixtureProbe", object);
	if (const auto* castle = CastleBehavior::find(object))
		if (Object* keep = castle->getKeep())
			keep->setStatus(MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_NO_ATTACK));
}

PolygonTrigger* findArea()
{
	for (const PolygonTrigger* polygon = PolygonTrigger::getFirstPolygonTrigger(); polygon; polygon = polygon->getNext())
	{
		if (std::string_view(polygon->getTriggerName().str()).starts_with("Dynamic_CastlePerimeterCamp_of_id_"))
		{
			if (auto* area = TheScriptEngine->getQualifiedTriggerAreaByName(polygon->getTriggerName()))
				return area;
		}
	}
	return nullptr;
}

void logArea(const RetailTestCommands::Context& ctx)
{
	const PolygonTrigger* area = findArea();
	const Object* probe = TheScriptEngine->getUnitNamed("CastlePerimeterFixtureProbe");
	Bool inside = FALSE;
	Bool entered = FALSE;
	if (area)
	{
		auto* condition = newInstance(Condition)(Condition::NAMED_INSIDE_AREA);
		condition->getParameter(0)->friend_setString("CastlePerimeterFixtureProbe");
		condition->getParameter(1)->friend_setString(area->getTriggerName());
		inside = TheScriptConditions->evaluateCondition(condition);
		condition->setConditionType(Condition::NAMED_ENTERED_AREA);
		condition->getParameter(0)->friend_setString("CastlePerimeterFixtureProbe");
		condition->getParameter(1)->friend_setString(area->getTriggerName());
		entered = TheScriptConditions->evaluateCondition(condition);
		deleteInstance(condition);
	}
	std::fprintf(stderr, "TEST castle-perimeter area frame=%u points=%d inside=%d cached=%d entered=%d\n",
		ctx.frame, area ? area->getNumPoints() : 0, inside, area && probe && probe->isInside(area), entered);
}

void guardArea(const RetailTestCommands::Context& ctx)
{
	Object* sentry = ctx.requireObject();
	const auto* area = findArea();
	if (sentry && sentry->getAI() && area)
		sentry->getAI()->aiGuardArea(area, GUARDMODE_GUARD_WITHOUT_PURSUIT, CMD_FROM_SCRIPT);
}

void logGuard(const RetailTestCommands::Context& ctx)
{
	const Object* sentry = ctx.requireObject();
	const auto* ai = sentry ? sentry->getAI() : nullptr;
	std::fprintf(stderr, "TEST castle-perimeter guard frame=%u area=%d\n", ctx.frame,
		ai && ai->getGuardTargetType() == GUARDTARGET_AREA);
}

void applyCondition(const RetailTestCommands::Context& ctx)
{
	Object* object = ctx.requireObject();
	if (!object)
		return;

	const AsciiString name("CastlePerimeterFixtureWall");
	object->setName(name);
	TheScriptEngine->addObjectToCache(object);
	auto* action = newInstance(ScriptAction)(ScriptAction::UNIT_SET_MODELCONDITION_FOR_DURATION);
	action->getParameter(0)->friend_setString(name);
	action->getParameter(1)->friend_setString(ctx.argument);
	action->getParameter(2)->friend_setReal(ctx.position.x);
	action->getParameter(3)->friend_setReal(0.0f);
	TheScriptEngine->friend_executeAction(action, object->getTeam());
	deleteInstance(action);
}

void logWall(const RetailTestCommands::Context& ctx)
{
	const Object* object = ctx.findObject(TRUE);
	if (!object || !object->getDrawable())
	{
		std::fprintf(stderr, "TEST castle-perimeter wall frame=%u missing\n", ctx.frame);
		return;
	}
	const auto& flags = object->getDrawable()->getModelConditionFlags();
	std::fprintf(stderr, "TEST castle-perimeter wall frame=%u under=%d progress=%d paid=%d captured=%d door=%d\n",
		ctx.frame, object->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION), object->getConstructionPercent() > 0.0f,
		object->getConstructionCostPaid(), flags.test(MODELCONDITION_CAPTURED), flags.test(MODELCONDITION_DOOR_1_OPENING));
}

void logMoney(const RetailTestCommands::Context& ctx)
{
	if (ctx.player)
		std::fprintf(stderr, "TEST castle-perimeter money frame=%u cash=%u\n",
			ctx.frame, ctx.player->getMoney()->countMoney());
}

void logPlot(const RetailTestCommands::Context& ctx)
{
	const Object* pad = ctx.requireObject();
	const auto* plot = pad ? FoundationAIUpdate::find(*pad) : nullptr;
	const Object* building = plot ? plot->getStructure() : nullptr;
	const Bool matches = building && !building->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION) &&
		std::string_view(building->getTemplate()->getName().str()) == ctx.argument;
	std::fprintf(stderr, "TEST castle-perimeter plot frame=%u index=%d matches=%d\n",
		ctx.frame, ctx.ordinal, matches);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailCastlePerimeterTestCommands()
{
#if RTS_ZEROHOUR
	registerAction("castle-perimeter-unpack", unpack);
	registerAction("castle-perimeter-area", logArea);
	registerAction("castle-perimeter-guard", guardArea);
	registerAction("castle-perimeter-guard-state", logGuard);
	registerAction("castle-perimeter-condition", applyCondition);
	registerAction("castle-perimeter-wall", logWall);
	registerAction("castle-perimeter-money", logMoney);
	registerAction("castle-perimeter-plot", logPlot);
#endif
}
}
