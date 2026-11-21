#include "PreRTS.h"

#include "GameLogic/Module/RotwkPowerState.h"

#include <algorithm>

#include "Common/Xfer.h"
#include "Common/SpecialPowerINI.h"
#include "GameClient/Drawable.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"

void Object::grantPowerInvisibility(UnsignedInt duration, Real range, const ModelConditionFlags& forbidden)
{
	if (duration == 0 || isEffectivelyDead())
		return;

	if (!m_rotwkPowerState)
		m_rotwkPowerState = std::make_unique<RotwkPowerState>();
	RotwkPowerState& state = *m_rotwkPowerState;
	const UnsignedInt now = TheGameLogic->getFrame();
	state.invisibilityEnd = std::max(now, state.invisibilityEnd) + duration;
	state.detectionRange = range;
	state.invisibilityForbidden = forbidden;
	updateRotwkPowerState();
}

void Object::setPowerModelCondition(Int condition, UnsignedInt duration)
{
	if (condition < 0 || duration == 0)
		return;

	if (!m_rotwkPowerState)
		m_rotwkPowerState = std::make_unique<RotwkPowerState>();
	RotwkPowerState& state = *m_rotwkPowerState;
	if (state.condition != MODELCONDITION_INVALID && state.condition != condition)
		clearModelConditionState(static_cast<ModelConditionFlagType>(state.condition));
	state.condition = condition;
	state.conditionEnd = TheGameLogic->getFrame() + duration;
	setModelConditionState(static_cast<ModelConditionFlagType>(condition));
}

void Object::updateRotwkPowerState()
{
	if (!m_rotwkPowerState)
		return;

	RotwkPowerState& state = *m_rotwkPowerState;
	const UnsignedInt now = TheGameLogic->getFrame();
	if (state.conditionEnd != 0 && now >= state.conditionEnd)
	{
		clearModelConditionState(static_cast<ModelConditionFlagType>(state.condition));
		state.condition = MODELCONDITION_INVALID;
		state.conditionEnd = 0;
	}

	Bool hidden = now < state.invisibilityEnd && !isEffectivelyDead();
	if (hidden && getDrawable() && getDrawable()->getModelConditionFlags().anyIntersectionWith(state.invisibilityForbidden))
		hidden = FALSE;
	if (hidden && state.detectionRange > 0.0f)
	{
		SimpleObjectIterator* nearby = ThePartitionManager->iterateObjectsInRange(getPosition(),
			state.detectionRange, FROM_CENTER_2D, nullptr, ITER_FASTEST);
		MemoryPoolObjectHolder hold(nearby);
		for (Object* other = nearby->first(); other; other = nearby->next())
		{
			if (!other->isEffectivelyDead() && !other->isContained() && getRelationship(other) == ENEMIES)
			{
				hidden = FALSE;
				break;
			}
		}
	}

	const auto stealthed = MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_STEALTHED);
	if (hidden && !testStatus(OBJECT_STATUS_STEALTHED))
	{
		setStatus(stealthed);
		state.powerStealthed = TRUE;
	}
	else if (!hidden && state.powerStealthed)
	{
		clearStatus(stealthed);
		state.powerStealthed = FALSE;
	}
	if (state.passenger != INVALID_ID && now >= state.consumeFrame)
	{
		Object* passenger = TheGameLogic->findObjectByID(state.passenger);
		state.passenger = INVALID_ID;
		if (passenger && passenger->getContainedBy() == this && !passenger->isEffectivelyDead() && !isEffectivelyDead())
		{
			getContain()->removeFromContain(passenger);
			passenger->kill();
			if (state.healFraction > 0.0f)
				attemptHealing(getBodyModule()->getMaxHealth() * state.healFraction, this);
		}
	}
	if (now >= state.invisibilityEnd && state.conditionEnd == 0 && state.passenger == INVALID_ID)
		m_rotwkPowerState.reset();
}

void Object::xferRotwkPowerState(Xfer* xfer)
{
	Bool present = m_rotwkPowerState != nullptr;
	xfer->xferBool(&present);
	if (xfer->getXferMode() == XFER_LOAD)
		m_rotwkPowerState = present ? std::make_unique<RotwkPowerState>() : nullptr;
	if (!present)
		return;

	RotwkPowerState& state = *m_rotwkPowerState;
	state.invisibilityForbidden.xfer(xfer);
	xfer->xferReal(&state.detectionRange);
	xfer->xferUnsignedInt(&state.invisibilityEnd);
	xfer->xferBool(&state.powerStealthed);
	xfer->xferInt(&state.condition);
	xfer->xferUnsignedInt(&state.conditionEnd);
	xfer->xferObjectID(&state.passenger);
	xfer->xferUnsignedInt(&state.consumeFrame);
	xfer->xferReal(&state.healFraction);
}

void Object::schedulePassengerConsumption(ObjectID passenger, const SpecialPowerAnimation& animation, Real healFraction)
{
	if (!m_rotwkPowerState)
		m_rotwkPowerState = std::make_unique<RotwkPowerState>();

	const auto frames = [](UnsignedInt msecs)
	{
		return static_cast<UnsignedInt>((static_cast<UnsignedInt64>(msecs) * LOGICFRAMES_PER_SECOND + 999) / 1000);
	};
	RotwkPowerState& state = *m_rotwkPowerState;
	state.passenger = passenger;
	state.consumeFrame = TheGameLogic->getFrame() + frames(animation.m_triggerTime);
	state.healFraction = healFraction;
	setPowerModelCondition(animation.m_condition, frames(animation.m_duration));
}
