#include "PreRTS.h"

#include "GameLogic/Module/RetailSlowDeath.h"

#include <algorithm>
#include <vector>

#include "Common/GameCommon.h"
#include "Common/INI.h"
#include "Common/MapObject.h"
#include "Common/RandomValue.h"
#include "Common/Xfer.h"
#include "GameLogic/Damage.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/TerrainLogic.h"
#include "WWMath/wwmath.h"

namespace
{
Coord3D worldPosition(const Object& object, const Coord3D& offset)
{
	Vector3 world;
	Matrix3D::Transform_Vector(*object.getTransformMatrix(), Vector3(offset.x, offset.y, offset.z), &world);
	return {world.X, world.Y, world.Z};
}

Bool isTerrainClear(const ClearanceTestingSlowDeathBehaviorModuleData& data, const Coord3D& center, Real angle)
{
	if (data.m_maxHeightFraction > 1.0f && data.m_minHeightFraction > 1.0f)
		return true;

	const GeometryInfo& shape = data.m_clearance;
	const Real radius = shape.getBoundingCircleRadius();
	const Int left = static_cast<Int>(WWMath::Floor((center.x - radius) / MAP_XY_FACTOR));
	const Int bottom = static_cast<Int>(WWMath::Floor((center.y - radius) / MAP_XY_FACTOR));
	const Int right = static_cast<Int>(WWMath::Floor((center.x + radius) / MAP_XY_FACTOR));
	const Int top = static_cast<Int>(WWMath::Floor((center.y + radius) / MAP_XY_FACTOR));
	const Real cosine = WWMath::CosTrig(angle);
	const Real sine = WWMath::SinTrig(angle);
	Int sampleCount = 0;
	Int tooHigh = 0;
	Int tooLow = 0;
	for (Int y = bottom; y <= top; ++y)
	{
		for (Int x = left; x <= right; ++x)
		{
			const Real sampleX = (x + 0.5f) * MAP_XY_FACTOR;
			const Real sampleY = (y + 0.5f) * MAP_XY_FACTOR;
			const Real dx = sampleX - center.x;
			const Real dy = sampleY - center.y;
			if (shape.getGeomType() == GEOMETRY_BOX
				? WWMath::Fabs(dx * cosine + dy * sine) > shape.getMajorRadius() ||
					WWMath::Fabs(dy * cosine - dx * sine) > shape.getMinorRadius()
				: dx * dx + dy * dy > radius * radius)
				continue;
			const Real height = TheTerrainLogic->getGroundHeight(sampleX, sampleY) - center.z;
			++sampleCount;
			tooHigh += height > data.m_maxHeight;
			tooLow += height < data.m_minHeight;
		}
	}
	return sampleCount == 0 || (static_cast<Real>(tooHigh) / sampleCount < data.m_maxHeightFraction &&
		static_cast<Real>(tooLow) / sampleCount < data.m_minHeightFraction);
}
}

void ClearanceTestingSlowDeathBehaviorModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	SlowDeathBehaviorModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "ClearanceGeometry", GeometryInfo::parseGeometryType, nullptr, offsetof(ClearanceTestingSlowDeathBehaviorModuleData, m_clearance) },
		{ "ClearanceGeometryMajorRadius", GeometryInfo::parseGeometryMajorRadius, nullptr, offsetof(ClearanceTestingSlowDeathBehaviorModuleData, m_clearance) },
		{ "ClearanceGeometryMinorRadius", GeometryInfo::parseGeometryMinorRadius, nullptr, offsetof(ClearanceTestingSlowDeathBehaviorModuleData, m_clearance) },
		{ "ClearanceGeometryHeight", GeometryInfo::parseGeometryHeight, nullptr, offsetof(ClearanceTestingSlowDeathBehaviorModuleData, m_clearance) },
		{ "ClearanceGeometryIsSmall", GeometryInfo::parseGeometryIsSmall, nullptr, offsetof(ClearanceTestingSlowDeathBehaviorModuleData, m_clearance) },
		{ "ClearanceGeometryOffset", INI::parseCoord3D, nullptr, offsetof(ClearanceTestingSlowDeathBehaviorModuleData, m_offset) },
		{ "ClearanceMaxHeight", INI::parseReal, nullptr, offsetof(ClearanceTestingSlowDeathBehaviorModuleData, m_maxHeight) },
		{ "ClearanceMaxHeightFraction", INI::parseReal, nullptr, offsetof(ClearanceTestingSlowDeathBehaviorModuleData, m_maxHeightFraction) },
		{ "ClearanceMinHeight", INI::parseReal, nullptr, offsetof(ClearanceTestingSlowDeathBehaviorModuleData, m_minHeight) },
		{ "ClearanceMinHeightFraction", INI::parseReal, nullptr, offsetof(ClearanceTestingSlowDeathBehaviorModuleData, m_minHeightFraction) },
		{ "DamageAmountRequired", INI::parseReal, nullptr, offsetof(ClearanceTestingSlowDeathBehaviorModuleData, m_minDamage) },
		{ "MinKillerAngle", INI::parseAngleReal, nullptr, offsetof(ClearanceTestingSlowDeathBehaviorModuleData, m_minKillerAngle) },
		{ "MaxKillerAngle", INI::parseAngleReal, nullptr, offsetof(ClearanceTestingSlowDeathBehaviorModuleData, m_maxKillerAngle) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

ClearanceTestingSlowDeathBehavior::~ClearanceTestingSlowDeathBehavior() = default;

Bool ClearanceTestingSlowDeathBehavior::isDieApplicable(const DamageInfo* damageInfo) const
{
	const auto* data = getClearanceTestingSlowDeathBehaviorModuleData();
	if (!SlowDeathBehavior::isDieApplicable(damageInfo) || damageInfo->in.m_amount < data->m_minDamage)
		return false;

	const Object& object = *getObject();
	const Real heading = object.getOrientation();
	if (data->m_maxKillerAngle > data->m_minKillerAngle)
	{
		const Object* killer = TheGameLogic->findObjectByID(damageInfo->in.m_sourceID);
		if (!killer)
			return false;
		const Coord3D& from = *object.getPosition();
		const Coord3D& to = *killer->getPosition();
		const Real angle = WWMath::Atan2Origin(to.y - from.y, to.x - from.x) - heading;
		const Real middle = (data->m_minKillerAngle + data->m_maxKillerAngle) * 0.5f;
		if (WWMath::Fabs(normalizeAngle(angle - middle)) > data->m_maxKillerAngle - middle)
			return false;
	}

	const Coord3D center = worldPosition(object, data->m_offset);
	SimpleObjectIterator* collisions = ThePartitionManager->iteratePotentialCollisions(&center, data->m_clearance, heading);
	MemoryPoolObjectHolder collisionHolder(collisions);
	for (Object* other = collisions->first(); other; other = collisions->next())
	{
		if (other != &object && !other->isEffectivelyDead() && !other->isKindOf(KINDOF_NO_COLLIDE) &&
			(other->isKindOf(KINDOF_STRUCTURE) || other->isKindOf(KINDOF_IMMOBILE)))
			return false;
	}
	return isTerrainClear(*data, center, heading);
}

void ClearanceTestingSlowDeathBehavior::crc(Xfer* xfer)
{
	SlowDeathBehavior::crc(xfer);
}

void ClearanceTestingSlowDeathBehavior::xfer(Xfer* xfer)
{
	XferVersion version = 1;
	xfer->xferVersion(&version, 1);
	SlowDeathBehavior::xfer(xfer);
}

void ClearanceTestingSlowDeathBehavior::loadPostProcess()
{
	SlowDeathBehavior::loadPostProcess();
}

void GiantBirdSlowDeathBehaviorModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	SlowDeathBehaviorModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "CrashAvoidKindOfs", KindOfMaskType::parseFromINI, nullptr, offsetof(GiantBirdSlowDeathBehaviorModuleData, m_crashAvoidKinds) },
		{ "CrashAvoidRadius", INI::parseReal, nullptr, offsetof(GiantBirdSlowDeathBehaviorModuleData, m_crashAvoidRadius) },
		{ "CrashAvoidStrength", INI::parseReal, nullptr, offsetof(GiantBirdSlowDeathBehaviorModuleData, m_crashAvoidStrength) },
		{ "NeedToMaintainFlailingHeight", INI::parseBool, nullptr, offsetof(GiantBirdSlowDeathBehaviorModuleData, m_holdFlailingHeight) },
		{ "EjectRiderIfApplicable", INI::parseBool, nullptr, offsetof(GiantBirdSlowDeathBehaviorModuleData, m_ejectRider) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

GiantBirdSlowDeathBehavior::~GiantBirdSlowDeathBehavior() = default;

Bool GiantBirdSlowDeathBehavior::controlsMotion(const Object& bird)
{
	for (BehaviorModule** module = bird.getBehaviorModules(); *module; ++module)
		if (const auto* death = dynamic_cast<const GiantBirdSlowDeathBehavior*>(*module))
			if (death->isSlowDeathActivated())
				return true;
	return false;
}

void GiantBirdSlowDeathBehavior::beginSlowDeath(const DamageInfo* damageInfo)
{
	SlowDeathBehavior::beginSlowDeath(damageInfo);
	Object* bird = getObject();
	if (!isSlowDeathActivated() || bird->isDestroyed())
		return;

	const auto* data = getGiantBirdSlowDeathBehaviorModuleData();
	if (data->m_ejectRider && bird->getContain())
		bird->getContain()->removeAllContained();

	const Coord3D& position = *bird->getPosition();
	const Coord3D& heading = *bird->getUnitDirectionVector2D();
	PhysicsBehavior* physics = bird->getPhysics();
	const Real speed = physics ? physics->getForwardSpeed2D() : 0.0f;
	m_crashSite = {position.x + heading.x * speed * 5.0f, position.y + heading.y * speed * 5.0f, position.z};
	const Real radius = data->m_crashAvoidRadius;
	if (radius > 0.0f && data->m_crashAvoidStrength > 0.0f && data->m_crashAvoidKinds.any())
	{
		SimpleObjectIterator* obstacles = ThePartitionManager->iterateObjectsInRange(
			&m_crashSite, radius, FROM_CENTER_2D, nullptr, ITER_FASTEST);
		MemoryPoolObjectHolder obstacleHolder(obstacles);
		std::vector<const Object*> blockers;
		for (Object* obstacle = obstacles->first(); obstacle; obstacle = obstacles->next())
			if (obstacle != bird && !obstacle->isEffectivelyDead() && obstacle->isAnyKindOf(data->m_crashAvoidKinds))
				blockers.push_back(obstacle);
		std::sort(blockers.begin(), blockers.end(), [](const Object* a, const Object* b) { return a->getID() < b->getID(); });
		const Coord3D intended = m_crashSite;
		for (const Object* obstacle : blockers)
		{
			const Real dx = intended.x - obstacle->getPosition()->x;
			const Real dy = intended.y - obstacle->getPosition()->y;
			const Real distance = WWMath::Sqrt(dx * dx + dy * dy);
			const Real push = (radius - distance) * data->m_crashAvoidStrength / std::max(1.0f, distance);
			m_crashSite.x += dx * push;
			m_crashSite.y += dy * push;
		}
	}

	if (physics)
	{
		physics->setStickToGround(false);
		physics->setAllowToFall(true);
		physics->setImmuneToFallingDamage(true);
		physics->setAllowBouncing(false);
	}
	m_crashing = true;
	setWakeFrame(bird, UPDATE_SLEEP_NONE);
}

UpdateSleepTime GiantBirdSlowDeathBehavior::update()
{
	Object* bird = getObject();
	if (m_crashing)
	{
		const Real height = bird->getHeightAboveTerrainOrWater();
		if (height < 1.0f)
		{
			m_crashing = false;
			doPhaseStuff(SDPHASE_HIT_GROUND);
		}
		else if (PhysicsBehavior* physics = bird->getPhysics())
		{
			const Coord3D& position = *bird->getPosition();
			const Coord3D& velocity = *physics->getVelocity();
			const Real dx = m_crashSite.x - position.x;
			const Real dy = m_crashSite.y - position.y;
			const Real distance = WWMath::Sqrt(dx * dx + dy * dy);
			const Real speed = std::max(1.0f, physics->getVelocityMagnitude());
			const Real desired = std::min(speed, distance) / std::max(1.0f, distance);
			const Real gain = physics->getMass() * 0.1f;
			const Real vertical = getGiantBirdSlowDeathBehaviorModuleData()->m_holdFlailingHeight && distance > 10.0f
				? -velocity.z * gain : 0.0f;
			Coord3D steer{(dx * desired - velocity.x) * gain, (dy * desired - velocity.y) * gain, vertical};
			physics->applyMotiveForce(&steer);
		}
	}
	return SlowDeathBehavior::update();
}

void GiantBirdSlowDeathBehavior::xferCrashState(Xfer* xfer)
{
	xfer->xferCoord3D(&m_crashSite);
	xfer->xferBool(&m_crashing);
}

void GiantBirdSlowDeathBehavior::crc(Xfer* xfer)
{
	SlowDeathBehavior::crc(xfer);
	xferCrashState(xfer);
}

void GiantBirdSlowDeathBehavior::xfer(Xfer* xfer)
{
	XferVersion version = 1;
	xfer->xferVersion(&version, 1);
	SlowDeathBehavior::xfer(xfer);
	xferCrashState(xfer);
}

void GiantBirdSlowDeathBehavior::loadPostProcess()
{
	SlowDeathBehavior::loadPostProcess();
}

ShipSlowDeathBehavior::~ShipSlowDeathBehavior() = default;

void ShipSlowDeathBehavior::beginSlowDeath(const DamageInfo* damageInfo)
{
	SlowDeathBehavior::beginSlowDeath(damageInfo);
	Object* ship = getObject();
	if (!isSlowDeathActivated() || ship->isDestroyed())
		return;

	m_startFrame = TheGameLogic->getFrame();
	m_heading = ship->getOrientation();
	const Real rate = DEG_TO_RAD(90.0f) / std::max(1u, getDestructionFrame() - m_startFrame);
	m_pitchPerFrame = GameLogicRandomValue(0, 1) ? rate : -rate;
	m_rollPerFrame = GameLogicRandomValue(0, 1) ? rate : -rate;
	setWakeFrame(ship, UPDATE_SLEEP_NONE);
}

UpdateSleepTime ShipSlowDeathBehavior::update()
{
	const UpdateSleepTime sleep = SlowDeathBehavior::update();
	Object* ship = getObject();
	if (!ship->isDestroyed())
	{
		const UnsignedInt elapsed = TheGameLogic->getFrame() - m_startFrame;
		const Real pitch = elapsed * m_pitchPerFrame;
		const Real roll = elapsed * m_rollPerFrame;
		const Coord3D& position = *ship->getPosition();
		Matrix3D transform(true);
		transform.Rotate_Z(WWMath::SinTrig(m_heading), WWMath::CosTrig(m_heading));
		transform.Rotate_Y(WWMath::SinTrig(pitch), WWMath::CosTrig(pitch));
		transform.Rotate_X(WWMath::SinTrig(roll), WWMath::CosTrig(roll));
		transform.Set_Translation(Vector3(position.x, position.y, position.z));
		ship->setTransformMatrix(&transform);
	}
	return sleep;
}

void ShipSlowDeathBehavior::xferTiltState(Xfer* xfer)
{
	xfer->xferReal(&m_pitchPerFrame);
	xfer->xferReal(&m_rollPerFrame);
	xfer->xferUnsignedInt(&m_startFrame);
	xfer->xferReal(&m_heading);
}

void ShipSlowDeathBehavior::crc(Xfer* xfer)
{
	SlowDeathBehavior::crc(xfer);
	xferTiltState(xfer);
}

void ShipSlowDeathBehavior::xfer(Xfer* xfer)
{
	XferVersion version = 1;
	xfer->xferVersion(&version, 1);
	SlowDeathBehavior::xfer(xfer);
	xferTiltState(xfer);
}

void ShipSlowDeathBehavior::loadPostProcess()
{
	SlowDeathBehavior::loadPostProcess();
}
