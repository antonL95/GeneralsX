#include "PreRTS.h"

#include "GameLogic/Module/GiantBirdAIUpdate.h"

#include <algorithm>
#include <limits>

#include "Common/INI.h"
#include "Common/RetailFileLayers.h"
#include "Common/RetailSaveBlocks.h"
#include "Common/RotwkDuration.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"
#include "GameClient/FXList.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/ContainModule.h"
#if !RTS_GENERALS
#include "GameLogic/Module/CreateObjectDie.h"
#endif
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Module/RetailSlowDeath.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/Pathfinder/Path.h"
#include "GameLogic/Pathfinder/PathNode.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Weapon.h"
#include "WWMath/wwmath.h"

namespace
{
#if !RTS_GENERALS
constexpr UnsignedInt FRAMES_PER_RETAIL_TICK = RotwkDuration::engineFrames(1, LOGICFRAMES_PER_SECOND);
constexpr UnsignedInt FOLLOW_THROUGH_DURATION = RotwkDuration::engineFrames(40, LOGICFRAMES_PER_SECOND);
constexpr Real GRAB_TOSS_SPEED_SCALE = 0.5f;
constexpr Real GRAB_TOSS_VERTICAL_VELOCITY = -6.0f;

Real flightLength(const Coord3D& delta)
{
	return WWMath::SqrtfOrigin(delta.lengthSqr());
}

Coord3D scaled(Coord3D value, Real factor)
{
	value.scale(factor);
	return value;
}

Coord3D flightDirection(const Coord3D& delta)
{
	const Real length = flightLength(delta);
	return length > 0.0f ? scaled(delta, 1.0f / length) : Coord3D{};
}

Bool useLowSwoopExit(const Object& bird, const Weapon& weapon, const Object* target)
{
	return target && weapon.getClipSize() > 1 && !target->isAirborneTarget() &&
		!target->isKindOf(KINDOF_MACHINE) && !weapon.getTemplate()->canGrabTarget(&bird, target);
}
#endif
}

void GiantBirdAIUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	AIUpdateModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "FollowThroughDistance", INI::parseReal, nullptr, offsetof(GiantBirdAIUpdateModuleData, m_followThroughDistance) },
		{ "FollowThroughCheckStep", INI::parseReal, nullptr, offsetof(GiantBirdAIUpdateModuleData, m_followThroughCheckStep) },
		{ "FollowThroughGradient", INI::parseReal, nullptr, offsetof(GiantBirdAIUpdateModuleData, m_followThroughGradient) },
		{ "GrabTossTimeTrigger", INI::parseReal, nullptr, offsetof(GiantBirdAIUpdateModuleData, m_grabTossTimeTrigger) },
		{ "GrabTossHeightTrigger", INI::parseReal, nullptr, offsetof(GiantBirdAIUpdateModuleData, m_grabTossHeightTrigger) },
		{ "TossFX", INI::parseFXList, nullptr, offsetof(GiantBirdAIUpdateModuleData, m_tossFX) },
		{ "SpecialContactPoints", ThingTemplate::parseContactPointFlags, nullptr, offsetof(GiantBirdAIUpdateModuleData, m_specialContactPoints) },
		{ "AttackLocomotorType", INI::parseAsciiString, nullptr, offsetof(GiantBirdAIUpdateModuleData, m_attackLocomotorType) },
		{ "ReturnForAmmoLocomotorType", INI::parseAsciiString, nullptr, offsetof(GiantBirdAIUpdateModuleData, m_returnForAmmoLocomotorType) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields, 0);
}

GiantBirdAIUpdate::GiantBirdAIUpdate(Thing* thing, const ModuleData* data) : AIUpdateInterface(thing, data)
{
}

GiantBirdAIUpdate::~GiantBirdAIUpdate() = default;

void GiantBirdAIUpdate::aiDoCommand(const AICommandParms* parms)
{
	if (parms->m_cmdSource != CMD_FROM_AI)
	{
		m_followingThrough = FALSE;
		m_swoopVictim = INVALID_ID;
		setSwooping(FALSE);
#if !RTS_GENERALS
		m_followThroughUntil = 0;
		clearFlightPath();
#endif
	}
	AIUpdateInterface::aiDoCommand(parms);
}

void GiantBirdAIUpdate::setSwooping(Bool swooping)
{
	if (m_swooping == swooping)
		return;
	m_swooping = swooping;
	if (swooping)
		getObject()->setModelConditionState(MODELCONDITION_SWOOPING);
	else
		getObject()->clearModelConditionState(MODELCONDITION_SWOOPING);
}

Bool GiantBirdAIUpdate::canStrike(const Object& bird, const Coord3D& target)
{
	if (!RetailFileLayers::enabled())
		return TRUE;
	const auto* ai = static_cast<const GiantBirdAIUpdate*>(bird.findUpdateModule(NAMEKEY("GiantBirdAIUpdate")));
	if (!ai)
		return TRUE;
#if !RTS_GENERALS
	const Weapon* weapon = bird.getCurrentWeapon();
	if (weapon && !weapon->getTemplate()->canSwoop())
		return TRUE;
	const Locomotor* locomotor = ai->getCurLocomotor();
	if (!locomotor || locomotor->getAppearance() != LOCO_GIANT_BIRD)
		return TRUE;
	if (!ai->m_swooping || ai->m_followingThrough || ai->m_flightSampleCount == 0)
		return FALSE;
	const Real step = ai->m_flightSpeed * FRAMES_PER_RETAIL_TICK;
	const Real remaining = ai->m_flightSamples[ai->m_flightSampleCount - 1].distance - ai->m_flightDistance;
	const Object* victim = ai->getCurrentVictim();
	if (victim && victim->getID() != ai->m_swoopVictim)
		return FALSE;
	const Coord3D contact = victim ? ai->findContact(*victim) :
		Coord3D{target.x, target.y, target.z + locomotor->getPreferredAttackHeight()};
	return remaining <= step && (contact - *bird.getPosition()).lengthSqr() <= sqr(std::max(8.5f, 3.0f * step));
#else
	(void)target;
	return TRUE;
#endif
}

void GiantBirdAIUpdate::trackGrabbedPassenger(Object& bird, const Object& passenger)
{
	if (RetailFileLayers::enabled())
		if (auto* ai = static_cast<GiantBirdAIUpdate*>(bird.findUpdateModule(NAMEKEY("GiantBirdAIUpdate"))))
		{
			ai->m_grabbedPassenger = passenger.getID();
			ai->m_killPassengerOnLanding = passenger.isKindOf(KINDOF_GRAB_AND_DROP);
		}
}

void GiantBirdAIUpdate::releaseGrabbedPassenger()
{
	if (!RetailFileLayers::enabled() || m_grabbedPassenger == INVALID_ID)
		return;
	Object* bird = getObject();
	Object* passenger = TheGameLogic->findObjectByID(m_grabbedPassenger);
	if (!passenger || passenger->getContainedBy() != bird || !bird->getContain())
	{
		m_grabbedPassenger = INVALID_ID;
		m_killPassengerOnLanding = FALSE;
		return;
	}

	const auto* data = getGiantBirdAIUpdateModuleData();
	const Coord3D& pos = *bird->getPosition();
	const Real height = pos.z - TheTerrainLogic->getGroundHeight(pos.x, pos.y);
	const UnsignedInt heldFrames = TheGameLogic->getFrame() - passenger->getContainedByFrame();
	const Bool timeReached = data->m_grabTossTimeTrigger > 0.0f &&
		heldFrames > data->m_grabTossTimeTrigger * LOGICFRAMES_PER_SECOND;
	const Bool heightReached = data->m_grabTossHeightTrigger > 0.0f && height > data->m_grabTossHeightTrigger;
	if (!timeReached && !heightReached)
		return;

	const Bool killOnLanding = m_killPassengerOnLanding;
	m_grabbedPassenger = INVALID_ID;
	m_killPassengerOnLanding = FALSE;
	bird->getContain()->removeFromContain(passenger);
	passenger->setOrientation(bird->getOrientation());
	if (auto* physics = passenger->getPhysics())
	{
#if !RTS_GENERALS
		if (getCurLocomotor())
		{
			Vector3 forward = bird->getTransformMatrix()->Get_X_Vector();
			const Real forwardLength = WWMath::SqrtfOrigin(
				forward.X * forward.X + forward.Y * forward.Y + forward.Z * forward.Z);
			if (forwardLength > 0.0f)
			{
				forward.X /= forwardLength;
				forward.Y /= forwardLength;
				forward.Z /= forwardLength;
			}
			else
			{
				const Coord3D& heading = *bird->getUnitDirectionVector2D();
				forward.Set(heading.x, heading.y, 0.0f);
			}
			constexpr Real framesPerRetailTick =
				static_cast<Real>(LOGICFRAMES_PER_SECOND) / RotwkDuration::FRAMES_PER_SECOND;
			const Real birdSpeed = bird->getPhysics() ? bird->getPhysics()->getVelocityMagnitude() : getCurLocomotorSpeed();
			const Real halfRetailSpeed = birdSpeed * framesPerRetailTick * GRAB_TOSS_SPEED_SCALE;
			const Coord3D launchVelocity = {
				forward.X * halfRetailSpeed,
				forward.Y * halfRetailSpeed,
				GRAB_TOSS_VERTICAL_VELOCITY
			};
			physics->launchFromContainment(launchVelocity);
			FXList::doFXObj(data->m_tossFX, passenger, bird);
			if (killOnLanding)
			{
				bird->scoreTheKill(passenger);
				physics->setKillWhenLanding(TRUE);
			}
		}
#else
		(void)killOnLanding;
		if (const auto* carrierPhysics = bird->getPhysics())
			carrierPhysics->transferVelocityTo(physics);
		physics->setAllowToFall(TRUE);
		FXList::doFXObj(data->m_tossFX, passenger, bird);
#endif
	}
	passenger->setModelConditionState(MODELCONDITION_FREEFALL);
}

void GiantBirdAIUpdate::tryGrabDebrisPortion(Object* victim)
{
#if !RTS_GENERALS
	if (!RetailFileLayers::enabled() || m_grabbedPassenger != INVALID_ID || !victim ||
		!victim->isEffectivelyDead() || !victim->isKindOf(KINDOF_MACHINE))
		return;
	Object* bird = getObject();
	ContainModuleInterface* contain = bird->getContain();
	if (!contain || contain->getContainCount() >= contain->getContainMax())
		return;
	static const NameKeyType createDieKey = NAMEKEY("CreateObjectDie");
	AsciiString debrisName;
	if (BehaviorModule** behaviors = victim->getBehaviorModules())
	{
		for (; *behaviors; ++behaviors)
		{
			if ((*behaviors)->getModuleNameKey() == createDieKey)
			{
				const auto* createDie = static_cast<const CreateObjectDie*>(*behaviors);
				if (!createDie->getDebrisPortionOfSelf().isEmpty())
				{
					debrisName = createDie->getDebrisPortionOfSelf();
					break;
				}
			}
		}
	}
	if (debrisName.isEmpty())
		return;
	const ThingTemplate* debrisTemplate = TheThingFactory->findTemplate(debrisName, FALSE);
	if (!debrisTemplate)
		return;
	Object* debris = TheThingFactory->newObject(debrisTemplate, victim->getTeam());
	if (!debris)
		return;
	debris->setTransformMatrix(bird->getTransformMatrix());
	contain->addToContain(debris);
	if (debris->getContainedBy() == bird)
	{
		m_grabbedPassenger = debris->getID();
		m_killPassengerOnLanding = FALSE;
		m_swoopVictim = INVALID_ID;
	}
	else
		TheGameLogic->destroyObject(debris);
#else
	(void)victim;
#endif
}

#if !RTS_GENERALS
void GiantBirdAIUpdate::clearFlightPath()
{
	m_flightPointCount = 0;
	m_flightSampleCount = 0;
	m_flightGoal = {};
	m_flightDistance = 0.0f;
	m_flightSpeed = 0.0f;
	m_flightSlope = 0.0f;
	m_flightTurn = 0.0f;
	m_nextSpeedFrame = 0;
}

void GiantBirdAIUpdate::sampleFlightPath()
{
	m_flightSampleCount = 1;
	m_flightSamples.front() = {m_flightPoints[1], 0.0f};
	for (UnsignedInt segment = 0; segment + 3 < m_flightPointCount; ++segment)
	{
		const Coord3D& before = m_flightPoints[segment];
		const Coord3D& start = m_flightPoints[segment + 1];
		const Coord3D& end = m_flightPoints[segment + 2];
		const Coord3D& after = m_flightPoints[segment + 3];
		const Coord3D startTangent = scaled(end - before, 0.5f);
		const Coord3D endTangent = scaled(after - start, 0.5f);
		for (UnsignedInt i = 1; i <= SAMPLES_PER_SEGMENT; ++i)
		{
			const Real t = static_cast<Real>(i) / SAMPLES_PER_SEGMENT;
			const Real u = 1.0f - t;
			const Coord3D position = start + scaled(end - start, t * t * (1.0f + 2.0f * u)) +
				scaled(startTangent, t * u * u) - scaled(endTangent, t * t * u);
			const FlightSample& previous = m_flightSamples[m_flightSampleCount - 1];
			m_flightSamples[m_flightSampleCount++] = {position, previous.distance + flightLength(position - previous.position)};
		}
	}
}

Coord3D GiantBirdAIUpdate::flightPosition(Real distance) const
{
	const auto begin = m_flightSamples.begin();
	const auto end = begin + m_flightSampleCount;
	const auto sample = std::lower_bound(begin, end, distance,
		[](const FlightSample& point, Real at) { return point.distance < at; });
	if (sample == begin)
		return sample->position;
	if (sample == end)
		return (end - 1)->position;
	const FlightSample& before = *(sample - 1);
	const Real span = sample->distance - before.distance;
	return span > 0.0f ? before.position + scaled(sample->position - before.position, (distance - before.distance) / span)
		: sample->position;
}

Real GiantBirdAIUpdate::flightClearance(const Coord3D& point, Coord3D* offset) const
{
	const Object& bird = *getObject();
	const Real radius = bird.getGeometryInfo().getBoundingCircleRadius();
	const Real height = getCurLocomotor()->getPreferredHeight();
	Real minimum = TheTerrainLogic->getGroundHeight(point.x, point.y) + height * 0.5f;
	for (const Real x : {point.x - radius, point.x + radius})
		for (const Real y : {point.y - radius, point.y + radius})
			minimum = std::max(minimum, TheTerrainLogic->getGroundHeight(x, y) + height * 0.3885f);
	Real clearance = std::max(0.0f, minimum - point.z);
	Coord3D separation = {0.0f, 0.0f, clearance};
	auto* nearby = ThePartitionManager->iterateObjectsInRange(&point, radius, FROM_BOUNDINGSPHERE_3D, nullptr, ITER_SORTED_BY_ID);
	MemoryPoolObjectHolder holder(nearby);
	for (const Object* other = nearby->first(); other; other = nearby->next())
	{
		if (other == &bird || other->getID() == m_swoopVictim || other->getContainedBy() ||
			other->isEffectivelyDead() || other->isKindOf(KINDOF_INFANTRY) || other->isKindOf(KINDOF_HORDE) ||
			other->isKindOf(KINDOF_VEHICLE) || other->isKindOf(KINDOF_NO_COLLIDE))
			continue;
		if (!other->isKindOf(KINDOF_IMMOBILE) &&
			(*other->getPosition() - *bird.getPosition()).lengthSqr() > sqr(1.2f * radius))
			continue;
		const Coord3D away = point - *other->getPosition();
		const Real gap = flightLength(away);
		const Real intrusion = radius + 2.0f * other->getGeometryInfo().getBoundingCircleRadius() - gap;
		if (intrusion <= 0.0f)
			continue;
		clearance = std::max(clearance, intrusion);
		separation += gap > 0.0f ? scaled(away, intrusion / gap) : Coord3D{0.0f, 0.0f, intrusion};
	}
	if (offset)
		*offset = separation;
	return clearance;
}

void GiantBirdAIUpdate::buildFlightPath(const Coord3D& goal, const Coord3D* next, FlightPathShape shape)
{
	const Object& bird = *getObject();
	const Coord3D origin = *bird.getPosition();
	const Coord3D displacement = {goal.x - origin.x, goal.y - origin.y, 0.0f};
	const Real planarDistance = flightLength(displacement);
	const Real tangentDepth = std::min(1500.0f, 3.0f * planarDistance);
	const Vector3 facing = bird.getTransformMatrix()->Get_X_Vector();
	const Coord3D forward = {facing.X, facing.Y, facing.Z};
	Coord3D before = origin - scaled(flightDirection(forward), tangentDepth);
	if (shape == FlightPathShape::Lift && bird.isSignificantlyAboveTerrain())
		before.z -= tangentDepth;
	if (shape == FlightPathShape::PositionStrike)
		before = {origin.x, origin.y, 0.0f};

	m_flightPoints[0] = before;
	m_flightPoints[1] = origin;
	m_flightPointCount = 2;
	if (next)
		m_flightPoints[m_flightPointCount++] = goal;

	const Coord3D& end = next ? *next : goal;
	const Real reach = WWMath::Floor(std::clamp(planarDistance * 0.25f, 50.0f, 150.0f));
	const Coord3D heading = flightDirection(displacement);
	Coord3D after = end + scaled(heading, reach);
	if (flightClearance(after) > 0.0f)
	{
		const Real bearing = WWMath::Atan2Origin(heading.y, heading.x);
		const auto pointAt = [&](Real angle)
		{
			return Coord3D{end.x + reach * WWMath::CosTrig(angle), end.y + reach * WWMath::SinTrig(angle), end.z};
		};
		std::array<Bool, 2> clear{};
		for (Real arc = 0.0f; arc < PI && !(clear[0] && clear[1]); arc += 0.2f)
		{
			for (UnsignedInt side = 0; side < clear.size(); ++side)
			{
				const Real direction = side == 0 ? -1.0f : 1.0f;
				const Real angle = bearing + direction * arc;
				if (clear[side] || flightClearance(pointAt(angle)) > 0.0f)
					continue;
				const Coord3D turnPoint = pointAt(angle + direction * 0.4f);
				if (clear[1 - side])
					m_flightPoints[m_flightPointCount++] = turnPoint;
				else
					after = turnPoint;
				clear[side] = TRUE;
			}
		}
	}
	if (shape == FlightPathShape::LowExit)
		after.z -= tangentDepth;
	m_flightPoints[m_flightPointCount++] = end;
	m_flightPoints[m_flightPointCount++] = after;
	m_flightGoal = goal;
	m_flightDistance = 0.0f;
	if (m_nextSpeedFrame == 0)
		m_flightSpeed = bird.getPhysics()->getVelocityMagnitude();
	sampleFlightPath();

	const Real radius = bird.getGeometryInfo().getBoundingCircleRadius();
	const Real step = WWMath::Floor(radius * 0.5f);
	if (step <= 0.0f)
		return;
	Real blockedSince = 0.0f;
	Real distance = step;
	UnsignedInt inserted = 0;
	for (UnsignedInt check = 0; check < 200 && distance < m_flightSamples[m_flightSampleCount - 1].distance &&
		inserted < MAX_CLEARANCE_INSERTIONS; ++check, distance += step)
	{
		if (flightClearance(flightPosition(distance)) > 0.0f)
		{
			if (blockedSince == 0.0f)
				blockedSince = distance;
			continue;
		}
		if (blockedSince == 0.0f)
			continue;
		const Real midpoint = (blockedSince + distance) * 0.5f;
		blockedSince = 0.0f;
		Coord3D point = flightPosition(midpoint);
		Coord3D offset;
		if (flightClearance(point, &offset) <= 0.0f)
			continue;
		point += offset;
		UnsignedInt insert = 2;
		while (insert + 2 < m_flightPointCount && m_flightSamples[(insert - 1) * SAMPLES_PER_SEGMENT].distance < midpoint)
			++insert;
		std::move_backward(m_flightPoints.begin() + insert, m_flightPoints.begin() + m_flightPointCount,
			m_flightPoints.begin() + m_flightPointCount + 1);
		m_flightPoints[insert] = point;
		++m_flightPointCount;
		++inserted;
		sampleFlightPath();
		distance = midpoint + radius;
	}
}

void GiantBirdAIUpdate::advanceFlightPath(Locomotor& locomotor, Real desiredSpeed)
{
	Object& bird = *getObject();
	PhysicsBehavior& physics = *bird.getPhysics();
	const BodyDamageType damage = bird.getBodyModule()->getDamageState();
	const Real maxSpeed = locomotor.getMaxSpeedForCondition(damage, &bird);
	const Real end = m_flightSamples[m_flightSampleCount - 1].distance;
	const Real radius = bird.getGeometryInfo().getBoundingCircleRadius();
	const Real lookAhead = radius * 0.5f * std::max(1.0f, locomotor.getLookAheadMult());
	const UnsignedInt now = TheGameLogic->getFrame();
	const Coord3D center = flightPosition(m_flightDistance);
	const Bool hasBack = m_flightDistance >= lookAhead;
	const Bool hasFront = m_flightDistance + lookAhead < end;
	const Coord3D back = hasBack ? flightDirection(center - flightPosition(m_flightDistance - lookAhead)) : Coord3D{};
	const Coord3D front = hasFront ? flightDirection(flightPosition(m_flightDistance + lookAhead) - center) : Coord3D{};
	const Real facing = bird.getOrientation();
	const Real backTurn = back.x || back.y ? stdAngleDiff(WWMath::Atan2Origin(back.y, back.x), facing) : 0.0f;
	const Real frontTurn = front.x || front.y ? stdAngleDiff(WWMath::Atan2Origin(front.y, front.x), facing) : 0.0f;
	const Real sampleWeight = hasBack && hasFront ? 0.5f : 1.0f;
	const Real turn = (backTurn + frontTurn) * sampleWeight;
	const Real turnRate = locomotor.getMaxTurnRate(damage);
	if (now >= m_nextSpeedFrame)
	{
		Real slope = (back.z + front.z) * sampleWeight;
		const Real turnLimit = turnRate * FRAMES_PER_RETAIL_TICK;
		m_flightTurn = std::clamp(turn, -turnLimit, turnLimit) * 0.2f + m_flightTurn * 0.8f;
		if (end - m_flightDistance < m_flightSpeed * FRAMES_PER_RETAIL_TICK * 4.0f && slope < 0.0f)
			slope = -slope;
		else if (WWMath::Fabs(turn) > turnLimit * 3.0f)
			slope += 1.5f;
		m_flightSlope = slope;
		m_flightSpeed += std::clamp(desiredSpeed - m_flightSpeed,
			-locomotor.getBraking() * FRAMES_PER_RETAIL_TICK,
			locomotor.getMaxAcceleration(damage) * FRAMES_PER_RETAIL_TICK);
		m_flightSpeed = std::clamp(m_flightSpeed * (1.0f - 0.4f * slope), std::min(locomotor.getMinSpeed(), maxSpeed), maxSpeed);
		m_nextSpeedFrame = now + FRAMES_PER_RETAIL_TICK;
	}
	m_flightDistance = std::min(end, m_flightDistance + m_flightSpeed);
	Coord3D position = flightPosition(m_flightDistance);
	position.z = std::max(position.z, TheTerrainLogic->getGroundHeight(position.x, position.y) + radius * 0.25f);
	if (turn != 0.0f)
		bird.setOrientation(facing + std::clamp(turn, -turnRate, turnRate));
	physics.setTurning(m_flightTurn < 0.0f ? TURN_NEGATIVE : m_flightTurn > 0.0f ? TURN_POSITIVE : TURN_NONE);
	locomotor.setPhysicsOptions(&bird);
	bird.clearStatus(MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_BRAKING));
	physics.setFlightPathVelocity(position - *bird.getPosition());
	updateAirborneTargetStatus();
}

Coord3D GiantBirdAIUpdate::findContact(const Object& victim) const
{
	const Weapon* weapon = getObject()->getCurrentWeapon();
	Coord3D aim;
	if (weapon && weapon->getTemplate()->getPreferredTargetPosition(&victim, aim) &&
		aim.z > TheTerrainLogic->getGroundHeight(aim.x, aim.y))
		return aim;

	Coord3D contact = *victim.getPosition();
	contact.z += getCurLocomotor()->getPreferredAttackHeight();
	Real bestDistance = std::numeric_limits<Real>::max();
	const Coord3D& bird = *getObject()->getPosition();
	const UnsignedInt mask = getGiantBirdAIUpdateModuleData()->m_specialContactPoints;
	const auto& geometry = victim.getTemplate()->getRotWKObjectData();
	Real maxHeight = std::max(0.0f, victim.getGeometryInfo().getMaxHeightAbovePosition() + geometry.primaryGeometry.offset.z);
	for (const auto& part : geometry.additionalGeometries)
	{
		if (part.details.active)
			maxHeight = std::max(maxHeight, part.shape.getMaxHeightAbovePosition() + part.details.offset.z);
	}
	for (const auto* points : {&geometry.geometryContactPoints, &geometry.attackContactPoints})
	{
		for (const auto& point : *points)
		{
			if (!(point.flags & mask))
				continue;
			Vector3 world;
			const Real z = std::clamp(point.position.z, maxHeight * 0.1f, maxHeight);
			Matrix3D::Transform_Vector(*victim.getTransformMatrix(), Vector3(point.position.x, point.position.y, z), &world);
			const Coord3D pos = {world.X, world.Y, world.Z};
			const Real distance = (pos - bird).lengthSqr();
			if (distance < bestDistance)
			{
				bestDistance = distance;
				contact = pos;
			}
		}
	}
	return contact;
}

void GiantBirdAIUpdate::startFollowThrough(Bool lift)
{
	const GiantBirdAIUpdateModuleData& data = *getGiantBirdAIUpdateModuleData();
	if (data.m_followThroughDistance <= 0.0f)
		return;
	chooseLocomotorSet(LOCOMOTORSET_NORMAL);
	const Locomotor* locomotor = getCurLocomotor();
	if (!locomotor)
		return;

	const Object& bird = *getObject();
	const Coord3D origin = *bird.getPosition();
	const Vector3 facing = bird.getTransformMatrix()->Get_X_Vector();
	const Coord3D direction = {facing.X, facing.Y, facing.Z};
	Region3D extent;
	TheTerrainLogic->getExtent(&extent);
	Coord3D goal = origin;
	Real checked = 0.0f;
	const Real radius = bird.getGeometryInfo().getBoundingCircleRadius();
	for (Real distance = data.m_followThroughCheckStep;
		distance > checked && distance <= data.m_followThroughDistance; distance += data.m_followThroughCheckStep)
	{
		const Coord3D candidate = origin + scaled(direction, distance);
		const Real maxHeight = origin.z + distance * data.m_followThroughGradient;
		if (candidate.x <= extent.lo.x || candidate.y <= extent.lo.y || candidate.x >= extent.hi.x || candidate.y >= extent.hi.y ||
			TheTerrainLogic->getGroundHeight(candidate.x, candidate.y) > maxHeight)
			break;
		auto* nearby = ThePartitionManager->iterateObjectsInRange(&candidate, radius, FROM_BOUNDINGSPHERE_2D);
		MemoryPoolObjectHolder holder(nearby);
		Bool blocked = FALSE;
		for (const Object* other = nearby->first(); other; other = nearby->next())
		{
			if (other == &bird || other->getContainedBy() || other->isEffectivelyDead())
				continue;
			if (!(other->isKindOf(KINDOF_OBSTACLE) || other->isKindOf(KINDOF_STRUCTURE) ||
				other->isKindOf(KINDOF_AIRCRAFT) || other->isKindOf(KINDOF_HUGE_VEHICLE)))
				continue;
			if (other->getPosition()->z + other->getGeometryInfo().getMaxHeightAbovePosition() > maxHeight)
			{
				blocked = TRUE;
				break;
			}
		}
		if (blocked)
			break;
		goal = candidate;
		checked = distance;
	}
	const Real ground = TheTerrainLogic->getGroundHeight(goal.x, goal.y);
	const Real height = locomotor->getPreferredHeight();
	goal.z = std::min(goal.z + height, std::max(ground + height * 1.25f, goal.z + height * 0.25f));
	m_followThroughGoal = goal;
	m_followingThrough = TRUE;
	m_followThroughUntil = TheGameLogic->getFrame() + FOLLOW_THROUGH_DURATION;
	const FlightPathShape shape = lift || checked < data.m_followThroughDistance * 0.5f
		? FlightPathShape::Lift : FlightPathShape::Normal;
	buildFlightPath(goal, nullptr, shape);
}

Bool GiantBirdAIUpdate::updateFollowThrough(Locomotor& locomotor)
{
	if (!m_followingThrough)
		return FALSE;
	if (m_swoopVictim != INVALID_ID && m_grabbedPassenger == INVALID_ID)
	{
		if (Object* victim = TheGameLogic->findObjectByID(m_swoopVictim))
			tryGrabDebrisPortion(victim);
		else
			m_swoopVictim = INVALID_ID;
	}
	const UnsignedInt now = TheGameLogic->getFrame();
	if (m_followThroughUntil == 0)
	{
		m_followThroughUntil = now + FOLLOW_THROUGH_DURATION;
		m_followThroughGoal.z = TheTerrainLogic->getGroundHeight(m_followThroughGoal.x, m_followThroughGoal.y) + locomotor.getPreferredHeight();
		buildFlightPath(m_followThroughGoal, nullptr, FlightPathShape::Normal);
	}
	if (now < m_followThroughUntil && m_flightDistance < m_flightSamples[m_flightSampleCount - 1].distance)
	{
		advanceFlightPath(locomotor, getCurLocomotorSpeed());
		return TRUE;
	}
	m_followingThrough = FALSE;
	m_followThroughUntil = 0;
	m_swoopVictim = INVALID_ID;
	clearFlightPath();
	return FALSE;
}

Bool GiantBirdAIUpdate::updateSwoop()
{
	Object& bird = *getObject();
	const Weapon* weapon = bird.getCurrentWeapon();
	const Object* victim = getCurrentVictim();
	const Coord3D* strike = victim ? victim->getPosition() : getCurrentVictimPos();
	if (!isAttacking() || !weapon || !weapon->getTemplate()->canSwoop() || !strike || (victim && victim->isEffectivelyDead()))
	{
		if (m_swooping)
			clearFlightPath();
		setSwooping(FALSE);
		m_swoopVictim = INVALID_ID;
		return FALSE;
	}

	const AsciiString& authored = getGiantBirdAIUpdateModuleData()->m_attackLocomotorType;
	const Int set = authored.isEmpty() ? LOCOMOTORSET_PANIC : AIUpdateModuleData::findLocomotorSetType(authored);
	if (set >= LOCOMOTORSET_NORMAL && set < LOCOMOTORSET_COUNT && set != LOCOMOTORSET_NORMAL_UPGRADED)
		chooseLocomotorSet(static_cast<LocomotorSetType>(set));
	Locomotor* locomotor = getCurLocomotor();
	if (!locomotor)
		return FALSE;

	Coord3D contact = victim ? findContact(*victim) : *strike;
	if (!victim)
		contact.z += locomotor->getPreferredAttackHeight();
	const ObjectID victimID = victim ? victim->getID() : INVALID_ID;
	const Real contactShift = flightLength(contact - m_flightGoal);
	const Real refreshDistance = m_flightSpeed * FRAMES_PER_RETAIL_TICK * 8.0f;
	const Real nearContact = bird.getGeometryInfo().getBoundingCircleRadius();
	const Real contactDistance = (contact - *bird.getPosition()).lengthSqr();
	const Bool moved = contactShift > 0.0f && (contactDistance < nearContact * nearContact * 0.5f ||
		(contactShift > 10.0f && (contactShift > refreshDistance || contactDistance < sqr(refreshDistance))));
	const Bool rebuild = !m_swooping || m_flightPointCount == 0 || victimID != m_swoopVictim || moved;
	m_swoopVictim = victimID;
	if (rebuild)
	{
		FlightPathShape shape = FlightPathShape::PositionStrike;
		if (victim)
			shape = useLowSwoopExit(bird, *weapon, victim) ? FlightPathShape::LowExit : FlightPathShape::Normal;
		buildFlightPath(contact, nullptr, shape);
	}
	setSwooping(TRUE);
	advanceFlightPath(*locomotor, getCurLocomotorSpeed());
	return TRUE;
}

Bool GiantBirdAIUpdate::updateTravelFlight(Locomotor& locomotor)
{
	if (!isMoving() || isWaitingForPath() || isAttacking() || isDoingGroundMovement())
		return FALSE;
	const PathNode* destination = getPath() ? getPath()->getLastNode() : nullptr;
	const Coord3D* target = destination ? destination->getPosition() : getGoalPosition();
	if (!target)
		return FALSE;
	Coord3D goal = *target;
	goal.z = TheTerrainLogic->getGroundHeight(goal.x, goal.y) + locomotor.getPreferredHeight();
	if (m_flightPointCount == 0 || !goal.equals(m_flightGoal))
	{
		const Int waypoint = friend_getCurrentGoalPathIndex();
		const Coord3D* nextWaypoint = waypoint >= 0 ? friend_getGoalPathPosition(waypoint + 1) : nullptr;
		Coord3D next;
		if (nextWaypoint)
		{
			next = *nextWaypoint;
			next.z = TheTerrainLogic->getGroundHeight(next.x, next.y) + locomotor.getPreferredHeight();
		}
		buildFlightPath(goal, nextWaypoint ? &next : nullptr, FlightPathShape::Normal);
	}
	if (m_flightDistance >= m_flightSamples[m_flightSampleCount - 1].distance)
		return FALSE;
	advanceFlightPath(locomotor, std::min(getDesiredSpeed(), getCurLocomotorSpeed()));
	return TRUE;
}
#endif

UpdateSleepTime GiantBirdAIUpdate::update()
{
	const UpdateSleepTime sleep = AIUpdateInterface::update();
	releaseGrabbedPassenger();
	return getObject()->isEffectivelyDead() || !getCurLocomotor() ? sleep : UPDATE_SLEEP_NONE;
}

UpdateSleepTime GiantBirdAIUpdate::doLocomotor()
{
	Object* bird = getObject();
	if (bird->isEffectivelyDead() && GiantBirdSlowDeathBehavior::controlsMotion(*bird))
		return UPDATE_SLEEP_FOREVER;

#if !RTS_GENERALS
	Locomotor* locomotor = getCurLocomotor();
	PhysicsBehavior* physics = bird->getPhysics();
	if (!locomotor || locomotor->getAppearance() != LOCO_GIANT_BIRD || bird->isEffectivelyDead() ||
		!physics || physics->getIsStunned() || bird->isDisabledByType(DISABLED_HELD))
		return AIUpdateInterface::doLocomotor();

	if (RetailFileLayers::enabled() && getCurLocomotorSetType() == LOCOMOTORSET_SUPERSONIC &&
		bird->findUpdateModule(NAMEKEY("StrafeAreaUpdate")))
		return AIUpdateInterface::doLocomotor();

	if (m_swooping)
	{
		for (Int slot = PRIMARY_WEAPON; slot < WEAPONSLOT_COUNT; ++slot)
		{
			const Weapon* weapon = bird->getWeaponInWeaponSlot(static_cast<WeaponSlotType>(slot));
			if (weapon && weapon->getTemplate()->canSwoop() && weapon->getLastShotFrame() == TheGameLogic->getFrame())
			{
				Object* victim = m_swoopVictim != INVALID_ID ? TheGameLogic->findObjectByID(m_swoopVictim) : getCurrentVictim();
				const Bool lift = useLowSwoopExit(*bird, *weapon, victim);
				tryGrabDebrisPortion(victim);
				setSwooping(FALSE);
				startFollowThrough(lift);
				break;
			}
		}
	}
	if (m_followingThrough && updateFollowThrough(*getCurLocomotor()))
		return UPDATE_SLEEP_NONE;
	if (updateSwoop())
		return UPDATE_SLEEP_NONE;
	chooseLocomotorSet(LOCOMOTORSET_NORMAL);
	if (Locomotor* normal = getCurLocomotor())
	{
		normal->restorePreferredHeightFromTemplate();
		if (updateTravelFlight(*normal))
			return UPDATE_SLEEP_NONE;
	}
	if (m_flightSampleCount && m_flightDistance >= m_flightSamples[m_flightSampleCount - 1].distance)
		physics->scrubVelocity2D(0.0f);
	clearFlightPath();
#endif
	return AIUpdateInterface::doLocomotor();
}

void GiantBirdAIUpdate::xferFlightState(Xfer* xfer)
{
	xfer->xferCoord3D(&m_followThroughGoal);
	xfer->xferBool(&m_followingThrough);
	xfer->xferBool(&m_swooping);
}

void GiantBirdAIUpdate::xferRetailBlocks(Xfer* xfer, Bool present)
{
	RetailSaveBlocks::transferSection(xfer, xfer->getXferMode() == XFER_LOAD, present, {
		{ "grabbed-passenger", [this](Xfer* x) { x->xferObjectID(&m_grabbedPassenger); },
			[this] { m_grabbedPassenger = INVALID_ID; } },
		{ "swoop-victim", [this](Xfer* x) { x->xferObjectID(&m_swoopVictim); },
			[this] { m_swoopVictim = INVALID_ID; } },
		{ "grab-kill-on-landing", [this](Xfer* x) { x->xferBool(&m_killPassengerOnLanding); },
			[this] { m_killPassengerOnLanding = FALSE; } },
#if !RTS_GENERALS
		{ "giant-bird-flight-path", [this](Xfer* x)
		{
			x->xferUnsignedByte(&m_flightPointCount);
			for (UnsignedInt i = 0; i < std::min<UnsignedInt>(m_flightPointCount, MAX_FLIGHT_POINTS); ++i)
				x->xferCoord3D(&m_flightPoints[i]);
			x->xferCoord3D(&m_flightGoal);
			x->xferReal(&m_flightDistance);
			x->xferReal(&m_flightSpeed);
			x->xferReal(&m_flightSlope);
			x->xferReal(&m_flightTurn);
			x->xferUnsignedInt(&m_nextSpeedFrame);
			x->xferUnsignedInt(&m_followThroughUntil);
		}, [this]
		{
			clearFlightPath();
			m_followThroughUntil = 0;
		} },
#endif
	});
}

void GiantBirdAIUpdate::crc(Xfer* xfer)
{
	AIUpdateInterface::crc(xfer);
	xferFlightState(xfer);
	if (RetailFileLayers::enabled())
		xferRetailBlocks(xfer, TRUE);
}

void GiantBirdAIUpdate::xfer(Xfer* xfer)
{
	const XferVersion currentVersion = RetailFileLayers::enabled() ? 2 : 1;
	XferVersion version = currentVersion;
	xfer->xferVersion(&version, currentVersion);
	AIUpdateInterface::xfer(xfer);
	xferFlightState(xfer);
	if (RetailFileLayers::enabled())
		xferRetailBlocks(xfer, version >= 2);
}

void GiantBirdAIUpdate::loadPostProcess()
{
	AIUpdateInterface::loadPostProcess();
#if !RTS_GENERALS
	if (m_flightPointCount >= 4 && m_flightPointCount <= MAX_FLIGHT_POINTS)
		sampleFlightPath();
	else
	{
		clearFlightPath();
		m_followThroughUntil = 0;
	}
#endif
}
