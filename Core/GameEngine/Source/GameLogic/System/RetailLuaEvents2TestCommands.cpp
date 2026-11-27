#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include "GameLogic/Damage.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Object.h"
#include "GameLogic/PolygonTrigger.h"

namespace
{
void addTrigger(const RetailTestCommands::Context& ctx)
{
	if (!ctx.hasPosition)
		return;
	const Int x = REAL_TO_INT(ctx.position.x);
	const Int y = REAL_TO_INT(ctx.position.y);
	auto* trigger = newInstance(PolygonTrigger)(4, 32005);
	trigger->setTriggerName("LuaEventsWalkThrough");
	trigger->addPoint({x - 20, y - 20, 0});
	trigger->addPoint({x + 20, y - 20, 0});
	trigger->addPoint({x + 20, y + 20, 0});
	trigger->addPoint({x - 20, y + 20, 0});
	PolygonTrigger::addPolygonTrigger(trigger);
}

void hitObject(const RetailTestCommands::Context& ctx)
{
	const auto* source = ctx.requireObject(true);
	auto* target = ctx.findTarget();
	if (!source || !target)
		return;
	DamageInfo damage;
	damage.in.m_sourceID = source->getID();
	damage.in.m_damageType = DAMAGE_UNRESISTABLE;
	damage.in.m_deathType = DEATH_NORMAL;
	damage.in.m_amount = 10.0f;
	target->attemptDamage(&damage);
}

void reposition(const RetailTestCommands::Context& ctx)
{
	if (!ctx.hasPosition)
		return;
	if (Object* object = ctx.requireObject(true))
	{
		const Coord3D position = ctx.groundLocation();
		object->setPosition(&position);
	}
}

void logState(const RetailTestCommands::Context& ctx)
{
	const Object* object = ctx.requireObject(true);
	if (!object)
		return;
	const auto& conditions = object->getLogicModelConditions();
	const auto* ai = object->getAIUpdateInterface();
	const auto* position = object->getPosition();
	std::fprintf(stderr, "TEST lua events frame=%u chant=%d enrage=%d fear=%d terror=%d fleeing=%d x=%.3f y=%.3f health=%.2f\n",
		ctx.frame, object->testStatus(OBJECT_STATUS_CHANTING), object->testStatus(OBJECT_STATUS_ENRAGED),
		conditions.test(MODELCONDITION_EMOTION_AFRAID), conditions.test(MODELCONDITION_EMOTION_TERROR),
		ai && ai->getRetailFearDeadline() > ctx.frame, position->x, position->y, object->getBodyModule()->getHealth());
}
}
#endif

namespace RetailTestCommands
{
void registerRetailLuaEvents2TestCommands()
{
#if !RTS_GENERALS
	registerAction("lua-events-area", addTrigger);
	registerAction("lua-events-hit", hitObject);
	registerAction("lua-events-reposition", reposition);
	registerAction("lua-events-state", logState);
#endif
}
}
