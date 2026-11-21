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

// FILE: ObjectCreationUpgrade.cpp /////////////////////////////////////////////////////////////////////////////
// Author: Matthew D. Campbell, April 2002
// Desc:	 upgrades that spawn OCLs
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/ModelState.h"
#include "Common/Player.h"
#include "Common/Xfer.h"
#include "GameClient/Drawable.h"
#include "GameLogic/Module/ObjectCreationUpgrade.h"
#include "Common/Player.h"
#include "Common/RetailFileLayers.h"
#include "Common/Upgrade.h"
#include "GameLogic/Object.h"
#include "GameLogic/Object.h"
#include "GameLogic/ObjectCreationList.h"

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
ObjectCreationUpgradeModuleData::ObjectCreationUpgradeModuleData()
{

	m_ocl = nullptr;

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
/* static */ void ObjectCreationUpgradeModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpgradeModuleData::buildFieldParse( p );

	static const FieldParse dataFieldParse[] =
	{
		{ "UpgradeObject", INI::parseObjectCreationList, nullptr, offsetof( ObjectCreationUpgradeModuleData, m_ocl ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(dataFieldParse);
	if (RetailFileLayers::enabled())
	{
		static const FieldParse retailFields[] =
		{
			{ "GrantUpgrade", INI::parseAsciiString, nullptr, offsetof(ObjectCreationUpgradeModuleData, m_grantUpgrade) },
			{ "Delay", INI::parseDurationUnsignedInt, nullptr, offsetof(ObjectCreationUpgradeModuleData, m_delay) },
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
ObjectCreationUpgrade::ObjectCreationUpgrade( Thing *thing, const ModuleData* moduleData ) :
							UpgradeModule( thing, moduleData )
{

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
ObjectCreationUpgrade::~ObjectCreationUpgrade()
{

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void ObjectCreationUpgrade::onDelete()
{
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void ObjectCreationUpgrade::upgradeImplementation()
{
	// spawn everything in the OCL
	if (getObjectCreationUpgradeModuleData() && getObjectCreationUpgradeModuleData()->m_ocl)
	{
		ObjectCreationList::create((getObjectCreationUpgradeModuleData()->m_ocl), getObject(), nullptr);
	}
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void ObjectCreationUpgrade::crc( Xfer *xfer )
{

	// extend base class
	UpgradeModule::crc( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void ObjectCreationUpgrade::xfer( Xfer *xfer )
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
void ObjectCreationUpgrade::loadPostProcess()
{

	// extend base class
	UpgradeModule::loadPostProcess();

}

RetailObjectCreationUpgrade::RetailObjectCreationUpgrade(Thing* thing, const ModuleData* data)
	: UpdateModule(thing, data)
{
}

RetailObjectCreationUpgrade::~RetailObjectCreationUpgrade() = default;

void RetailObjectCreationUpgrade::getUpgradeActivationMasks(UpgradeMaskType& activation, UpgradeMaskType& conflicting) const
{
	getObjectCreationUpgradeModuleData()->m_upgradeMuxData.getUpgradeActivationMasks(activation, conflicting);
}

void RetailObjectCreationUpgrade::performUpgradeFX()
{
	getObjectCreationUpgradeModuleData()->m_upgradeMuxData.performUpgradeFX(getObject());
}

void RetailObjectCreationUpgrade::processUpgradeRemoval()
{
	getObjectCreationUpgradeModuleData()->m_upgradeMuxData.muxDataProcessUpgradeRemoval(getObject());
}

Bool RetailObjectCreationUpgrade::requiresAllActivationUpgrades() const
{
	return getObjectCreationUpgradeModuleData()->m_upgradeMuxData.m_requiresAllTriggers;
}

void RetailObjectCreationUpgrade::upgradeImplementation()
{
	const auto* data = getObjectCreationUpgradeModuleData();
	if (!data->m_ocl && data->m_grantUpgrade.isEmpty())
		return;
	setUpgradeExecuted(TRUE);
	m_pending = TRUE;
	if (data->m_delay)
		setWakeFrame(getObject(), UPDATE_SLEEP(data->m_delay));
	else
		update();
}

Bool RetailObjectCreationUpgrade::resetUpgrade(const UpgradeMaskType& mask)
{
	if (!UpgradeMux::resetUpgrade(mask))
		return FALSE;
	m_pending = FALSE;
	return TRUE;
}

UpdateSleepTime RetailObjectCreationUpgrade::update()
{
	if (!m_pending)
		return UPDATE_SLEEP_FOREVER;
	m_pending = FALSE;
	Object* object = getObject();
	const Player* player = object->getControllingPlayer();
	if (!player || object->isEffectivelyDead())
		return UPDATE_SLEEP_FOREVER;
	UpgradeMaskType upgrades = player->getCompletedUpgradeMask();
	upgrades.set(object->getObjectCompletedUpgradeMask());
	if (!testUpgradeConditions(upgrades))
		return UPDATE_SLEEP_FOREVER;

	const auto* data = getObjectCreationUpgradeModuleData();
	if (data->m_ocl)
		ObjectCreationList::create(data->m_ocl, object, nullptr);
	if (data->m_grantUpgrade.isNotEmpty())
	{
		const UpgradeTemplate* upgrade = TheUpgradeCenter->findUpgrade(data->m_grantUpgrade);
		if (upgrade && !object->hasUpgrade(upgrade))
			object->giveUpgrade(upgrade);
	}
	return UPDATE_SLEEP_FOREVER;
}

void RetailObjectCreationUpgrade::crc(Xfer* xfer)
{
	UpdateModule::crc(xfer);
	upgradeMuxCRC(xfer);
	xfer->xferBool(&m_pending);
}

void RetailObjectCreationUpgrade::xfer(Xfer* xfer)
{
	XferVersion version = 2;
	xfer->xferVersion(&version, 2);
	if (version >= 2)
	{
		UpdateModule::xfer(xfer);
		xfer->xferBool(&m_pending);
	}
	else
	{
		XferVersion upgradeVersion = 1;
		xfer->xferVersion(&upgradeVersion, 1);
		BehaviorModule::xfer(xfer);
	}
	upgradeMuxXfer(xfer);
}

void RetailObjectCreationUpgrade::loadPostProcess()
{
	UpdateModule::loadPostProcess();
	upgradeMuxLoadPostProcess();
}
