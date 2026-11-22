#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>
#include <cstdlib>

#include "Common/MessageStream.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/Damage.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/DozerAIUpdate.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Module/RubbleRiseUpdate.h"
#include "GameLogic/Module/WallHubBehavior.h"
#include "GameLogic/Object.h"

namespace
{
DozerAIInterface* workerAI(Object& object)
{
	auto* ai = object.getAIUpdateInterface();
	return ai ? ai->getDozerAIInterface() : nullptr;
}

const char* templateName(const Object* object)
{
	return object ? object->getTemplate()->getName().str() : "-";
}

void runRetailPackage36TestCommand(const RetailTestCommands::Context& ctx)
{
	Object* source = ctx.requireObject();
	if (!source)
		return;
	const char* name = templateName(source);
	if (ctx.action == "pkg36-foundation")
	{
		const ThingTemplate* type = TheThingFactory->findTemplate(ctx.argument, FALSE);
		if (!type)
			return;
		ctx.selectObject(source);
		GameMessage* message = TheMessageStream->appendMessage(GameMessage::MSG_FOUNDATION_CONSTRUCT);
		message->appendIntegerArgument(type->getTemplateID());
	}
	else if (ctx.action == "pkg36-site")
	{
		const auto* behavior = GettingBuiltBehavior::find(*source);
		const auto* loop = behavior ? behavior->getActiveLoop() : nullptr;
		const auto* rubble = static_cast<const RubbleRiseUpdate*>(source->findUpdateModule(NAMEKEY("RubbleRiseUpdate")));
		const Object* builder = TheGameLogic->findObjectByID(source->getBuilderID());
		const Object* producer = TheGameLogic->findObjectByID(source->getProducerID());
		const auto* body = source->getBodyModule();
		std::fprintf(stderr, "TEST pkg36 site frame=%u name=%s builder=%s producer=%s progress=%.1f under=%d health=%.1f rise=%.1f loop=%s\n",
			ctx.frame, name, templateName(builder), templateName(producer),
			source->getConstructionPercent(), source->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION),
			body->getHealth(), rubble ? rubble->getRiseOffset() : 0.0f, loop ? loop->str() : "-");
	}
	else if (ctx.action == "pkg36-damage")
	{
		DamageInfo damage;
		damage.in.m_damageType = DAMAGE_UNRESISTABLE;
		damage.in.m_deathType = DEATH_NORMAL;
		damage.in.m_amount = static_cast<Real>(std::atof(ctx.argument));
		source->attemptDamage(&damage);
	}
	else if (ctx.action == "pkg36-horde")
	{
		const HordeContain* contain = HordeContain::get(source);
		const auto* members = contain ? contain->getContainedItemsList() : nullptr;
		Int repairing = 0;
		if (members)
			for (Object* member : *members)
				if (auto* ai = workerAI(*member); ai && ai->isTaskPending(DOZER_TASK_REPAIR))
					++repairing;
		std::fprintf(stderr, "TEST pkg36 horde frame=%u name=%s repairing=%d\n", ctx.frame, name, repairing);
	}
	else if (ctx.action == "pkg36-lua")
	{
		const auto* ai = source->getAIUpdateInterface();
		std::fprintf(stderr, "TEST pkg36 lua frame=%u object=%s list=%s\n", ctx.frame, name,
			ai ? ai->getRetailLuaEventsList().str() : "-");
	}
	else if (ctx.action == "pkg36-attackinfo")
	{
		auto* ai = source->getAIUpdateInterface();
		std::fprintf(stderr, "TEST pkg36 attackinfo frame=%u object=%s resolved=%d\n", ctx.frame, name,
			ai && ai->getAttackInfo());
	}
	else if (ctx.action == "pkg36-dock")
	{
		auto* worker = workerAI(*source);
		const Coord3D* point = worker ? worker->getDockPoint(DOZER_TASK_REPAIR, DOZER_DOCK_POINT_ACTION) : nullptr;
		Coord3D position{};
		if (point)
			position = *point;
		std::fprintf(stderr, "TEST pkg36 dock frame=%u name=%s valid=%d x=%.1f y=%.1f\n",
			ctx.frame, name, point != nullptr, position.x, position.y);
	}
}
}
#endif

namespace RetailTestCommands
{
void registerRetailPackage36TestCommands()
{
#if !RTS_GENERALS
	registerPrefix("pkg36-", runRetailPackage36TestCommand);
#endif
}
}
