/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// FILE: FlammableUpdate.cpp /////////////////////////////////////////////////////////////////////////
// Author: Graham Smallwood, April 2002
// Desc:   Update that manages Aflame and Burned statuses and their effects
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/AudioEventRTS.h"
#include "Common/GameAudio.h"
#include "Common/INIAudit.h"
#include "Common/RetailFileLayers.h"
#include "Common/RetailSaveBlocks.h"
#include "Common/RotwkDuration.h"
#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"

#include "GameClient/ClientRandomValue.h"
#include "GameClient/Drawable.h"
#include "GameClient/FXList.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/RetailTestCommands.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/FlammableUpdate.h"
#include "GameLogic/Module/FireSpreadUpdate.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <limits>

namespace
{
constexpr UnsignedInt retailTick = RotwkDuration::engineFrames(1, LOGICFRAMES_PER_SECOND);
constexpr UnsignedInt waterQuenchDelay = 3 * LOGICFRAMES_PER_SECOND;

void parseFireFX(INI* ini, void*, void* store, const void*)
{
	FlammableUpdateModuleData::FireFX entry;
	entry.effect = ini->getNextSubToken("FX");
	if (const char* key = ini->getNextTokenOrNull(INI::getSepsColon()))
	{
		if (stricmp(key, "BONE") != 0)
			throw INI_INVALID_DATA;
		entry.bone = ini->getNextToken(INI::getSepsColon());
	}
	static_cast<std::vector<FlammableUpdateModuleData::FireFX>*>(store)->push_back(entry);
}

Bool isInWater(const Object* object)
{
	const Coord3D* pos = object->getPosition();
	return !object->isKindOf(KINDOF_SHIP) && !object->isAirborneTarget() &&
		TheTerrainLogic->isUnderwater(pos->x, pos->y);
}
}

//-------------------------------------------------------------------------------------------------
FlammableUpdateModuleData::FlammableUpdateModuleData()
{
	m_burnedDelay = 0;
	m_aflameDuration = 0;
	m_aflameDamageDelay = 0;
	m_aflameDamageAmount = 0;
	m_burningSoundName.clear();
	m_flameDamageLimitData = 20.0f;
	m_flameDamageExpirationDelay = LOGICFRAMES_PER_SECOND * (RetailFileLayers::enabled() ? 5 : 2);
}

//-------------------------------------------------------------------------------------------------
/*static*/ void FlammableUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
  UpdateModuleData::buildFieldParse(p);

	static const FieldParse dataFieldParse[] =
	{
		{ "BurnedDelay",						INI::parseDurationUnsignedInt,	nullptr, offsetof( FlammableUpdateModuleData, m_burnedDelay ) },
		{ "AflameDuration",					INI::parseDurationUnsignedInt,	nullptr, offsetof( FlammableUpdateModuleData, m_aflameDuration ) },
		{ "AflameDamageDelay",			INI::parseDurationUnsignedInt,	nullptr, offsetof( FlammableUpdateModuleData, m_aflameDamageDelay ) },
		{ "AflameDamageAmount",			INI::parseInt,									nullptr, offsetof( FlammableUpdateModuleData, m_aflameDamageAmount ) },
		{ "BurningSoundName",				INI::parseAsciiString,					nullptr,	offsetof( FlammableUpdateModuleData, m_burningSoundName) },
		{ "FlameDamageLimit",				INI::parseReal,									nullptr,	offsetof( FlammableUpdateModuleData, m_flameDamageLimitData ) },
		{ "FlameDamageExpiration",	INI::parseDurationUnsignedInt,	nullptr,	offsetof( FlammableUpdateModuleData, m_flameDamageExpirationDelay ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
  p.add(dataFieldParse);
	if (RetailFileLayers::enabled() || INIAudit::isEnabled())
	{
		static const FieldParse retailFields[] =
		{
			{ "FireFXList", parseFireFX, nullptr, offsetof(FlammableUpdateModuleData, m_fireFX) },
			{ "CustomAnimAndDuration", SpecialPowerAnimation::parse, nullptr, offsetof(FlammableUpdateModuleData, m_burnAnimation) },
			{ "DamageType", DamageTypeFlags::parseSingleBitFromINI, nullptr, offsetof(FlammableUpdateModuleData, m_ignitionDamage) },
			{ "RunToWaterDepth", INI::parseReal, nullptr, offsetof(FlammableUpdateModuleData, m_waterDepth) },
			{ "RunToWaterSearchRadius", INI::parseInt, nullptr, offsetof(FlammableUpdateModuleData, m_waterSearchRadius) },
			{ "RunToWaterSearchIncrement", INI::parseInt, nullptr, offsetof(FlammableUpdateModuleData, m_waterSearchIncrement) },
			{ "SetBurnedStatus", INI::parseBool, nullptr, offsetof(FlammableUpdateModuleData, m_setBurnedStatus) },
			{ "SwapModelWhenAflame", INI::parseBool, nullptr, offsetof(FlammableUpdateModuleData, m_swapModelWhenAflame) },
			{ "SwapModelWhenQuenched", INI::parseBool, nullptr, offsetof(FlammableUpdateModuleData, m_swapModelWhenQuenched) },
			{ "SwapTextureWhenAflame", INI::parseBool, nullptr, offsetof(FlammableUpdateModuleData, m_swapTextureWhenAflame) },
			{ "SwapTextureWhenQuenched", INI::parseBool, nullptr, offsetof(FlammableUpdateModuleData, m_swapTextureWhenQuenched) },
			{ "BurnContained", INI::parseBool, nullptr, offsetof(FlammableUpdateModuleData, m_burnContained) },
			{ "RunToWater", INI::parseBool, nullptr, offsetof(FlammableUpdateModuleData, m_runToWater) },
			{ "PanicLocomotorWhileAflame", INI::parseBool, nullptr, offsetof(FlammableUpdateModuleData, m_panicLocomotor) },
			{ nullptr, nullptr, nullptr, 0 }
		};
		p.add(retailFields);
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
FlammableUpdate::FlammableUpdate( Thing *thing, const ModuleData* moduleData ) : UpdateModule( thing, moduleData )
{
	m_status = FS_NORMAL;
	m_aflameEndFrame = 0;
	m_burnedEndFrame = 0;
	m_damageEndFrame = 0;
	m_audioHandle = 0;
	m_flameDamageLimit = getFlammableUpdateModuleData()->m_flameDamageLimitData;
	m_flameSource = INVALID_ID;
	m_lastFlameDamageDealt = 0;

	setWakeFrame(getObject(), UPDATE_SLEEP_FOREVER);
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
FlammableUpdate::~FlammableUpdate()
{
	stopBurningSound();
}

//-------------------------------------------------------------------------------------------------
/** Damage has been dealt, this is an opportunity to reach to that damage */
//-------------------------------------------------------------------------------------------------
void FlammableUpdate::onDamage( DamageInfo *damageInfo )
{
	if (RetailFileLayers::enabled())
	{
		onRetailDamage(damageInfo);
		return;
	}

	if( damageInfo->in.m_damageType == DAMAGE_FLAME || damageInfo->in.m_damageType == DAMAGE_PARTICLE_BEAM )
	{
		UnsignedInt now = TheGameLogic->getFrame();
		if( now - getFlammableUpdateModuleData()->m_flameDamageExpirationDelay > m_lastFlameDamageDealt )
		{
			// If it has been a long time since our last flame damage, reset the threshold
			m_flameDamageLimit = getFlammableUpdateModuleData()->m_flameDamageLimitData;
		}
		m_lastFlameDamageDealt = now;
#if RETAIL_COMPATIBLE_CRC || PRESERVE_NO_XP_FROM_FLAME_KILLS
		m_flameSource = getObject()->getID();
#else
		// TheSuperHackers @bugfix Stubbjax 03/09/2025 Allow flame damage to award xp to the flame source.
		m_flameSource = damageInfo->in.m_sourceID;
#endif

		Object *me = getObject();
		if( !me->getStatusBits().test( OBJECT_STATUS_AFLAME ) && !me->getStatusBits().test( OBJECT_STATUS_BURNED ) )
		{
			// If I'm not on fire, and I haven't burned up, see if I should try to catch fire.
			m_flameDamageLimit -= damageInfo->out.m_actualDamageDealt;
			if( m_flameDamageLimit <= 0 )
			{
				tryToIgnite();
			}
		}
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
UpdateSleepTime FlammableUpdate::update()
{
	if (RetailFileLayers::enabled())
		return updateRetailBurn();

	Object *me = getObject();
	DEBUG_ASSERTCRASH(m_status == FS_AFLAME, ("hmm, should be aflame"));

	UnsignedInt now = TheGameLogic->getFrame();
	const FlammableUpdateModuleData *data = getFlammableUpdateModuleData();

	if( m_damageEndFrame != 0 && now >= m_damageEndFrame )
	{
		m_damageEndFrame = now + data->m_aflameDamageDelay;
		doAflameDamage();
	}

	if( m_burnedEndFrame != 0 && now >= m_burnedEndFrame )
	{
		// So this status is set, but I am still aflame on an independent timer.
		me->setStatus( MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_BURNED ) );
		me->setModelConditionState( MODELCONDITION_SMOLDERING );
	}

	if( m_aflameEndFrame != 0 && now >= m_aflameEndFrame )
	{
		// This is the important one.  I am no longer on fire.
		if( me->getStatusBits().test( OBJECT_STATUS_BURNED ) )
		{
			// If I am burned, then I will never catch fire again.
			m_status = FS_BURNED;
		}
		else
		{
			// otherwise I am free to burn again
			m_status = FS_NORMAL;
		}
		stopBurningSound();
		me->clearStatus( MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_AFLAME ) );
		me->getBodyModule()->setAflame( FALSE );
		me->clearModelConditionState( MODELCONDITION_AFLAME );
	}

	return calcSleepTime();
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
UpdateSleepTime FlammableUpdate::calcSleepTime()
{
	UnsignedInt now = TheGameLogic->getFrame();
	if (m_status == FS_AFLAME && m_aflameEndFrame != 0 && m_aflameEndFrame > now)
	{
		UnsignedInt soonest = m_aflameEndFrame;
		if (m_burnedEndFrame != 0 && m_burnedEndFrame < soonest && m_burnedEndFrame > now) soonest = m_burnedEndFrame;
		if (m_damageEndFrame != 0 && m_damageEndFrame < soonest && m_damageEndFrame > now) soonest = m_damageEndFrame;
		DEBUG_ASSERTCRASH(soonest - now > 0, ("hmm"));
		// UPDATE_SLEEP requires a count-of-frames, not an absolute-frame, so subtract 'now'
		return UPDATE_SLEEP(soonest - now);
	}
	else
	{
		return UPDATE_SLEEP_FOREVER;
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void FlammableUpdate::tryToIgnite()
{
	const Bool retail = RetailFileLayers::enabled();
	if (retail ? wouldIgnite() : m_status == FS_NORMAL)
	{
		Object *me = getObject();
		me->setStatus( MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_AFLAME ) );
		me->getBodyModule()->setAflame( TRUE );
		me->setModelConditionState( MODELCONDITION_AFLAME );
		startBurningSound();

		// bleah. this sucks. (srj)
		static const NameKeyType key_FireSpreadUpdate = NAMEKEY("FireSpreadUpdate");
		FireSpreadUpdate* fu = (FireSpreadUpdate*)getObject()->findUpdateModule(key_FireSpreadUpdate);
		if (fu != nullptr)
		{
			fu->startFireSpreading();
		}

		m_status = FS_AFLAME;

		const FlammableUpdateModuleData *data = getFlammableUpdateModuleData();
		UnsignedInt now = TheGameLogic->getFrame();
		m_aflameEndFrame = retail && !data->m_aflameDuration ? UPDATE_SLEEP_FOREVER : now + data->m_aflameDuration;
		m_burnedEndFrame = data->m_burnedDelay ? now + data->m_burnedDelay : 0;
		m_damageEndFrame = data->m_aflameDamageDelay ? now + data->m_aflameDamageDelay : 0;

		if (retail)
		{
			startRetailBurn();
			setWakeFrame(me, UPDATE_SLEEP(retailTick));
		}
		else
			setWakeFrame(me, calcSleepTime());
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void FlammableUpdate::doAflameDamage()
{
	const FlammableUpdateModuleData *data = getFlammableUpdateModuleData();

	DamageInfo info;
	info.in.m_amount = data->m_aflameDamageAmount;
	info.in.m_sourceID = m_flameSource;
	info.in.m_damageType = DAMAGE_FLAME;
	info.in.m_deathType = DEATH_BURNED;

	getObject()->attemptDamage( &info );
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void FlammableUpdate::startBurningSound()
{
	const FlammableUpdateModuleData *data = getFlammableUpdateModuleData();

	AudioEventRTS audio(data->m_burningSoundName, getObject()->getID());
	m_audioHandle = TheAudio->addAudioEvent( &audio );
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void FlammableUpdate::stopBurningSound()
{
	if (m_audioHandle)
	{
		TheAudio->removeAudioEvent( m_audioHandle );
		m_audioHandle = 0;
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
Bool FlammableUpdate::wouldIgnite()
{
	if (RetailFileLayers::enabled())
	{
		const Object* me = getObject();
		return m_status == FS_NORMAL && !me->isEffectivelyDead() &&
			!me->testStatus(OBJECT_STATUS_BURNED) && !isInWater(me);
	}

	if( m_status == FS_NORMAL )
		return TRUE;

	return FALSE;
}

DisabledMaskType FlammableUpdate::getDisabledTypesToProcess() const
{
	return RetailFileLayers::enabled() ? MAKE_DISABLED_MASK(DISABLED_PARALYZED) : DISABLEDMASK_NONE;
}

void FlammableUpdate::onRetailDamage(DamageInfo* damageInfo)
{
	const auto* data = getFlammableUpdateModuleData();
	const Real applied = damageInfo->out.m_actualDamageDealt;
	if (applied <= 0.0f)
		return;
	if (damageInfo->in.m_sourceID != getObject()->getID())
		m_flameSource = damageInfo->in.m_sourceID;
	if ((data->m_ignitionDamage != DAMAGE_FORCE && damageInfo->in.m_damageType != data->m_ignitionDamage) ||
		!wouldIgnite())
		return;

	const UnsignedInt now = TheGameLogic->getFrame();
	if (now - m_lastFlameDamageDealt > data->m_flameDamageExpirationDelay)
		m_flameDamageLimit = data->m_flameDamageLimitData;
	m_lastFlameDamageDealt = now;
	m_flameDamageLimit -= applied;
	if (m_flameDamageLimit <= 0.0f)
	{
		m_flameDamageLimit = 0.0f;
		tryToIgnite();
		if (m_status == FS_AFLAME && data->m_burnContained)
			burnPassengers();
	}
}

void FlammableUpdate::onHealing(DamageInfo* damageInfo)
{
	if (!RetailFileLayers::enabled() || m_status != FS_AFLAME)
		return;
	const Real threshold = getFlammableUpdateModuleData()->m_flameDamageLimitData;
	if (m_flameDamageLimit >= threshold)
		return;
	m_flameDamageLimit = std::min(threshold, m_flameDamageLimit + damageInfo->in.m_amount);
	if (m_flameDamageLimit >= threshold)
		finishRetailBurn();
}

void FlammableUpdate::startRetailBurn()
{
	Object* me = getObject();
	const auto* data = getFlammableUpdateModuleData();
	if (data->m_swapModelWhenAflame)
		me->setModelConditionState(MODELCONDITION_BURNT_MODEL);
	if (data->m_swapTextureWhenAflame)
		me->setModelConditionState(MODELCONDITION_BURNT_TEXTURE);

	for (const auto& entry : data->m_fireFX)
	{
		const FXList* effect = TheFXListStore->findFXList(entry.effect.str());
		if (!effect)
			continue;
		if (RetailTestCommands::tracesCombat())
			std::fprintf(stderr, "TEST flammable fx frame=%u source=%s effect=%s bone=%s\n",
				TheGameLogic->getFrame(), me->getTemplate()->getName().str(), entry.effect.str(), entry.bone.str());

		const ScopedVisualRandom random(TRUE);
		Matrix3D transform;
		const Drawable* drawable = me->getDrawable();
		if (drawable && entry.bone.isNotEmpty() &&
			drawable->getCurrentWorldspaceClientBonePositions(entry.bone.str(), transform))
		{
			const Vector3 translation = transform.Get_Translation();
			const Coord3D origin{translation.X, translation.Y, translation.Z};
			FXList::doFXPos(effect, &origin, &transform);
		}
		else
			FXList::doFXObj(effect, me);
	}

	const UnsignedInt now = TheGameLogic->getFrame();
	const auto& animation = data->m_burnAnimation;
	const UnsignedInt duration = RotwkDuration::engineFrames(
		RotwkDuration::retailFrames(animation.m_duration), LOGICFRAMES_PER_SECOND);
	m_burnAnimationEndFrame = now + duration;
	if (animation.m_condition >= 0 && duration > 0)
	{
		me->setTimedModelCondition(static_cast<ModelConditionFlagType>(animation.m_condition), duration);
		if (!me->isDisabledByType(DISABLED_PARALYZED))
			me->setDisabledUntil(DISABLED_PARALYZED, m_burnAnimationEndFrame - 1);
	}
}

void FlammableUpdate::burnPassengers()
{
	Object* me = getObject();
	const auto* contain = me->getContain();
	const auto* contained = contain ? contain->getContainedItemsList() : nullptr;
	if (!contained)
		return;

	std::vector<ObjectID> passengers;
	for (const Object* passenger : *contained)
		passengers.push_back(passenger->getID());
	for (ObjectID id : passengers)
	{
		Object* passenger = TheGameLogic->findObjectByID(id);
		if (!passenger || passenger->getContainedBy() != me || passenger->isEffectivelyDead())
			continue;
		DamageInfo damage;
		damage.in.m_amount = static_cast<Real>(std::max(1, getFlammableUpdateModuleData()->m_aflameDamageAmount / 2));
		damage.in.m_sourceID = me->getID();
		damage.in.m_damageType = DAMAGE_FLAME;
		damage.in.m_deathType = DEATH_BURNED;
		passenger->attemptDamage(&damage);
	}
}

void FlammableUpdate::runToWater()
{
	Object* me = getObject();
	AIUpdateInterface* ai = me->getAI();
	const auto* data = getFlammableUpdateModuleData();
	const Int increment = data->m_waterSearchIncrement;
	const Int radius = data->m_waterSearchRadius;
	if (!ai || increment <= 0 || radius <= 0 || isInWater(me))
		return;

	const Coord3D position = *me->getPosition();
	Region3D extent;
	TheTerrainLogic->getExtent(&extent);
	const auto firstSample = [radius, increment](Real center, Real lowerBound) {
		const std::int64_t origin = static_cast<std::int64_t>(center - static_cast<Real>(radius));
		const std::int64_t shift = static_cast<std::int64_t>(WWMath::Ceil(lowerBound)) - origin;
		return origin + (shift > 0 ? ((shift + increment - 1) / increment) * increment : 0);
	};
	const auto lastSample = [radius](Real center, Real upperBound) {
		return std::min(center + static_cast<Real>(radius), upperBound);
	};
	const auto endX = lastSample(position.x, extent.hi.x);
	const auto endY = lastSample(position.y, extent.hi.y);
	const auto startY = firstSample(position.y, extent.lo.y);
	Coord3D best{};
	Real bestDistance = std::numeric_limits<Real>::max();
	for (auto x = firstSample(position.x, extent.lo.x); x <= endX; x += increment)
		for (auto y = startY; y <= endY; y += increment)
		{
			Coord3D candidate{static_cast<Real>(x), static_cast<Real>(y), position.z};
			const Real dx = candidate.x - position.x;
			const Real dy = candidate.y - position.y;
			const Real distance = dx * dx + dy * dy;
			if (distance >= bestDistance)
				continue;
			Real water, ground;
			if (!TheTerrainLogic->isUnderwater(candidate.x, candidate.y, &water, &ground) ||
				water - ground <= data->m_waterDepth)
				continue;
			candidate.z = ground;
			if (!TheAI->pathfinder()->clientSafeQuickDoesPathExist(ai->getLocomotorSet(), &position, &candidate))
				continue;
			best = candidate;
			bestDistance = distance;
		}
	if (bestDistance == std::numeric_limits<Real>::max())
		return;

	const LocomotorSetType previous = ai->getCurLocomotorSetType();
	if (data->m_panicLocomotor && previous != LOCOMOTORSET_PANIC && ai->chooseLocomotorSet(LOCOMOTORSET_PANIC))
		m_previousLocomotorSet = previous;
	m_runningToWater = TRUE;
	ai->aiMoveToPositionEvenIfSleeping(&best, CMD_FROM_SCRIPT);
	if (RetailTestCommands::tracesCombat())
		std::fprintf(stderr, "TEST flammable water run frame=%u source=%s to=%.1f,%.1f\n",
			TheGameLogic->getFrame(), me->getTemplate()->getName().str(), best.x, best.y);
}

void FlammableUpdate::markBurned()
{
	if (getFlammableUpdateModuleData()->m_setBurnedStatus)
	{
		getObject()->setStatus(MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_BURNED));
		getObject()->setModelConditionState(MODELCONDITION_SMOLDERING);
	}
}

void FlammableUpdate::finishRetailBurn()
{
	Object* me = getObject();
	const auto* data = getFlammableUpdateModuleData();
	markBurned();
	m_status = me->testStatus(OBJECT_STATUS_BURNED) ? FS_BURNED : FS_NORMAL;
	m_aflameEndFrame = 0;
	m_damageEndFrame = 0;
	m_burnedEndFrame = 0;
	m_burnAnimationEndFrame = 0;
	m_flameDamageLimit = data->m_flameDamageLimitData;
	stopBurningSound();
	me->clearStatus(MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_AFLAME));
	me->clearModelConditionState(MODELCONDITION_AFLAME);
	if (data->m_swapModelWhenQuenched)
		me->clearModelConditionState(MODELCONDITION_BURNT_MODEL);
	if (data->m_swapTextureWhenQuenched)
		me->clearModelConditionState(MODELCONDITION_BURNT_TEXTURE);
	me->getBodyModule()->setAflame(FALSE);

	if (AIUpdateInterface* ai = me->getAI(); ai && !me->isEffectivelyDead())
	{
		if (m_runningToWater)
			ai->aiIdle(CMD_FROM_SCRIPT);
		if (m_previousLocomotorSet != LOCOMOTORSET_INVALID && ai->getCurLocomotorSetType() == LOCOMOTORSET_PANIC)
		{
			const auto previous = static_cast<LocomotorSetType>(m_previousLocomotorSet);
			if (previous == LOCOMOTORSET_NORMAL_UPGRADED || !ai->chooseLocomotorSet(previous))
				ai->chooseLocomotorSet(LOCOMOTORSET_NORMAL);
		}
	}
	m_previousLocomotorSet = LOCOMOTORSET_INVALID;
	m_runningToWater = FALSE;
}

UpdateSleepTime FlammableUpdate::updateRetailBurn()
{
	Object* me = getObject();
	if (m_status != FS_AFLAME || me->isEffectivelyDead())
		return UPDATE_SLEEP_FOREVER;

	const auto* data = getFlammableUpdateModuleData();
	const UnsignedInt now = TheGameLogic->getFrame();
	if (m_burnAnimationEndFrame)
	{
		if (now < m_burnAnimationEndFrame)
			return UPDATE_SLEEP(retailTick);
		m_burnAnimationEndFrame = 0;
		if (data->m_runToWater)
			runToWater();
	}

	AIUpdateInterface* ai = me->getAI();
	const Bool checksWater = ai && !me->isKindOf(KINDOF_SHIP);
	if (checksWater && isInWater(me))
	{
		m_aflameEndFrame = std::min(m_aflameEndFrame, now + waterQuenchDelay);
		if (m_runningToWater)
		{
			ai->aiIdle(CMD_FROM_SCRIPT);
			m_runningToWater = FALSE;
		}
	}

	if (m_damageEndFrame && now >= m_damageEndFrame)
	{
		m_damageEndFrame = now + data->m_aflameDamageDelay;
		doAflameDamage();
		if (m_status != FS_AFLAME || me->isEffectivelyDead())
			return UPDATE_SLEEP_FOREVER;
	}
	if (m_burnedEndFrame && now >= m_burnedEndFrame)
	{
		m_burnedEndFrame = 0;
		markBurned();
	}
	if (now >= m_aflameEndFrame)
	{
		finishRetailBurn();
		return UPDATE_SLEEP_FOREVER;
	}
	if (checksWater)
		return UPDATE_SLEEP(retailTick);
	return frameToSleepTime(m_aflameEndFrame, m_damageEndFrame ? m_damageEndFrame : FOREVER,
		m_burnedEndFrame ? m_burnedEndFrame : FOREVER);
}

void FlammableUpdate::xferRetailBlocks(Xfer* xfer, Bool present)
{
	RetailSaveBlocks::transferSection(xfer, xfer->getXferMode() == XFER_LOAD, present, {
		{"burn-animation-and-water", [this](Xfer* block) {
			block->xferUnsignedInt(&m_burnAnimationEndFrame);
			block->xferInt(&m_previousLocomotorSet);
			block->xferBool(&m_runningToWater);
		}, [this] {
			m_burnAnimationEndFrame = 0;
			m_previousLocomotorSet = LOCOMOTORSET_INVALID;
			m_runningToWater = FALSE;
		}}
	});
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void FlammableUpdate::crc( Xfer *xfer )
{

	// extend base class
	UpdateModule::crc( xfer );

	if (RetailFileLayers::enabled())
	{
		xfer->xferUser(&m_status, sizeof(m_status));
		xfer->xferUnsignedInt(&m_aflameEndFrame);
		xfer->xferUnsignedInt(&m_burnedEndFrame);
		xfer->xferUnsignedInt(&m_damageEndFrame);
		xfer->xferReal(&m_flameDamageLimit);
		xfer->xferUnsignedInt(&m_lastFlameDamageDealt);
		xfer->xferObjectID(&m_flameSource);
		xferRetailBlocks(xfer, TRUE);
	}
}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version
	* 2. TheSuperHackers @tweak Serialize flame source
	*/
// ------------------------------------------------------------------------------------------------
void FlammableUpdate::xfer( Xfer *xfer )
{

	// version
#if RETAIL_COMPATIBLE_XFER_SAVE
	constexpr XferVersion classicVersion = 1;
#else
	constexpr XferVersion classicVersion = 2;
#endif
	constexpr XferVersion retailVersion = 3;
	const Bool retail = RetailFileLayers::enabled();
	const XferVersion currentVersion = retail ? retailVersion : classicVersion;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	UpdateModule::xfer( xfer );

	// flammability status
	xfer->xferUser( &m_status, sizeof( FlammabilityStatusType ) );

	// aflame end frame
	xfer->xferUnsignedInt( &m_aflameEndFrame );

	// burned end frame
	xfer->xferUnsignedInt( &m_burnedEndFrame );

	// damage end frame
	xfer->xferUnsignedInt( &m_damageEndFrame );

	// flame damage limit
	xfer->xferReal( &m_flameDamageLimit );

	// last flame damage dealt
	xfer->xferUnsignedInt( &m_lastFlameDamageDealt );

	if (version >= 2)
	{
		xfer->xferObjectID(&m_flameSource);
	}

	if (retail)
		xferRetailBlocks(xfer, version >= retailVersion);
}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void FlammableUpdate::loadPostProcess()
{

	// extend base class
	UpdateModule::loadPostProcess();

	if (RetailFileLayers::enabled() && m_status == FS_AFLAME && !getObject()->isEffectivelyDead())
		startBurningSound();
}
