#include "PreRTS.h"
#include "GameLogic/Module/RetailSiegeContain.h"

#include "Common/RetailFileLayers.h"
#include "Common/RetailSaveBlocks.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Module/RetailGate.h"
#include "GameLogic/Module/RetailSiegeDocking.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"
#include "WWMath/wwmath.h"

void SiegeEngineContainModuleData::buildFieldParse(MultiIniFieldParse& fields)
{
	HordeTransportContainModuleData::buildFieldParse(fields);
	static const FieldParse table[] = {
		{ "CrewFilter", SpecialPowerObjectFilter::parse, nullptr, offsetof(SiegeEngineContainModuleData, m_crewFilter) },
		{ "ObjectStatusOfCrew", ObjectStatusMaskType::parseFromINI, nullptr, offsetof(SiegeEngineContainModuleData, m_crewStatus) },
		{ "CrewMax", INI::parseInt, nullptr, offsetof(SiegeEngineContainModuleData, m_crewMax) },
		{ "InitialCrew", parseInitialCrew, nullptr, offsetof(SiegeEngineContainModuleData, m_initialCrew) },
		{ "CrewAllowedToFire", INI::parseBool, nullptr, offsetof(SiegeEngineContainModuleData, m_crewAllowedToFire) },
		{ "SpeedPercentPerCrew", INI::parsePercentToReal, nullptr, offsetof(SiegeEngineContainModuleData, m_speedPercentPerCrew) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	fields.add(table);
}

void SiegeEngineContainModuleData::parseInitialCrew(INI* ini, void*, void* store, const void*)
{
	auto& crew = *static_cast<InitialPayload*>(store);
	crew.name = ini->getNextToken();
	crew.count = INI::scanInt(ini->getNextToken());
}

SiegeEngineContain::SiegeEngineContain(Thing* thing, const ModuleData* data)
	: HordeTransportContain(thing, data)
{
}

SiegeEngineContain::~SiegeEngineContain() = default;

HordeSiegeEngineContain::HordeSiegeEngineContain(Thing* thing, const ModuleData* data)
	: SiegeEngineContain(thing, data)
{
}

HordeSiegeEngineContain::~HordeSiegeEngineContain() = default;

SiegeEngineContain* SiegeEngineContain::find(const Object* object)
{
	if (!object || !RetailFileLayers::enabled() || !object->areModulesReady())
		return nullptr;
	if (auto* module = object->findUpdateModule(NAMEKEY("SiegeEngineContain")))
		return static_cast<SiegeEngineContain*>(module);
	return static_cast<SiegeEngineContain*>(object->findUpdateModule(NAMEKEY("HordeSiegeEngineContain")));
}

Bool SiegeEngineContain::isCrew(const Object* rider) const
{
	const auto* data = getSiegeEngineContainModuleData();
	return rider && data->m_crewMax > 0 &&
		(data->m_crewFilter.matchesObject(getObject(), rider) ||
		 (!data->m_initialCrew.name.isEmpty() && rider->getTemplate()->getName() == data->m_initialCrew.name));
}

Int SiegeEngineContain::countCrew() const
{
	return static_cast<Int>(std::count_if(getContainList().begin(), getContainList().end(),
		[this](const Object* rider) { return isCrew(rider); }));
}

Int SiegeEngineContain::getContainMax() const
{
	const auto* data = getSiegeEngineContainModuleData();
	return data->m_slotCapacity + data->m_crewMax;
}

Bool SiegeEngineContain::isValidContainerFor(const Object* rider, Bool checkCapacity) const
{
	if (!RetailFileLayers::enabled() || !rider)
		return FALSE;
	const auto* data = getSiegeEngineContainModuleData();
	if (isCrew(rider))
	{
		const Relationship relationship = rider->getRelationship(getObject());
		const Bool relationAllowed = (relationship == ALLIES && data->m_allowAlliesInside) ||
			(relationship == NEUTRAL && data->m_allowNeutralInside) ||
			(relationship == ENEMIES && data->m_allowEnemiesInside) ||
			(data->m_allowOwnPlayerInsideOverride && rider->getControllingPlayer() == getObject()->getControllingPlayer());
		return relationAllowed && (!checkCapacity || countCrew() < data->m_crewMax);
	}
	if (rider->getRelationship(getObject()) == ALLIES)
	{
		const auto* siege = SiegeAIUpdate::find(getObject());
		const auto* portal = DynamicPortalBehaviour::find(getObject());
		Coord3D landing;
		if (siege && siege->evacuatesOnDock() && portal &&
			portal->allowsClimber(*rider) && siege->getReleasePosition(landing))
			return TRUE;
	}
	if (!OpenContain::isValidContainerFor(rider, FALSE))
		return FALSE;
	if (!checkCapacity)
		return TRUE;
	Int usedSlots = 0;
	for (const Object* other : getContainList())
	{
		if (!isCrew(other))
			usedSlots += std::max(1, getPassengerSlotCount(other));
	}
	return usedSlots + std::max(1, getPassengerSlotCount(rider)) <= data->m_slotCapacity;
}

namespace
{
Real segmentLength(const Coord3D& a, const Coord3D& b)
{
	const Real dx = b.x - a.x;
	const Real dy = b.y - a.y;
	const Real dz = b.z - a.z;
	return WWMath::Sqrt(dx * dx + dy * dy + dz * dz);
}

Real polylineLength(const std::vector<Coord3D>& points)
{
	Real total = 0.0f;
	for (size_t i = 1; i < points.size(); ++i)
		total += segmentLength(points[i - 1], points[i]);
	return total;
}

void samplePolyline(const std::vector<Coord3D>& points, Real distance, Real fallbackFacing,
	Coord3D& outPos, Real& outFacing, Int* outSegment)
{
	if (points.empty())
	{
		outPos = {0.0f, 0.0f, 0.0f};
		outFacing = fallbackFacing;
		if (outSegment)
			*outSegment = 0;
		return;
	}
	if (points.size() == 1 || distance <= 0.0f)
	{
		outPos = points.front();
		outFacing = fallbackFacing;
		if (points.size() > 1)
		{
			const Real dx = points[1].x - points[0].x;
			const Real dy = points[1].y - points[0].y;
			if (dx * dx + dy * dy > 0.0001f)
				outFacing = WWMath::Atan2(dy, dx);
		}
		if (outSegment)
			*outSegment = 0;
		return;
	}
	Real remaining = distance;
	for (size_t i = 1; i < points.size(); ++i)
	{
		const Real len = segmentLength(points[i - 1], points[i]);
		const Real dx = points[i].x - points[i - 1].x;
		const Real dy = points[i].y - points[i - 1].y;
		const Real segFacing = (dx * dx + dy * dy > 0.0001f) ? WWMath::Atan2(dy, dx) : fallbackFacing;
		if (remaining <= len || i + 1 == points.size())
		{
			const Real t = len > 0.0f ? std::min(1.0f, remaining / len) : 1.0f;
			outPos.x = points[i - 1].x + dx * t;
			outPos.y = points[i - 1].y + dy * t;
			outPos.z = points[i - 1].z + (points[i].z - points[i - 1].z) * t;
			outFacing = segFacing;
			if (outSegment)
				*outSegment = static_cast<Int>(i - 1);
			return;
		}
		remaining -= len;
	}
}

Real climbSpeedPerFrame(const Object* rider)
{
	auto readSpeed = [](const Object* unit) -> Real
	{
		if (!unit)
			return 0.0f;
		const auto* ai = unit->getAI();
		const auto* loco = ai ? ai->getCurLocomotor() : nullptr;
		const auto* body = unit->getBodyModule();
		if (!loco || !body)
			return 0.0f;
		return loco->getMaxSpeedForCondition(body->getDamageState(), unit);
	};
	Real speed = readSpeed(rider);
	if (speed <= 0.0f)
	{
		if (const auto* horde = HordeContain::get(rider))
		{
			for (const Object* member : *horde->getContainedItemsList())
			{
				if (member && !member->isEffectivelyDead())
				{
					speed = readSpeed(member);
					if (speed > 0.0f)
						break;
				}
			}
		}
	}
	if (speed <= 0.0f)
		speed = PATHFIND_CELL_SIZE_F / LOGICFRAMES_PER_SECONDS_REAL;
	return speed;
}
}

Bool SiegeEngineContain::isClimbing(ObjectID id) const
{
	for (const auto& climber : m_climbers)
		if (climber.riderId == id)
			return TRUE;
	return FALSE;
}

Bool SiegeEngineContain::getClimbProgress(ObjectID id, Real& distance, Int& segmentIndex) const
{
	for (const auto& climber : m_climbers)
	{
		if (climber.riderId != id)
			continue;
		distance = climber.distance;
		segmentIndex = 0;
		const auto* portal = DynamicPortalBehaviour::find(getObject());
		std::vector<Coord3D> waypoints;
		if (portal && portal->getClimbWaypoints(waypoints))
		{
			Coord3D unusedPos;
			Real unusedFacing = 0.0f;
			samplePolyline(waypoints, climber.distance, getObject()->getOrientation(), unusedPos, unusedFacing, &segmentIndex);
		}
		return TRUE;
	}
	return FALSE;
}

Real SiegeEngineContain::crewSpeedScale() const
{
	return getSiegeEngineContainModuleData()->m_speedPercentPerCrew * static_cast<Real>(countCrew());
}

void SiegeEngineContain::releasePassengersOnWall()
{
	if (getContainCount() == countCrew())
		return;
	Coord3D landing;
	const auto* ai = SiegeAIUpdate::find(getObject());
	if (!ai || !ai->getReleasePosition(landing))
		return;
	const auto riders = getContainList();
	for (Object* rider : riders)
		if (rider && !isCrew(rider) && !rider->isEffectivelyDead())
			startClimb(rider, landing);
}

void SiegeEngineContain::exitObjectViaDoor(Object* rider, ExitDoorType door)
{
	if (RetailFileLayers::enabled() && rider && rider->getContainedBy() == getObject() && !getObject()->isEffectivelyDead())
	{
		if (isCrew(rider))
			return;
		Coord3D landing;
		const auto* ai = SiegeAIUpdate::find(getObject());
		if (ai && ai->getReleasePosition(landing))
		{
			startClimb(rider, landing);
			return;
		}
	}
	HordeTransportContain::exitObjectViaDoor(rider, door);
}

void SiegeEngineContain::startClimb(Object* rider, const Coord3D& landing)
{
	if (!rider || isClimbing(rider->getID()))
		return;
	const auto* portal = DynamicPortalBehaviour::find(getObject());
	std::vector<Coord3D> waypoints;
	if (!portal || !portal->getClimbWaypoints(waypoints) || waypoints.size() < 2)
	{
		releaseOnWall(rider, landing);
		return;
	}
	waypoints.back() = landing;
	m_climbers.push_back({rider->getID(), 0.0f});
	positionClimber(rider, waypoints, 0.0f);
	setWakeFrame(getObject(), UPDATE_SLEEP_NONE);
}

void SiegeEngineContain::positionClimber(Object* rider, const std::vector<Coord3D>& waypoints, Real distance) const
{
	Coord3D pos;
	Real facing = getObject()->getOrientation();
	samplePolyline(waypoints, distance, facing, pos, facing, nullptr);
	rider->setPosition(&pos);
	rider->setOrientation(facing);
	if (const auto* horde = HordeContain::get(rider))
	{
		Int index = 0;
		for (Object* member : *horde->getContainedItemsList())
		{
			if (!member || member->isEffectivelyDead())
				continue;
			const Real spacing = member->getGeometryInfo().getMajorRadius() * 2.0f;
			const Real memberDist = std::max(0.0f, distance - static_cast<Real>(index) * spacing);
			Coord3D memberPos;
			Real memberFacing = getObject()->getOrientation();
			samplePolyline(waypoints, memberDist, memberFacing, memberPos, memberFacing, nullptr);
			member->setPosition(&memberPos);
			member->setOrientation(memberFacing);
			++index;
		}
	}
}

void SiegeEngineContain::updateClimbers()
{
	if (m_climbers.empty())
		return;
	const auto* siege = SiegeAIUpdate::find(getObject());
	const auto* portal = DynamicPortalBehaviour::find(getObject());
	Coord3D landing;
	std::vector<Coord3D> waypoints;
	if (getObject()->isEffectivelyDead() || !siege || !siege->isDocked() ||
		!siege->getReleasePosition(landing) || !portal || !portal->getClimbWaypoints(waypoints) ||
		waypoints.size() < 2)
	{
		m_climbers.clear();
		return;
	}
	waypoints.back() = landing;
	const Real totalLength = polylineLength(waypoints);
	for (size_t i = 0; i < m_climbers.size(); )
	{
		Object* rider = TheGameLogic->findObjectByID(m_climbers[i].riderId);
		if (!rider || rider->isEffectivelyDead() || rider->getContainedBy() != getObject())
		{
			m_climbers.erase(m_climbers.begin() + static_cast<ptrdiff_t>(i));
			continue;
		}
		m_climbers[i].distance += climbSpeedPerFrame(rider);
		if (m_climbers[i].distance >= totalLength)
		{
			m_climbers.erase(m_climbers.begin() + static_cast<ptrdiff_t>(i));
			releaseOnWall(rider, landing);
			continue;
		}
		positionClimber(rider, waypoints, m_climbers[i].distance);
		++i;
	}
}

void SiegeEngineContain::releaseOnWall(Object* rider, const Coord3D& landing)
{
	removeFromContain(rider);
	auto place = [this](Object* unit, const Coord3D& position) {
		if (auto* ai = unit->getAI())
		{
			ai->destroyPath();
			ai->setLocomotorGoalNone();
			ai->aiIdle(CMD_FROM_AI);
		}
		if (auto* physics = unit->getPhysics())
			physics->resetDynamicPhysics();
		TheAI->pathfinder()->removeObjectFromPathfindMap(unit);
		unit->setLayer(LAYER_WALL);
		unit->setDestinationLayer(LAYER_WALL);
		unit->setPosition(&position);
		unit->setOrientation(getObject()->getOrientation());
		TheAI->pathfinder()->addObjectToPathfindMap(unit);
	};
	place(rider, landing);
	if (const auto* horde = HordeContain::get(rider))
	{
		for (Object* member : *horde->getContainedItemsList())
		{
			if (!member || member->isEffectivelyDead())
				continue;
			Coord3D position = horde->getMemberPosition(member->getID(), FALSE);
			if (TheAI->pathfinder()->isPointOnWall(&position))
				position.z = TheTerrainLogic->getLayerHeight(position.x, position.y, LAYER_WALL);
			else
				position = landing;
			place(member, position);
		}
	}
}

void SiegeEngineContain::createPayload()
{
	const auto& crew = getSiegeEngineContainModuleData()->m_initialCrew;
	const ThingTemplate* tmpl = crew.name.isEmpty() ? nullptr : TheThingFactory->findTemplate(crew.name, FALSE);
	if (tmpl)
	{
		enableLoadSounds(FALSE);
		for (Int i = 0; i < crew.count && countCrew() < getSiegeEngineContainModuleData()->m_crewMax; ++i)
		{
			if (Object* member = TheThingFactory->newObject(tmpl, getObject()->getTeam()))
#if RTS_GENERALS
				addToContain(member);
#else
				addToContain(member, FALSE);
#endif
		}
		enableLoadSounds(TRUE);
	}
	TransportContain::createPayload();
}

#if RTS_GENERALS
void SiegeEngineContain::onContaining(Object* rider)
{
	HordeTransportContain::onContaining(rider);
#else
void SiegeEngineContain::onContaining(Object* rider, Bool wasSelected)
{
	HordeTransportContain::onContaining(rider, wasSelected);
#endif
	if (isCrew(rider))
		rider->setStatus(getSiegeEngineContainModuleData()->m_crewStatus);
	positionSingleOccupants();
}

void SiegeEngineContain::onRemoving(Object* rider)
{
	if (rider)
	{
		const ObjectID id = rider->getID();
		m_climbers.erase(std::remove_if(m_climbers.begin(), m_climbers.end(),
			[id](const ClimbingRider& c) { return c.riderId == id; }), m_climbers.end());
	}
	HordeTransportContain::onRemoving(rider);
	if (isCrew(rider))
		rider->clearStatus(getSiegeEngineContainModuleData()->m_crewStatus);
}

Bool SiegeEngineContain::isSpecificRiderFreeToExit(Object* rider)
{
	return (getObject()->isEffectivelyDead() || !isCrew(rider)) &&
		TransportContain::isSpecificRiderFreeToExit(rider);
}

#if RTS_GENERALS
Bool SiegeEngineContain::isPassengerAllowedToFire() const
{
	return getSiegeEngineContainModuleData()->m_crewAllowedToFire ||
		HordeTransportContain::isPassengerAllowedToFire();
}
#else
Bool SiegeEngineContain::isPassengerAllowedToFire(ObjectID id) const
{
	const Object* rider = TheGameLogic->findObjectByID(id);
	if (isCrew(rider))
		return getSiegeEngineContainModuleData()->m_crewAllowedToFire;
	return HordeTransportContain::isPassengerAllowedToFire(id);
}
#endif

void SiegeEngineContain::positionSingleOccupants()
{
	const auto* data = getSiegeEngineContainModuleData();
	if (getContainList().empty() || getObject()->isEffectivelyDead())
		return;
	std::vector<Object*> positioned;
	for (const auto& prefix : data->m_passengerBonePrefixes)
	{
		std::vector<Coord3D> positions(getContainCount());
		const Int found = getObject()->getMultiLogicalBonePosition(
			prefix.bonePrefix.str(), static_cast<Int>(positions.size()), positions.data(), nullptr);
		Int index = 0;
		for (Object* rider : getContainList())
		{
			if (index >= found)
				break;
			if (!rider->isAnyKindOf(prefix.kindOf) || HordeContain::get(rider) ||
				isClimbing(rider->getID()) ||
				std::find(positioned.begin(), positioned.end(), rider) != positioned.end())
				continue;
			rider->setPosition(&positions[index++]);
			if (data->m_forceOrientationContainer)
				rider->setOrientation(getObject()->getOrientation());
			positioned.push_back(rider);
		}
	}
}

void SiegeEngineContain::containReactToTransformChange()
{
	HordeTransportContain::containReactToTransformChange();
	positionSingleOccupants();
}

UpdateSleepTime SiegeEngineContain::update()
{
	const UpdateSleepTime sleep = HordeTransportContain::update();
	positionSingleOccupants();
	updateClimbers();
	return m_climbers.empty() ? sleep : UPDATE_SLEEP_NONE;
}

void SiegeEngineContain::xferRetailBlocks(Xfer* xfer, Bool present)
{
	RetailSaveBlocks::transferSection(xfer, xfer->getXferMode() == XFER_LOAD, present,
	{
		{ "siege-climbers", [this](Xfer* x)
		{
			UnsignedInt count = static_cast<UnsignedInt>(m_climbers.size());
			x->xferUnsignedInt(&count);
			if (x->getXferMode() == XFER_LOAD)
				m_climbers.resize(count);
			for (UnsignedInt i = 0; i < count; ++i)
			{
				x->xferObjectID(&m_climbers[i].riderId);
				x->xferReal(&m_climbers[i].distance);
			}
		}, [this]
		{
			m_climbers.clear();
		} }
	});
}

void SiegeEngineContain::crc(Xfer* xfer)
{
	HordeTransportContain::crc(xfer);
	if (RetailFileLayers::enabled())
		xferRetailBlocks(xfer, TRUE);
}

void SiegeEngineContain::xfer(Xfer* xfer)
{
	HordeTransportContain::xfer(xfer);
	if (RetailFileLayers::enabled())
	{
		XferVersion version = 1;
		xfer->xferVersion(&version, 1);
		xferRetailBlocks(xfer, version >= 1);
	}
}

void SiegeEngineContain::loadPostProcess()
{
	HordeTransportContain::loadPostProcess();
	if (!m_climbers.empty())
		setWakeFrame(getObject(), UPDATE_SLEEP_NONE);
}

void HordeSiegeEngineContain::crc(Xfer* xfer)
{
	SiegeEngineContain::crc(xfer);
}

void HordeSiegeEngineContain::xfer(Xfer* xfer)
{
	SiegeEngineContain::xfer(xfer);
}

void HordeSiegeEngineContain::loadPostProcess()
{
	SiegeEngineContain::loadPostProcess();
}
