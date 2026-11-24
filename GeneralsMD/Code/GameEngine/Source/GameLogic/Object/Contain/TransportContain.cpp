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

// FILE: TransportContain.cpp //////////////////////////////////////////////////////////////////////
// Author: Steven Johnson, March 2002
// Desc:   Contain module for transport units.
///////////////////////////////////////////////////////////////////////////////////////////////////

// USER INCLUDES //////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/INIException.h"
#include "Common/Player.h"
#include "Common/RetailFileLayers.h"
#include "Common/RetailSaveBlocks.h"
#include "Common/Team.h"
#include "Common/ThingTemplate.h"
#include "Common/ThingFactory.h"
#include "Common/Upgrade.h"
#include "Common/Xfer.h"
#include "GameClient/Drawable.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Module/StealthUpdate.h"
#include "GameLogic/Module/TransportContain.h"
#include "Common/ModuleFactory.h"
#include "GameLogic/Object.h"
#include "GameLogic/Weapon.h"


// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
TransportContainModuleData::TransportContainModuleData()
{

	m_slotCapacity = 0;
	m_scatterNearbyOnExit = true;
	m_orientLikeContainerOnExit = false;
	m_keepContainerVelocityOnExit = false;
	m_goAggressiveOnExit = FALSE;
  m_armedRidersUpgradeWeaponSet = FALSE;
	m_resetMoodCheckTimeOnExit = true;
	m_destroyRidersWhoAreNotFreeToExit = false;
	m_exitPitchRate = 0.0f;
	m_initialPayload.count = 0;
	m_healthRegen = 0.0f;
	m_exitDelay = 0;
	m_isDelayExitInAir = FALSE;

	//
	// by default we say that transports can have infantry inside them, this will be totally
	// overwritten by any data provided from the INI entry tho
	//
	m_allowInsideKindOf = MAKE_KINDOF_MASK( KINDOF_INFANTRY );

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void TransportContainModuleData::parseInitialPayload( INI* ini, void *instance, void *store, const void* /*userData*/ )
{
	TransportContainModuleData* self = (TransportContainModuleData*)instance;
	const char* name = ini->getNextToken();
	const char* countStr = ini->getNextTokenOrNull();
	Int count = countStr ? INI::scanInt(countStr) : 1;

	self->m_initialPayload.name.set(name);
	self->m_initialPayload.count = count;
}

void TransportContainModuleData::parseUpgradePayload(INI* ini, void* instance, void*, const void*)
{
	auto& payloads = static_cast<TransportContainModuleData*>(instance)->m_upgradePayloads;
	if (payloads.size() == MAX_UPGRADE_PAYLOADS)
		throw INIException("Too many UpgradeCreationTrigger entries");
	UpgradePayload payload;
	payload.upgrade.set(ini->getNextToken());
	payload.object.set(ini->getNextToken());
	payload.count = INI::scanUnsignedInt(ini->getNextToken());
	payloads.push_back(payload);
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void TransportContainModuleData::parseConditionForEntry(INI* ini, void*, void* store, const void*)
{
	const char* prefix = ini->getNextToken(INI::getSepsColon());
	if (!prefix || stricmp(prefix, "ModelConditionState") != 0)
		throw INIException("ConditionForEntry requires ModelConditionState");
	const char* flagName = ini->getNextToken();
	*static_cast<Int*>(store) = INI::scanIndexList(flagName, ModelConditionFlags::getBitNames());
}

void TransportContainModuleData::buildFieldParse(MultiIniFieldParse& p)
{
  OpenContainModuleData::buildFieldParse(p);

	static const FieldParse dataFieldParse[] =
	{
		{ "Slots",	INI::parseInt,		nullptr, offsetof( TransportContainModuleData, m_slotCapacity ) },
		{ "ScatterNearbyOnExit",	INI::parseBool,		nullptr, offsetof( TransportContainModuleData, m_scatterNearbyOnExit ) },
		{ "OrientLikeContainerOnExit",	INI::parseBool,		nullptr, offsetof( TransportContainModuleData, m_orientLikeContainerOnExit ) },
		{ "KeepContainerVelocityOnExit",	INI::parseBool,		nullptr, offsetof( TransportContainModuleData, m_keepContainerVelocityOnExit ) },
		{ "GoAggressiveOnExit",	INI::parseBool,		nullptr, offsetof( TransportContainModuleData, m_goAggressiveOnExit ) },
		{ "ResetMoodCheckTimeOnExit",	INI::parseBool,		nullptr, offsetof( TransportContainModuleData, m_resetMoodCheckTimeOnExit ) },
		{ "DestroyRidersWhoAreNotFreeToExit",	INI::parseBool,		nullptr, offsetof( TransportContainModuleData, m_destroyRidersWhoAreNotFreeToExit ) },
		{ "ExitBone",	INI::parseAsciiString,		nullptr, offsetof( TransportContainModuleData, m_exitBone ) },
		{ "ExitPitchRate",	INI::parseAngularVelocityReal,		nullptr, offsetof( TransportContainModuleData, m_exitPitchRate ) },
		{ "InitialPayload", parseInitialPayload, nullptr, 0 },
		{ "HealthRegen%PerSec", INI::parseReal, nullptr, offsetof( TransportContainModuleData, m_healthRegen ) },
		{ "ExitDelay",	INI::parseDurationUnsignedInt,		nullptr, offsetof( TransportContainModuleData, m_exitDelay ) },
		{ "ArmedRidersUpgradeMyWeaponSet",	INI::parseBool,		nullptr, offsetof( TransportContainModuleData, m_armedRidersUpgradeWeaponSet ) },
		{ "DelayExitInAir",	INI::parseBool,		nullptr, offsetof( TransportContainModuleData, m_isDelayExitInAir ) },

    { nullptr, nullptr, nullptr, 0 }
  };
  p.add(dataFieldParse);
	if (ModuleFactory::registersRotwkModules())
	{
		static const FieldParse retailFields[] =
		{
			{ "TypeOneForWeaponSet", KindOfMaskType::parseFromINI, nullptr, offsetof(TransportContainModuleData, m_passengerTypeOne) },
			{ "TypeTwoForWeaponSet", KindOfMaskType::parseFromINI, nullptr, offsetof(TransportContainModuleData, m_passengerTypeTwo) },
			{ "TypeOneForWeaponState", KindOfMaskType::parseFromINI, nullptr, offsetof(TransportContainModuleData, m_passengerTypeOneForWeaponState) },
			{ "TypeTwoForWeaponState", KindOfMaskType::parseFromINI, nullptr, offsetof(TransportContainModuleData, m_passengerTypeTwoForWeaponState) },
			{ "TypeThreeForWeaponState", KindOfMaskType::parseFromINI, nullptr, offsetof(TransportContainModuleData, m_passengerTypeThreeForWeaponState) },
			{ "ForceOrientationContainer", INI::parseBool, nullptr, offsetof(TransportContainModuleData, m_forceOrientationContainer) },
			{ "CanGrabStructure", INI::parseBool, nullptr, offsetof(TransportContainModuleData, m_canGrabStructure) },
			{ "ShouldThrowOutPassengers", INI::parseBool, nullptr, offsetof(TransportContainModuleData, m_shouldThrowOutPassengers) },
			{ "ThrowOutPassengersDelay", INI::parseDurationUnsignedInt, nullptr, offsetof(TransportContainModuleData, m_throwOutPassengersDelay) },
			{ "ThrowOutPassengersVelocity", INI::parseCoord3D, nullptr, offsetof(TransportContainModuleData, m_throwOutPassengersVelocity) },
			{ "ReleaseSnappyness", INI::parseReal, nullptr, offsetof(TransportContainModuleData, m_releaseSnappyness) },
			{ "UpgradeCreationTrigger", parseUpgradePayload, nullptr, 0 },
			{ "GrabWeapon", INI::parseWeaponTemplate, nullptr, offsetof(TransportContainModuleData, m_grabWeapon) },
			{ "FireGrabWeaponOnVictim", INI::parseBool, nullptr, offsetof(TransportContainModuleData, m_fireGrabWeaponOnVictim) },
			{ "ConditionForEntry", parseConditionForEntry, nullptr, offsetof(TransportContainModuleData, m_conditionForEntry) },
			{ "FadeFilter", SpecialPowerObjectFilter::parse, nullptr, offsetof(TransportContainModuleData, m_fadeFilter) },
			{ "FadePassengerOnEnter", INI::parseBool, nullptr, offsetof(TransportContainModuleData, m_fadePassengerOnEnter) },
			{ "FadePassengerOnExit", INI::parseBool, nullptr, offsetof(TransportContainModuleData, m_fadePassengerOnExit) },
			{ "EnterFadeTime", INI::parseReal, nullptr, offsetof(TransportContainModuleData, m_enterFadeTime) },
			{ "ExitFadeTime", INI::parseReal, nullptr, offsetof(TransportContainModuleData, m_exitFadeTime) },
			{ "FadeReverse", INI::parseBool, nullptr, offsetof(TransportContainModuleData, m_fadeReverse) },
			{ nullptr, nullptr, nullptr, 0 }
		};
		p.add(retailFields);
	}
}



// PRIVATE ////////////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
Int TransportContain::getContainMax() const
{
	if (getTransportContainModuleData())
		return getTransportContainModuleData()->m_slotCapacity;

	return 0;
}

// PUBLIC /////////////////////////////////////////////////////////////////////////////////////////
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
TransportContain::TransportContain( Thing *thing, const ModuleData *moduleData ) :
								 OpenContain( thing, moduleData )
{
	m_extraSlotsInUse = 0;
	m_frameExitNotBusy = 0;
	m_payloadCreated = FALSE;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
TransportContain::~TransportContain()
{

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
/**
	can this container contain this kind of object?
	and, if checkCapacity is TRUE, does this container have enough space left to hold the given unit?
*/
Int TransportContain::getPassengerSlotCount(const Object* rider) const
{
	const Int slots = rider->getTransportSlotCount();
	const auto* data = getTransportContainModuleData();
	if (rider->isKindOf(KINDOF_STRUCTURE))
		return slots;
	return slots == 0 && data->m_hasPassengerFilter ? 1 : slots;
}

Bool TransportContain::isValidContainerFor(const Object* rider, Bool checkCapacity) const
{

	if (!rider)
		return false;

	const TransportContainModuleData* data = getTransportContainModuleData();
	if (data->m_conditionForEntry >= 0 &&
		!getObject()->getLogicModelConditions().test(static_cast<ModelConditionFlagType>(data->m_conditionForEntry)))
		return FALSE;

	// The point of this new code is to determine when something is a "fake" container, to
	// look at the object inside of it to use that as the valid check. There is a case, when a
	// paratrooper (an infantry contained in a parachute). In this case, when we pass this object
	// to contain in a transport plane, we want to check the infantry, not the parachute.
	if (rider->getContain() && rider->getContain()->isSpecialZeroSlotContainer())
	{
		// Report the first thing inside it!
		const ContainedItemsList *items = rider->getContain()->getContainedItemsList();
		if (items && !items->empty())
		{
			if (items->front())
			{
				// Replace the object we are checking with the *first* object contained within it.
				rider = items->front();
			}
		}
	}


	// extend functionality
	if( OpenContain::isValidContainerFor( rider, checkCapacity ) == false )
		return false;

//	// only allied objects can be transported.
//	// order matters: we want to know if I consider it to be an ally, not vice versa
//	if (getObject()->getRelationship(rider) != ALLIES)
//		return false;

// no... actually, only OUR OWN units can be transported.
	return fitsPassenger(*rider->getTemplate(), rider->getControllingPlayer(), getPassengerSlotCount(rider), checkCapacity);
}

Bool TransportContain::canCarryNewPassenger(const ThingTemplate& passenger, const Team& team) const
{
	const Object* carrier = getObject();
	const TransportContainModuleData* data = getTransportContainModuleData();
	if (data->m_conditionForEntry >= 0 &&
		!carrier->getLogicModelConditions().test(static_cast<ModelConditionFlagType>(data->m_conditionForEntry)))
		return FALSE;
	const Relationship relationship = carrier->getIsUndetectedDefector() ? ALLIES : team.getRelationship(carrier->getTeam());
	const Int rawSlots = passenger.getRawTransportSlotCount();
	const Int slots = rawSlots == 0 && !passenger.isKindOf(KINDOF_STRUCTURE) && data->m_hasPassengerFilter ? 1 : rawSlots;
	const Player* owner = team.getControllingPlayer();
	return allowsPassenger(passenger, relationship, owner == carrier->getControllingPlayer()) &&
		fitsPassenger(passenger, owner, slots, TRUE);
}

Bool TransportContain::fitsPassenger(const ThingTemplate& passenger, const Player* owner, Int slots, Bool checkCapacity) const
{
	const auto* data = getTransportContainModuleData();
	const Bool grabStructure = data->m_canGrabStructure && passenger.isKindOf(KINDOF_STRUCTURE);
	if (data->m_hasPassengerFilter ? !data->m_passengerFilter.matchesTemplate(passenger) :
		!grabStructure && owner != getObject()->getControllingPlayer())
		return FALSE;
	if (grabStructure)
		return static_cast<Int>(getContainCount()) < getContainMax();
	return slots != 0 && (!checkCapacity || m_extraSlotsInUse + static_cast<Int>(getContainCount()) + slots <= getContainMax());
}



//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void TransportContain::letRidersUpgradeWeaponSet()
{

  const TransportContainModuleData * d = getTransportContainModuleData();

  if ( ! d->m_armedRidersUpgradeWeaponSet )
    return;

  Object *self = getObject();
  if ( self == nullptr )
    return;

  Bool anyRiderHasViableWeapon = FALSE;

	const ContainedItemsList* riderList = getContainedItemsList();
	if( riderList )
	{
		ContainedItemsList::const_iterator it;
		it = riderList->begin();

		while( it != riderList->end() )
		{
			Object *rider = *it;

			//Advance to the next iterator
			it++;

      if ( rider )
      {
		if(rider->isKindOf(KINDOF_INFANTRY) == false)
			continue;
        Weapon *weapon = nullptr;
        for ( Int w = PRIMARY_WEAPON; w < WEAPONSLOT_COUNT; ++ w )
        {
          weapon = rider->getWeaponInWeaponSlot( (WeaponSlotType)w );
          if ( weapon )
          {
            if ( weapon->getTemplate()->isContactWeapon() == FALSE && weapon->isDamageWeapon() == TRUE ) // THIS MAY NEED TO CHECK MORE WEAPON ATTRIBUTES TO WORK BEST
            {
              anyRiderHasViableWeapon = TRUE;
              break;
            }
          }
        }
      }

    }
  }

  if ( anyRiderHasViableWeapon )
  	self->setWeaponSetFlag( WEAPONSET_PLAYER_UPGRADE );
  else
  	self->clearWeaponSetFlag( WEAPONSET_PLAYER_UPGRADE );

}



//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void TransportContain::updatePassengerWeaponSet()
{
	const TransportContainModuleData* data = getTransportContainModuleData();
	if (!data->m_passengerTypeOne.any() && !data->m_passengerTypeTwo.any())
		return;

	Bool hasTypeOne = FALSE;
	Bool hasTypeTwo = FALSE;
	for (const Object* passenger : getContainList())
	{
		if (passenger->isAnyKindOf(data->m_passengerTypeOne))
			hasTypeOne = TRUE;
		else if (passenger->isAnyKindOf(data->m_passengerTypeTwo))
			hasTypeTwo = TRUE;
	}

	Object* carrier = getObject();
	WeaponSetFlags enabled;
	WeaponSetFlags disabled;
	(hasTypeOne ? enabled : disabled).set(WEAPONSET_PASSENGER_TYPE_ONE);
	(hasTypeTwo ? enabled : disabled).set(WEAPONSET_PASSENGER_TYPE_TWO);
	carrier->setWeaponSetFlags(disabled, FALSE);
	carrier->setWeaponSetFlags(enabled, TRUE);
}

void TransportContain::clearPassengerWeaponState()
{
	const TransportContainModuleData* data = getTransportContainModuleData();
	if (!data->m_passengerTypeOneForWeaponState.any() && !data->m_passengerTypeTwoForWeaponState.any() &&
		!data->m_passengerTypeThreeForWeaponState.any())
		return;

	ModelConditionFlags clearFlags;
	clearFlags.set(MODELCONDITION_WEAPONSTATE_ONE);
	clearFlags.set(MODELCONDITION_WEAPONSTATE_TWO);
	clearFlags.set(MODELCONDITION_WEAPONSTATE_THREE);
	getObject()->clearModelConditionFlags(clearFlags);
}

void TransportContain::updatePassengerWeaponState(const Object* rider)
{
	const TransportContainModuleData* data = getTransportContainModuleData();
	if (rider->isAnyKindOf(data->m_passengerTypeOneForWeaponState))
		getObject()->setModelConditionState(MODELCONDITION_WEAPONSTATE_ONE);
	else if (rider->isAnyKindOf(data->m_passengerTypeTwoForWeaponState))
		getObject()->setModelConditionState(MODELCONDITION_WEAPONSTATE_TWO);
	else if (rider->isAnyKindOf(data->m_passengerTypeThreeForWeaponState))
		getObject()->setModelConditionState(MODELCONDITION_WEAPONSTATE_THREE);
}

void TransportContain::applyPassengerFade(Object* rider, Real timeMsec, Bool fadeOut) const
{
	const TransportContainModuleData* data = getTransportContainModuleData();
	if (!rider || timeMsec <= 0.0f || !data->m_fadeFilter.matchesObject(nullptr, rider))
		return;

	const UnsignedInt frames = static_cast<UnsignedInt>(REAL_TO_INT_FLOOR(ConvertDurationFromMsecsToFrames(timeMsec)));
	const auto apply = [frames, fadeOut](Object* target)
	{
		Drawable* draw = target ? target->getDrawable() : nullptr;
		if (!draw)
			return;
		if (fadeOut)
			draw->fadeOut(frames);
		else
			draw->fadeIn(frames);
	};

	if (rider->isKindOf(KINDOF_HORDE))
	{
		if (const HordeContain* horde = HordeContain::get(rider))
		{
			for (Object* member : *horde->getContainedItemsList())
				apply(member);
			return;
		}
	}
	apply(rider);
}

void TransportContain::onPassengerBoarding(Object* rider)
{
	const TransportContainModuleData* data = getTransportContainModuleData();
	const Object* carrier = getObject();
	if (!data->m_grabWeapon || carrier->getRelationship(rider) == ALLIES)
		return;

	const Bool protectVictim = !data->m_fireGrabWeaponOnVictim && !rider->testStatus(OBJECT_STATUS_UNATTACKABLE);
	const ObjectStatusMaskType unattackable = MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_UNATTACKABLE);
	if (protectVictim)
		rider->setStatus(unattackable);
	TheWeaponStore->createAndFireTempWeapon(data->m_grabWeapon, carrier, carrier->getPosition(), TRUE);
	if (protectVictim)
		rider->clearStatus(unattackable);
}

void TransportContain::onContaining( Object *rider, Bool wasSelected )
{
	OpenContain::onContaining( rider, wasSelected );
	const auto* data = getTransportContainModuleData();
	if (data->m_shouldThrowOutPassengers)
		m_throwPassengersAt = TheGameLogic->getFrame() + data->m_throwOutPassengersDelay;
	updatePassengerWeaponSet();
	updatePassengerWeaponState(rider);

	rider->setDisabled( DISABLED_HELD );

	Int transportSlotCount = getPassengerSlotCount(rider);

	DEBUG_ASSERTCRASH(transportSlotCount > 0 || (data->m_canGrabStructure && rider->isKindOf(KINDOF_STRUCTURE)),
		("Hmm, this object isnt transportable"));
	m_extraSlotsInUse += transportSlotCount - 1;
	DEBUG_ASSERTCRASH(data->m_canGrabStructure || (m_extraSlotsInUse >= 0 && m_extraSlotsInUse + getContainCount() <= getContainMax()),
		("Hmm, bad slot count"));

	if( getContainCount() == 1 )
	{
		Drawable *draw = getObject()->getDrawable();

		if( draw )
			draw->setModelConditionState( MODELCONDITION_LOADED );

	}

	if (data->m_fadePassengerOnEnter)
		applyPassengerFade(rider, data->m_enterFadeTime, data->m_fadeReverse);

  if ( d->m_armedRidersUpgradeWeaponSet )
    letRidersUpgradeWeaponSet();

	//Kris: October 20, 2003 - Patch 1.01
	//Force Jarmen Kell to transfer weapon timer for snipe to and from the combat bike.
	if( getObject()->isKindOf( KINDOF_CLIFF_JUMPER ) && rider->isKindOf( KINDOF_HERO ) && rider->isKindOf( KINDOF_SALVAGER ) )
	{
		//Admittedly brutal hack, but considering the state of the game (post-ship), this is a particularly surgical fix.
		Weapon *bikeWeapon = getObject()->getWeaponInWeaponSlot( SECONDARY_WEAPON );
		Weapon *riderWeapon = rider->getWeaponInWeaponSlot( SECONDARY_WEAPON );
		if( bikeWeapon && riderWeapon )
		{
			//Transfer the reload time from the rider to the bike
			bikeWeapon->transferNextShotStatsFrom( *riderWeapon );
		}
	}

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void TransportContain::onRemoving( Object *rider )
{
	const TransportContainModuleData* d = getTransportContainModuleData();
	if (const AsciiString* bone = getPassengerBone(rider->getID()))
	{
		if (Drawable* draw = rider->getDrawable(); draw && !isEnclosingContainerFor(rider))
			draw->releaseRetailPassenger(*getObject(), *bone, forcePassengerOrientation(), d->m_releaseSnappyness);
	}
	OpenContain::onRemoving(rider);
	updatePassengerWeaponSet();
	clearPassengerWeaponState();

	rider->clearDisabled( DISABLED_HELD );

	if (!d->m_exitBone.isEmpty())
	{
		Drawable* draw = getObject()->getDrawable();
		if (draw)
		{
			Coord3D bonePos, worldPos;
			if (draw->getPristineBonePositions(d->m_exitBone.str(), 0, &bonePos, nullptr, 1) == 1)
			{
				getObject()->convertBonePosToWorldPos(&bonePos, nullptr, &worldPos, nullptr);
				rider->setPosition(&worldPos);
			}
		}
	}

	if (d->m_orientLikeContainerOnExit)
	{
		rider->setOrientation(getObject()->getOrientation());
	}

	if (d->m_keepContainerVelocityOnExit)
	{
		PhysicsBehavior* parent = getObject()->getPhysics();
		PhysicsBehavior* child = rider->getPhysics();
		if (parent && child)
		{
			Coord3D startingForce = *parent->getVelocity();
			Real mass = child->getMass();
			startingForce.x *= mass;
			startingForce.y *= mass;
			startingForce.z *= mass;
			child->applyMotiveForce( &startingForce );

			Real pitchRate = child->getCenterOfMassOffset() * d->m_exitPitchRate;
			child->setPitchRate( pitchRate );
		}
	}

	Int transportSlotCount = getPassengerSlotCount(rider);
	DEBUG_ASSERTCRASH(transportSlotCount > 0 || (d->m_canGrabStructure && rider->isKindOf(KINDOF_STRUCTURE)),
		("Hmm, this object isnt transportable"));
	m_extraSlotsInUse -= transportSlotCount - 1;
	DEBUG_ASSERTCRASH(d->m_canGrabStructure || (m_extraSlotsInUse >= 0 && m_extraSlotsInUse + getContainCount() <= getContainMax()),
		("Hmm, bad slot count"));

	// when we are empty again, clear the model condition for loaded
	if( getContainCount() == 0 )
	{
		m_throwPassengersAt = 0;
		Drawable *draw = getObject()->getDrawable();

		if( draw )
			draw->clearModelConditionState( MODELCONDITION_LOADED );

	}

	if (getObject()->isAboveTerrain())
	{
		// temporarily mark the guy as being allowed to fall
		// (overriding his locomotor's stick-to-ground attribute).
		// this will be reset (by PhysicsBehavior) when he touches the ground.
		PhysicsBehavior* physics = rider->getPhysics();
		if (physics)
			physics->setAllowToFall(true);
	}

	// AI might need help using this transport in a good way.  Make the passengers aggressive.
	//There is no computer player check since Aggressive only means something for computer players anyway
	if( d->m_goAggressiveOnExit && rider->getAI() )
	{
		rider->getAI()->setAttitude( ATTITUDE_AGGRESSIVE );
	}
	if (getObject()->isEffectivelyDead()) {
		scatterToNearbyPosition(rider);
	}
	if( d->m_resetMoodCheckTimeOnExit && rider->getAI() )
	{
		rider->getAI()->wakeUpAndAttemptToTarget();
	}

	m_frameExitNotBusy = TheGameLogic->getFrame() + d->m_exitDelay;

	if (d->m_fadePassengerOnExit)
		applyPassengerFade(rider, d->m_exitFadeTime, FALSE);

  if ( d->m_armedRidersUpgradeWeaponSet )
    letRidersUpgradeWeaponSet();

	//Kris: October 20, 2003 - Patch 1.01
	//Force Jarmen Kell to transfer weapon timer for snipe to and from the combat bike.
	if( getObject()->isKindOf( KINDOF_CLIFF_JUMPER ) && rider->isKindOf( KINDOF_HERO ) && rider->isKindOf( KINDOF_SALVAGER ) )
	{
		//Admittedly brutal hack, but considering the state of the game (post-ship), this is a particularly surgical fix.
		Weapon *bikeWeapon = getObject()->getWeaponInWeaponSlot( SECONDARY_WEAPON );
		Weapon *riderWeapon = rider->getWeaponInWeaponSlot( SECONDARY_WEAPON );
		if( bikeWeapon && riderWeapon )
		{
			//Transfer the reload time from the bike to the rider
			riderWeapon->transferNextShotStatsFrom( *bikeWeapon );
		}
	}

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void TransportContain::createPayload()
{
	TransportContainModuleData* self = (TransportContainModuleData*)getTransportContainModuleData();

	Int count = self->m_initialPayload.count;
	const ThingTemplate* payloadTemplate = TheThingFactory->findTemplate( self->m_initialPayload.name );
	Object* object = getObject();
	ContainModuleInterface *contain = object->getContain();

	if( contain )
	{
		contain->enableLoadSounds( FALSE );
		for( int i = 0; i < count; i++ )
		{
			//We are creating a transport that comes with a initial payload, so add it now!
			Object* payload = TheThingFactory->newObject( payloadTemplate, object->getControllingPlayer()->getDefaultTeam() );
			if( contain->isValidContainerFor( payload, true ) )
			{
				contain->addToContain( payload );
			}
			else
			{
				DEBUG_CRASH( ( "DeliverPayload: PutInContainer %s is full, or not valid for the payload %s!", object->getName().str(), self->m_initialPayload.name.str() ) );
			}
		}
		contain->enableLoadSounds( TRUE );
	}

	m_payloadCreated = TRUE;

}

void TransportContain::createUpgradePayloads()
{
	Object* transport = getObject();
	if (transport->isEffectivelyDead())
		return;
	const auto& payloads = getTransportContainModuleData()->m_upgradePayloads;
	for (UnsignedInt index = 0; index < payloads.size(); ++index)
	{
		const UnsignedByte bit = 1u << index;
		if (m_consumedUpgradePayloads & bit)
			continue;
		const auto& payload = payloads[index];
		const UpgradeTemplate* upgrade = TheUpgradeCenter->findUpgrade(payload.upgrade);
		if (!upgrade || !(transport->hasUpgrade(upgrade) || transport->getControllingPlayer()->hasUpgradeComplete(upgrade)))
			continue;
		m_consumedUpgradePayloads |= bit;
		const ThingTemplate* passenger = TheThingFactory->findTemplate(payload.object, FALSE);
		if (!passenger)
			continue;
		for (UnsignedInt count = 0; count < payload.count; ++count)
		{
			Object* created = TheThingFactory->newObject(passenger, transport->getTeam());
			if (!created)
				break;
			created->setPosition(transport->getPosition());
			if (isValidContainerFor(created, TRUE))
				addToContain(created);
			if (created->getContainedBy() != transport)
			{
				TheGameLogic->destroyObject(created);
				break;
			}
		}
	}
}

void TransportContain::throwOutPassengers()
{
	const Object* transport = getObject();
	const Coord3D& velocity = getTransportContainModuleData()->m_throwOutPassengersVelocity;
	const Vector3 impulse = transport->getTransformMatrix()->Rotate_Vector(Vector3(velocity.x, velocity.y, velocity.z));
	const Coord3D worldVelocity{impulse.X, impulse.Y, impulse.Z};
	const ContainedItemsList passengers = getContainList();
	for (Object* passenger : passengers)
	{
		removeFromContain(passenger);
		if (PhysicsBehavior* physics = passenger->getPhysics())
			physics->launchFromContainment(worldVelocity);
	}
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
UpdateSleepTime TransportContain::update()
{
	const TransportContainModuleData *moduleData = getTransportContainModuleData();

	if (moduleData->m_shouldThrowOutPassengers && getContainCount() && TheGameLogic->getFrame() >= m_throwPassengersAt)
		throwOutPassengers();
	if (!moduleData->m_upgradePayloads.empty())
		createUpgradePayloads();

	if (moduleData->m_passengerTypeOne.any())
	{
		const auto& passengers = getContainList();
		for (auto it = passengers.begin(); it != passengers.end(); )
		{
			Object* passenger = *it++;
			if (passenger->isKindOf(KINDOF_CLUB) && passenger->isAnyKindOf(moduleData->m_passengerTypeOne)
				&& passenger->isEffectivelyDead())
				removeFromContain(passenger);
		}
	}
	if (m_payloadCreated == FALSE)
	{
#if RETAIL_COMPATIBLE_CRC
		createPayload();
#else
		// TheSuperHackers @bugfix Caball009 25/05/2026 Don't create payload
		// for destroyed object to avoid leaving the payload in an invalid state.
		if (!getObject()->isDestroyed())
		{
			createPayload();
		}
#endif
	}

	if( moduleData && moduleData->m_healthRegen )
	{
		ContainModuleInterface *contain = getObject()->getContain();
		if( contain )
		{
			//This transport has a health regeneration value, so go through and heal all inside.
			const ContainedItemsList* items = contain->getContainedItemsList();
			if( items )
			{
				ContainedItemsList::const_iterator it;
				it = items->begin();

				while( it != items->end() )
				{
					Object *object = *it;

					//Advance to the next iterator
					it++;

					//Determine if we need healing or not.
					BodyModuleInterface *body = object->getBodyModule();
					if( body->getHealth() < body->getMaxHealth() )
					{
						//Calculate the health to be regenerated on each unit.
						Real regen = body->getMaxHealth() * moduleData->m_healthRegen / 100.0f * SECONDS_PER_LOGICFRAME_REAL;

						//Perform the actual healing for this frame.
//						DamageInfo damageInfo;
//						damageInfo.in.m_damageType = DAMAGE_HEALING;
//						damageInfo.in.m_deathType = DEATH_NONE;
//						damageInfo.in.m_sourceID = getObject()->getID();
//						damageInfo.in.m_amount = regen;
//						object->attemptDamage( &damageInfo );
						object->attemptHealing( regen, getObject() );
					}
				}
			}
		}
	}
	return OpenContain::update(); //extend
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void TransportContain::unreserveDoorForExit( ExitDoorType exitDoor )
{
	/* nothing */
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void TransportContain::killRidersWhoAreNotFreeToExit()
{
	const TransportContainModuleData* d = getTransportContainModuleData();
	ContainedItemsList::const_iterator it = getContainList().begin();
	while( it != getContainList().end() )
	{
		// get the object
		Object *obj = *it;
		// increment the iterator, since death will pull this guy from the list somewhere else
		++it;

		if (!isSpecificRiderFreeToExit(obj)) // only TransportContain has a meaningful failure for isFreeToExit
		{
			// If we cannot exit this guy legally, kill the bastard before removeAllContained forces him out.
			if (d->m_destroyRidersWhoAreNotFreeToExit)
				TheGameLogic->destroyObject(obj);
			else
#if RETAIL_COMPATIBLE_CRC
				obj->kill();
#else
				// TheSuperHackers @info Burned death prevents infantry corpses dropping out of the container.
				obj->kill(DAMAGE_UNRESISTABLE, d->m_isBurnedDeathToUnits ? DEATH_BURNED : DEATH_NORMAL);
#endif
		}
	}
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
Bool TransportContain::isSpecificRiderFreeToExit(Object* specificObject)
{
	if( specificObject == nullptr )
		return TRUE;	// I can, in general, exit people.

	// This is a override, not an extend.  I will check for game legality for
	// okaying the call to exitObjectViaDoor.
  const Object* me = getObject();

	// this is present solely for some transports to override, so that they can land before
	// allowing people to exit...
	const AIUpdateInterface* ai = me->getAIUpdateInterface();
	if (ai && ai->getAiFreeToExit(specificObject) != FREE_TO_EXIT)
		return FALSE;

#if !RETAIL_COMPATIBLE_CRC
	// TheSuperHackers @bugfix Stubbjax/bobtista 01/08/2026 If our container is itself contained,
	// then we are not free to exit.
	// GeneralsX @bugfix UnicodeApocalypse 12/09/2026 Except for special Overlord-style containers (e.g. Battle Bunker).
	if (me->isContained())
	{
		const ContainModuleInterface* outerContain = me->getContainedBy()->getContain();
		if (outerContain == nullptr || !outerContain->isSpecialOverlordStyleContainer())
			return FALSE;
	}
#endif

  // I can always kick people out if I am in the air, I know what I'm doing
  if (me->isUsingAirborneLocomotor())
   	return TRUE;

  const Coord3D *myPosition = me->getPosition();
 	if (!specificObject->getAIUpdateInterface())
		return FALSE;

	const Locomotor *hisLocomotor = specificObject->getAIUpdateInterface()->getCurLocomotor();
	if( hisLocomotor == nullptr )
   	return FALSE;

  // He can't get to this spot naturally, so I can't force him there.  (amphib transport)
  if (!TheAI->pathfinder()->validMovementTerrain(me->getLayer(), hisLocomotor, myPosition))
   	return FALSE;

  return TRUE;
}

Bool TransportContain::isPassengerAllowedToFire( ObjectID id ) const
{
	Object *passenger = TheGameLogic->findObjectByID(id);

	if( passenger != nullptr )
	{
		//only allow infantry, and turrets and such.  no vehicles.
		if( passenger->isKindOf(KINDOF_INFANTRY) == FALSE )
			return FALSE;
	}

  if ( ! getObject() )
    return FALSE;

  // but wait! I may be riding on an Overlord
  // This code detects the case of whether the contained passenger is in a bunker riding on an overlord, inside a helix!
  // Oh  my  God.
  const Object *heWhoContainsMe = getObject()->getContainedBy();
  if ( heWhoContainsMe)
  {
    ContainModuleInterface *hisContain = heWhoContainsMe->getContain();
    DEBUG_ASSERTCRASH( hisContain,("TransportContain::isPassengerAllowedToFire()... CONTAINER WITHOUT A CONTAIN! AARRGH!") );
    if ( hisContain && hisContain->isSpecialOverlordStyleContainer() )
      return hisContain->isPassengerAllowedToFire( id );
  }



	return OpenContain::isPassengerAllowedToFire();
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
ExitDoorType TransportContain::reserveDoorForExit( const ThingTemplate* objType, Object *specificObject )
{
	return isSpecificRiderFreeToExit(specificObject) ? DOOR_1 : DOOR_NONE_AVAILABLE;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
Bool TransportContain::isExitBusy() const	///< Contain style exiters are getting the ability to space out exits, so ask this before reserveDoor as a kind of no-commitment check.
{
	const TransportContainModuleData *data = getTransportContainModuleData();
	if( data->m_isDelayExitInAir && getObject()->isAboveTerrain() )
		return TRUE;

	return TheGameLogic->getFrame() < m_frameExitNotBusy;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void TransportContain::onCapture( Player *oldOwner, Player *newOwner )
{
	if( oldOwner != newOwner )
	{
		if( getObject()->isDisabledByType( DISABLED_UNMANNED ) )
		{
			//If this vehicle was sniped, then instantly eject everyone (otherwise, the next guy to eject will recapture it).
			removeAllContained();
		}
		else
		{
			//Use standard
#if RETAIL_COMPATIBLE_CRC
			orderAllPassengersToExit( CMD_FROM_AI, FALSE );
#else
      // TheSuperHackers @bugfix Stubbjax 20/11/2025 Only eject passengers if the new owner is not allied with the old owner.
			if (oldOwner->getRelationship(newOwner->getDefaultTeam()) != ALLIES)
				orderAllPassengersToExit(CMD_FROM_AI, FALSE);
#endif
		}
	}
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void TransportContain::crc( Xfer *xfer )
{

	// extend base class
	OpenContain::crc( xfer );
	const auto* data = getTransportContainModuleData();
	if (RetailFileLayers::enabled() && (data->m_shouldThrowOutPassengers || !data->m_upgradePayloads.empty()))
		xferRetailTransportBlocks(xfer, TRUE);

}

void TransportContain::xferRetailTransportBlocks(Xfer* xfer, Bool present)
{
	RetailSaveBlocks::transferSection<Xfer>(xfer, xfer->getXferMode() == XFER_LOAD, present, {
		{"upgrade-creation-triggers", [this](Xfer* block) {
			block->xferUnsignedByte(&m_consumedUpgradePayloads);
		}, [this] { m_consumedUpgradePayloads = 0; }},
		{"throw-passengers", [this](Xfer* block) {
			block->xferUnsignedInt(&m_throwPassengersAt);
		}, [this] { m_throwPassengersAt = 0; }}
	});
}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void TransportContain::xfer( Xfer *xfer )
{

	// version
	const Bool retail = RetailFileLayers::enabled();
	XferVersion currentVersion = retail ? 2 : 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	OpenContain::xfer( xfer );

	// payload created
	xfer->xferBool( &m_payloadCreated );

	// extra slots in use
	xfer->xferInt( &m_extraSlotsInUse );

	// frame exit not busy
	xfer->xferUnsignedInt( &m_frameExitNotBusy );

	if (retail)
		xferRetailTransportBlocks(xfer, version >= 2);

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void TransportContain::loadPostProcess()
{

	// extend base class
	OpenContain::loadPostProcess();

	if (getTransportContainModuleData()->m_canGrabStructure)
	{
		m_extraSlotsInUse = 0;
		for (const Object* rider : *getContainedItemsList())
			m_extraSlotsInUse += getPassengerSlotCount(rider) - 1;
	}

}
