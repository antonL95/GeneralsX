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

// FILE: TransportContain.h ////////////////////////////////////////////////////////////////////////
// Author: Steven Johnson, March 2002
// Desc:   Contain module for transport units.
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

// USER INCLUDES //////////////////////////////////////////////////////////////////////////////////
#include "GameLogic/Module/OpenContain.h"
#include "Common/SpecialPowerINI.h"

//-------------------------------------------------------------------------------------------------
class TransportContainModuleData : public OpenContainModuleData
{
public:
	struct InitialPayload
	{
		AsciiString name;
		Int count;
	};
	struct UpgradePayload
	{
		AsciiString upgrade;
		AsciiString object;
		UnsignedInt count;
	};

	Int								m_slotCapacity;								///< max units that can be inside us
	Real							m_exitPitchRate;
	AsciiString				m_exitBone;
	InitialPayload		m_initialPayload;
	Real							m_healthRegen;
	UnsignedInt				m_exitDelay;
	Bool							m_scatterNearbyOnExit;
	Bool							m_orientLikeContainerOnExit;
	Bool							m_keepContainerVelocityOnExit;
	Bool							m_goAggressiveOnExit;
	Bool							m_armedRidersUpgradeWeaponSet;
	Bool							m_resetMoodCheckTimeOnExit;
	Bool							m_destroyRidersWhoAreNotFreeToExit;
	Bool							m_isDelayExitInAir;

	KindOfMaskType m_passengerTypeOne;
	KindOfMaskType m_passengerTypeTwo;
	KindOfMaskType m_passengerTypeOneForWeaponState;
	KindOfMaskType m_passengerTypeTwoForWeaponState;
	KindOfMaskType m_passengerTypeThreeForWeaponState;
	Bool m_forceOrientationContainer = TRUE;
	const WeaponTemplate* m_grabWeapon = nullptr;
	Bool m_fireGrabWeaponOnVictim = TRUE;
	Int m_conditionForEntry = -1;
	SpecialPowerObjectFilter m_fadeFilter;
	Bool m_fadePassengerOnEnter = FALSE;
	Bool m_fadePassengerOnExit = FALSE;
	Real m_enterFadeTime = 0.0f;
	Real m_exitFadeTime = 0.0f;
	Bool m_canGrabStructure = FALSE;
	Bool m_shouldThrowOutPassengers = FALSE;
	UnsignedInt m_throwOutPassengersDelay = 0;
	Coord3D m_throwOutPassengersVelocity{};
	Real m_releaseSnappyness = 0.7f;
	std::vector<UpgradePayload> m_upgradePayloads;
	enum { MAX_UPGRADE_PAYLOADS = 4 };
	Bool m_fadeReverse = FALSE;

	TransportContainModuleData();

	static void buildFieldParse(MultiIniFieldParse& p);
	static void parseInitialPayload( INI* ini, void *instance, void *store, const void* /*userData*/ );
	static void parseUpgradePayload(INI* ini, void* instance, void* store, const void* custom);
	static void parseConditionForEntry(INI* ini, void* instance, void* store, const void* custom);

};

//-------------------------------------------------------------------------------------------------
class TransportContain : public OpenContain
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( TransportContain, "TransportContain" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( TransportContain, TransportContainModuleData )

public:

	TransportContain( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	virtual Bool isValidContainerFor( const Object* obj, Bool checkCapacity) const override;
	Int getPassengerSlotCount(const Object* rider) const;
	Bool canCarryNewPassenger(const ThingTemplate& passenger, const Team& team) const;
	Bool forcePassengerOrientation() const override { return getTransportContainModuleData()->m_forceOrientationContainer; }

	virtual void onCapture( Player *oldOwner, Player *newOwner ) override; // have to kick everyone out on capture.
	virtual void onContaining( Object *obj, Bool wasSelected ) override;		///< object now contains 'obj'
	virtual void onRemoving( Object *obj ) override;			///< object no longer contains 'obj'
	virtual UpdateSleepTime update() override;							///< called once per frame

	virtual Bool isRiderChangeContain() const override { return FALSE; }
  virtual Bool isSpecialOverlordStyleContainer() const override {return FALSE;}

	virtual Int getContainMax() const override;

	virtual Int getExtraSlotsInUse() override { return m_extraSlotsInUse; }///< Transports have the ability to carry guys how take up more than spot.

	virtual Bool isExitBusy() const override;	///< Contain style exiters are getting the ability to space out exits, so ask this before reserveDoor as a kind of no-commitment check.
	virtual ExitDoorType reserveDoorForExit( const ThingTemplate* objType, Object *specificObject ) override;
	virtual void unreserveDoorForExit( ExitDoorType exitDoor ) override;
	virtual Bool isDisplayedOnControlBar() const override {return TRUE;}///< Does this container display its contents on the ControlBar?

protected:

	// exists primarily for TransportContain to override
	virtual void killRidersWhoAreNotFreeToExit() override;
	virtual Bool isSpecificRiderFreeToExit(Object* obj);
	virtual Bool isPassengerAllowedToFire( ObjectID id = INVALID_ID ) const override;	///< Hey, can I shoot out of this container?

	virtual void createPayload();
	void letRidersUpgradeWeaponSet();

	Bool m_payloadCreated;

private:

	void createUpgradePayloads();
	void throwOutPassengers();
	void xferRetailTransportBlocks(Xfer* xfer, Bool present);
	void updatePassengerWeaponSet();
	void updatePassengerWeaponState(const Object* rider);
	void clearPassengerWeaponState();
	void applyPassengerFade(Object* rider, Real timeMsec, Bool fadeOut) const;
	void onPassengerBoarding(Object* rider) override;
	Bool fitsPassenger(const ThingTemplate& passenger, const Player* owner, Int slots, Bool checkCapacity) const;
	UnsignedByte m_consumedUpgradePayloads = 0;
	UnsignedInt m_throwPassengersAt = 0;
	Int m_extraSlotsInUse;
	UnsignedInt m_frameExitNotBusy;

};
