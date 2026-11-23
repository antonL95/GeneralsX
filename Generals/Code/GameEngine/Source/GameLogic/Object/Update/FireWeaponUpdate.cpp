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

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/RetailFileLayers.h"
#include "Common/Xfer.h"
#include "GameLogic/Object.h"
#include "GameLogic/AI.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/FireWeaponUpdate.h"
#include "GameLogic/WeaponSlotSelectionRules.h"
#include "GameLogic/WeaponStatus.h"


//-------------------------------------------------------------------------------------------------
FireWeaponUpdateModuleData::FireWeaponUpdateModuleData()
{
	m_weaponTemplate = nullptr;
}

//-------------------------------------------------------------------------------------------------
/*static*/ void FireWeaponUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
  UpdateModuleData::buildFieldParse(p);

	static const FieldParse dataFieldParse[] =
	{
		{ "Weapon",	INI::parseWeaponTemplate,	nullptr, offsetof( FireWeaponUpdateModuleData, m_weaponTemplate ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
  p.add(dataFieldParse);
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
FireWeaponUpdate::FireWeaponUpdate( Thing *thing, const ModuleData* moduleData ) :
	UpdateModule( thing, moduleData ),
	m_weapon(nullptr)
{
	const WeaponTemplate *tmpl = getFireWeaponUpdateModuleData()->m_weaponTemplate;
	if (tmpl)
	{
		m_weapon = TheWeaponStore->allocateNewWeapon(tmpl, PRIMARY_WEAPON);
		m_weapon->loadAmmoNow( getObject() );
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
FireWeaponUpdate::~FireWeaponUpdate()
{
	deleteInstance(m_weapon);
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
UpdateSleepTime FireWeaponUpdate::update()
{
	// If my weapon is ready, shoot it.
	if( isOkayToFire() )
	{
		m_weapon->forceFireWeapon( getObject(), getObject()->getPosition(), TRUE );
	}
	return UPDATE_SLEEP_NONE;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
Bool FireWeaponUpdate::isOkayToFire()
{
	if( m_weapon == nullptr )
		return FALSE;

	// Weapon is reloading
	if( m_weapon->getStatus() != READY_TO_FIRE )
		return FALSE;

	return TRUE;
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void FireWeaponUpdate::crc( Xfer *xfer )
{
	// extend base class
	UpdateModule::crc( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void FireWeaponUpdate::xfer( Xfer *xfer )
{

	// version
	constexpr XferVersion weaponVersion = 1;
	constexpr XferVersion emptyVersion = 2;
	const Bool retail = RetailFileLayers::enabled();
	const XferVersion currentVersion = retail ? emptyVersion : weaponVersion;
	XferVersion version = retail && !m_weapon ? emptyVersion : weaponVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	UpdateModule::xfer( xfer );

	// weapon
	if (version < emptyVersion)
		xfer->xferSnapshot(m_weapon);

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void FireWeaponUpdate::loadPostProcess()
{

	// extend base class
	UpdateModule::loadPostProcess();

}

DualWeaponBehavior::~DualWeaponBehavior() = default;

void DualWeaponBehaviorModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpdateModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "SwitchWeaponOnCloseRangeDistance", INI::parseReal, nullptr, offsetof(DualWeaponBehaviorModuleData, m_switchWeaponOnCloseRangeDistance) },
		{ "UseCloseRangeWhileMounted", INI::parseBool, nullptr, offsetof(DualWeaponBehaviorModuleData, m_useCloseRangeWhileMounted) },
		{ "MinimumSwitchTime", INI::parseDurationUnsignedInt, nullptr, offsetof(DualWeaponBehaviorModuleData, m_minimumSwitchTime) },
		{ "UseHordeRangeWeapon", INI::parseBool, nullptr, offsetof(DualWeaponBehaviorModuleData, m_useHordeRangeWeapon) },
		{ "UseRealVictimRange", INI::parseBool, nullptr, offsetof(DualWeaponBehaviorModuleData, m_useRealVictimRange) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

UpdateSleepTime DualWeaponBehavior::update()
{
	Object* obj = getObject();
	const AIUpdateInterface* ai = obj->getAI();
	if (!ai || obj->isEffectivelyDead())
		return UPDATE_SLEEP_FOREVER;

	if (obj->testWeaponSetFlag(WEAPONSET_HERO_MODE))
		return UPDATE_SLEEP(5);

	const Object* containedBy = obj->getContainedBy();
	const Bool inHorde = containedBy != nullptr && containedBy->isKindOf(KINDOF_HORDE);
	if (containedBy != nullptr && !inHorde)
		return UPDATE_SLEEP(5);

	const auto* data = getDualWeaponBehaviorModuleData();
	const Bool currentlyCloseRange = obj->testWeaponSetFlag(WEAPONSET_CLOSE_RANGE);
	const Bool isMounted = obj->testWeaponSetFlag(WEAPONSET_MOUNTED);
	Bool hasCloseEnemy = FALSE;

	if (!m_forceCloseRange && !(data->m_useHordeRangeWeapon && inHorde) &&
		!(data->m_useCloseRangeWhileMounted && isMounted))
	{
		const Real switchDist = data->m_switchWeaponOnCloseRangeDistance;
		if (switchDist > 0.0f)
		{
			const Object* enemy = nullptr;
			const Coord3D* selfPos = obj->getPosition();
			if (data->m_useRealVictimRange && ai->getCurrentVictim() != nullptr)
			{
				const Object* victim = ai->getCurrentVictim();
				if (!victim->isEffectivelyDead())
				{
					const Coord3D* vPos = victim->getPosition();
					if (WeaponSlotSelectionRules::isEnemyWithinDualWeaponRange(
						vPos->x - selfPos->x, vPos->y - selfPos->y, vPos->z - selfPos->z, switchDist))
					{
						enemy = victim;
					}
				}
			}
			else
			{
				enemy = TheAI->findClosestEnemy(obj, switchDist, AI::CAN_ATTACK);
				if (enemy && !enemy->isEffectivelyDead())
				{
					const Coord3D* ePos = enemy->getPosition();
					if (!WeaponSlotSelectionRules::isEnemyWithinDualWeaponRange(
						ePos->x - selfPos->x, ePos->y - selfPos->y, ePos->z - selfPos->z, switchDist))
					{
						enemy = nullptr;
					}
				}
			}
			hasCloseEnemy = (enemy != nullptr && !enemy->isEffectivelyDead());
		}
	}

	const UnsignedInt now = TheGameLogic->getFrame();
	const auto decision = WeaponSlotSelectionRules::evaluateDualWeaponDecision(
		currentlyCloseRange != FALSE,
		m_forceCloseRange != FALSE,
		data->m_useHordeRangeWeapon != FALSE,
		inHorde != FALSE,
		data->m_useCloseRangeWhileMounted != FALSE,
		isMounted != FALSE,
		hasCloseEnemy != FALSE,
		now,
		m_lastSwitchFrame,
		data->m_minimumSwitchTime);

	if (decision.sleepFrames > 0)
		return UPDATE_SLEEP(decision.sleepFrames);

	m_lastSwitchFrame = decision.nextLastSwitchFrame;
	if (decision.switched)
	{
		if (decision.shouldUseCloseRange)
		{
			obj->setWeaponSetFlag(WEAPONSET_CLOSE_RANGE);
			obj->setModelConditionState(MODELCONDITION_WEAPONSET_CLOSE_RANGE);
		}
		else
		{
			obj->clearWeaponSetFlag(WEAPONSET_CLOSE_RANGE);
			obj->clearModelConditionState(MODELCONDITION_WEAPONSET_CLOSE_RANGE);
		}
	}
	return UPDATE_SLEEP(5);
}

void DualWeaponBehavior::crc(Xfer* xfer)
{
	UpdateModule::crc(xfer);
	xfer->xferBool(&m_forceCloseRange);
	xfer->xferUnsignedInt(&m_lastSwitchFrame);
	UnsignedInt wakeDelay = static_cast<UnsignedInt>(getWakeFrame());
	xfer->xferUnsignedInt(&wakeDelay);
}

void DualWeaponBehavior::xfer(Xfer* xfer)
{
	XferVersion version = 1;
	xfer->xferVersion(&version, 1);
	UpdateModule::xfer(xfer);
	xfer->xferBool(&m_forceCloseRange);
	xfer->xferUnsignedInt(&m_lastSwitchFrame);
}

void DualWeaponBehavior::loadPostProcess()
{
	UpdateModule::loadPostProcess();
}
