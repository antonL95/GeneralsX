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

// FILE: GrantUpgradeCreate.cpp ////////////////////////////////////////////////////////////////////////
// Author: Kris Morness, April 2002
// Desc:   GrantUpgrade create module
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#define DEFINE_OBJECT_STATUS_NAMES
#include "Common/Player.h"
#include "Common/GameState.h"
#include "Common/RetailFileLayers.h"
#include "Common/Upgrade.h"
#include "Common/Xfer.h"
#include "GameLogic/Module/GrantUpgradeCreate.h"
#include "GameLogic/Object.h"

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
GrantUpgradeCreateModuleData::GrantUpgradeCreateModuleData()
{
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void GrantUpgradeCreateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
  CreateModuleData::buildFieldParse(p);

	static const FieldParse dataFieldParse[] =
	{
		{ "UpgradeToGrant",	INI::parseAsciiString,							nullptr, offsetof( GrantUpgradeCreateModuleData, m_upgradeName ) },
		{ "ExemptStatus",		ObjectStatusMaskType::parseFromINI, nullptr, offsetof( GrantUpgradeCreateModuleData, m_exemptStatus ) },
		{ nullptr, nullptr, nullptr, 0 }
	};

  p.add(dataFieldParse);
	if (RetailFileLayers::enabled())
	{
		static const FieldParse retailFields[] =
		{
			{ "GiveOnBuildComplete", INI::parseBool, nullptr, offsetof(GrantUpgradeCreateModuleData, m_giveOnBuildComplete) },
			{ nullptr, nullptr, nullptr, 0 }
		};
		p.add(retailFields);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
GrantUpgradeCreate::GrantUpgradeCreate( Thing *thing, const ModuleData* moduleData ) : CreateModule( thing, moduleData )
{
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
GrantUpgradeCreate::~GrantUpgradeCreate()
{

}

//-------------------------------------------------------------------------------------------------
/** The create callback. */
//-------------------------------------------------------------------------------------------------
void GrantUpgradeCreate::onCreate()
{
	if (RetailFileLayers::enabled())
	{
		const auto* data = getGrantUpgradeCreateModuleData();
		if ((!TheGameState || !TheGameState->isInLoadGame()) && !data->m_giveOnBuildComplete &&
			data->m_exemptStatus.test(OBJECT_STATUS_UNDER_CONSTRUCTION) &&
			!getObject()->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION))
			grantRetailUpgrade(TRUE);
		return;
	}

	ObjectStatusMaskType exemptStatus = getGrantUpgradeCreateModuleData()->m_exemptStatus;
	ObjectStatusMaskType currentStatus = getObject()->getStatusBits();
	if( exemptStatus.test( OBJECT_STATUS_UNDER_CONSTRUCTION ) )
	{
		if(	!currentStatus.test( OBJECT_STATUS_UNDER_CONSTRUCTION ) )
		{
			const UpgradeTemplate *upgradeTemplate = TheUpgradeCenter->findUpgrade( getGrantUpgradeCreateModuleData()->m_upgradeName );
			if( !upgradeTemplate )
			{
				DEBUG_CRASH( ("GrantUpdateCreate for %s can't find upgrade template %s.", getObject()->getName().str(), getGrantUpgradeCreateModuleData()->m_upgradeName.str() ) );
				return;
			}

			Player *player = getObject()->getControllingPlayer();
			if( upgradeTemplate->getUpgradeType() == UPGRADE_TYPE_PLAYER )
			{
				// get the player
				player->addUpgrade( upgradeTemplate, UPGRADE_STATUS_COMPLETE );
			}
			else
			{
				getObject()->giveUpgrade( upgradeTemplate );
			}

			player->getAcademyStats()->recordUpgrade( upgradeTemplate, TRUE );
		}
	}

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void GrantUpgradeCreate::onBuildComplete()
{
	if( ! shouldDoOnBuildComplete() )
		return;

	CreateModule::onBuildComplete(); // extend

	if (RetailFileLayers::enabled())
	{
		grantRetailUpgrade(FALSE);
		return;
	}

	const UpgradeTemplate *upgradeTemplate = TheUpgradeCenter->findUpgrade( getGrantUpgradeCreateModuleData()->m_upgradeName );
	if( !upgradeTemplate )
	{
		DEBUG_CRASH( ("GrantUpdateCreate for %s can't find upgrade template %s.", getObject()->getName().str(), getGrantUpgradeCreateModuleData()->m_upgradeName.str() ) );
		return;
	}

	if( upgradeTemplate->getUpgradeType() == UPGRADE_TYPE_PLAYER )
	{
		// get the player
		Player *player = getObject()->getControllingPlayer();
		player->addUpgrade( upgradeTemplate, UPGRADE_STATUS_COMPLETE );
	}
	else
	{
		getObject()->giveUpgrade( upgradeTemplate );
	}
}

void GrantUpgradeCreate::onRetailOCLComplete()
{
	if (getGrantUpgradeCreateModuleData()->m_giveOnBuildComplete)
		onBuildComplete();
}

void GrantUpgradeCreate::grantRetailUpgrade(Bool record)
{
	Object* object = getObject();
	const auto* upgrade = TheUpgradeCenter->findUpgrade(getGrantUpgradeCreateModuleData()->m_upgradeName);
	Player* player = object->getControllingPlayer();
	if (!upgrade || !player)
		return;

	if (upgrade->getUpgradeType() == UPGRADE_TYPE_PLAYER)
	{
		if (player->hasUpgradeComplete(upgrade))
			return;
		player->addUpgrade(upgrade, UPGRADE_STATUS_COMPLETE);
	}
	else
	{
		if (object->hasUpgrade(upgrade))
			return;
		object->giveUpgrade(upgrade);
	}
	if (record)
		player->getAcademyStats()->recordUpgrade(upgrade, TRUE);
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void GrantUpgradeCreate::crc( Xfer *xfer )
{

	// extend base class
	CreateModule::crc( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void GrantUpgradeCreate::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	CreateModule::xfer( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void GrantUpgradeCreate::loadPostProcess()
{

	// extend base class
	CreateModule::loadPostProcess();

}
