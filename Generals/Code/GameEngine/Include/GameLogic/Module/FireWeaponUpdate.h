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

// FILE: FireWeaponUpdate.h /////////////////////////////////////////////////////////////////////////
// Author: Graham Smallwood, August 2002
// Desc:   Update fires a weapon at its own feet as quickly as the weapon allows
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "GameLogic/Module/UpdateModule.h"
#include "GameLogic/Weapon.h"

//-------------------------------------------------------------------------------------------------
class FireWeaponUpdateModuleData : public UpdateModuleData
{
public:
	const WeaponTemplate* m_weaponTemplate;
	FireWeaponUpdateModuleData();

	static void buildFieldParse(MultiIniFieldParse& p);

private:

};

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
class FireWeaponUpdate : public UpdateModule
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( FireWeaponUpdate, "FireWeaponUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( FireWeaponUpdate, FireWeaponUpdateModuleData )

public:

	FireWeaponUpdate( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	virtual UpdateSleepTime update() override;

protected:

	Bool isOkayToFire();

	Weapon* m_weapon;

};

class DualWeaponBehaviorModuleData : public UpdateModuleData
{
public:
	Real m_switchWeaponOnCloseRangeDistance = 0.0f;
	Bool m_useCloseRangeWhileMounted = FALSE;
	UnsignedInt m_minimumSwitchTime = 0;
	Bool m_useHordeRangeWeapon = FALSE;
	Bool m_useRealVictimRange = FALSE;

	static void buildFieldParse(MultiIniFieldParse& p);
};

class DualWeaponBehavior : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(DualWeaponBehavior, "DualWeaponBehavior")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(DualWeaponBehavior, DualWeaponBehaviorModuleData)

public:
	DualWeaponBehavior(Thing* thing, const ModuleData* data) : UpdateModule(thing, data) {}
	UpdateSleepTime update() override;

private:
	Bool m_forceCloseRange = FALSE;
	UnsignedInt m_lastSwitchFrame = 0;
};
