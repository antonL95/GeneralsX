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

// FILE: LifetimeUpdate.cpp ///////////////////////////////////////////////////////////////////////
// Author: Colin Day, December 2001
// Desc:   Update that will count down a lifetime and destroy object when it reaches zero
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"

#define DEFINE_DEATH_NAMES
#include "Common/ModuleFactory.h"
#include "Common/RandomValue.h"
#include "Common/RetailFileLayers.h"
#include "Common/Xfer.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/LifetimeUpdate.h"
#include "GameLogic/Module/RotwkParseOnlyModules.h"
#include "GameLogic/Object.h"

void LifetimeUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpdateModuleData::buildFieldParse(p);
	static const FieldParse dataFieldParse[] =
	{
		{ "MinLifetime", INI::parseDurationUnsignedInt, nullptr, offsetof(LifetimeUpdateModuleData, m_minFrames) },
		{ "MaxLifetime", INI::parseDurationUnsignedInt, nullptr, offsetof(LifetimeUpdateModuleData, m_maxFrames) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(dataFieldParse);
	if (ModuleFactory::registersRotwkModules())
	{
		static const FieldParse retailFields[] =
		{
			{ "WaitForWakeUp", INI::parseBool, nullptr, offsetof(LifetimeUpdateModuleData, m_waitForWakeUp) },
			{ "ScoreKill", INI::parseBool, nullptr, offsetof(LifetimeUpdateModuleData, m_scoreKill) },
			{ "DeathType", INI::parseIndexList, TheDeathNames, offsetof(LifetimeUpdateModuleData, m_deathType) },
			{ nullptr, nullptr, nullptr, 0 }
		};
		p.add(retailFields);
	}
}

LifetimeUpdate::LifetimeUpdate( Thing *thing, const ModuleData* moduleData ) : UpdateModule( thing, moduleData )
{
	const LifetimeUpdateModuleData* d = getLifetimeUpdateModuleData();
	m_dieFrame = 0;
	if (d->m_waitForWakeUp)
	{
		setWakeFrame(getObject(), UPDATE_SLEEP_FOREVER);
		return;
	}
	UnsignedInt delay;
	if( getObject()->isKindOf( KINDOF_HULK ) && TheGameLogic->getHulkMaxLifetimeOverride() != -1 )
	{
		delay = calcSleepDelay( TheGameLogic->getHulkMaxLifetimeOverride(), TheGameLogic->getHulkMaxLifetimeOverride() );
	}
	else
	{
		delay = calcSleepDelay(d->m_minFrames, d->m_maxFrames);
	}

	setWakeFrame(getObject(), UPDATE_SLEEP(delay));
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
LifetimeUpdate::~LifetimeUpdate()
{
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void LifetimeUpdate::setLifetimeRange( UnsignedInt minFrames, UnsignedInt maxFrames )
{
	UnsignedInt delay = calcSleepDelay(minFrames, maxFrames);
	setWakeFrame(getObject(), UPDATE_SLEEP(delay));
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
UnsignedInt LifetimeUpdate::calcSleepDelay(UnsignedInt minFrames, UnsignedInt maxFrames)
{
	UnsignedInt delay = GameLogicRandomValue( minFrames, maxFrames );
	if (delay < 1) delay = 1;
	m_dieFrame = TheGameLogic->getFrame() + delay;
	if (RetailFileLayers::enabled())
		m_lifetimeFrames = maxFrames > 0 ? delay : 0;
	return delay;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
UpdateSleepTime LifetimeUpdate::update()
{
	if (RetailFileLayers::enabled() && OathbreakersFadeAwayBehavior::begin(getObject()))
		return UPDATE_SLEEP_FOREVER;
	getObject()->kill(DAMAGE_UNRESISTABLE, getLifetimeUpdateModuleData()->m_deathType);
	return UPDATE_SLEEP_FOREVER;
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void LifetimeUpdate::crc( Xfer *xfer )
{

	// extend base class
	UpdateModule::crc( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void LifetimeUpdate::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = RetailFileLayers::enabled() ? 2 : 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	UpdateModule::xfer( xfer );

	// die frame
	xfer->xferUnsignedInt( &m_dieFrame );
	if (version >= 2)
		xfer->xferUnsignedInt(&m_lifetimeFrames);
	else if (RetailFileLayers::enabled() && xfer->getXferMode() == XFER_LOAD)
		m_lifetimeFrames = getLifetimeUpdateModuleData()->m_maxFrames;

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void LifetimeUpdate::loadPostProcess()
{

	// extend base class
	UpdateModule::loadPostProcess();

}
