#include "PreRTS.h"

#include "GameLogic/Module/BezierProjectileBehavior.h"

#include <algorithm>

#include "Common/BezierSegment.h"
#include "Common/INI.h"
#include "Common/RetailFileLayers.h"
#include "Common/RetailSaveBlocks.h"
#include "Common/Xfer.h"
#include "GameClient/Drawable.h"
#include "GameClient/FXList.h"
#include "GameLogic/AI.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/RotwkModifierModules.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Weapon.h"
#include "WWMath/wwmath.h"

void BezierProjectileBehaviorModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	DumbProjectileBehaviorModuleData::buildFieldParse(p);

	static const FieldParse dataFieldParse[] =
	{
		{ "GroundHitFX", INI::parseFXList, nullptr, offsetof(BezierProjectileBehaviorModuleData, m_groundHitFX) },
		{ "GroundBounceFX", INI::parseFXList, nullptr, offsetof(BezierProjectileBehaviorModuleData, m_groundBounceFX) },
		{ "GroundHitWeapon", INI::parseWeaponTemplate, nullptr, offsetof(BezierProjectileBehaviorModuleData, m_groundHitWeapon) },
		{ "GroundBounceWeapon", INI::parseWeaponTemplate, nullptr, offsetof(BezierProjectileBehaviorModuleData, m_groundBounceWeapon) },
		{ "BounceCount", INI::parseInt, nullptr, offsetof(BezierProjectileBehaviorModuleData, m_bounceCount) },
		{ "BounceDistance", INI::parseReal, nullptr, offsetof(BezierProjectileBehaviorModuleData, m_bounceDistance) },
		{ "BounceFirstHeight", INI::parseReal, nullptr, offsetof(BezierProjectileBehaviorModuleData, m_bounceFirstHeight) },
		{ "BounceSecondHeight", INI::parseReal, nullptr, offsetof(BezierProjectileBehaviorModuleData, m_bounceSecondHeight) },
		{ "BounceFirstPercentIndent", INI::parsePercentToReal, nullptr, offsetof(BezierProjectileBehaviorModuleData, m_bounceFirstPercentIndent) },
		{ "BounceSecondPercentIndent", INI::parsePercentToReal, nullptr, offsetof(BezierProjectileBehaviorModuleData, m_bounceSecondPercentIndent) },
		{ "CrushStyle", INI::parseBool, nullptr, offsetof(BezierProjectileBehaviorModuleData, m_crushStyle) },
		{ "DieOnImpact", INI::parseBool, nullptr, offsetof(BezierProjectileBehaviorModuleData, m_dieOnImpact) },
		{ "IgnoreTerrainHeight", INI::parseBool, nullptr, offsetof(BezierProjectileBehaviorModuleData, m_ignoreTerrainHeight) },
		{ "CurveFlattenMinDist", INI::parseReal, nullptr, offsetof(BezierProjectileBehaviorModuleData, m_curveFlattenMinDist) },
		{ "PreLandingStateTime", INI::parseDurationUnsignedInt, nullptr, offsetof(BezierProjectileBehaviorModuleData, m_preLandingStateTime) },
		{ "PreLandingEmotion", INI::parseAsciiString, nullptr, offsetof(BezierProjectileBehaviorModuleData, m_preLandingEmotion) },
		{ "PreLandingEmotionRadius", INI::parseReal, nullptr, offsetof(BezierProjectileBehaviorModuleData, m_preLandingEmotionRadius) },
		{ "InvisibleFrames", INI::parseInt, nullptr, offsetof(BezierProjectileBehaviorModuleData, m_invisibleFrames) },
		{ "FinalStuckTime", INI::parseDurationUnsignedInt, nullptr, offsetof(BezierProjectileBehaviorModuleData, m_finalStuckTime) },
		{ "FadeInTime", INI::parseEngineDurationUnsignedInt, nullptr, offsetof(BezierProjectileBehaviorModuleData, m_fadeInTime) },
		{ "FirstPercentHeight", INI::parsePercentToReal, nullptr, offsetof(BezierProjectileBehaviorModuleData, m_firstPercentHeight) },
		{ "SecondPercentHeight", INI::parsePercentToReal, nullptr, offsetof(BezierProjectileBehaviorModuleData, m_secondPercentHeight) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(dataFieldParse);
}

BezierProjectileBehavior::~BezierProjectileBehavior() = default;

void BezierProjectileBehavior::xferRetailBlocks(Xfer* xfer, Bool present)
{
	RetailSaveBlocks::transferSection<Xfer>(xfer, xfer->getXferMode() == XFER_LOAD, present, {
		{"bounce-index", [this](Xfer* block) { block->xferInt(&m_bounceIndex); }, [this] { m_bounceIndex = 0; }},
		{"curve-flatten-scale", [this](Xfer* block) { block->xferReal(&m_curveFlattenScale); }, [this] { m_curveFlattenScale = 1.0f; }},
		{"launcher-pos", [this](Xfer* block) { block->xferCoord3D(&m_launcherPos); }, [this] { m_launcherPos.zero(); }},
		{"crush-hits", [this](Xfer* block) {
			UnsignedInt count = static_cast<UnsignedInt>(m_crushHitIDs.size());
			block->xferUnsignedInt(&count);
			if (block->getXferMode() == XFER_LOAD)
				m_crushHitIDs.resize(count);
			for (ObjectID& id : m_crushHitIDs)
				block->xferObjectID(&id);
		}, [this] { m_crushHitIDs.clear(); }},
	});
}

void BezierProjectileBehavior::crc(Xfer* xfer)
{
	DumbProjectileBehavior::crc(xfer);
	if (RetailFileLayers::enabled())
		xferRetailBlocks(xfer, TRUE);
}

void BezierProjectileBehavior::crcSelfFlight(Xfer* xfer)
{
	DumbProjectileBehavior::crcSelfFlight(xfer);
	if (RetailFileLayers::enabled())
		xferRetailBlocks(xfer, TRUE);
}

void BezierProjectileBehavior::xfer(Xfer* xfer)
{
	XferVersion currentVersion = RetailFileLayers::enabled() ? 2 : 1;
	XferVersion version = currentVersion;
	xfer->xferVersion(&version, currentVersion);
	DumbProjectileBehavior::xfer(xfer);
	if (RetailFileLayers::enabled())
		xferRetailBlocks(xfer, version >= 2);
}

void BezierProjectileBehavior::loadPostProcess()
{
	DumbProjectileBehavior::loadPostProcess();
}

UpdateSleepTime BezierProjectileBehavior::update()
{
	if (!hasLaunched())
		return UPDATE_SLEEP_FOREVER;
	const auto* data = getBezierProjectileBehaviorModuleData();
	if (RetailFileLayers::enabled() && data->m_preLandingEmotion.compareNoCase("DOOM") == 0 &&
		data->m_preLandingEmotionRadius > 0.0f && isApproachingLanding(data->m_preLandingStateTime))
	{
		SimpleObjectIterator* iter = ThePartitionManager->iterateObjectsInRange(&landingPosition(), data->m_preLandingEmotionRadius, FROM_CENTER_2D);
		MemoryPoolObjectHolder holder(iter);
		for (Object* target = iter->first(); target; target = iter->next())
			if (getObject()->getRelationship(target) == ENEMIES)
				EmotionTrackerUpdate::signal(target, EMOTION_DOOM, getObject());
	}
	const UpdateSleepTime sleep = DumbProjectileBehavior::update();
	if ((isSelfFlight() || isPassengerFlight()) && isWithinLandingTime(data->m_preLandingStateTime))
		getObject()->setModelConditionState(MODELCONDITION_ABOUT_TO_HIT);
	return sleep;
}

void BezierProjectileBehavior::projectileLaunchAtObjectOrPosition(const Object* victim, const Coord3D* victimPos,
	const Object* launcher, WeaponSlotType wslot, Int specificBarrelToUse, const WeaponTemplate* detWeap,
	const ParticleSystemTemplate* exhaustSysOverride)
{
	m_launcherPos = launcher ? *launcher->getPosition() : *getObject()->getPosition();
	m_bounceIndex = 0;
	m_crushHitIDs.clear();
	DumbProjectileBehavior::projectileLaunchAtObjectOrPosition(victim, victimPos, launcher, wslot, specificBarrelToUse,
		detWeap, exhaustSysOverride);
	if (isSelfFlight() || isPassengerFlight())
		getObject()->setStatus(MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_NO_ATTACK));
	setWakeFrame(getObject(), UPDATE_SLEEP_NONE);
}

void BezierProjectileBehavior::projectileFireAtObjectOrPosition(const Object* victim, const Coord3D* victimPos,
	const WeaponTemplate* detWeap, const ParticleSystemTemplate* exhaustSysOverride)
{
	DumbProjectileBehavior::projectileFireAtObjectOrPosition(victim, victimPos, detWeap, exhaustSysOverride);
	if (m_bounceIndex == 0 && hasLaunched())
	{
		const auto* data = getBezierProjectileBehaviorModuleData();
		if (data->m_invisibleFrames > 0 || data->m_fadeInTime > 0)
		{
			if (Drawable* draw = getObject()->getDrawable())
			{
				const UnsignedInt invisibleFrames = static_cast<UnsignedInt>(std::max(0, data->m_invisibleFrames) * 0.03f);
				draw->startRetailProjectileFade(invisibleFrames, data->m_fadeInTime);
			}
		}
	}
}

Bool BezierProjectileBehavior::calcFlightPath(Bool recalcNumSegments)
{
	const auto* data = getBezierProjectileBehaviorModuleData();
	const Real firstIndent = m_bounceIndex == 0 ? data->m_firstPercentIndent : data->m_bounceFirstPercentIndent;
	const Real secondIndent = m_bounceIndex == 0 ? data->m_secondPercentIndent : data->m_bounceSecondPercentIndent;
	const Real firstHeight = m_bounceIndex == 0 ? data->m_firstHeight : data->m_bounceFirstHeight;
	const Real secondHeight = m_bounceIndex == 0 ? data->m_secondHeight : data->m_bounceSecondHeight;

	m_curveFlattenScale = 1.0f;

	Coord3D controlPoints[4];
	controlPoints[0] = m_flightPathStart;
	controlPoints[3] = m_flightPathEnd;

	const Real dx = controlPoints[3].x - controlPoints[0].x;
	const Real dy = controlPoints[3].y - controlPoints[0].y;
	controlPoints[1].x = controlPoints[0].x + dx * firstIndent;
	controlPoints[1].y = controlPoints[0].y + dy * firstIndent;
	controlPoints[2].x = controlPoints[0].x + dx * secondIndent;
	controlPoints[2].y = controlPoints[0].y + dy * secondIndent;

	if (data->m_ignoreTerrainHeight)
	{
		const Real dz = controlPoints[3].z - controlPoints[0].z;
		controlPoints[1].z = controlPoints[0].z + data->m_firstPercentHeight * dz;
		controlPoints[2].z = controlPoints[0].z + data->m_secondPercentHeight * dz;
	}
	else
	{
		Real highestInterveningTerrain = 0.0f;
		if (!ThePartitionManager->estimateTerrainExtremesAlongLine(controlPoints[0], controlPoints[3], nullptr,
			&highestInterveningTerrain, nullptr, nullptr))
		{
			return FALSE;
		}
		if (data->m_curveFlattenMinDist <= 0.0f)
		{
			const Real baseZ = std::max({highestInterveningTerrain, controlPoints[0].z, controlPoints[3].z});
			controlPoints[1].z = baseZ + firstHeight;
			controlPoints[2].z = baseZ + secondHeight;
		}
		else
		{
			const Coord3D delta{dx, dy, controlPoints[3].z - controlPoints[0].z};
			const Real dist = delta.length();
			const Real halfMin = data->m_curveFlattenMinDist * 0.5f;
			if (dist < halfMin)
				m_curveFlattenScale = 0.0f;
			else if (dist < data->m_curveFlattenMinDist && halfMin > 0.0f)
				m_curveFlattenScale = (dist - halfMin) / halfMin;
			const Real dz = controlPoints[3].z - controlPoints[0].z;
			controlPoints[1].z = std::max(controlPoints[0].z + dz * firstIndent, highestInterveningTerrain) +
				m_curveFlattenScale * firstHeight;
			controlPoints[2].z = std::max(controlPoints[0].z + dz * secondIndent, highestInterveningTerrain) +
				m_curveFlattenScale * secondHeight;
		}
	}

	BezierSegment flightCurve(controlPoints);
	if (recalcNumSegments)
	{
		const Real speed = m_bounceIndex != 0 ? m_flightPathSpeed * 0.5f : m_flightPathSpeed;
		const Real flightDistance = flightCurve.getApproximateLength();
		m_flightPathSegments = static_cast<Int>(WWMath::Ceil(WWMath::Div_FixNaN(flightDistance, speed, 1.0f)));
	}
	if (m_flightPathSegments < 2)
		m_flightPathSegments = 2;

	flightCurve.getSegmentPoints(m_flightPathSegments, &m_flightPath);
	return TRUE;
}

Bool BezierProjectileBehavior::hasCrushedObject(ObjectID id) const
{
	return std::find(m_crushHitIDs.begin(), m_crushHitIDs.end(), id) != m_crushHitIDs.end();
}

void BezierProjectileBehavior::recordCrushedObject(ObjectID id)
{
	if (id != INVALID_ID && !hasCrushedObject(id))
		m_crushHitIDs.push_back(id);
}

void BezierProjectileBehavior::backOffFromCollision(const Object* other)
{
	if (!other || m_flightPath.empty())
		return;
	const UnsignedInt maxIndex = static_cast<UnsignedInt>(m_flightPath.size() - 1);
	const UnsignedInt curStep = std::min(static_cast<UnsignedInt>(std::max(0, m_currentFlightPathStep)), maxIndex);
	const UnsignedInt prevIndex = curStep > 0 ? curStep - 1 : 0;
	const Coord3D& prevPos = m_flightPath[prevIndex];
	Object* obj = getObject();
	Coord3D pos = *obj->getPosition();
	const Coord3D stepBack{
		(prevPos.x - pos.x) * 0.1f,
		(prevPos.y - pos.y) * 0.1f,
		(prevPos.z - pos.z) * 0.1f
	};
	const Real orient = obj->getOrientation();
	for (Int attempt = 0; attempt <= 12; ++attempt)
	{
		pos.x += stepBack.x;
		pos.y += stepBack.y;
		pos.z += stepBack.z;
		if (!ThePartitionManager->geomCollidesWithGeom(&pos, obj->getGeometryInfo(), orient,
			other->getPosition(), other->getGeometryInfo(), other->getOrientation()))
		{
			break;
		}
	}
	pos.x -= stepBack.x;
	pos.y -= stepBack.y;
	pos.z -= stepBack.z;
	obj->setPosition(&pos);
}

Bool BezierProjectileBehavior::projectileHandleCollision(Object* other)
{
	if (!getObject()->isKindOf(KINDOF_PROJECTILE) && (!hasLaunched() || isSelfFlight()))
		return FALSE;

	if (!other)
	{
		detonate(nullptr);
		return TRUE;
	}

	if (!m_detonationWeaponTmpl)
		return TRUE;

	Object* launcher = TheGameLogic->findObjectByID(m_launcherID);
	if (!m_detonationWeaponTmpl->shouldProjectileCollideWith(launcher, getObject(), other, m_victimID))
		return TRUE;

	const auto* data = getBezierProjectileBehaviorModuleData();
	if (!data->m_crushStyle)
		return DumbProjectileBehavior::projectileHandleCollision(other);

	backOffFromCollision(other);
	if (!hasCrushedObject(other->getID()))
	{
		recordCrushedObject(other->getID());
		if (TheWeaponStore)
			TheWeaponStore->handleProjectileDetonation(m_detonationWeaponTmpl, getObject(),
				other->getPosition(), m_extraBonusFlags, TRUE, other->getID());
	}
	return TRUE;
}

void BezierProjectileBehavior::startBounce()
{
	const auto* data = getBezierProjectileBehaviorModuleData();
	Object* obj = getObject();
	if (!data->m_dieOnImpact)
		setWakeFrame(obj, UPDATE_SLEEP_NONE);
	++m_bounceIndex;
	if (m_flightPath.size() < 2)
	{
		finishCrushStyleLanding();
		return;
	}
	const Coord3D& prev = m_flightPath[m_flightPath.size() - 2];
	const Coord3D& last = m_flightPath[m_flightPath.size() - 1];
	Real dx = last.x - prev.x;
	Real dy = last.y - prev.y;
	const Real lenSqr = dx * dx + dy * dy;
	if (lenSqr > 0.0f)
	{
		const Real invLen = 1.0f / WWMath::SqrtOrigin(lenSqr);
		dx *= invLen;
		dy *= invLen;
	}
	Coord3D nextTarget;
	nextTarget.x = obj->getPosition()->x + dx * data->m_bounceDistance;
	nextTarget.y = obj->getPosition()->y + dy * data->m_bounceDistance;
	nextTarget.z = TheTerrainLogic->getGroundHeight(nextTarget.x, nextTarget.y);
	if (m_lifespanFrame != 0)
		m_lifespanFrame = TheGameLogic->getFrame() + data->m_maxLifespan;
	projectileFireAtObjectOrPosition(nullptr, &nextTarget, m_detonationWeaponTmpl, nullptr);
	if (hasLaunched())
		setWakeFrame(obj, UPDATE_SLEEP_NONE);
}

void BezierProjectileBehavior::finishCrushStyleLanding()
{
	const auto* data = getBezierProjectileBehaviorModuleData();
	Object* obj = getObject();
	Coord3D landPos = *obj->getPosition();
	Coord3D probePos = landPos;
	probePos.z += 10.0f;
	const PathfindLayerEnum layer = TheTerrainLogic->getHighestLayerForDestination(&probePos);
	obj->setLayer(layer);
	landPos.z = TheTerrainLogic->getLayerHeight(landPos.x, landPos.y, layer);
	obj->setPosition(&landPos);

	m_bounceIndex = 0;
	m_crushHitIDs.clear();
	m_launcherPos.zero();
	clearFlight();

	if (data->m_dieOnImpact)
	{
		obj->setModelConditionState(MODELCONDITION_SPLATTED);
		obj->kill(DAMAGE_UNRESISTABLE, DEATH_NORMAL);
	}
	obj->clearStatus(MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_NO_ATTACK));
	if (AIUpdateInterface* ai = obj->getAI())
	{
		if (data->m_finalStuckTime == 0)
			ai->aiIdle(CMD_FROM_AI);
		else
			ai->setRetailSpawnBusyTime(data->m_finalStuckTime);
	}
}

void BezierProjectileBehavior::detonate(const Object* hit)
{
	if (m_flightPath.empty())
		return;

	const auto* data = getBezierProjectileBehaviorModuleData();
	Object* obj = getObject();

	if (!hit)
	{
		const FXList* fx = m_bounceIndex == 0 ? data->m_groundHitFX : data->m_groundBounceFX;
		if (fx)
			FXList::doFXPos(fx, obj->getPosition(), obj->getTransformMatrix(), 0.0f, nullptr, 0.0f, obj);

		const WeaponTemplate* groundWeapon = m_bounceIndex == 0 ? data->m_groundHitWeapon : data->m_groundBounceWeapon;
		if (groundWeapon && TheWeaponStore)
			TheWeaponStore->createAndFireTempWeapon(groundWeapon, obj, obj->getPosition(), TRUE);

		if (m_bounceIndex < data->m_bounceCount)
		{
			startBounce();
			return;
		}
		if (data->m_crushStyle)
		{
			finishCrushStyleLanding();
			return;
		}
		DumbProjectileBehavior::detonate(nullptr);
		return;
	}

	backOffFromCollision(hit);
	if (data->m_crushStyle)
	{
		if (!hasCrushedObject(hit->getID()))
		{
			recordCrushedObject(hit->getID());
			if (m_detonationWeaponTmpl && TheWeaponStore)
				TheWeaponStore->handleProjectileDetonation(m_detonationWeaponTmpl, obj,
					hit->getPosition(), m_extraBonusFlags, TRUE, hit->getID());
		}
		return;
	}
	DumbProjectileBehavior::detonate(hit);
}
