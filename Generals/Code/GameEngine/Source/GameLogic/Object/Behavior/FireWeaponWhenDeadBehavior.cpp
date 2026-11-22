/*
**	Command & Conquer Generals(tm)
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

// FILE: FireWeaponWhenDeadBehavior.cpp ///////////////////////////////////////////////////////////////////////
// Author:
// Desc:
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine
#define DEFINE_SLOWDEATHPHASE_NAMES

#include "Common/Thing.h"
#include "Common/ThingTemplate.h"
#include "Common/INI.h"
#include "Common/RandomValue.h"
#include "Common/Xfer.h"
#include "Common/Player.h"
#include "Common/RetailFileLayers.h"
#include "GameClient/Drawable.h"
#include "GameClient/FXList.h"
#include "GameClient/InGameUI.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/FireWeaponWhenDeadBehavior.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/ObjectCreationList.h"
#include "GameLogic/Weapon.h"


const Int MAX_IDX = 32;

const Real BEGIN_MIDPOINT_RATIO = 0.35f;
const Real END_MIDPOINT_RATIO = 0.65f;


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
FireWeaponWhenDeadBehavior::FireWeaponWhenDeadBehavior( Thing *thing, const ModuleData* moduleData ) :
	UpdateModule( thing, moduleData )
{
	if (getFireWeaponWhenDeadBehaviorModuleData()->m_initiallyActive)
	{
		giveSelfUpgrade();
	}
	if (getUpdate())
		setWakeFrame(getObject(), UPDATE_SLEEP_FOREVER);
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
FireWeaponWhenDeadBehavior::~FireWeaponWhenDeadBehavior()
{
}

//-------------------------------------------------------------------------------------------------
/** The die callback. */
//-------------------------------------------------------------------------------------------------
void FireWeaponWhenDeadBehavior::onDie( const DamageInfo *damageInfo )
{
	const auto* data = getFireWeaponWhenDeadBehaviorModuleData();
	if (getUpdate())
	{
		m_fireFrame = TheGameLogic->getFrame() + data->m_delay;
		m_pendingDeathType = damageInfo->in.m_deathType;
		setWakeFrame(getObject(), UPDATE_SLEEP(data->m_delay));
		return;
	}
	fireDeathWeapon(damageInfo);
}

UpdateModuleInterface* FireWeaponWhenDeadBehavior::getUpdate()
{
	return RetailFileLayers::enabled() && getFireWeaponWhenDeadBehaviorModuleData()->m_delay ? this : nullptr;
}

UpdateSleepTime FireWeaponWhenDeadBehavior::update()
{
	if (!m_fireFrame || getObject()->isDestroyed())
		return UPDATE_SLEEP_FOREVER;
	const UnsignedInt now = TheGameLogic->getFrame();
	if (now < m_fireFrame)
		return UPDATE_SLEEP(m_fireFrame - now);

	DamageInfo damageInfo;
	damageInfo.in.m_deathType = m_pendingDeathType;
	m_fireFrame = 0;
	m_pendingDeathType = DEATH_NORMAL;
	fireDeathWeapon(&damageInfo);
	return UPDATE_SLEEP_FOREVER;
}

void FireWeaponWhenDeadBehavior::fireDeathWeapon(const DamageInfo* damageInfo)
{
	const FireWeaponWhenDeadBehaviorModuleData* d = getFireWeaponWhenDeadBehaviorModuleData();
	Object *obj = getObject();

	if (!isUpgradeActive())
		return;

	// right type?
	if (!d->m_dieMuxData.isDieApplicable(getObject(), damageInfo))
		return;

	if (obj->getStatusBits().test(OBJECT_STATUS_UNDER_CONSTRUCTION) &&
		(!RetailFileLayers::enabled() || !d->m_activeDuringConstruction))
		return;


	UpgradeMaskType activation, conflicting;
	getUpgradeActivationMasks( activation, conflicting );

	if( obj->getObjectCompletedUpgradeMask().testForAny( conflicting ) )
	{
		return;
	}
	if( obj->getControllingPlayer() && obj->getControllingPlayer()->getCompletedUpgradeMask().testForAny( conflicting ) )
	{
		return;
	}

	if (d->m_deathWeapon)
	{
		// fire the default weapon
		if (RetailFileLayers::enabled())
		{
			const Coord3D& offset = d->m_weaponOffset;
			Vector3 target;
			Matrix3D::Transform_Vector(*obj->getTransformMatrix(), Vector3(offset.x, offset.y, offset.z), &target);
			Coord3D position{target.X, target.Y, target.Z};
			TheWeaponStore->createAndFireTempWeapon(d->m_deathWeapon, obj, &position);
		}
		else
		{
			TheWeaponStore->createAndFireTempWeapon(d->m_deathWeapon, obj, obj->getPosition());
		}
	}
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void FireWeaponWhenDeadBehavior::crc( Xfer *xfer )
{

	// extend base class
	BehaviorModule::crc( xfer );

	// extend upgrade mux
	UpgradeMux::upgradeMuxCRC( xfer );
	if (getUpdate())
		xferPendingFire(xfer);

}

void FireWeaponWhenDeadBehavior::xferPendingFire(Xfer* xfer)
{
	xfer->xferUnsignedInt(&m_fireFrame);
	xfer->xferUser(&m_pendingDeathType, sizeof(m_pendingDeathType));
}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void FireWeaponWhenDeadBehavior::xfer( Xfer *xfer )
{

	// version
	const XferVersion currentVersion = getUpdate() ? 2 : 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	if (version >= 2)
		UpdateModule::xfer(xfer);
	else
		BehaviorModule::xfer( xfer );

	// extend upgrade mux
	UpgradeMux::upgradeMuxXfer( xfer );
	if (version >= 2)
		xferPendingFire(xfer);

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void FireWeaponWhenDeadBehavior::loadPostProcess()
{

	// extend base class
	BehaviorModule::loadPostProcess();

	// extend upgrade mux
	UpgradeMux::upgradeMuxLoadPostProcess();

}
