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

// FILE: SubObjectsUpgrade.cpp /////////////////////////////////////////////////
//-----------------------------------------------------------------------------
//
//                       Electronic Arts Pacific.
//
//                       Confidential Information
//                Copyright (C) 2002 - All Rights Reserved
//
//-----------------------------------------------------------------------------
//
//	Created:	September 2002
//
//	Filename: SubObjectsUpgrade.cpp
//
//	Author:		Kris Morness
//
//	Purpose:	Shows or hides a list of subobjects based on upgrade statii. It
//            will override any animation subobjects states.
//
//-----------------------------------------------------------------------------
///////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#define DEFINE_OBJECT_STATUS_NAMES
#include "Common/ModuleFactory.h"
#include "Common/Player.h"
#include "Common/RetailFileLayers.h"
#include "Common/Xfer.h"
#include "GameClient/Drawable.h"
#include "GameLogic/Object.h"
#include "GameLogic/Module/SubObjectsUpgrade.h"

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
namespace
{
void parseUpgradeTexture(INI* ini, void*, void* store, const void*)
{
	AsciiString original = ini->getNextAsciiString();
	ini->getNextToken();
	AsciiString replacement = ini->getNextAsciiString();
	static_cast<std::vector<std::pair<AsciiString, AsciiString>>*>(store)->emplace_back(original, replacement);
}
}

void SubObjectsUpgradeModuleData::buildFieldParse(MultiIniFieldParse& p)
{
  UpgradeModuleData::buildFieldParse(p);

	static const FieldParse dataFieldParse[] =
	{
		{ "ShowSubObjects", INI::parseAsciiStringVectorAppend, nullptr, offsetof( SubObjectsUpgradeModuleData, m_showSubObjectNames ) },
		{ "HideSubObjects", INI::parseAsciiStringVectorAppend, nullptr, offsetof( SubObjectsUpgradeModuleData, m_hideSubObjectNames ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
  p.add(dataFieldParse);
	if (ModuleFactory::registersRotwkModules())
	{
		static const FieldParse retailFields[] =
		{
			{ "UpgradeTexture", parseUpgradeTexture, nullptr, offsetof(SubObjectsUpgradeModuleData, m_upgradeTextures) },
			{ "ExcludeSubobjects", INI::parseAsciiStringVectorAppend, nullptr, offsetof(SubObjectsUpgradeModuleData, m_excludeSubObjects) },
			{ "FadeTimeInSeconds", INI::parseReal, nullptr, offsetof(SubObjectsUpgradeModuleData, m_fadeTimeInSeconds) },
			{ "WaitBeforeFadeInSeconds", INI::parseReal, nullptr, offsetof(SubObjectsUpgradeModuleData, m_waitBeforeFadeInSeconds) },
			{ "RecolorHouse", INI::parseBool, nullptr, offsetof(SubObjectsUpgradeModuleData, m_recolorHouse) },
			{ "SkipFadeOnCreate", INI::parseBool, nullptr, offsetof(SubObjectsUpgradeModuleData, m_skipFadeOnCreate) },
			{ "HideSubObjectsOnRemove", INI::parseBool, nullptr, offsetof(SubObjectsUpgradeModuleData, m_hideSubObjectsOnRemove) },
			{ "UnHideSubObjectsOnRemove", INI::parseBool, nullptr, offsetof(SubObjectsUpgradeModuleData, m_unHideSubObjectsOnRemove) },
			{ nullptr, nullptr, nullptr, 0 }
		};
		p.add(retailFields);
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
SubObjectsUpgrade::SubObjectsUpgrade( Thing *thing, const ModuleData* moduleData ) : UpgradeModule( thing, moduleData )
{
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
SubObjectsUpgrade::~SubObjectsUpgrade()
{
}

void SubObjectsUpgrade::onDrawableBoundToObject()
{
	if (!RetailFileLayers::enabled())
		return;
	Drawable* draw = getObject()->getDrawable();
	if (!draw)
		return;
	if (isAlreadyUpgraded())
	{
		upgradeImplementation();
		return;
	}
	const SubObjectsUpgradeModuleData* data = getSubObjectsUpgradeModuleData();
	if (data->m_showSubObjectNames.empty())
		return;
	for (const AsciiString& name : data->m_showSubObjectNames)
		draw->showSubObject(name, FALSE);
	draw->updateSubObjects();
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void SubObjectsUpgrade::upgradeImplementation()
{
	const SubObjectsUpgradeModuleData *data = getSubObjectsUpgradeModuleData();
	UpgradeMaskType activation, conflicting;
	getUpgradeActivationMasks( activation, conflicting );

	//First make sure we have the right combination of upgrades

	if( getObject()->getObjectCompletedUpgradeMask().testForAny( conflicting ) )
	{
		//If it has ANY of the conflicting OBJECT upgrades, then don't do it!
		return;
	}
	if( getObject()->getControllingPlayer()->getCompletedUpgradeMask().testForAny( conflicting ) )
	{
		//If it has ANY of the conflicting PLAYER upgrades, then don't do it!
		return;
	}

	Object *obj = getObject();
	Drawable *draw = obj->getDrawable();
	if( draw )
	{
		std::vector<AsciiString>::const_iterator subObjectName;
		Bool updateSubObjects = false;

		//Show these subobjects
		for( subObjectName = data->m_showSubObjectNames.begin(); subObjectName != data->m_showSubObjectNames.end(); ++subObjectName )
		{
			draw->showSubObject( *subObjectName, true );
			updateSubObjects = true;
		}

		//Hide these subobjects
		for( subObjectName = data->m_hideSubObjectNames.begin(); subObjectName != data->m_hideSubObjectNames.end(); ++subObjectName )
		{
			draw->showSubObject( *subObjectName, false );
			updateSubObjects = true;
		}

		if (!data->m_upgradeTextures.empty())
		{
			setUpgradeExecuted(TRUE);
			updateSubObjects = TRUE;
		}

		if( updateSubObjects )
		{
			draw->updateSubObjects();
		}
	}
}

//------------------------------------------------------------------------------------------------
Bool SubObjectsUpgrade::resetUpgrade(const UpgradeMaskType& keyMask)
{
	if (!UpgradeModule::resetUpgrade(keyMask))
		return FALSE;

	const auto* data = getSubObjectsUpgradeModuleData();
	if (Drawable* draw = getObject()->getDrawable())
	{
		if (data->m_hideSubObjectsOnRemove)
		{
			for (const AsciiString& name : data->m_showSubObjectNames)
				draw->showSubObject(name, FALSE);
		}
		if (data->m_unHideSubObjectsOnRemove)
		{
			for (const AsciiString& name : data->m_hideSubObjectNames)
				draw->showSubObject(name, TRUE);
		}
		if (data->m_hideSubObjectsOnRemove || data->m_unHideSubObjectsOnRemove || !data->m_upgradeTextures.empty())
			draw->updateSubObjects();
	}
	return TRUE;
}

void SubObjectsUpgrade::crc( Xfer *xfer )
{

	// extend base class
	UpgradeModule::crc( xfer );

}

//------------------------------------------------------------------------------------------------
// Xfer method
// Version Info:
// 1: Initial version
//------------------------------------------------------------------------------------------------
void SubObjectsUpgrade::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	UpgradeModule::xfer( xfer );

}

//------------------------------------------------------------------------------------------------
void SubObjectsUpgrade::loadPostProcess()
{

	// extend base class
	UpgradeModule::loadPostProcess();

}
