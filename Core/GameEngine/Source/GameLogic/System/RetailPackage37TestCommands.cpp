#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "Common/ThingTemplate.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"

namespace
{
void handlePackage37(const RetailTestCommands::Context& ctx)
{
	if (ctx.action == "pkg37-count")
	{
		Int count = 0;
		for (const Object* candidate = TheGameLogic->getFirstObject(); candidate; candidate = candidate->getNextObject())
			if (!candidate->isEffectivelyDead() && candidate->getControllingPlayer() == ctx.player &&
				std::strcmp(candidate->getTemplate()->getName().str(), ctx.targetName) == 0)
				++count;
		std::fprintf(stderr, "TEST pkg37 count frame=%u type=%s count=%d\n", ctx.frame, ctx.targetName, count);
		return;
	}

	Object* object = ctx.requireObject();
	if (!object)
		return;

	if (ctx.action == "pkg37-place")
	{
		if (ctx.hasPosition)
		{
			const Coord3D position = ctx.groundLocation();
			object->setPosition(&position);
		}
		return;
	}
	if (ctx.action == "pkg37-hold-fire")
	{
		object->setStatus(MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_NO_ATTACK));
		return;
	}
	if (ctx.action == "pkg37-heal")
	{
		object->attemptHealing(std::strtof(ctx.argument, nullptr), object);
		return;
	}
	if (ctx.action == "pkg37-hit" || ctx.action == "pkg37-fire")
	{
		DamageInfo damage;
		damage.in.m_amount = std::strtof(ctx.argument, nullptr);
		damage.in.m_damageType = ctx.action == "pkg37-fire" ? DAMAGE_FLAME : DAMAGE_UNRESISTABLE;
		damage.in.m_deathType = ctx.action == "pkg37-fire" ? DEATH_BURNED : DEATH_NORMAL;
		object->attemptDamage(&damage);
		std::fprintf(stderr, "TEST pkg37 hit frame=%u object=%s actual=%.1f\n", ctx.frame,
			object->getTemplate()->getName().str(), damage.out.m_actualDamageDealt);
		return;
	}

	const Coord3D& pos = *object->getPosition();
	Real waterHeight = 0;
	Real groundHeight = 0;
	const Bool wet = TheTerrainLogic->isUnderwater(pos.x, pos.y, &waterHeight, &groundHeight);
	const auto& model = object->getLogicModelConditions();
	const AIUpdateInterface* ai = object->getAI();
	const auto* contain = object->getContain();
	const auto* passengers = contain ? contain->getContainedItemsList() : nullptr;
	Int burnedPassengers = 0;
	if (passengers)
		for (const Object* passenger : *passengers)
		{
			const auto* lastDamage = passenger->getBodyModule()->getLastDamageInfo();
			if (lastDamage && lastDamage->in.m_sourceID == object->getID() && lastDamage->in.m_damageType == DAMAGE_FLAME)
				++burnedPassengers;
		}
	std::fprintf(stderr, "TEST pkg37 state frame=%u object=%s hp=%.2f aflame=%d burned=%d model=%d%d%d burntModel=%d panic=%d moving=%d passengers=%zu burnedPassengers=%d position=%.1f,%.1f water=%.1f\n",
		ctx.frame, object->getTemplate()->getName().str(), object->getBodyModule()->getHealth(),
		object->testStatus(OBJECT_STATUS_AFLAME), object->testStatus(OBJECT_STATUS_BURNED),
		model.test(MODELCONDITION_AFLAME), model.test(MODELCONDITION_SMOLDERING), model.test(MODELCONDITION_USER_3),
		model.test(MODELCONDITION_BURNT_MODEL),
		ai && ai->getCurLocomotorSetType() == LOCOMOTORSET_PANIC,
		ai && ai->isMoving(),
		passengers ? passengers->size() : size_t(0), burnedPassengers, pos.x, pos.y,
		wet ? waterHeight - groundHeight : 0.0f);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailPackage37TestCommands()
{
#if !RTS_GENERALS
	registerPrefix("pkg37-", handlePackage37);
#endif
}
}
