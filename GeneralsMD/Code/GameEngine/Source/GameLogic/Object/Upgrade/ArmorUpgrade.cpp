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

// FILE: ArmorUpgrade.cpp /////////////////////////////////////////////////
//-----------------------------------------------------------------------------
//
//                       Electronic Arts Pacific.
//
//                       Confidential Information
//                Copyright (C) 2002 - All Rights Reserved
//
//-----------------------------------------------------------------------------
//
//	created:	May 2002
//
//	Filename: 	ArmorUpgrade.cpp
//
//	author:		Chris Brue
//
//	purpose:
//
//-----------------------------------------------------------------------------
///////////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
// SYSTEM INCLUDES ////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// USER INCLUDES //////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/Xfer.h"
#include "Common/Player.h"
#include "Common/RetailFileLayers.h"
#include "Common/Upgrade.h"
#include "GameLogic/Object.h"
#include "GameLogic/Module/ArmorUpgrade.h"
#include "GameLogic/Module/BodyModule.h"
//-----------------------------------------------------------------------------
// DEFINES ////////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// PUBLIC FUNCTIONS ///////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// PRIVATE FUNCTIONS //////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void ArmorUpgradeModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpgradeModuleData::buildFieldParse(p);
	if (RetailFileLayers::enabled())
	{
		static const FieldParse fields[] =
		{
			{ "ArmorSetFlag", ArmorSetFlags::parseSingleBitFromINI, nullptr, offsetof(ArmorUpgradeModuleData, m_armorSetFlag) },
			{ "IgnoreArmorUpgrade", INI::parseBool, nullptr, offsetof(ArmorUpgradeModuleData, m_ignoreArmorUpgrade) },
			{ "KillArmorUpgrade", INI::parseBool, nullptr, offsetof(ArmorUpgradeModuleData, m_killArmorUpgrade) },
			{ "CustomAnimAndDuration", SpecialPowerAnimation::parse, nullptr, offsetof(ArmorUpgradeModuleData, m_animation) },
			{ nullptr, nullptr, nullptr, 0 }
		};
		p.add(fields);
	}
}

ArmorUpgrade::ArmorUpgrade( Thing *thing, const ModuleData* moduleData ) : UpgradeModule( thing, moduleData )
{
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
ArmorUpgrade::~ArmorUpgrade()
{
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void ArmorUpgrade::upgradeImplementation()
{
	// Very simple; just need to flag the Object as having the player upgrade, and the WeaponSet chooser
	// will do the work of picking the right one from ini.  This comment is as long as the code.
	// STILL IN PROGRESS
	Object *obj = getObject();
	if( !obj )
		return;

	BodyModuleInterface* body = obj->getBodyModule();
	const auto* data = getArmorUpgradeModuleData();
	if (RetailFileLayers::enabled() && !data->m_ignoreArmorUpgrade)
	{
		obj->applyUpgradeAnimation(data->m_animation);
		if (body)
		{
			static constexpr ModelConditionFlagType conditions[] =
			{
				MODELCONDITION_ARMORSET_VETERAN, MODELCONDITION_ARMORSET_ELITE, MODELCONDITION_ARMORSET_HERO,
				MODELCONDITION_ARMORSET_PLAYER_UPGRADE, MODELCONDITION_ARMORSET_WEAK_VERSUS_BASEDEFENSES,
				MODELCONDITION_INVALID, MODELCONDITION_INVALID, MODELCONDITION_INVALID,
				MODELCONDITION_ARMORSET_ALTERNATE_FORMATION, MODELCONDITION_ARMORSET_MOUNTED,
				MODELCONDITION_ARMORSET_PLAYER_UPGRADE_2, MODELCONDITION_ARMORSET_PLAYER_UPGRADE_3,
				MODELCONDITION_ARMORSET_UNBESIEGEABLE, MODELCONDITION_ARMORSET_PLAYER_UPGRADE
			};
			static_assert(ARRAY_SIZE(conditions) == ARMORSET_CREATE_A_HERO_01);
			const auto flag = static_cast<ArmorSetType>(data->m_armorSetFlag);
			const auto condition = flag < ARMORSET_CREATE_A_HERO_01 ? conditions[flag] : MODELCONDITION_ARMORSET_VETERAN;
			if (data->m_killArmorUpgrade)
			{
				body->clearArmorSetFlag(flag);
				if (condition != MODELCONDITION_INVALID)
					obj->clearModelConditionState(condition);
			}
			else
			{
				body->setArmorSetFlag(flag);
				if (condition != MODELCONDITION_INVALID)
					obj->setModelConditionState(condition);
			}
		}
	}
	else if (body && !RetailFileLayers::enabled())
		body->setArmorSetFlag(ARMORSET_PLAYER_UPGRADE);

	// Unique case for AMERICA to test for upgrade to set flag
	if(isTriggeredBy("Upgrade_AmericaChemicalSuits"))
	{
		obj->getDrawable()->setTerrainDecal(TERRAIN_DECAL_CHEMSUIT);
	}
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void ArmorUpgrade::crc( Xfer *xfer )
{

	// extend base class
	UpgradeModule::crc( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void ArmorUpgrade::xfer( Xfer *xfer )
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
void ArmorUpgrade::loadPostProcess()
{

	// extend base class
	UpgradeModule::loadPostProcess();

}
