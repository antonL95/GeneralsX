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

// FILE: ObjectCreationUpgrade.h ///////////////////////////////////////////////////////////////////////////
// Author: Matthew D. Campbell, April 2002
// Desc:	 upgrades that spawn OCLs
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "GameLogic/Module/UpgradeModule.h"
#include "GameLogic/Module/UpdateModule.h"

// FORWARD REFERENCES /////////////////////////////////////////////////////////////////////////////
class Thing;
class Player;
class ObjectCreationList;

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
class ObjectCreationUpgradeModuleData : public UpgradeModuleData
{

public:

	ObjectCreationUpgradeModuleData();

	static void buildFieldParse(MultiIniFieldParse& p);

	const ObjectCreationList *m_ocl;			///< the object creation list to make
	AsciiString m_grantUpgrade;
	UnsignedInt m_delay = 0;
	UnsignedInt m_fadeInFrames = 0;
	AsciiString m_spawnTemplate;
	Coord3D m_spawnOffset{0, 0, 0};
	Bool m_destroyWhenSold = FALSE;

};

class RetailObjectCreationUpgrade final : public UpdateModule, public UpgradeMux
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(RetailObjectCreationUpgrade, "RetailObjectCreationUpgrade")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(RetailObjectCreationUpgrade, ObjectCreationUpgradeModuleData)

public:
	RetailObjectCreationUpgrade(Thing* thing, const ModuleData* data);
	static Int getInterfaceMask() { return MODULEINTERFACE_UPGRADE | MODULEINTERFACE_UPDATE; }
	UpgradeModuleInterface* getUpgrade() override { return this; }
	UpdateSleepTime update() override;
	Bool resetUpgrade(const UpgradeMaskType& mask) override;
	void onDelete() override;
	void onCapture(Player* oldOwner, Player* newOwner) override;
	Bool getSpawnDeckHeight(ObjectID child, Real& height) const;
	Object* getSlavedSpawn() const;

private:
	void upgradeImplementation() override;
	void getUpgradeActivationMasks(UpgradeMaskType& activation, UpgradeMaskType& conflicting) const override;
	void performUpgradeFX() override;
	void processUpgradeRemoval() override;
	Bool requiresAllActivationUpgrades() const override;
	Bool isSubObjectsUpgrade() override { return FALSE; }

	Bool m_pending = FALSE;
	ObjectID m_spawned = INVALID_ID;
};

class SlaveWatcherBehaviorModuleData final : public BehaviorModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& p);

	AsciiString m_removeUpgrade;
	AsciiString m_grantUpgrade;
	Bool m_shareUpgrades = FALSE;
};

class SlaveWatcherBehavior final : public BehaviorModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(SlaveWatcherBehavior, "SlaveWatcherBehavior")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(SlaveWatcherBehavior, SlaveWatcherBehaviorModuleData)

public:
	SlaveWatcherBehavior(Thing* thing, const ModuleData* data);
	static Int getInterfaceMask() { return 0; }
	void onSpawnGone();
	void shareUpgrades(Object& child) const;
};

//-------------------------------------------------------------------------------------------------
/** The OCL upgrade module */
//-------------------------------------------------------------------------------------------------
class ObjectCreationUpgrade : public UpgradeModule
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( ObjectCreationUpgrade, "ObjectCreationUpgrade" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( ObjectCreationUpgrade, ObjectCreationUpgradeModuleData );

public:

	ObjectCreationUpgrade( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype defined by MemoryPoolObject

	virtual void onDelete() override;																///< we have some work to do when this module goes away

protected:

	virtual void upgradeImplementation() override; ///< Here's the actual work of Upgrading
	virtual Bool isSubObjectsUpgrade() override { return false; }

};
