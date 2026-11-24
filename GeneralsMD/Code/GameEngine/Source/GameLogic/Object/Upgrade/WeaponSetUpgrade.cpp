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

// FILE: WeaponSetUpgrade.cpp /////////////////////////////////////////////////////////////////////////////
// Author: Graham Smallwood, March 2002
// Desc:	 UpgradeModule that sets a weapon set bit for the Best Fit weapon set chooser to discover
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/Xfer.h"
#include "GameLogic/Object.h"
#include "GameLogic/Module/WeaponSetUpgrade.h"

void WeaponSetUpgradeModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpgradeModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "CustomAnimAndDuration", SpecialPowerAnimation::parse, nullptr, offsetof(WeaponSetUpgradeModuleData, m_upgradeAnimation) },
		{ "WeaponCondition", WeaponSetFlags::parseFromINI, nullptr, offsetof(WeaponSetUpgradeModuleData, m_weaponConditions) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
WeaponSetUpgrade::WeaponSetUpgrade( Thing *thing, const ModuleData* moduleData ) : UpgradeModule( thing, moduleData )
{
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
WeaponSetUpgrade::~WeaponSetUpgrade()
{
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void WeaponSetUpgrade::upgradeImplementation()
{
	setUpgradeAnimation(TRUE);
	getObject()->setWeaponSetFlags(getWeaponSetUpgradeModuleData()->m_weaponConditions, TRUE);
}

Bool WeaponSetUpgrade::resetUpgrade(const UpgradeMaskType& keyMask)
{
	if (!UpgradeModule::resetUpgrade(keyMask))
		return FALSE;

	getObject()->setWeaponSetFlags(getWeaponSetUpgradeModuleData()->m_weaponConditions, FALSE);
	setUpgradeAnimation(FALSE);
	return TRUE;
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void WeaponSetUpgrade::crc( Xfer *xfer )
{

	// extend base class
	UpgradeModule::crc( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void WeaponSetUpgrade::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	UpgradeModule::xfer( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void WeaponSetUpgrade::loadPostProcess()
{

	// extend base class
	UpgradeModule::loadPostProcess();

}
