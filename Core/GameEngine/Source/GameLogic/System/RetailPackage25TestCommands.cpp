#include "PreRTS.h"

#if RTS_ZEROHOUR
#include "Common/ThingTemplate.h"

#include "GameLogic/Locomotor.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Module/AttributeModifierHelper.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Weapon.h"


#include <cstdio>
#include <string_view>

static void probeCharge(Object* object, const Locomotor* locomotor)
{
	const auto attack = MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_IS_ATTACKING);
	const auto ability = MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_IS_USING_ABILITY);
	const Bool wasAttacking = object->testStatus(OBJECT_STATUS_IS_ATTACKING);
	const Bool wasUsingAbility = object->testStatus(OBJECT_STATUS_IS_USING_ABILITY);
	const Bool wasCharging = object->getLogicModelConditions().test(MODELCONDITION_CHARGING);
	object->setStatus(attack);
	const Bool normal = locomotor->hasRetailChargeSpeed(BODY_PRISTINE, object);
	const Bool damaged = locomotor->hasRetailChargeSpeed(BODY_REALLYDAMAGED, object);
	const Real attackSpeed = locomotor->getMaxSpeedForCondition(BODY_PRISTINE, object);
	const Real damagedSpeed = locomotor->getMaxSpeedForCondition(BODY_REALLYDAMAGED, object);
	object->clearStatus(attack);
	object->setStatus(ability);
	object->setModelConditionState(MODELCONDITION_CHARGING);
	const Bool special = locomotor->hasRetailChargeSpeed(BODY_PRISTINE, object);
	const Real specialSpeed = locomotor->getMaxSpeedForCondition(BODY_PRISTINE, object);
	object->setStatus(attack, wasAttacking);
	object->setStatus(ability, wasUsingAbility);
	if (!wasCharging)
		object->clearModelConditionState(MODELCONDITION_CHARGING);
	std::fprintf(stderr, "TEST pkg25 charge object=%s normal=%d damaged=%d special=%d base=%.4f attack=%.4f hurt=%.4f specialSpeed=%.4f\n",
		object->getTemplate()->getName().str(), normal, damaged, special, locomotor->getMaxSpeedForCondition(BODY_PRISTINE),
		attackSpeed, damagedSpeed, specialSpeed);
}

void runRetailPackage25TestCommand(std::string_view action, Object* object, Object* target, const Coord3D* position, UnsignedInt frame)
{
	AIUpdateInterface* ai = object ? object->getAI() : nullptr;
	if (!ai)
		return;

	if (action == "pkg25-move" && position)
		ai->aiMoveToPositionEvenIfSleeping(position, CMD_FROM_SCRIPT);
	else if (action == "pkg25-stop")
		ai->aiIdle(CMD_FROM_SCRIPT);
	else if (action == "pkg25-burn")
		std::fprintf(stderr, "TEST pkg25 burn frame=%u object=%s entered=%d\n", frame, object->getTemplate()->getName().str(), ai->enterRetailBurningDeath());
	else if (action == "pkg25-contain" && target && target->getContain())
		target->getContain()->addToContain(object);
	else if (action == "pkg25-exit" && object->getContainedBy() && object->getContainedBy()->getContain())
		object->getContainedBy()->getContain()->removeFromContain(object);
	else if (action == "pkg25-attack" && target)
		ai->aiAttackObject(target, NO_MAX_SHOTS_LIMIT, CMD_FROM_PLAYER);
	else if ((action == "pkg25-fear" || action == "pkg25-back") && target)
		ai->fleeFromFear(target, LOGICFRAMES_PER_SECOND, MODELCONDITION_EMOTION_UNCONTROLLABLY_AFRAID, 1.0f, action == "pkg25-back");
	else if (action == "pkg25-charge-follow")
	{
		const auto* horde = HordeContain::get(object->getContainedBy());
		HordeContain::FormationGoal goal;
		if (!horde || !horde->getFormationGoal(object, goal))
			return;
		ai->aiIdle(CMD_FROM_SCRIPT);
		Coord3D point = goal.slot;
		point.x -= 85.0f;
		point.z = TheTerrainLogic->getGroundHeight(point.x, point.y);
		object->setPosition(&point);
		object->setOrientation(0.0f);
		if (auto* physics = object->getPhysics())
			physics->resetDynamicPhysics();

		object->setStatus(MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_IS_ATTACKING));
		ai->wakeForFormation();
	}
	else if (action == "pkg25-slow")
		AttributeModifiers::apply(object, AsciiString("SarumanCrushEffect"), 180);
	else if (action == "pkg25-member")
	{
		const auto* horde = HordeContain::get(object->getContainedBy());
		const auto* physics = object->getPhysics();
		const auto* locomotor = ai->getCurLocomotor();
		HordeContain::FormationGoal goal;
		if (!horde || !physics || !locomotor || !horde->getFormationGoal(object, goal))
			return;
		const auto damage = object->getBodyModule()->getDamageState();
		const auto& flags = object->getLogicModelConditions();
		std::fprintf(stderr, "TEST pkg25 member frame=%u object=%s loco=%s velocity=%.4f base=%.4f adjusted=%.4f requested=%.4f backing=%d walk=%d accel=%d decel=%d idle=%d clear=%d error=%.2f\n",
			frame, object->getTemplate()->getName().str(), locomotor->getTemplateName().str(),
			physics->getVelocityMagnitude(), locomotor->getMaxSpeedForCondition(damage), locomotor->getMaxSpeedForCondition(damage, object),
			goal.speed, goal.backing, flags.test(MODELCONDITION_WALKING), flags.test(MODELCONDITION_ACCELERATE),
			flags.test(MODELCONDITION_DECELERATE), ai->isIdle(), HordeContain::canSteerToFormationGoal(object, goal), goal.error);
	}

	const Locomotor* locomotor = ai->getCurLocomotor();
	if (!locomotor)
		return;
	if (action == "pkg25-charge")
		probeCharge(object, locomotor);
	if (action == "pkg25-state" || action == "pkg25-burn" || action == "pkg25-contain" || action == "pkg25-exit")
	{
		const Coord3D& pos = *object->getPosition();
		const Coord3D& goal = *ai->getGoalPosition();
		const auto& flags = object->getLogicModelConditions();
		const PhysicsBehavior* physics = object->getPhysics();
		std::fprintf(stderr, "TEST pkg25 state frame=%u object=%s set=%d loco=%s speed=%.4f x=%.3f y=%.3f facing=%.4f actual=%.4f goal=%.3f,%.3f moving=%d walk=%d accel=%d decel=%d charge=%d combo=%d container=%d radius=%.1f cavalry=%d\n",
			frame, object->getTemplate()->getName().str(), static_cast<Int>(ai->getCurLocomotorSetType()),
			locomotor->getTemplateName().str(), ai->getCurLocomotorSpeed(), pos.x, pos.y, object->getOrientation(),
			physics ? physics->getVelocityMagnitude() : 0.0f, goal.x, goal.y, ai->isMoving(),
			flags.test(MODELCONDITION_WALKING), flags.test(MODELCONDITION_ACCELERATE), flags.test(MODELCONDITION_DECELERATE),
			locomotor->hasRetailChargeSpeed(object->getBodyModule()->getDamageState(), object),
			flags.test(MODELCONDITION_USING_COMBO_LOCOMOTOR), object->getContainedBy() != nullptr,
			locomotor->getBurningDeathRadius(), locomotor->isBurningDeathCavalry());
	}
}
#endif
