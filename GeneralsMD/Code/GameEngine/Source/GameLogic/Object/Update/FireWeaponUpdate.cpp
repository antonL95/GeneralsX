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

// FILE: FireWeaponUpdate.h /////////////////////////////////////////////////////////////////////////
// Author: Graham Smallwood, August 2002
// Desc:   Update fires a weapon at its own feet as quickly as the weapon allows
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/RetailFileLayers.h"
#include "Common/INIAudit.h"
#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"
#include "GameLogic/Object.h"
#include "GameLogic/Module/FireWeaponUpdate.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/WeaponStatus.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/RetailTestCommands.h"

#include <cstdio>



//-------------------------------------------------------------------------------------------------
FireWeaponUpdateModuleData::FireWeaponUpdateModuleData()
{
	m_weaponTemplate = nullptr;
  m_initialDelayFrames = 0;
	m_exclusiveWeaponDelay = 0;
}

//-------------------------------------------------------------------------------------------------
/*static*/ void FireWeaponUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
  UpdateModuleData::buildFieldParse(p);

	static const FieldParse dataFieldParse[] =
	{
		{ "Weapon",								INI::parseWeaponTemplate,	      nullptr, offsetof( FireWeaponUpdateModuleData, m_weaponTemplate ) },
		{ "InitialDelay",					INI::parseDurationUnsignedInt,	nullptr, offsetof( FireWeaponUpdateModuleData, m_initialDelayFrames ) },
		{ "ExclusiveWeaponDelay",	INI::parseDurationUnsignedInt,	nullptr, offsetof( FireWeaponUpdateModuleData, m_exclusiveWeaponDelay ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
  p.add(dataFieldParse);

	if (RetailFileLayers::enabled() || INIAudit::isEnabled())
	{
		static const FieldParse retailFields[] =
		{
			{ "AliveOnly", INI::parseBool, nullptr, offsetof(FireWeaponUpdateModuleData, m_aliveOnly) },
			{ "ChargingModeTrigger", INI::parseBool, nullptr, offsetof(FireWeaponUpdateModuleData, m_chargingModeTrigger) },
			{ "HeroModeTrigger", INI::parseBool, nullptr, offsetof(FireWeaponUpdateModuleData, m_heroModeTrigger) },
			{ "FireWeaponNugget", parseNugget, nullptr, offsetof(FireWeaponUpdateModuleData, m_nuggets) },
			{ nullptr, nullptr, nullptr, 0 }
		};
		p.add(retailFields);
	}
}

void FireWeaponUpdateModuleData::parseNugget(INI* ini, void*, void* store, const void*)
{
	static const FieldParse fields[] =
	{
		{ "WeaponName", INI::parseAsciiString, nullptr, offsetof(Nugget, weaponName) },
		{ "FireDelay", INI::parseDurationUnsignedInt, nullptr, offsetof(Nugget, delay) },
		{ "OneShot", INI::parseBool, nullptr, offsetof(Nugget, oneShot) },
		{ "Offset", INI::parseCoord3D, nullptr, offsetof(Nugget, offset) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	Nugget nugget;
	ini->initFromINI(&nugget, fields);
	static_cast<std::vector<Nugget>*>(store)->push_back(std::move(nugget));
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


  m_initialDelayFrame = TheGameLogic->getFrame() + getFireWeaponUpdateModuleData()->m_initialDelayFrames;

	if (RetailFileLayers::enabled() && !m_weapon)
	{
		for (const auto& nugget : getFireWeaponUpdateModuleData()->m_nuggets)
		{
			const WeaponTemplate* nested = nugget.weaponName.isEmpty() ? nullptr : TheWeaponStore->findWeaponTemplate(nugget.weaponName);
			Weapon* weapon = nested ? TheWeaponStore->allocateNewWeapon(nested, PRIMARY_WEAPON) : nullptr;
			if (weapon)
				weapon->loadAmmoNow(getObject());
			m_payloads.push_back({weapon, TheGameLogic->getFrame() + nugget.delay});
		}
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
FireWeaponUpdate::~FireWeaponUpdate()
{
	deleteInstance(m_weapon);
	for (const auto& payload : m_payloads)
		deleteInstance(payload.weapon);
}

void FireWeaponUpdate::setRetailDamageFieldWeapon(const WeaponTemplate* weapon)
{
	deleteInstance(m_weapon);
	m_weapon = TheWeaponStore->allocateNewWeapon(weapon, PRIMARY_WEAPON);
	m_weapon->loadAmmoNow(getObject());
	m_initialDelayFrame = TheGameLogic->getFrame() + 1;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
UpdateSleepTime FireWeaponUpdate::update()
{

  if ( TheGameLogic->getFrame() < m_initialDelayFrame )
    return UPDATE_SLEEP_NONE;

	if (RetailFileLayers::enabled())
	{
		const auto* data = getFireWeaponUpdateModuleData();
		const Object* object = getObject();
		const auto& conditions = object->getLogicModelConditions();
		if (object->isDestroyed() || (data->m_aliveOnly && object->isEffectivelyDead()) ||
			(data->m_chargingModeTrigger && !conditions.test(MODELCONDITION_CHARGING)) ||
			(data->m_heroModeTrigger && !conditions.test(MODELCONDITION_HERO)))
			return UPDATE_SLEEP_NONE;

		const UnsignedInt frame = TheGameLogic->getFrame();
		for (size_t index = 0; !m_weapon && index < m_payloads.size(); ++index)
		{
			Payload& payload = m_payloads[index];
			if (payload.spent || frame <= payload.startFrame || !payload.weapon ||
				payload.weapon->getStatus() != READY_TO_FIRE)
				continue;

			const auto& nugget = data->m_nuggets[index];
			const Coord3D& forward = *object->getUnitDirectionVector2D();
			Coord3D target = *object->getPosition();
			target.x += forward.x * nugget.offset.x - forward.y * nugget.offset.y;
			target.y += forward.y * nugget.offset.x + forward.x * nugget.offset.y;
			if (RetailTestCommands::tracesCombat())
				std::fprintf(stderr, "TEST timed weapon frame=%u source=%u(%s) weapon=%s target=%.1f,%.1f,%.1f\n",
					frame, static_cast<UnsignedInt>(object->getID()), object->getTemplate()->getName().str(),
					payload.weapon->getName().str(), target.x, target.y, target.z);
			payload.weapon->forceFireWeapon(object, &target);
			payload.spent = nugget.oneShot;
			if (object->isDestroyed() || (data->m_aliveOnly && object->isEffectivelyDead()))
				break;
		}
	}

	// If my weapon is ready, shoot it.
	if( isOkayToFire() )
	{
		m_weapon->forceFireWeapon( getObject(), getObject()->getPosition() );
	}
	return UPDATE_SLEEP_NONE;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
Bool FireWeaponUpdate::isOkayToFire()
{
	const Object *me = getObject();
	const FireWeaponUpdateModuleData *data = getFireWeaponUpdateModuleData();

	if( m_weapon == nullptr )
		return FALSE;

	// Weapon is reloading
	if( m_weapon->getStatus() != READY_TO_FIRE )
		return FALSE;

	if( me->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION) )
		return FALSE; // no hitting with a 0% building, cheater

	// Firing a real weapon suppresses this module
	if( data->m_exclusiveWeaponDelay > 0  &&  ( TheGameLogic->getFrame() < (me->getLastShotFiredFrame() + data->m_exclusiveWeaponDelay) ) )
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
	constexpr XferVersion weaponVersion = 2;
	constexpr XferVersion emptyVersion = 3;
	constexpr XferVersion payloadVersion = 4;
	const Bool retail = RetailFileLayers::enabled();
	const XferVersion currentVersion = retail ? payloadVersion : weaponVersion;
	const Bool hasOverride = m_weapon && !getFireWeaponUpdateModuleData()->m_weaponTemplate;
	XferVersion version = !m_payloads.empty() || hasOverride ? payloadVersion : retail && !m_weapon ? emptyVersion : weaponVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	UpdateModule::xfer( xfer );

	// weapon
	if (version < emptyVersion)
		xfer->xferSnapshot(m_weapon);

  if ( version >= 2 )
    xfer->xferUnsignedInt( &m_initialDelayFrame );

	if (version >= payloadVersion)
	{
		AsciiString overrideName = hasOverride ? m_weapon->getName() : AsciiString();
		xfer->xferAsciiString(&overrideName);
		if (overrideName.isNotEmpty())
		{
			if (xfer->getXferMode() == XFER_LOAD)
			{
				const WeaponTemplate* weapon = TheWeaponStore->findWeaponTemplate(overrideName);
				if (!weapon)
					throw INI_INVALID_DATA;
				deleteInstance(m_weapon);
				m_weapon = TheWeaponStore->allocateNewWeapon(weapon, PRIMARY_WEAPON);
			}
			xfer->xferSnapshot(m_weapon);
		}
		UnsignedInt count = static_cast<UnsignedInt>(m_payloads.size());
		xfer->xferUnsignedInt(&count);
		if (count != m_payloads.size())
			throw INI_INVALID_DATA;
		for (auto& payload : m_payloads)
		{
			xfer->xferUnsignedInt(&payload.startFrame);
			xfer->xferBool(&payload.spent);
			Bool hasWeapon = payload.weapon != nullptr;
			xfer->xferBool(&hasWeapon);
			if (hasWeapon != (payload.weapon != nullptr))
				throw INI_INVALID_DATA;
			if (hasWeapon)
				xfer->xferSnapshot(payload.weapon);
		}
	}
}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void FireWeaponUpdate::loadPostProcess()
{

	// extend base class
	UpdateModule::loadPostProcess();

}

AimWeaponBehavior::~AimWeaponBehavior() = default;

void AimWeaponBehaviorModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpdateModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "AimLowThreshold", INI::parseReal, nullptr, offsetof(AimWeaponBehaviorModuleData, m_low) },
		{ "AimHighThreshold", INI::parseReal, nullptr, offsetof(AimWeaponBehaviorModuleData, m_high) },
		{ "AimNearDistance", INI::parseReal, nullptr, offsetof(AimWeaponBehaviorModuleData, m_near) },
		{ "AimFarDistance", INI::parseReal, nullptr, offsetof(AimWeaponBehaviorModuleData, m_far) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

UpdateSleepTime AimWeaponBehavior::update()
{
	Object* self = getObject();
	const Weapon* weapon = self->getCurrentWeapon();
	const AIUpdateInterface* ai = self->getAI();
	const ModelConditionFlags aim = MAKE_MODELCONDITION_MASK5(
		MODELCONDITION_AIM_LOW, MODELCONDITION_AIM_STRAIGHT, MODELCONDITION_AIM_HIGH,
		MODELCONDITION_AIM_NEAR, MODELCONDITION_AIM_FAR);
	ModelConditionFlags next;
	if (weapon && weapon->getTemplate()->isAimingWeapon() && ai &&
		(self->testStatus(OBJECT_STATUS_IS_AIMING_WEAPON) || self->testStatus(OBJECT_STATUS_IS_FIRING_WEAPON)))
	{
		const auto* data = getAimWeaponBehaviorModuleData();
		const Object* victim = ai->getCurrentVictim();
		const Coord3D* target = victim ? victim->getPosition() : ai->getGoalPosition();
		const Coord3D* source = self->getPosition();
		const Real dx = target->x - source->x;
		const Real dy = target->y - source->y;
		const Real distance = std::max(1.0f, WWMath::SqrtfOrigin(dx * dx + dy * dy));
		const Real slope = (target->z - source->z) / distance;
		next.set(slope > data->m_high ? MODELCONDITION_AIM_HIGH :
			slope < data->m_low ? MODELCONDITION_AIM_LOW : MODELCONDITION_AIM_STRAIGHT);
		if (distance < data->m_near)
			next.set(MODELCONDITION_AIM_NEAR);
		else if (data->m_far > 0.0f && distance > data->m_far)
			next.set(MODELCONDITION_AIM_FAR);
	}
	ModelConditionFlags clear = aim;
	clear.clear(next);
	if (!self->getLogicModelConditions().testSetAndClear(next, clear))
		self->clearAndSetModelConditionFlags(clear, next);
	return UPDATE_SLEEP_NONE;
}

void AimWeaponBehavior::crc(Xfer* xfer)
{
	UpdateModule::crc(xfer);
}

void AimWeaponBehavior::xfer(Xfer* xfer)
{
	XferVersion version = 1;
	xfer->xferVersion(&version, 1);
	UpdateModule::xfer(xfer);
}

void AimWeaponBehavior::loadPostProcess()
{
	UpdateModule::loadPostProcess();
}
