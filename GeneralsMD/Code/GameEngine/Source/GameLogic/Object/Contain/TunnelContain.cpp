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

// FILE: TunnelContain.cpp ////////////////////////////////////////////////////////////////////////////
// Author: Graham Smallwood, March 2002
// Desc:   A version of OpenContain that overrides where the passengers are stored: the Owning Player's
//					TunnelTracker.  All queries about capacity and contents are also redirected.
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/GameState.h"
#include "Common/Player.h"
#include "Common/RandomValue.h"
#include "Common/RetailFileLayers.h"
#include "Common/RetailSaveBlocks.h"
#include "Common/RotwkDuration.h"
#include "Common/ThingTemplate.h"
#include "Common/TunnelTracker.h"
#include "Common/Xfer.h"
#include "GameClient/Drawable.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/LogicRandomValue.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Module/OpenContain.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Module/TunnelContain.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/TerrainLogic.h"


///////////////////////////////////////////////////////////////////////////////////////////////////
// PUBLIC FUNCTIONS ///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
TunnelContain::TunnelContain( Thing *thing, const ModuleData* moduleData ) : OpenContain( thing, moduleData )
{
	m_needToRunOnBuildComplete = true;
	m_isCurrentlyRegistered = FALSE;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
TunnelContain::~TunnelContain()
{
}

const TunnelContain* TunnelContain::find(const Object* object)
{
	const ContainModuleInterface* contain = object ? object->getContain() : nullptr;
	return contain && contain->isTunnelContain() ? static_cast<const TunnelContain*>(contain) : nullptr;
}

Coord3D TunnelContain::toWorld(const Coord3D& offset) const
{
	Vector3 world;
	Matrix3D::Transform_Vector(*getObject()->getTransformMatrix(), Vector3(offset.x, offset.y, offset.z), &world);
	return {world.X, world.Y, world.Z};
}

Coord3D TunnelContain::getEntryPosition() const
{
	return toWorld(getTunnelContainModuleData()->m_entryPosition);
}

Coord3D TunnelContain::getEntryOffset() const
{
	return toWorld(getTunnelContainModuleData()->m_entryOffset);
}

Bool TunnelContain::canReachEntrance(const Object* passenger) const
{
	const AIUpdateInterface* ai = passenger ? passenger->getAI() : nullptr;
	if (!ai)
		return FALSE;

	const Coord3D approach = getEntryOffset();
	Pathfinder* pathfinder = TheAI->pathfinder();
	const Coord3D* source = passenger->getPosition();
	const PathfindCell* sourceCell = pathfinder->getCell(passenger->getLayer(), source);
	const auto reachable = [&](const Coord3D& position) -> Bool {
		const PathfindLayerEnum layer = TheTerrainLogic->getLayerForDestination(&position);
		const PathfindCell* destination = pathfinder->getCell(layer, &position);
		if (!destination)
			return FALSE;
		if (destination->getType() == PathfindCell::CELL_OBSTACLE ||
			(sourceCell && sourceCell->getType() == PathfindCell::CELL_OBSTACLE))
			return pathfinder->clientSafeQuickDoesPathExistForUI(ai->getLocomotorSet(), source, &position);
		return pathfinder->clientSafeQuickDoesPathExist(ai->getLocomotorSet(), source, &position);
	};
	if (reachable(approach))
		return TRUE;

	const Object* tunnel = getObject();
	const Coord3D& center = *tunnel->getPosition();
	const PathfindCell* cell = pathfinder->getCell(tunnel->getLayer(), &center);
	if (!cell || cell->getType() != PathfindCell::CELL_OBSTACLE)
		return reachable(center);

	const Real radius = tunnel->getGeometryInfo().getBoundingCircleRadius();
	const Int edge = static_cast<Int>(radius / PATHFIND_CELL_SIZE_F) + 2;
	const auto reachableEdge = [&](Real dx, Real dy) -> Bool {
		for (Int step = 1; step <= edge; ++step)
		{
			const Coord3D point{center.x + dx * step, center.y + dy * step, center.z};
			const PathfindCell* edgeCell = pathfinder->getCell(tunnel->getLayer(), &point);
			if (!edgeCell || edgeCell->getType() != PathfindCell::CELL_OBSTACLE)
				return reachable(point);
		}
		return FALSE;
	};
	return reachableEdge(PATHFIND_CELL_SIZE_F, 0.0f) ||
		reachableEdge(-PATHFIND_CELL_SIZE_F, 0.0f) ||
		reachableEdge(0.0f, PATHFIND_CELL_SIZE_F) ||
		reachableEdge(0.0f, -PATHFIND_CELL_SIZE_F);
}

Bool TunnelContain::isExitBusy() const
{
	return TheGameLogic->getFrame() < m_nextExitFrame;
}

Bool TunnelContain::getNaturalRallyPoint(Coord3D& position, Bool offset) const
{
	if (!RetailFileLayers::enabled())
		return OpenContain::getNaturalRallyPoint(position, offset);
	position = toWorld(getTunnelContainModuleData()->m_exitOffset);
	return TRUE;
}

void TunnelContain::exitObjectViaDoor(Object* obj, ExitDoorType exitDoor)
{
	if (RetailFileLayers::enabled())
		exitToOffset(obj, FALSE);
	else
		OpenContain::exitObjectViaDoor(obj, exitDoor);
}

void TunnelContain::exitObjectInAHurry(Object* obj)
{
	if (RetailFileLayers::enabled())
		exitToOffset(obj, TRUE);
	else
		OpenContain::exitObjectInAHurry(obj);
}

void TunnelContain::exitToOffset(Object* obj, Bool quick)
{
	if (!obj || !isContained(obj))
		return;

	removeFromContain(obj);
	if (obj->getContainedBy())
		return;

	const Object* tunnel = getObject();
	const Real angle = tunnel->getOrientation();
	AIUpdateInterface* ai = obj->getAI();
	Pathfinder* pathfinder = TheAI->pathfinder();
	Coord3D start = getEntryPosition();
	if (const Locomotor* locomotor = ai ? ai->getCurLocomotor() : nullptr)
	{
		const PathfindLayerEnum layer = tunnel->getLayer();
		if (!pathfinder->validMovementTerrain(layer, locomotor, &start))
		{
			const Real radius = tunnel->getGeometryInfo().getMajorRadius();
			const Real dx = radius * Cos(angle);
			const Real dy = radius * Sin(angle);
			start.x -= dx;
			start.y -= dy;
			if (!pathfinder->validMovementTerrain(layer, locomotor, &start))
			{
				start.x += dx + dx;
				start.y += dy + dy;
				if (!pathfinder->validMovementTerrain(layer, locomotor, &start))
					start = *tunnel->getPosition();
			}
		}
	}
	obj->setPosition(&start);
	obj->setOrientation(angle);
	obj->setLayer(tunnel->getLayer());
	pathfinder->addObjectToPathfindMap(obj);
	if (!ai)
		return;

	Coord3D end = toWorld(getTunnelContainModuleData()->m_exitOffset);
	pathfinder->adjustToPossibleDestination(obj, ai->getLocomotorSet(), &end);
	std::vector<Coord3D> path{end, end};
	if (const Coord3D* rally = getRallyPoint())
		path.back() = *rally;

	if (quick)
		ai->doQuickExit(&path);
	else
		ai->aiFollowPath(&path, getObject(), CMD_FROM_AI);
	pathfinder->updateGoal(obj, &end, TheTerrainLogic->getLayerForDestination(&end));
}

Bool TunnelContain::getContainerPipsToShow(Int& numTotal, Int& numFull)
{
	if (getTunnelContainModuleData()->m_showPips)
		return OpenContain::getContainerPipsToShow(numTotal, numFull);
	numTotal = numFull = 0;
	return FALSE;
}

Bool TunnelContain::isEnclosingContainerFor(const Object*) const
{
	return !RetailFileLayers::enabled() ||
		getTunnelContainModuleData()->m_objectStatusOfContained.test(OBJECT_STATUS_ENCLOSED);
}

void TunnelContain::addToContain(Object* obj)
{
	if (!RetailFileLayers::enabled())
		OpenContain::addToContain(obj);
	else if (isValidContainerFor(obj, TRUE))
		OpenContain::addToContain(obj, FALSE);
}

void TunnelContain::addToContainList( Object *obj )
{
	Player *owningPlayer = getObject()->getControllingPlayer();

	if(!owningPlayer->getTunnelSystem())
		return;

	owningPlayer->getTunnelSystem()->addToContainList( obj );
}

//-------------------------------------------------------------------------------------------------
/** Remove 'obj' from the m_containList of objects in this module.
	* This will trigger an onRemoving event for the object that this module
	* is a part of and an onRemovedFrom event for the object being removed */
//-------------------------------------------------------------------------------------------------
void TunnelContain::removeFromContain( Object *obj, Bool exposeStealthUnits )
{
	if (RetailFileLayers::enabled())
	{
		const TunnelContain* source = obj ? find(obj->getContainedBy()) : nullptr;
		Player* owner = getObject()->getControllingPlayer();
		TunnelTracker* tracker = owner ? owner->getTunnelSystem() : nullptr;
		if (!source || !tracker || source->getObject()->getControllingPlayer() != owner)
			return;
		tracker->removeFromContain(obj, exposeStealthUnits);
		onRemoving(obj);
		obj->onRemovedFrom(getObject());
		return;
	}

	// sanity
	if( obj == nullptr )
		return;

	// trigger an onRemoving event for 'm_object' no longer containing 'itemToRemove->m_object'
	if( getObject()->getContain() )
	{
		getObject()->getContain()->onRemoving( obj );
	}

	// trigger an onRemovedFrom event for 'remove'
	obj->onRemovedFrom( getObject() );

	//
	// we can only remove this object from the contains list of this module if
	// it is actually contained by this module
	//
	Player *owningPlayer = getObject()->getControllingPlayer();
	if( owningPlayer == nullptr )
		return; //game tear down.  We do the onRemove* stuff first because this is allowed to fail but that still needs to be done

	if(!owningPlayer->getTunnelSystem())
		return;

	owningPlayer->getTunnelSystem()->removeFromContain( obj, exposeStealthUnits );
}



//--------------------------------------------------------------------------------------------------------
/** Force all contained objects in the contained list to exit, and kick them in the pants on the way out*/
//--------------------------------------------------------------------------------------------------------
void TunnelContain::harmAndForceExitAllContained( DamageInfo *info )
{
	Player *owningPlayer = getObject()->getControllingPlayer();

	if(!owningPlayer->getTunnelSystem())
		return;

	const ContainedItemsList *fullList = owningPlayer->getTunnelSystem()->getContainedItemsList();

	Object *obj;
	ContainedItemsList::const_iterator it;

	//Kris: Patch 1.01 -- November 6, 2003
	//No longer advances the iterator and saves it. The iterator is fetched from the beginning after
	//each loop. This is to prevent a crash where dropping a bunker buster on a tunnel network containing
	//multiple units (if they have the suicide bomb upgrade - demo general). In this case, multiple bunker
	//busters would hit the tunnel network in close succession. Missile #1 would iterate the list, killing
	//infantry #1. Infantry #1 would explode and destroy Missile #2. Missile #2 would start iterating the
	//same list, killing the remaining units. When Missile #1 picked up and continued processing the list
	//it would crash because it's iterator was deleted from under it.
	it = (*fullList).begin();
	while( it != (*fullList).end() )
	{
		obj = *it;
		removeFromContain( obj, true );
    obj->attemptDamage( info );
		it = (*fullList).begin();
	}

}


//-------------------------------------------------------------------------------------------------
/** Remove all contained objects from the contained list */
//-------------------------------------------------------------------------------------------------
void TunnelContain::killAllContained()
{
	// TheSuperHackers @bugfix xezon 04/06/2025 Empty the TunnelSystem's Contained Items List
	// straight away to prevent a potential child call to catastrophically modify it as well.
	// This scenario can happen if the killed occupant(s) apply deadly damage on death
	// to the host container, which then attempts to remove all remaining occupants
	// on the death of the host container. This is reproducible by shooting with
	// Neutron Shells on a GLA Tunnel containing GLA Terrorists.

	Player *owningPlayer = getObject()->getControllingPlayer();

	if(!owningPlayer->getTunnelSystem())
		return;

	ContainedItemsList list;
	owningPlayer->getTunnelSystem()->swapContainedItemsList(list);

	ContainedItemsList::iterator it = list.begin();

	while ( it != list.end() )
	{
		Object *obj = *it++;
		DEBUG_ASSERTCRASH( obj, ("Contain list must not contain null element"));

		removeFromContain( obj, true );

		obj->kill();
	}
}
//-------------------------------------------------------------------------------------------------
/** Remove all contained objects from the contained list */
//-------------------------------------------------------------------------------------------------
void TunnelContain::removeAllContained( Bool exposeStealthUnits )
{
	Player *owningPlayer = getObject()->getControllingPlayer();

	if(!owningPlayer->getTunnelSystem())
		return;

	ContainedItemsList list;
	owningPlayer->getTunnelSystem()->swapContainedItemsList(list);

	ContainedItemsList::iterator it = list.begin();

	while ( it != list.end() )
	{
		Object *obj = *it++;
		DEBUG_ASSERTCRASH( obj, ("Contain list must not contain null element"));

		removeFromContain( obj, exposeStealthUnits );
	}
}

//-------------------------------------------------------------------------------------------------
/** Iterate the contained list and call the callback on each of the objects */
//-------------------------------------------------------------------------------------------------
void TunnelContain::iterateContained( ContainIterateFunc func, void *userData, Bool reverse )
{
	Player *owningPlayer = getObject()->getControllingPlayer();

	if(!owningPlayer->getTunnelSystem())
		return;

	owningPlayer->getTunnelSystem()->iterateContained( func, userData, reverse );
}

//-------------------------------------------------------------------------------------------------
void TunnelContain::onContaining( Object *obj, Bool wasSelected )
{
	OpenContain::onContaining( obj, wasSelected );

	if (RetailFileLayers::enabled())
		obj->setPosition(getObject()->getPosition());

	// objects inside a building are held
	obj->setDisabled( DISABLED_HELD );

	obj->getControllingPlayer()->getAcademyStats()->recordUnitEnteredTunnelNetwork();




  obj->handlePartitionCellMaintenance();


}

//-------------------------------------------------------------------------------------------------
void TunnelContain::onRemoving( Object *obj )
{
	if (RetailFileLayers::enabled())
	{
		const TunnelContain* source = find(obj->getContainedBy());
		const Bool enclosed = source && source->isEnclosingContainerFor(obj);
		if (source)
			const_cast<TunnelContain*>(source)->OpenContain::onRemoving(obj);
		obj->clearDisabled(DISABLED_HELD);
		obj->setPosition(getObject()->getPosition());
		obj->setSafeOcclusionFrame(TheGameLogic->getFrame() + obj->getTemplate()->getOcclusionDelay());
		if (enclosed)
			addOrRemoveObjFromWorld(obj, TRUE);
		m_nextExitFrame = TheGameLogic->getFrame() + getTunnelContainModuleData()->m_exitDelay;
		return;
	}

	OpenContain::onRemoving(obj);

	// object is no longer held inside a garrisoned building
	obj->clearDisabled( DISABLED_HELD );

	/// place the object in the world at position of the container m_object
#if RETAIL_COMPATIBLE_CRC
	ThePartitionManager->registerObject( obj );
	obj->setPosition( getObject()->getPosition() );
	if( obj->getDrawable() )
	{
		obj->setSafeOcclusionFrame(TheGameLogic->getFrame()+obj->getTemplate()->getOcclusionDelay());
		obj->getDrawable()->setDrawableHidden( false );
	}
#else
	// TheSuperHackers @bugfix Now correctly adds the objects to the world without issues with shrouded portable structures.
	obj->setPosition(getObject()->getPosition());
	obj->setSafeOcclusionFrame(TheGameLogic->getFrame() + obj->getTemplate()->getOcclusionDelay());
	addOrRemoveObjFromWorld(obj, TRUE);
#endif

	doUnloadSound();
}

//-------------------------------------------------------------------------------------------------
void TunnelContain::onSelling()
{
	// A TunnelContain tells everyone to leave if this is the last tunnel
	Player *owningPlayer = getObject()->getControllingPlayer();
	if( owningPlayer == nullptr )
		return;
	TunnelTracker *tunnelTracker = owningPlayer->getTunnelSystem();
	if( tunnelTracker == nullptr )
		return;

	// We are the last tunnel, so kick everyone out.  This makes tunnels act like Palace and Bunker
	// rather than killing the occupants as if the last tunnel died.
	if( tunnelTracker->friend_getTunnelCount() == 1 )
		removeAllContained(FALSE);// Can't be order to exit, as I have no time to organize their exits.
	// If they don't go right now, I will delete them in a moment

	// Unregister after the kick out, or else the unregistering will activate a cavein-kill.
	// We need to do this in case someone sells their last two tunnels at the same time.
	if( m_isCurrentlyRegistered )
	{
		tunnelTracker->onTunnelDestroyed( getObject() );
		m_isCurrentlyRegistered = FALSE;
	}
}

//-------------------------------------------------------------------------------------------------
// GeneralsX @bugfix Okladnoj 28/08/2026 Import shared tunnel-network passenger lookup from upstream PR #3136.
// https://github.com/TheSuperHackers/GeneralsGameCode/pull/3136
// A whole network shares one passenger list, so a passenger is contained by
// the endpoint it entered and not by the one that was ordered to unload.
Bool TunnelContain::isContained( const Object *obj ) const
{
	if (OpenContain::isContained(obj))
		return TRUE;

	const ContainedItemsList *items = getContainedItemsList();
	return items != nullptr && std::find(items->begin(), items->end(), obj) != items->end();
}

//-------------------------------------------------------------------------------------------------
Bool TunnelContain::isValidContainerFor(const Object* obj, Bool checkCapacity) const
{
	Player *owningPlayer = getObject()->getControllingPlayer();
	if (!owningPlayer || !owningPlayer->getTunnelSystem())
		return FALSE;
	if (RetailFileLayers::enabled() && (!m_isCurrentlyRegistered || getObject()->isEffectivelyDead() ||
		!OpenContain::isValidContainerFor(obj, checkCapacity)))
		return FALSE;
	return owningPlayer->getTunnelSystem()->isValidContainerFor(obj, checkCapacity);
}

UnsignedInt TunnelContain::getContainCount() const
{
	Player *owningPlayer = getObject()->getControllingPlayer();
	if( owningPlayer && owningPlayer->getTunnelSystem() )
	{
		return owningPlayer->getTunnelSystem()->getContainCount();
	}
	return 0;
}

UnsignedInt TunnelContain::getHeroUnitsContained() const
{
	Player *owningPlayer = getObject()->getControllingPlayer();
	if( owningPlayer && owningPlayer->getTunnelSystem() )
	{
		return owningPlayer->getTunnelSystem()->getHeroUnitsContained();
	}
	return 0;
}

Int TunnelContain::getContainMax() const
{
	Player *owningPlayer = getObject()->getControllingPlayer();
	if( owningPlayer && owningPlayer->getTunnelSystem() )
	{
		return owningPlayer->getTunnelSystem()->getContainMax();
	}
	return 0;
}

const ContainedItemsList* TunnelContain::getContainedItemsList() const
{
	Player *owningPlayer = getObject()->getControllingPlayer();
	if( owningPlayer && owningPlayer->getTunnelSystem() )
	{
		return owningPlayer->getTunnelSystem()->getContainedItemsList();
	}
	return nullptr;
}

UnsignedInt TunnelContain::getFullTimeForHeal() const
{
	const TunnelContainModuleData* modData = getTunnelContainModuleData();
	return modData->m_framesForFullHeal;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// PRIVATE FUNCTIONS //////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////


//-------------------------------------------------------------------------------------------------
void TunnelContain::scatterToNearbyPosition(Object* obj)
{
	Object *theContainer = getObject();

	//
	// for now we will just set the position of the object that is being removed from us
	// at a random angle away from our center out some distance
	//

	//
	// pick an angle that is in the view of the current camera position so that
	// the thing will come out "toward" the player and they can see it
	// NOPE, can't do that ... all players screen angles will be different, unless
	// we maintain the angle of each players screen in the player structure or something
	//
	Real angle = GameLogicRandomValueReal( 0.0f, 2.0f * PI );
//	angle = TheTacticalView->getAngle();
//	angle -= GameLogicRandomValueReal( PI / 3.0f, 2.0f * (PI / 3.0F) );

	Real minRadius = theContainer->getGeometryInfo().getBoundingCircleRadius();
	Real maxRadius = minRadius + minRadius / 2.0f;
	const Coord3D *containerPos = theContainer->getPosition();
	Real dist = GameLogicRandomValueReal( minRadius, maxRadius );

	Coord3D pos;
	pos.x = dist * Cos( angle ) + containerPos->x;
	pos.y = dist * Sin( angle ) + containerPos->y;
	pos.z = TheTerrainLogic->getGroundHeight( pos.x, pos.y );

	// set orientation
	obj->setOrientation( angle );

	AIUpdateInterface *ai = obj->getAIUpdateInterface();
	if( ai )
	{
		// set position of the object at center of building and move them toward pos
		obj->setPosition( theContainer->getPosition() );
		ai->ignoreObstacle(theContainer);
 		ai->aiMoveToPosition( &pos, CMD_FROM_AI );

	}
	else
	{

		// no ai, just set position at the target pos
		obj->setPosition( &pos );

	}
}

//-------------------------------------------------------------------------------------------------
/** The die callback. */
//-------------------------------------------------------------------------------------------------
void TunnelContain::onDie(const DamageInfo* damageInfo)
{
	if (getTunnelContainModuleData()->m_dieMuxData.isDieApplicable(getObject(), damageInfo))
		unregisterTunnel(TRUE);
}

//-------------------------------------------------------------------------------------------------
void TunnelContain::onDelete()
{
	unregisterTunnel(FALSE);
}

void TunnelContain::rebindPassenger(Object* passenger, const Object* former, Object* replacement)
{
	const TunnelContain* previous = find(former);
	auto* next = const_cast<TunnelContain*>(find(replacement));
	if (RetailFileLayers::enabled() && previous && next)
	{
		const auto& incoming = next->getTunnelContainModuleData()->m_objectStatusOfContained;
		ObjectStatusMaskType outgoing = previous->getTunnelContainModuleData()->m_objectStatusOfContained;
		outgoing.clear(incoming);
		passenger->clearStatus(outgoing);
		passenger->setStatus(incoming);
		if (const HordeContain* horde = HordeContain::get(passenger))
		{
			outgoing.clear(horde->getActiveFormation()->m_objectStatusOfContained);
			outgoing.set(OBJECT_STATUS_UNSELECTABLE, FALSE);
			for (Object* member : *horde->getContainedItemsList())
			{
				member->clearStatus(outgoing);
				member->setStatus(incoming);
			}
		}

		const Bool enclosed = next->isEnclosingContainerFor(passenger);
		if (enclosed != previous->isEnclosingContainerFor(passenger))
			next->addOrRemoveObjFromWorld(passenger, !enclosed);
	}
	passenger->onContainedBy(replacement);
}

void TunnelContain::unregisterTunnel(Bool died)
{
	if (!m_isCurrentlyRegistered)
		return;
	Player* owner = getObject()->getControllingPlayer();
	TunnelTracker* tracker = owner ? owner->getTunnelSystem() : nullptr;
	if (!tracker)
		return;

	m_isCurrentlyRegistered = FALSE;
	ContainedItemsList passengers;
	if (RetailFileLayers::enabled() && tracker->friend_getTunnelCount() == 1)
		tracker->swapContainedItemsList(passengers);
	tracker->onTunnelDestroyed(getObject());

	const auto* data = getTunnelContainModuleData();
	const Bool kill = died && (data->m_killPassengersOnDeath || !data->m_ejectPassengersOnDeath);
	for (Object* passenger : passengers)
	{
		const Bool destroy = !data->m_ejectPassengersOnDeath || passenger->testStatus(OBJECT_STATUS_ENCLOSED);
		const Coord3D previousPosition = *passenger->getPosition();
		removeFromContain(passenger);
		if (!kill || passenger->isEffectivelyDead())
			continue;

		const auto killRider = [destroy](Object* rider) {
			if (destroy)
			{
				TheGameLogic->destroyObject(rider);
				return;
			}
			if (PhysicsBehavior* physics = rider->getPhysics())
			{
				const Real speed = GameLogicRandomValue(2, 5) * RotwkDuration::FRAMES_PER_SECOND;
				physics->launchFromImpact(*rider->getUnitDirectionVector2D(), speed, 0.25f, TRUE);
			}
			rider->kill();
		};
		if (!destroy)
			passenger->setPosition(&previousPosition);
		if (const HordeContain* horde = HordeContain::get(passenger))
		{
			const ContainedItemsList members = *horde->getContainedItemsList();
			for (Object* member : members)
				killRider(member);
		}
		if (!passenger->isEffectivelyDead())
			killRider(passenger);
	}
}

//-------------------------------------------------------------------------------------------------
void TunnelContain::onCreate()
{
}

//-------------------------------------------------------------------------------------------------
void TunnelContain::onObjectCreated()
{
	//Kris: July 29, 2003
	//Added this function to support the sneak attack (which doesn't call onBuildComplete).
	if( ! shouldDoOnBuildComplete() )
		return;

	m_needToRunOnBuildComplete = false;

	Player *owningPlayer = getObject()->getControllingPlayer();
	if( owningPlayer == nullptr )
		return;
	TunnelTracker *tunnelTracker = owningPlayer->getTunnelSystem();
	if( tunnelTracker == nullptr )
		return;

	tunnelTracker->onTunnelCreated( getObject() );
	m_isCurrentlyRegistered = TRUE;
}

//-------------------------------------------------------------------------------------------------
void TunnelContain::onBuildComplete()
{
	//Kris: July 29, 2003
	//Obsolete -- onObjectCreated handles it before this function gets called.
	/*
	if( ! shouldDoOnBuildComplete() )
		return;

	m_needToRunOnBuildComplete = false;

	Player *owningPlayer = getObject()->getControllingPlayer();
	if( owningPlayer == nullptr )
		return;
	TunnelTracker *tunnelTracker = owningPlayer->getTunnelSystem();
	if( tunnelTracker == nullptr )
		return;

	tunnelTracker->onTunnelCreated( getObject() );
	m_isCurrentlyRegistered = TRUE;
	*/
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
#if !(RTS_GENERALS && RETAIL_COMPATIBLE_CRC)
void TunnelContain::onCapture( Player *oldOwner, Player *newOwner )
{
	if( m_isCurrentlyRegistered )
	{
		TunnelTracker *oldTunnelTracker = oldOwner->getTunnelSystem();
		if( oldTunnelTracker )
		{
			DEBUG_ASSERTCRASH( oldTunnelTracker->getContainCount() == 0, ("You shouldn't force a capture of a Tunnel with people in it. Future ExitFromContainer scripts will fail."));
			oldTunnelTracker->onTunnelDestroyed(getObject());
		}

		TunnelTracker *newTunnelTracker = newOwner->getTunnelSystem();
		if( newTunnelTracker )
		{
			newTunnelTracker->onTunnelCreated(getObject());
		}
	}

	// extend base class
	OpenContain::onCapture( oldOwner, newOwner );
}
#endif

//-------------------------------------------------------------------------------------------------
void TunnelContain::orderAllPassengersToExit( CommandSourceType commandSource, Bool instantly )
{
	Player *owningPlayer = getObject()->getControllingPlayer();
	if( !owningPlayer || !owningPlayer->getTunnelSystem() )
		return;

	OpenContain::orderAllPassengersToExit( commandSource, instantly );
}

//-------------------------------------------------------------------------------------------------
void TunnelContain::orderAllPassengersToIdle( CommandSourceType commandSource )
{
	Player *owningPlayer = getObject()->getControllingPlayer();
	if( !owningPlayer || !owningPlayer->getTunnelSystem() )
		return;

	OpenContain::orderAllPassengersToIdle( commandSource );
}

// ------------------------------------------------------------------------------------------------
/** Per frame update */
// ------------------------------------------------------------------------------------------------
UpdateSleepTime TunnelContain::update()
{
	// extending functionality to heal the units within the tunnel system
	OpenContain::update();
	if (RetailFileLayers::enabled())
		updatePassengerBones();

	Object *obj = getObject();
	Player *controllingPlayer = nullptr;
	if (obj)
	{
		controllingPlayer = obj->getControllingPlayer();
	}
	if (controllingPlayer)
	{
		TunnelTracker *tunnelSystem = controllingPlayer->getTunnelSystem();
#if PRESERVE_TUNNEL_HEAL_STACKING || RETAIL_COMPATIBLE_CRC
		if (tunnelSystem)
		{
			const TunnelContainModuleData* modData = getTunnelContainModuleData();
			tunnelSystem->healObjects(modData->m_framesForFullHeal);
		}
#endif

		// check for attacked.
		BodyModuleInterface *body = obj->getBodyModule();
		if (body) {
			const DamageInfo *info = body->getLastDamageInfo();
			if (info) {
				const UnsignedInt *lastDamageTimestamp = body->getLastDamageTimestamp();
				if (lastDamageTimestamp && *lastDamageTimestamp + LOGICFRAMES_PER_SECOND > TheGameLogic->getFrame()) {
					// winner.
					ObjectID attackerID = info->in.m_sourceID;
					Object *attacker = TheGameLogic->findObjectByID(attackerID);
					if( attacker )
					{
						if (tunnelSystem && obj->getRelationship(attacker) == ENEMIES) {
							tunnelSystem->updateNemesis(attacker);
						}
					}
				}
			}
		}
	}
	return UPDATE_SLEEP_NONE;

}

Bool TunnelContain::getPassengerBonePosition(ObjectID passenger, Coord3D& position) const
{
	for (const auto& bone : m_passengerBones)
		if (bone.passenger == passenger)
			return getObject()->getSingleLogicalBonePosition(bone.name.str(), &position, nullptr);
	return FALSE;
}

void TunnelContain::positionPassengerAtBone(Object* passenger)
{
	if (passenger->isEffectivelyDead())
		return;
	const ObjectID id = passenger->getID();
	const AIUpdateInterface* ai = passenger->getAI();
	const Object* victim = ai ? ai->getCurrentVictim() : nullptr;
	const Bool enclosed = passenger->testStatus(OBJECT_STATUS_ENCLOSED);
	if (enclosed && (!passenger->testStatus(OBJECT_STATUS_CAN_ATTACK) || !victim))
	{
		if (std::erase_if(m_passengerBones, [id](const PassengerBone& bone) { return bone.passenger == id; }))
			passenger->setPosition(getObject()->getPosition());
		return;
	}

	Coord3D position;
	const Bool hasBone = getPassengerBonePosition(id, position);
	if (hasBone && !victim)
	{
		const Coord3D& current = *passenger->getPosition();
		if (position.x != current.x || position.y != current.y || position.z != current.z)
			passenger->setPosition(&position);
		return;
	}

	const Object* tunnel = getObject();
	Real bestDistance = 0.0f;
	AsciiString bestBone;
	Coord3D bestPosition{};
	const Coord3D& target = victim ? *victim->getPosition() : *passenger->getPosition();
	const auto distanceTo = [&target](const Coord3D& pivot) {
		const Real dx = pivot.x - target.x;
		const Real dy = pivot.y - target.y;
		return dx * dx + dy * dy;
	};
	auto assigned = std::find_if(m_passengerBones.begin(), m_passengerBones.end(),
		[id](const PassengerBone& bone) { return bone.passenger == id; });
	if (hasBone)
	{
		bestBone = assigned->name;
		bestPosition = position;
		bestDistance = distanceTo(position);
	}
	const auto considerBone = [&](const AsciiString& name) {
		for (const auto& bone : m_passengerBones)
			if (bone.passenger != id && stricmp(name.str(), bone.name.str()) == 0)
				return;
		Coord3D pivot;
		if (!tunnel->getSingleLogicalBonePosition(name.str(), &pivot, nullptr))
			return;
		const Real distance = distanceTo(pivot);
		if (bestBone.isEmpty() || distance < bestDistance)
		{
			bestBone = name;
			bestPosition = pivot;
			bestDistance = distance;
		}
	};

	for (const auto& prefix : getTunnelContainModuleData()->m_passengerBonePrefixes)
	{
		if (!passenger->isAnyKindOf(prefix.kindOf))
			continue;
		const Int count = tunnel->getMultiLogicalBonePosition(prefix.bonePrefix.str(), MAX_BONES_PER_PREFIX, nullptr, nullptr, FALSE);
		if (count == 0)
			considerBone(prefix.bonePrefix);
		for (Int slot = 1; slot <= count; ++slot)
		{
			AsciiString name;
			name.format("%s%02d", prefix.bonePrefix.str(), slot);
			considerBone(name);
		}
	}
	if (bestBone.isNotEmpty())
	{
		if (assigned == m_passengerBones.end())
			m_passengerBones.push_back({id, bestBone});
		else
			assigned->name = bestBone;
		const Coord3D& current = *passenger->getPosition();
		if (bestPosition.x != current.x || bestPosition.y != current.y || bestPosition.z != current.z)
			passenger->setPosition(&bestPosition);
	}
	else if (assigned != m_passengerBones.end())
		m_passengerBones.erase(assigned);
}

void TunnelContain::updatePassengerBones()
{
	if (getTunnelContainModuleData()->m_passengerBonePrefixes.empty())
		return;
	const Object* tunnel = getObject();
	std::erase_if(m_passengerBones, [tunnel](const PassengerBone& bone) {
		const Object* passenger = TheGameLogic->findObjectByID(bone.passenger);
		if (!passenger || passenger->isEffectivelyDead())
			return true;
		const Object* anchor = passenger->getContainedBy();
		return anchor != tunnel && (!anchor || anchor->getContainedBy() != tunnel);
	});

	const ContainedItemsList* passengers = getContainedItemsList();
	if (!passengers)
		return;
	for (Object* passenger : *passengers)
	{
		if (passenger->getContainedBy() != tunnel)
			continue;
		if (const HordeContain* horde = HordeContain::get(passenger))
		{
			for (Object* member : *horde->getContainedItemsList())
				positionPassengerAtBone(member);
		}
		else
			positionPassengerAtBone(passenger);
	}
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void TunnelContain::crc( Xfer *xfer )
{
	OpenContain::crc(xfer);
	if (RetailFileLayers::enabled())
		xferRetailBlocks(xfer, TRUE);
}

void TunnelContain::xferRetailBlocks(Xfer* xfer, Bool present)
{
	const Bool loading = xfer->getXferMode() == XFER_LOAD;
	RetailSaveBlocks::transferSection<Xfer>(xfer, loading, present, {
		{"exit-delay", [this](Xfer* block) {
			block->xferUnsignedInt(&m_nextExitFrame);
		}, [this] { m_nextExitFrame = 0; }},
		{"passenger-bones", [this, loading](Xfer* block) {
			UnsignedInt count = static_cast<UnsignedInt>(m_passengerBones.size());
			block->xferUnsignedInt(&count);
			if (loading)
			{
				if (count > getTunnelContainModuleData()->m_passengerBonePrefixes.size() * MAX_BONES_PER_PREFIX)
					throw SC_INVALID_DATA;
				m_passengerBones.resize(count);
			}
			for (auto& bone : m_passengerBones)
			{
				block->xferObjectID(&bone.passenger);
				block->xferAsciiString(&bone.name);
			}
		}, [this] { m_passengerBones.clear(); }}
	});
}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void TunnelContain::xfer( Xfer *xfer )
{

	const Bool retail = RetailFileLayers::enabled();
	XferVersion currentVersion = retail ? 2 : 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	OpenContain::xfer( xfer );

	// need to run on build complete
	xfer->xferBool( &m_needToRunOnBuildComplete );

	// Currently registered with owning player
	xfer->xferBool( &m_isCurrentlyRegistered );

	if (retail)
		xferRetailBlocks(xfer, version >= 2);
}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void TunnelContain::loadPostProcess()
{

	// extend base class
	OpenContain::loadPostProcess();

}
