#include "PreRTS.h"
#include "GameLogic/Module/RetailSiegeContain.h"

#include "Common/RetailFileLayers.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Module/RetailGate.h"
#include "GameLogic/Module/RetailSiegeDocking.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"

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
			releaseOnWall(rider, landing);
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
			releaseOnWall(rider, landing);
			return;
		}
	}
	HordeTransportContain::exitObjectViaDoor(rider, door);
}

void SiegeEngineContain::releaseOnWall(Object* rider, const Coord3D& landing)
{
	removeFromContain(rider);
	auto place = [this](Object* unit, const Coord3D& position) {
		if (auto* physics = unit->getPhysics())
			physics->resetDynamicPhysics();
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
	return sleep;
}

void SiegeEngineContain::crc(Xfer* xfer)
{
	HordeTransportContain::crc(xfer);
}

void SiegeEngineContain::xfer(Xfer* xfer)
{
	HordeTransportContain::xfer(xfer);
}

void SiegeEngineContain::loadPostProcess()
{
	HordeTransportContain::loadPostProcess();
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
