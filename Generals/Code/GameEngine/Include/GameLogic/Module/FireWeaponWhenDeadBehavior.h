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

// FILE: FireWeaponWhenDeadBehavior.h /////////////////////////////////////////////////////////////////////////
// Author: Colin Day, December 2001
// Desc:   Update that will count down a lifetime and destroy object when it reaches zero
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "Common/ModuleFactory.h"
#include "GameLogic/Module/UpdateModule.h"
#include "GameLogic/Damage.h"
#include "GameLogic/Module/DieModule.h"
#include "GameLogic/Module/UpgradeModule.h"


//-------------------------------------------------------------------------------------------------
class FireWeaponWhenDeadBehaviorModuleData : public BehaviorModuleData
{
public:
	UpgradeMuxData				m_upgradeMuxData;
	Bool									m_initiallyActive;
	DieMuxData						m_dieMuxData;
	const WeaponTemplate* m_deathWeapon;						///< fire this weapon when we are damaged

	Bool m_activeDuringConstruction = false;
	UnsignedInt m_delay = 0;
	Coord3D m_weaponOffset{};

	FireWeaponWhenDeadBehaviorModuleData()
	{
		m_initiallyActive = false;
		m_deathWeapon = nullptr;
	}

	static void buildFieldParse(MultiIniFieldParse& p)
	{
		static const FieldParse dataFieldParse[] =
		{
			{ "StartsActive",	INI::parseBool, nullptr, offsetof( FireWeaponWhenDeadBehaviorModuleData, m_initiallyActive ) },
			{ "DeathWeapon", INI::parseWeaponTemplate,	nullptr, offsetof( FireWeaponWhenDeadBehaviorModuleData, m_deathWeapon ) },
			{ nullptr, nullptr, nullptr, 0 }
		};

		BehaviorModuleData::buildFieldParse(p);
		p.add(dataFieldParse);
		if (ModuleFactory::registersRotwkModules())
		{
			static const FieldParse retailFields[] =
			{
				{ "ActiveDuringConstruction", INI::parseBool, nullptr, offsetof(FireWeaponWhenDeadBehaviorModuleData, m_activeDuringConstruction) },
				{ "DelayTime", INI::parseDurationUnsignedInt, nullptr, offsetof(FireWeaponWhenDeadBehaviorModuleData, m_delay) },
				{ "WeaponOffset", INI::parseCoord3D, nullptr, offsetof(FireWeaponWhenDeadBehaviorModuleData, m_weaponOffset) },
				{ nullptr, nullptr, nullptr, 0 }
			};
			p.add(retailFields);
		}
		p.add(UpgradeMuxData::getFieldParse(), offsetof( FireWeaponWhenDeadBehaviorModuleData, m_upgradeMuxData ));
		p.add(DieMuxData::getFieldParse(), offsetof( FireWeaponWhenDeadBehaviorModuleData, m_dieMuxData ));
	}
};

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
class FireWeaponWhenDeadBehavior : public UpdateModule,
																	 public UpgradeMux,
																	 public DieModuleInterface
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( FireWeaponWhenDeadBehavior, "FireWeaponWhenDeadBehavior" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( FireWeaponWhenDeadBehavior, FireWeaponWhenDeadBehaviorModuleData )

public:

	FireWeaponWhenDeadBehavior( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	// module methods
	static Int getInterfaceMask()
	{
		return MODULEINTERFACE_UPGRADE | MODULEINTERFACE_DIE |
			(ModuleFactory::registersRotwkModules() ? MODULEINTERFACE_UPDATE : 0);
	}

	// BehaviorModule
	virtual UpgradeModuleInterface* getUpgrade() override { return this; }
	virtual DieModuleInterface* getDie() override { return this; }

	// DamageModuleInterface
	virtual void onDie( const DamageInfo *damageInfo ) override;

	UpdateModuleInterface* getUpdate() override;
	UpdateSleepTime update() override;
	DisabledMaskType getDisabledTypesToProcess() const override { return DISABLEDMASK_ALL; }

protected:

	virtual void upgradeImplementation() override
	{
		// nothing!
	}

	virtual void getUpgradeActivationMasks(UpgradeMaskType& activation, UpgradeMaskType& conflicting) const override
	{
		getFireWeaponWhenDeadBehaviorModuleData()->m_upgradeMuxData.getUpgradeActivationMasks(activation, conflicting);
	}

	virtual void performUpgradeFX() override
	{
		getFireWeaponWhenDeadBehaviorModuleData()->m_upgradeMuxData.performUpgradeFX(getObject());
	}

	virtual Bool requiresAllActivationUpgrades() const override
	{
		return getFireWeaponWhenDeadBehaviorModuleData()->m_upgradeMuxData.m_requiresAllTriggers;
	}

	Bool isUpgradeActive() const { return isAlreadyUpgraded(); }

	virtual Bool isSubObjectsUpgrade() override { return false; }

private:
	void fireDeathWeapon(const DamageInfo* damageInfo);
	void xferPendingFire(Xfer* xfer);

	UnsignedInt m_fireFrame = 0;
	DeathType m_pendingDeathType = DEATH_NORMAL;
};
