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
#if !RTS_GENERALS
#include "Common/GameUtility.h"
#include "Common/GlobalData.h"
#include "Common/NameKeyGenerator.h"
#include "Common/Player.h"
#include "GameLogic/Module/RetailDetectionModules.h"
#include "GameLogic/Module/StealthUpdate.h"
#endif


#if !RTS_GENERALS
void Object::offerRetailInvisibility(ObjectID source, const AsciiString& module, UnsignedInt expiry)
{
	if (!m_rotwkPowerState)
		m_rotwkPowerState = std::make_unique<RotwkPowerState>();
	auto& entries = m_rotwkPowerState->invisibilitySources;
	const auto found = std::find_if(entries.begin(), entries.end(), [&](const auto& entry)
	{
		return entry.object == source && entry.module == module;
	});
	if (found == entries.end())
		entries.push_back({source, module, expiry});
	else
		found->expires = expiry;
}

Bool Object::hasPowerInvisibility() const
{
	return m_rotwkPowerState && m_rotwkPowerState->powerStealthed;
}

Bool Object::isPowerInvisibilityDetected(UnsignedInt now) const
{
	return hasPowerInvisibility() && m_rotwkPowerState->detectionEnd > now;
}

void Object::markRetailInvisibilityDetected(UnsignedInt duration)
{
	if (!m_rotwkPowerState || !m_rotwkPowerState->powerStealthed)
		return;
	m_rotwkPowerState->detectionEnd = std::max(m_rotwkPowerState->detectionEnd, TheGameLogic->getFrame() + duration);
	setStatus(MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_DETECTED));
}

#endif

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

#if !RTS_GENERALS
void Object::applyUpgradeAnimation(const SpecialPowerAnimation& animation)
{
	if (animation.m_condition < 0)
		return;

	if (animation.m_duration == 0)
		setModelConditionState(static_cast<ModelConditionFlagType>(animation.m_condition));
	else
		setTimedModelCondition(static_cast<ModelConditionFlagType>(animation.m_condition),
			static_cast<UnsignedInt>((static_cast<UnsignedInt64>(animation.m_duration) * LOGICFRAMES_PER_SECOND + 999) / 1000));
}
#endif

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
#if !RTS_GENERALS
	const Bool wasHidden = state.powerStealthed;
	StealthUpdate* stealth = getStealth();
	const RetailInvisibilityNugget* nugget = nullptr;
	Bool hasHint = FALSE;
	Bool hint = FALSE;
	std::erase_if(state.invisibilitySources, [&](const auto& entry)
	{
		if (now >= entry.expires)
			return true;
		const Object* source = TheGameLogic->findObjectByID(entry.object);
		const auto* module = source ? InvisibilityUpdate::find(source, TheNameKeyGenerator->nameToKey(entry.module)) : nullptr;
		if (!module || source->isEffectivelyDead())
			return true;
		const auto& candidate = module->getNugget();
		const Bool enabled = module->isEnabled();
		if (enabled && candidate.m_hintDetected.any())
		{
			hasHint = TRUE;
			hint = hint || getStatusBits().testForAny(candidate.m_hintDetected);
		}
		const Object* subject = getContainedBy();
		if (!subject || !subject->isKindOf(KINDOF_HORDE))
			subject = this;
		const Bool stronger = !nugget || candidate.m_type > nugget->m_type ||
			(candidate.m_type == nugget->m_type && candidate.m_detectionRange < nugget->m_detectionRange);
		if (enabled && stronger && candidate.canConceal(subject))
			nugget = &candidate;
		return false;
	});
	const Bool camouflage = !hidden && nugget && nugget->m_type == RetailInvisibilityNugget::CAMOUFLAGE;
	hidden = hidden || nugget;
	const UnsignedInt reinvisibilityDelay = TheGlobalData->m_reinvisibilityDelay;
	if (wasHidden && !hidden && !isEffectivelyDead())
		state.reinvisibilityFrame = now + reinvisibilityDelay;
	if (now >= state.reinvisibilityFrame)
		state.reinvisibilityFrame = 0;
	else
		hidden = FALSE;
	if (hidden && camouflage && nugget->m_detectionRange > 0.0f &&
		RetailDetection::hasNearbyObserver(this, nugget->m_detectionRange, nugget->m_options & RetailInvisibilityNugget::DETECTED_BY_FRIENDLIES,
			&TheGlobalData->m_camouflageDetectorObjectFilter))
		state.detectionEnd = std::max(state.detectionEnd, now + std::max(1u, reinvisibilityDelay));
	state.powerStealthed = hidden;
	if (hidden)
		setStatus(stealthed);
	else if (wasHidden && (!stealth || !stealth->isLegacyStealthed()))
		clearStatus(stealthed);

	if (hidden || wasHidden)
	{
		const auto detected = MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_DETECTED);
		if (isPowerInvisibilityDetected(now) || (stealth && stealth->isLegacyDetected(now)))
			setStatus(detected);
		else
			clearStatus(detected);

		const auto mask = MAKE_MODELCONDITION_MASK2(MODELCONDITION_INVISIBLE_STEALTH, MODELCONDITION_INVISIBLE_CAMOUFLAGE);
		ModelConditionFlags appearance;
		if (hidden)
			appearance.set(camouflage ?
				MODELCONDITION_INVISIBLE_CAMOUFLAGE : MODELCONDITION_INVISIBLE_STEALTH);
		clearAndSetModelConditionFlags(mask, appearance);

		if (getDrawable())
		{
			const Player* viewer = rts::getObservedOrLocalPlayer();
			const Bool friendly = viewer && (!viewer->isPlayerActive() || viewer->getRelationship(getTeam()) == ALLIES);
			const Bool exposed = testStatus(OBJECT_STATUS_DETECTED);
			const auto look = stealth && viewer ? stealth->calcStealthedStatusForPlayer(this, viewer) :
				!hidden ? STEALTHLOOK_NONE : friendly ?
				(exposed ? STEALTHLOOK_VISIBLE_FRIENDLY_DETECTED : STEALTHLOOK_VISIBLE_FRIENDLY) :
				(exposed ? STEALTHLOOK_VISIBLE_DETECTED : STEALTHLOOK_INVISIBLE);
			getDrawable()->setStealthLook(look);
			if ((!stealth || !stealth->isDisguised()) &&
				(look == STEALTHLOOK_VISIBLE_FRIENDLY || look == STEALTHLOOK_VISIBLE_FRIENDLY_DETECTED))
			{
				const Real opacity = stealth ? stealth->getFriendlyOpacity() : TheGlobalData->m_stealthFriendlyOpacity;
				getDrawable()->setEffectiveOpacity(0.0f, opacity);
			}
			else if (look == STEALTHLOOK_NONE && (!stealth || !stealth->isDisguised()))
				getDrawable()->setEffectiveOpacity(1.0f);
		}
	}
	if ((hasHint || state.hinting) && !hidden && !stealth && getDrawable() && isLocallyControlled())
	{
		getDrawable()->setSecondMaterialPassOpacity(hint ? 1.0f : 0.0f);
		state.hinting = hint;
	}
#else
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
#endif
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
	if (now >= state.invisibilityEnd && state.conditionEnd == 0 && state.passenger == INVALID_ID &&
		state.invisibilitySources.empty() && state.reinvisibilityFrame == 0)
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

#if !RTS_GENERALS
void Object::xferRetailInvisibilitySources(Xfer* xfer)
{
	if (!m_rotwkPowerState)
		return;
	auto& state = *m_rotwkPowerState;
	xfer->xferUnsignedInt(&state.detectionEnd);
	UnsignedInt count = state.invisibilitySources.size();
	xfer->xferUnsignedInt(&count);
	if (xfer->getXferMode() == XFER_LOAD)
		state.invisibilitySources.resize(count);
	for (auto& entry : state.invisibilitySources)
	{
		xfer->xferObjectID(&entry.object);
		xfer->xferAsciiString(&entry.module);
		xfer->xferUnsignedInt(&entry.expires);
	}
}
#endif

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
