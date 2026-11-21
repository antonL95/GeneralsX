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

// FILE: CostModifierUpgrade.h /////////////////////////////////////////////////
//-----------------------------------------------------------------------------
//
//                       Electronic Arts Pacific.
//
//                       Confidential Information
//                Copyright (C) 2002 - All Rights Reserved
//
//-----------------------------------------------------------------------------
//
//	created:	Aug 2002
//
//	Filename: 	CostModifierUpgrade.h
//
//	author:		Chris Huybregts
//
//	purpose:
//
//-----------------------------------------------------------------------------
///////////////////////////////////////////////////////////////////////////////

#pragma once

//-----------------------------------------------------------------------------
// SYSTEM INCLUDES ////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// USER INCLUDES //////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
#include "GameLogic/Module/UpgradeModule.h"
#include "Common/KindOf.h"
#include "Common/SpecialPowerINI.h"
//-----------------------------------------------------------------------------
// FORWARD REFERENCES /////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
class Thing;
class Player;
class UpgradeTemplate;
class ThingTemplate;

//-----------------------------------------------------------------------------
// TYPE DEFINES ///////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// INLINING ///////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// EXTERNALS //////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------

// FORWARD REFERENCES /////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
class CostModifierUpgradeModuleData : public UpgradeModuleData
{

public:

	CostModifierUpgradeModuleData();

	static void buildFieldParse(MultiIniFieldParse& p);

	Real m_percentage;
	KindOfMaskType m_kindOf;
	std::vector<Real> m_retailPercentages;
	SpecialPowerObjectFilter m_retailFilter;
	AsciiString m_retailLabel;
	std::vector<AsciiString> m_retailUpgrades;
	Bool m_retailStartsActive = FALSE;
	Bool m_retailSlaughter = FALSE;
	Bool m_retailUpgradeDiscount = FALSE;

	static void addRetailFields(MultiIniFieldParse& p);
	static void parsePercentage(INI* ini, void* instance, void* store, const void* userData);
	Bool usesRetailDiscount() const;
};

//-------------------------------------------------------------------------------------------------
/** The OCL upgrade module */
//-------------------------------------------------------------------------------------------------
class CostModifierUpgrade : public UpgradeModule
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( CostModifierUpgrade, "CostModifierUpgrade" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( CostModifierUpgrade, CostModifierUpgradeModuleData );

public:

	CostModifierUpgrade( Thing *thing, const ModuleData* moduleData );
	const CostModifierUpgradeModuleData* activeRetailData() const;
	static Real retailBuildFactor(const Player& player, const ThingTemplate& type);
	static Real retailUpgradeFactor(const Player& player, const UpgradeTemplate& upgrade);
	static Bool retailLabel(const Object& object, AsciiString& label, Real& discount);
	// virtual destructor prototype defined by MemoryPoolObject

	virtual void onDelete() override;																///< we have some work to do when this module goes away
	virtual void onCapture( Player *oldOwner, Player *newOwner ) override;

protected:

	virtual void upgradeImplementation() override; ///< Here's the actual work of Upgrading
	virtual Bool isSubObjectsUpgrade() override { return false; }

};
