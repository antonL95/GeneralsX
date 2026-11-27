#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if RTS_ZEROHOUR
#include <cstdio>
#include <string_view>

#include "Common/Player.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/Object.h"
#include "GameLogic/PolygonTrigger.h"
#include "GameLogic/ScriptConditions.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Scripts.h"

namespace
{
void transfer(const RetailTestCommands::Context& ctx)
{
	Object* packed = ctx.requireObject();
	Player* player = ctx.findPlayer(ctx.argument);
	if (!packed || !player)
		return;
	packed->setName(packed->getTemplate()->getName());
	TheScriptEngine->addObjectToCache(packed);
	auto* action = newInstance(ScriptAction)(ScriptAction::NAMED_BASE_UNPACK_FREE);
	action->getParameter(0)->friend_setString(packed->getName());
	action->getParameter(1)->friend_setString("CastleAreaBackfillKeep");
	TheScriptEngine->friend_executeAction(action, packed->getTeam());
	deleteInstance(action);
	packed->setTeam(player->getDefaultTeam());
}

void forgetAreas(const RetailTestCommands::Context& ctx)
{
	if (const Object* packed = ctx.requireObject())
		PolygonTrigger::retireCastleAreas(packed->getID());
}

void probe(const RetailTestCommands::Context& ctx)
{
	const Object* packed = ctx.requireObject();
	if (!packed)
		return;
	AsciiString prefix;
	prefix.format("Dynamic_%s_of_id_%d_at_index_", packed->getTemplate()->getName().str(), packed->getID());
	AsciiString areaName;
	areaName.format("%s0", prefix.str());
	const PolygonTrigger* area = TheScriptEngine->getQualifiedTriggerAreaByName(areaName);
	Int active = 0;
	for (const auto* polygon = PolygonTrigger::getFirstPolygonTrigger(); polygon; polygon = polygon->getNext())
	{
		if (polygon->isCastleArea() && !polygon->isRetired() &&
			std::string_view(polygon->getTriggerName().str()).starts_with(prefix.str()))
			++active;
	}
	Bool inside = FALSE;
	if (area)
	{
		auto* condition = newInstance(Condition)(Condition::NAMED_INSIDE_AREA);
		condition->getParameter(0)->friend_setString(packed->getName());
		condition->getParameter(1)->friend_setString(areaName);
		inside = TheScriptConditions->evaluateCondition(condition);
		deleteInstance(condition);
	}
	std::fprintf(stderr, "TEST castle-area-backfill frame=%u points=%d active=%d inside=%d cached=%d captured=%d\n",
		ctx.frame, area ? area->getNumPoints() : 0, active, inside, area && packed->isInside(area),
		packed->getControllingPlayer() == ctx.findPlayer("PlyrCivilian"));
	if (area)
	{
		std::fprintf(stderr, "TEST castle-area-backfill geometry frame=%u", ctx.frame);
		for (Int i = 0; i < area->getNumPoints(); ++i)
			std::fprintf(stderr, " %d,%d", area->getPoint(i)->x, area->getPoint(i)->y);
		std::fputc('\n', stderr);
	}
}
}
#endif

namespace RetailTestCommands
{
void registerRetailCastleAreaBackfillTestCommands()
{
#if RTS_ZEROHOUR
	registerAction("castle-area-backfill-transfer", transfer);
	registerAction("castle-area-backfill-forget", forgetAreas);
	registerAction("castle-area-backfill-probe", probe);
#endif
}
}
