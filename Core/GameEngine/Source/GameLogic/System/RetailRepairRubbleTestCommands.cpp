#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdlib>
#include <cstdio>

#include "Common/Player.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "GameClient/Drawable.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/Damage.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/DozerAIUpdate.h"
#include "GameLogic/Module/FoundationAIUpdate.h"
#include "GameLogic/Module/RetailGate.h"
#include "GameLogic/Module/RubbleRiseUpdate.h"

namespace
{
void handleRepairRubble(const RetailTestCommands::Context& ctx)
{
	if (ctx.action == "rr-count")
	{
		Player* player = ctx.findPlayer();
		Int count = 0;
		for (Object* obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject())
		{
			if (obj->isDestroyed() || obj->isEffectivelyDead())
				continue;
			if (player && obj->getControllingPlayer() != player)
				continue;
			if (obj->getTemplate() && obj->getTemplate()->getName().compareNoCase(ctx.argument) == 0)
				++count;
		}
		std::fprintf(stderr, "TEST rr count frame=%u template=%s count=%d\n",
			ctx.frame, ctx.argument, count);
		return;
	}

	Object* source = ctx.requireObject(FALSE);
	if (!source)
		return;

	if (ctx.action == "rr-wall-state")
	{
		const BodyModuleInterface* body = source->getBodyModule();
		const BodyDamageType dmg = body ? body->getDamageState() : BODY_PRISTINE;
		const Drawable* draw = source->getDrawable();
		const Bool rubble = draw ? draw->getModelConditionFlags().test(MODELCONDITION_RUBBLE)
			: source->getLogicModelConditions().test(MODELCONDITION_RUBBLE);
		const Bool really = draw ? draw->getModelConditionFlags().test(MODELCONDITION_REALLY_DAMAGED)
			: source->getLogicModelConditions().test(MODELCONDITION_REALLY_DAMAGED);
		std::fprintf(stderr,
			"TEST rr wall frame=%u health=%.1f dead=%d rubble=%d really=%d pristine=%d bookending=%d nowalk=%d nocoll=%d benefactor=%u\n",
			ctx.frame, body ? body->getHealth() : 0.0f, source->isEffectivelyDead() ? 1 : 0,
			rubble ? 1 : 0, really ? 1 : 0,
			dmg == BODY_PRISTINE ? 1 : 0,
			source->testStatus(OBJECT_STATUS_BOOKENDING) ? 1 : 0,
			source->testStatus(OBJECT_STATUS_CAN_NOT_WALK_ON) ? 1 : 0,
			source->testStatus(OBJECT_STATUS_NO_COLLISIONS) ? 1 : 0,
			static_cast<unsigned>(source->getSoleHealingBenefactor()));
	}
	else if (ctx.action == "rr-worker-state")
	{
		AIUpdateInterface* ai = source->getAI();
		DozerAIInterface* dozer = ai ? ai->getDozerAIInterface() : nullptr;
		const Bool repairing = dozer && dozer->isTaskPending(DOZER_TASK_REPAIR);
		std::fprintf(stderr, "TEST rr worker frame=%u ordinal=%d task=%d\n",
			ctx.frame, ctx.ordinal, repairing ? 1 : 0);
	}
	else if (ctx.action == "rr-order-repair")
	{
		Object* target = ctx.findTarget(ctx.argument, FALSE);
		AIUpdateInterface* ai = source->getAI();
		if (target && ai)
			ai->aiRepair(target, CMD_FROM_AI);
	}
	else if (ctx.action == "rr-siege-damage")
	{
		Object* attacker = ctx.findTarget(ctx.argument, FALSE);
		DamageInfo info;
		info.in.m_damageType = DAMAGE_SIEGE;
		info.in.m_deathType = DEATH_NORMAL;
		info.in.m_amount = 100.0f;
		info.in.m_sourceID = attacker ? attacker->getID() : INVALID_ID;
		source->attemptDamage(&info);
	}
	else if (ctx.action == "rr-kill")
	{
		DamageInfo info;
		info.in.m_damageType = DAMAGE_UNRESISTABLE;
		info.in.m_deathType = DEATH_NORMAL;
		info.in.m_amount = 10000.0f;
		source->attemptDamage(&info);
	}
	else if (ctx.action == "rr-heal-full")
	{
		source->healCompletely();
	}
	else if (ctx.action == "rr-cell-owner")
	{
		Real dx = 0.0f;
		Real dy = 0.0f;
		std::sscanf(ctx.argument, "%f,%f", &dx, &dy);
		Coord3D probe = *source->getPosition();
		probe.x += dx;
		probe.y += dy;
		const PathfindCell* cell = TheAI && TheAI->pathfinder() ? TheAI->pathfinder()->getCell(LAYER_GROUND,
			REAL_TO_INT_FLOOR(probe.x / PATHFIND_CELL_SIZE_F), REAL_TO_INT_FLOOR(probe.y / PATHFIND_CELL_SIZE_F)) : nullptr;
		const Bool owned = cell && cell->getType() == PathfindCell::CELL_OBSTACLE && cell->getObstacleID() == source->getID();
		std::fprintf(stderr, "TEST rr cell frame=%u dx=%.0f dy=%.0f owned=%d\n",
			ctx.frame, dx, dy, owned ? 1 : 0);
	}
	else if (ctx.action == "rr-plot-build")
	{
		FoundationAIUpdate* plot = FoundationAIUpdate::find(*source);
		const ThingTemplate* what = TheThingFactory->findTemplate(ctx.argument, FALSE);
		if (plot && what)
			plot->construct(what, source->getControllingPlayer(), FALSE);
	}
	else if (ctx.action == "rr-plot-state")
	{
		const Drawable* drawable = source->getDrawable();
		std::fprintf(stderr, "TEST rr plot frame=%u name=%s unselectable=%d hidden=%d\n",
			ctx.frame, ctx.targetName,
			source->testStatus(OBJECT_STATUS_UNSELECTABLE) ? 1 : 0,
			drawable && drawable->isDrawableEffectivelyHidden() ? 1 : 0);
	}
	else if (ctx.action == "rr-gate-open")
	{
		if (GateOpenAndCloseBehavior* gate = GateOpenAndCloseBehavior::find(source))
		{
			gate->request(TRUE);
			std::fprintf(stderr, "TEST rr gate frame=%u sound=%u\n",
				ctx.frame, gate->getSoundFrame());
		}
	}
	else if (ctx.action == "rr-rise-state")
	{
		const auto* rubble = static_cast<const RubbleRiseUpdate*>(source->findUpdateModule(NAMEKEY("RubbleRiseUpdate")));
		std::fprintf(stderr, "TEST rr rise frame=%u height=%.1f postrubble=%d postcollapse=%d\n",
			ctx.frame, rubble ? rubble->getRiseOffset() : 0.0f,
			source->getLogicModelConditions().test(MODELCONDITION_POST_RUBBLE) ? 1 : 0,
			source->getLogicModelConditions().test(MODELCONDITION_POST_COLLAPSE) ? 1 : 0);
	}
}
}
#endif

namespace RetailTestCommands
{
void registerRetailRepairRubbleTestCommands()
{
#if !RTS_GENERALS
	registerPrefix("rr-", handleRepairRubble);
#endif
}
}
