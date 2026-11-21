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

// FILE: LifetimeUpdate.h /////////////////////////////////////////////////////////////////////////
// Author: Colin Day, December 2001
// Desc:   Update that will count down a lifetime and destroy object when it reaches zero
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "GameLogic/Damage.h"
#include "GameLogic/Module/UpdateModule.h"

class LifetimeUpdateModuleData : public UpdateModuleData
{
public:
	UnsignedInt m_minFrames;
	UnsignedInt m_maxFrames;
	Bool m_waitForWakeUp;
	Bool m_scoreKill;
	DeathType m_deathType;

	LifetimeUpdateModuleData()
	{
		m_minFrames = 0;
		m_maxFrames = 0;
		m_waitForWakeUp = FALSE;
		m_scoreKill = FALSE;
		m_deathType = DEATH_NORMAL;
	}

	static void buildFieldParse(MultiIniFieldParse& p);
};

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
class LifetimeUpdate : public UpdateModule
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( LifetimeUpdate, "LifetimeUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( LifetimeUpdate, LifetimeUpdateModuleData )

public:

	LifetimeUpdate( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	void setLifetimeRange( UnsignedInt minFrames, UnsignedInt maxFrames );
	UnsignedInt getDieFrame() const { return m_dieFrame; }
	UnsignedInt getLifetimeFrames() const { return m_lifetimeFrames; }

	virtual UpdateSleepTime update() override;

private:

	UnsignedInt calcSleepDelay(UnsignedInt minFrames, UnsignedInt maxFrames);

	UnsignedInt m_dieFrame;
	UnsignedInt m_lifetimeFrames = 0;
};
