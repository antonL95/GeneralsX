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

// FILE: Weapon.cpp ///////////////////////////////////////////////////////////////////////////////
// Author: Colin Day, November 2001
// Desc:   Weapon descriptions
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#define DEFINE_DEATH_NAMES
#define DEFINE_WEAPONBONUSCONDITION_NAMES
#define DEFINE_WEAPONBONUSFIELD_NAMES
#define DEFINE_WEAPONCOLLIDEMASK_NAMES
#define DEFINE_WEAPONAFFECTSMASK_NAMES
#define DEFINE_WEAPONRELOAD_NAMES
#define DEFINE_WEAPONPREFIRE_NAMES

#include "Common/crc.h"
#include "Common/CRCDebug.h"
#include "Common/GameAudio.h"
#include "Common/GameState.h"
#include "Common/INI.h"
#include "Common/INIAudit.h"
#include "Common/PerfTimer.h"
#include "Common/RandomValue.h"
#include "Common/RetailFileLayers.h"
#include "Common/RetailSaveBlocks.h"
#include "Common/RotwkDuration.h"
#include "Common/Player.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"

#include "GameClient/Drawable.h"
#include "GameClient/FXList.h"
#include "GameClient/InGameUI.h"
#include "GameClient/ParticleSys.h"

#include "GameLogic/Damage.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/RetailTestCommands.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/Module/BehaviorModule.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Module/LaserUpdate.h"
#include "GameLogic/Module/UpdateModule.h"
#include "GameLogic/Module/SpecialPowerCompletionDie.h"
#include "GameLogic/Module/AssaultTransportAIUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/ObjectCreationList.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/Module/AttributeModifierHelper.h"
#include "GameLogic/WeaponNuggetRules.h"
#include "GameLogic/Module/ActiveBody.h"

#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/AssistedTargetingUpdate.h"
#include "GameLogic/Module/ProjectileStreamUpdate.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Module/SpawnBehavior.h"
#include "GameLogic/TerrainLogic.h"

#define RATIONALIZE_ATTACK_RANGE
#define ATTACK_RANGE_IS_2D

#ifdef ATTACK_RANGE_IS_2D
	const DistanceCalculationType ATTACK_RANGE_CALC_TYPE = FROM_BOUNDINGSPHERE_2D;
#else
	const DistanceCalculationType ATTACK_RANGE_CALC_TYPE = FROM_BOUNDINGSPHERE_3D;
#endif


// damage is ALWAYS 3d
const DistanceCalculationType DAMAGE_RANGE_CALC_TYPE = FROM_BOUNDINGSPHERE_3D;

//-------------------------------------------------------------------------------------------------
static void parsePerVetLevelAsciiString( INI* ini, void* /*instance*/, void * store, const void* /*userData*/ )
{
	AsciiString* s = (AsciiString*)store;
	VeterancyLevel v = (VeterancyLevel)INI::scanIndexList(ini->getNextToken(), TheVeterancyNames);
	s[v] = ini->getNextAsciiString();
}

//-------------------------------------------------------------------------------------------------
static void parseAllVetLevelsAsciiString( INI* ini, void* /*instance*/, void * store, const void* /*userData*/ )
{
	AsciiString* s = (AsciiString*)store;
	AsciiString a = ini->getNextAsciiString();
	for (Int i = LEVEL_FIRST; i <= LEVEL_LAST; ++i)
		s[i] = a;
}

//-------------------------------------------------------------------------------------------------
static void parsePerVetLevelFXList( INI* ini, void* /*instance*/, void * store, const void* /*userData*/ )
{
	typedef const FXList* ConstFXListPtr;
	ConstFXListPtr* s = (ConstFXListPtr*)store;
	VeterancyLevel v = (VeterancyLevel)INI::scanIndexList(ini->getNextToken(), TheVeterancyNames);
	const FXList* fx = nullptr;
	INI::parseFXList(ini, nullptr, &fx, nullptr);
	s[v] = fx;
}

//-------------------------------------------------------------------------------------------------
static void parseAllVetLevelsFXList( INI* ini, void* /*instance*/, void * store, const void* /*userData*/ )
{
	typedef const FXList* ConstFXListPtr;
	ConstFXListPtr* s = (ConstFXListPtr*)store;
	const FXList* fx = nullptr;
	INI::parseFXList(ini, nullptr, &fx, nullptr);
	for (Int i = LEVEL_FIRST; i <= LEVEL_LAST; ++i)
		s[i] = fx;
}

//-------------------------------------------------------------------------------------------------
static void parsePerVetLevelPSys( INI* ini, void* /*instance*/, void * store, const void* /*userData*/ )
{
	typedef const ParticleSystemTemplate* ConstParticleSystemTemplatePtr;
	ConstParticleSystemTemplatePtr* s = (ConstParticleSystemTemplatePtr*)store;
	VeterancyLevel v = (VeterancyLevel)INI::scanIndexList(ini->getNextToken(), TheVeterancyNames);
	ConstParticleSystemTemplatePtr pst = nullptr;
	INI::parseParticleSystemTemplate(ini, nullptr, &pst, nullptr);
	s[v] = pst;
}

//-------------------------------------------------------------------------------------------------
static void parseAllVetLevelsPSys( INI* ini, void* /*instance*/, void * store, const void* /*userData*/ )
{
	typedef const ParticleSystemTemplate* ConstParticleSystemTemplatePtr;
	ConstParticleSystemTemplatePtr* s = (ConstParticleSystemTemplatePtr*)store;
	ConstParticleSystemTemplatePtr pst = nullptr;
	INI::parseParticleSystemTemplate(ini, nullptr, &pst, nullptr);
	for (Int i = LEVEL_FIRST; i <= LEVEL_LAST; ++i)
		s[i] = pst;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// PUBLIC DATA ////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
WeaponStore *TheWeaponStore = nullptr;					///< the weapon store definition


///////////////////////////////////////////////////////////////////////////////////////////////////
// PRIVATE DATA ///////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
const FieldParse WeaponTemplate::TheWeaponTemplateFieldParseTable[] =
{

	{ "PrimaryDamage",						INI::parseReal,													nullptr,							offsetof(WeaponTemplate, m_primaryDamage) },
	{ "PrimaryDamageRadius",			INI::parseReal,													nullptr,							offsetof(WeaponTemplate, m_primaryDamageRadius) },
	{ "SecondaryDamage",					INI::parseReal,													nullptr,							offsetof(WeaponTemplate, m_secondaryDamage) },
	{ "SecondaryDamageRadius",		INI::parseReal,													nullptr,							offsetof(WeaponTemplate, m_secondaryDamageRadius) },
	{ "ShockWaveAmount",					INI::parseReal,													nullptr,							offsetof(WeaponTemplate, m_shockWaveAmount) },
	{ "ShockWaveRadius",					INI::parseReal,													nullptr,							offsetof(WeaponTemplate, m_shockWaveRadius) },
	{ "ShockWaveTaperOff",				INI::parseReal,													nullptr,							offsetof(WeaponTemplate, m_shockWaveTaperOff) },
	{ "AttackRange",							INI::parseReal,													nullptr,							offsetof(WeaponTemplate, m_attackRange) },
	{ "MinimumAttackRange",				INI::parseReal,													nullptr,							offsetof(WeaponTemplate, m_minimumAttackRange) },
	{ "RequestAssistRange",				INI::parseReal,													nullptr,							offsetof(WeaponTemplate, m_requestAssistRange) },
	{ "AcceptableAimDelta",				INI::parseAngleReal,										nullptr,							offsetof(WeaponTemplate, m_aimDelta) },
	{ "ScatterRadius",						INI::parseReal,													nullptr,							offsetof(WeaponTemplate, m_scatterRadius) },
	{ "ScatterTargetScalar",			INI::parseReal,													nullptr,							offsetof(WeaponTemplate, m_scatterTargetScalar) },
	{ "ScatterRadiusVsInfantry",	INI::parseReal,													nullptr,							offsetof( WeaponTemplate, m_infantryInaccuracyDist ) },
	{ "DamageType",								DamageTypeFlags::parseSingleBitFromINI,	nullptr,							offsetof(WeaponTemplate, m_damageType) },
	{ "DamageStatusType",					ObjectStatusMaskType::parseSingleBitFromINI,	nullptr,				offsetof(WeaponTemplate, m_damageStatusType) },
	{ "DeathType",								INI::parseIndexList,										TheDeathNames,		offsetof(WeaponTemplate, m_deathType) },
	{ "WeaponSpeed",							INI::parseVelocityReal,									nullptr,							offsetof(WeaponTemplate, m_weaponSpeed) },
	{ "MinWeaponSpeed",						INI::parseVelocityReal,									nullptr,							offsetof(WeaponTemplate, m_minWeaponSpeed) },
	{ "ScaleWeaponSpeed",					INI::parseBool,													nullptr,							offsetof(WeaponTemplate, m_isScaleWeaponSpeed) },
	{ "WeaponRecoil",							INI::parseAngleReal,										nullptr,							offsetof(WeaponTemplate, m_weaponRecoil) },
	{ "MinTargetPitch",						INI::parseAngleReal,										nullptr,							offsetof(WeaponTemplate, m_minTargetPitch) },
	{ "MaxTargetPitch",						INI::parseAngleReal,										nullptr,							offsetof(WeaponTemplate, m_maxTargetPitch) },
	{ "RadiusDamageAngle",				INI::parseAngleReal,										nullptr,							offsetof(WeaponTemplate, m_radiusDamageAngle) },
	{ "ProjectileObject",					INI::parseAsciiString,									nullptr,							offsetof(WeaponTemplate, m_projectileName) },
	{ "FireSound",								INI::parseAudioEventRTS,								nullptr,							offsetof(WeaponTemplate, m_fireSound) },
	{ "FireSoundLoopTime",				INI::parseDurationUnsignedInt,					nullptr,							offsetof(WeaponTemplate, m_fireSoundLoopTime) },
	{ "FireFX",											parseAllVetLevelsFXList,							nullptr,							offsetof(WeaponTemplate, m_fireFXs) },
	{ "ProjectileDetonationFX",			parseAllVetLevelsFXList,							nullptr,							offsetof(WeaponTemplate, m_projectileDetonateFXs) },
	{ "FireOCL",										parseAllVetLevelsAsciiString,					nullptr,							offsetof(WeaponTemplate, m_fireOCLNames) },
	{ "ProjectileDetonationOCL",		parseAllVetLevelsAsciiString,					nullptr,							offsetof(WeaponTemplate, m_projectileDetonationOCLNames) },
	{ "ProjectileExhaust",					parseAllVetLevelsPSys,								nullptr,							offsetof(WeaponTemplate, m_projectileExhausts) },
	{ "VeterancyFireFX",										parsePerVetLevelFXList,				nullptr,							offsetof(WeaponTemplate, m_fireFXs) },
	{ "VeterancyProjectileDetonationFX",		parsePerVetLevelFXList,				nullptr,							offsetof(WeaponTemplate, m_projectileDetonateFXs) },
	{ "VeterancyFireOCL",										parsePerVetLevelAsciiString,	nullptr,							offsetof(WeaponTemplate, m_fireOCLNames) },
	{ "VeterancyProjectileDetonationOCL",		parsePerVetLevelAsciiString,	nullptr,							offsetof(WeaponTemplate, m_projectileDetonationOCLNames) },
	{ "VeterancyProjectileExhaust",					parsePerVetLevelPSys,					nullptr,							offsetof(WeaponTemplate, m_projectileExhausts) },
	{ "ClipSize",									INI::parseInt,													nullptr,							offsetof(WeaponTemplate, m_clipSize) },
	{ "ContinuousFireOne",				INI::parseInt,													nullptr,							offsetof(WeaponTemplate, m_continuousFireOneShotsNeeded) },
	{ "ContinuousFireTwo",				INI::parseInt,													nullptr,							offsetof(WeaponTemplate, m_continuousFireTwoShotsNeeded) },
	{ "ContinuousFireCoast",			INI::parseDurationUnsignedInt,					nullptr,							offsetof(WeaponTemplate, m_continuousFireCoastFrames) },
 	{ "AutoReloadWhenIdle",				INI::parseDurationUnsignedInt,					nullptr,							offsetof(WeaponTemplate, m_autoReloadWhenIdleFrames) },
	{ "ClipReloadTime",						WeaponTemplate::parseClipReloadTime,			nullptr,							offsetof(WeaponTemplate, m_clipReloadTime) },
	{ "DelayBetweenShots",				WeaponTemplate::parseShotDelay,					nullptr,							0 },
	{ "ShotsPerBarrel",						INI::parseInt,													nullptr,							offsetof(WeaponTemplate, m_shotsPerBarrel) },
	{ "DamageDealtAtSelfPosition",INI::parseBool,													nullptr,							offsetof(WeaponTemplate, m_damageDealtAtSelfPosition) },
	{ "RadiusDamageAffects",			INI::parseBitString32,	TheWeaponAffectsMaskNames,				offsetof(WeaponTemplate, m_affectsMask) },
	{ "ProjectileCollidesWith",		INI::parseBitString32,	TheWeaponCollideMaskNames,				offsetof(WeaponTemplate, m_collideMask) },
	{ "AntiAirborneVehicle",			INI::parseBitInInt32,										(void*)WEAPON_ANTI_AIRBORNE_VEHICLE,	offsetof(WeaponTemplate, m_antiMask) },
	{ "AntiGround",								INI::parseBitInInt32,										(void*)WEAPON_ANTI_GROUND,						offsetof(WeaponTemplate, m_antiMask) },
	{ "AntiProjectile",						INI::parseBitInInt32,										(void*)WEAPON_ANTI_PROJECTILE,				offsetof(WeaponTemplate, m_antiMask) },
	{ "AntiSmallMissile",					INI::parseBitInInt32,										(void*)WEAPON_ANTI_SMALL_MISSILE,			offsetof(WeaponTemplate, m_antiMask) },
	{ "AntiMine",									INI::parseBitInInt32,										(void*)WEAPON_ANTI_MINE,							offsetof(WeaponTemplate, m_antiMask) },
	{ "AntiParachute",						INI::parseBitInInt32,										(void*)WEAPON_ANTI_PARACHUTE,					offsetof(WeaponTemplate, m_antiMask) },
	{ "AntiAirborneInfantry",			INI::parseBitInInt32,										(void*)WEAPON_ANTI_AIRBORNE_INFANTRY, offsetof(WeaponTemplate, m_antiMask) },
	{ "AntiBallisticMissile",			INI::parseBitInInt32,										(void*)WEAPON_ANTI_BALLISTIC_MISSILE, offsetof(WeaponTemplate, m_antiMask) },
	{ "AutoReloadsClip",					INI::parseIndexList,										TheWeaponReloadNames,							offsetof(WeaponTemplate, m_reloadType) },
	{ "ProjectileStreamName",			INI::parseAsciiString,									nullptr,							offsetof(WeaponTemplate, m_projectileStreamName) },
	{ "LaserName",								INI::parseAsciiString,									nullptr,							offsetof(WeaponTemplate, m_laserName) },
	{ "LaserBoneName",						INI::parseAsciiString,									nullptr,							offsetof(WeaponTemplate, m_laserBoneName) },
	{ "WeaponBonus",							WeaponTemplate::parseWeaponBonusSet,		nullptr,							0 },
	{ "HistoricBonusTime",				INI::parseDurationUnsignedInt,					nullptr,							offsetof(WeaponTemplate, m_historicBonusTime) },
	{ "HistoricBonusRadius",			INI::parseReal,													nullptr,							offsetof(WeaponTemplate, m_historicBonusRadius) },
	{ "HistoricBonusCount",				INI::parseInt,													nullptr,							offsetof(WeaponTemplate, m_historicBonusCount) },
	{ "HistoricBonusWeapon",			INI::parseWeaponTemplate,								nullptr,							offsetof(WeaponTemplate, m_historicBonusWeapon) },
	{ "LeechRangeWeapon",					INI::parseBool,													nullptr,							offsetof(WeaponTemplate, m_leechRangeWeapon) },
	{ "ScatterTarget",						WeaponTemplate::parseScatterTarget,			nullptr,							0 },
	{ "CapableOfFollowingWaypoints", INI::parseBool,											nullptr,							offsetof(WeaponTemplate, m_capableOfFollowingWaypoint) },
	{ "ShowsAmmoPips",						INI::parseBool,													nullptr,							offsetof(WeaponTemplate, m_isShowsAmmoPips) },
	{ "AllowAttackGarrisonedBldgs", INI::parseBool,												nullptr,							offsetof(WeaponTemplate, m_allowAttackGarrisonedBldgs) },
	{ "PlayFXWhenStealthed",			INI::parseBool,													nullptr,							offsetof(WeaponTemplate, m_playFXWhenStealthed) },
	{ "PreAttackDelay",						INI::parseDurationUnsignedInt,					nullptr,							offsetof( WeaponTemplate, m_preAttackDelay ) },
	{ "PreAttackType",						INI::parseIndexList,										TheWeaponPrefireNames, offsetof(WeaponTemplate, m_prefireType) },
	{ "ContinueAttackRange",			INI::parseReal,													nullptr,							offsetof(WeaponTemplate, m_continueAttackRange) },
	{ "SuspendFXDelay",						INI::parseDurationUnsignedInt,					nullptr,							offsetof(WeaponTemplate, m_suspendFXDelay) },
	{ "MissileCallsOnDie",			INI::parseBool,													nullptr,							offsetof(WeaponTemplate, m_dieOnDetonate) },
	{ nullptr,												nullptr,																		nullptr,							0 }

};

const FieldParse WeaponTemplate::TheRotwkWeaponFieldParseTable[] =
{
	{ "PreAttackFX", INI::parseFXList, nullptr, offsetof(WeaponTemplate, m_preAttackFX) },
	{ "PreAttackRandomAmount", INI::parseDurationUnsignedInt, nullptr, offsetof(WeaponTemplate, m_preAttackRandomAmount) },
	{ "ShareTimers", INI::parseBool, nullptr, offsetof(WeaponTemplate, m_shareTimers) },
	{ "RequireFollowThru", INI::parseBool, nullptr, offsetof(WeaponTemplate, m_requireFollowThru) },
	{ "LockWhenUsing", INI::parseBool, nullptr, offsetof(WeaponTemplate, m_lockWhenUsing) },
	{ "OverrideVoiceAttackSound", INI::parseAudioEventRTS, nullptr, offsetof(WeaponTemplate, m_overrideVoiceAttackSound) },
	{ "OverrideVoiceEnterStateAttackSound", INI::parseAudioEventRTS, nullptr, offsetof(WeaponTemplate, m_overrideVoiceEnterStateAttackSound) },
	{ "ProjectileFilterInContainer", [](INI* ini, void* instance, void* store, const void* userData) {
		static_cast<WeaponTemplate*>(instance)->m_hasProjectileFilterInContainer = TRUE;
		SpecialPowerObjectFilter::parse(ini, instance, store, userData);
	}, nullptr, offsetof(WeaponTemplate, m_projectileFilterInContainer) },
	{ "MaxAttackPassengers", INI::parseInt, nullptr, offsetof(WeaponTemplate, m_maxAttackPassengers) },
	{ "PassengerProportionalAttack", INI::parseBool, nullptr, offsetof(WeaponTemplate, m_passengerProportionalAttack) },
	{ "ShouldPlayUnderAttackEvaEvent", INI::parseBool, nullptr, offsetof(WeaponTemplate, m_shouldPlayUnderAttackEvaEvent) },
	{ "ProjectileSelf", INI::parseBool, nullptr, offsetof(WeaponTemplate, m_projectileSelf) },
	{ "RotatingTurret", INI::parseBool, nullptr, offsetof(WeaponTemplate, m_rotatingTurret) },
	{ "CanBeDodged", INI::parseBool, nullptr, offsetof(WeaponTemplate, m_canBeDodged) },
	{ "CanSwoop", INI::parseBool, nullptr, offsetof(WeaponTemplate, m_canSwoop) },
	{ "DisableScatterForTargetsOnWall", INI::parseBool, nullptr, offsetof(WeaponTemplate, m_disableScatterForTargetsOnWall) },
	{ "IgnoreLinearFirstTarget", INI::parseBool, nullptr, offsetof(WeaponTemplate, m_ignoreLinearFirstTarget) },
	{ "LinearTarget", WeaponTemplate::parseLinearTarget, nullptr, offsetof(WeaponTemplate, m_linearTargets) },
	{ "PreferredTargetBone", INI::parseAsciiString, nullptr, offsetof(WeaponTemplate, m_preferredTargetBone) },
	{ "RangeBonus", INI::parseReal, nullptr, offsetof(WeaponTemplate, m_rangeBonus) },
	{ "RangeBonusMinHeight", INI::parseReal, nullptr, offsetof(WeaponTemplate, m_rangeBonusMinHeight) },
	{ "RangeBonusPerFoot", INI::parseReal, nullptr, offsetof(WeaponTemplate, m_rangeBonusPerFoot) },
	{ "ScatterIndependently", INI::parseBool, nullptr, offsetof(WeaponTemplate, m_scatterIndependently) },
	{ "DamageNugget", WeaponNugget::parseDamageNugget, nullptr, offsetof(WeaponTemplate, m_nuggets) },
	{ "DOTNugget", WeaponNugget::parseDOTNugget, nullptr, offsetof(WeaponTemplate, m_nuggets) },
	{ "ProjectileNugget", WeaponNugget::parseProjectileNugget, nullptr, offsetof(WeaponTemplate, m_nuggets) },
	{ "MetaImpactNugget", WeaponNugget::parseMetaImpactNugget, nullptr, offsetof(WeaponTemplate, m_nuggets) },
	{ "WeaponOCLNugget", WeaponNugget::parseWeaponOCLNugget, nullptr, offsetof(WeaponTemplate, m_nuggets) },
	{ "AttributeModifierNugget", WeaponNugget::parseAttributeModifierNugget, nullptr, offsetof(WeaponTemplate, m_nuggets) },
	{ "SlaveAttackNugget", WeaponNugget::parseSlaveAttackNugget, nullptr, offsetof(WeaponTemplate, m_nuggets) },
	{ "HordeAttackNugget", WeaponNugget::parseHordeAttackNugget, nullptr, offsetof(WeaponTemplate, m_nuggets) },
	{ "DamageFieldNugget", WeaponNugget::parseDamageFieldNugget, nullptr, offsetof(WeaponTemplate, m_nuggets) },
	{ "FiringDuration", INI::parseDurationUnsignedInt, nullptr, offsetof(WeaponTemplate, m_firingDuration) },
	{ "IdleAfterFiringDelay", INI::parseDurationUnsignedInt, nullptr, offsetof(WeaponTemplate, m_idleAfterFiringDelay) },
	{ "HoldAfterFiringDelay", INI::parseDurationUnsignedInt, nullptr, offsetof(WeaponTemplate, m_holdAfterFiringDelay) },
	{ "HoldDuringReload", INI::parseBool, nullptr, offsetof(WeaponTemplate, m_holdDuringReload) },
	{ "CanFireWhileMoving", INI::parseBool, nullptr, offsetof(WeaponTemplate, m_canFireWhileMoving) },
	{ "CanFireWhileCharging", INI::parseBool, nullptr, offsetof(WeaponTemplate, m_canFireWhileCharging) },
	{ "FinishAttackOnceStarted", INI::parseBool, nullptr, offsetof(WeaponTemplate, m_finishAttackOnceStarted) },
	{ "ForceDisplayPercentReady", INI::parseBool, nullptr, offsetof(WeaponTemplate, m_forceDisplayPercentReady) },
	{ "InstantLoadClipOnActivate", INI::parseBool, nullptr, offsetof(WeaponTemplate, m_instantLoadClipOnActivate) },
	{ "IsAimingWeapon", INI::parseBool, nullptr, offsetof(WeaponTemplate, m_isAimingWeapon) },
	{ "ChaseWeapon", INI::parseBool, nullptr, offsetof(WeaponTemplate, m_chaseWeapon) },
	{ "HitPassengerPercentage", INI::parsePercentToReal, nullptr, offsetof(WeaponTemplate, m_hitPassengerPercentage) },
	{ "FireFlankFX", INI::parseFXList, nullptr, offsetof(WeaponTemplate, m_fireFlankFX) },
	{ "FXTrigger", INI::parseAsciiString, nullptr, offsetof(WeaponTemplate, m_fxTrigger) },
	{ "HitPercentage", WeaponTemplate::parseHitPercentage, nullptr, offsetof(WeaponTemplate, m_hitPercentage) },
	{ "MaxWeaponSpeed", INI::parseVelocityReal, nullptr, offsetof(WeaponTemplate, m_maxWeaponSpeed) },
	{ "MeleeWeapon", INI::parseBool, nullptr, offsetof(WeaponTemplate, m_meleeWeapon) },
	{ "HitStoredTarget", INI::parseBool, nullptr, offsetof(WeaponTemplate, m_hitStoredTarget) },
	{ "BombardType", INI::parseBool, nullptr, offsetof(WeaponTemplate, m_bombardType) },
	{ "NoVictimNeeded", INI::parseBool, nullptr, offsetof(WeaponTemplate, m_noVictimNeeded) },
	{ "UseInnateAttributes", INI::parseBool, nullptr, offsetof(WeaponTemplate, m_useInnateAttributes) },
	{ "AntiAirborneMonster", INI::parseBitInInt32, (void*)WEAPON_ANTI_AIRBORNE_MONSTER, offsetof(WeaponTemplate, m_antiMask) },
	{ "AntiStructure", [](INI* ini, void*, void* store, const void*) {
		*static_cast<Int*>(store) = INI::scanBool(ini->getNextToken());
	}, nullptr, offsetof(WeaponTemplate, m_antiStructure) },
	{ nullptr, nullptr, nullptr, 0 }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// PUBLIC FUNCTIONS ///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
WeaponTemplate::WeaponTemplate() : m_nextTemplate(nullptr)
{

	m_name													= "NoNameWeapon";
	m_nameKey												= NAMEKEY_INVALID;
	m_primaryDamage									= 0.0f;
	m_primaryDamageRadius						= 0.0f;
	m_secondaryDamage								= 0.0f;
	m_secondaryDamageRadius					= 0.0f;
	m_attackRange										= 0.0f;
	m_minimumAttackRange						= 0.0f;
	m_requestAssistRange						= 0.0f;
	m_aimDelta											= 0.0f;
	m_scatterRadius									= 0.0f;
	m_scatterTargetScalar						= 0.0f;
	m_shockWaveAmount								= 0.0f;
	m_shockWaveRadius								= 0.0f;
	m_shockWaveTaperOff							= 0.0f;
	m_damageType										= DAMAGE_EXPLOSION;
	m_deathType											= DEATH_NORMAL;
	m_weaponSpeed										= 999999.0f;	// effectively instant
	m_minWeaponSpeed								= 999999.0f;	// effectively instant
	m_isScaleWeaponSpeed						= FALSE;
	m_weaponRecoil									= 0.0f;		// no recoil
	m_minTargetPitch								= -PI;
	m_maxTargetPitch								= PI;
	m_radiusDamageAngle							= PI;	// PI each way, so full circle
	m_projectileName.clear();					// no projectile
	m_projectileTmpl								= nullptr;
	for (Int i = LEVEL_FIRST; i <= LEVEL_LAST; ++i)
	{
		m_fireOCLNames[i].clear();
		m_projectileDetonationOCLNames[i].clear();
		m_projectileExhausts[i]					= nullptr;
		m_fireOCLs[i]										= nullptr;
		m_projectileDetonationOCLs[i]		= nullptr;
		m_fireFXs[i]										= nullptr;
		m_projectileDetonateFXs[i]			= nullptr;
	}
	m_damageDealtAtSelfPosition			= false;
	m_affectsMask										= (WEAPON_AFFECTS_ALLIES | WEAPON_AFFECTS_ENEMIES | WEAPON_AFFECTS_NEUTRALS);
	// most projectile weapons don't want to collide with nontargeted enemies/allies or trees...
	m_collideMask										= (WEAPON_COLLIDE_STRUCTURES);
	m_reloadType										= AUTO_RELOAD;
	m_prefireType										= PREFIRE_PER_SHOT;
	m_clipSize											= 0;
	m_continuousFireOneShotsNeeded	= INT_MAX;
	m_continuousFireTwoShotsNeeded	= INT_MAX;
	m_continuousFireCoastFrames			= 0;
 	m_autoReloadWhenIdleFrames			= 0;
	m_clipReloadTime								= 0;
	m_maxClipReloadTime								= 0;
	m_minDelayBetweenShots					= 0;
	m_maxDelayBetweenShots					= 0;
	m_fireSoundLoopTime							= 0;
	m_extraBonus										= nullptr;
	m_shotsPerBarrel								= 1;
	m_antiMask											= WEAPON_ANTI_GROUND;	// but not air or projectile.
	m_projectileStreamName.clear();
	m_laserName.clear();
	m_laserBoneName.clear();
	m_historicBonusTime							= 0;
	m_historicBonusCount						= 0;
	m_historicBonusRadius						= 0;
	m_historicBonusWeapon						= nullptr;
	m_leechRangeWeapon							= FALSE;
	m_capableOfFollowingWaypoint		= FALSE;
	m_isShowsAmmoPips								= FALSE;
	m_allowAttackGarrisonedBldgs		= FALSE;
	m_playFXWhenStealthed						= FALSE;
	m_preAttackDelay								= 0;
	m_continueAttackRange						= 0.0f;
	m_infantryInaccuracyDist				= 0.0f;
	m_damageStatusType							= OBJECT_STATUS_NONE;
	m_suspendFXDelay								= 0;
	m_dieOnDetonate						= FALSE;
	m_hitPercentage = 100.0f;
	m_firingDuration = 0;
	m_maxWeaponSpeed = RetailFileLayers::enabled() || INIAudit::isEnabled()
		? 999999.0f / RotwkDuration::engineFrames(1, LOGICFRAMES_PER_SECOND)
		: 0.0f;
	m_meleeWeapon = FALSE;
	m_hitStoredTarget = FALSE;
	m_bombardType = FALSE;
	m_noVictimNeeded = FALSE;
	m_useInnateAttributes = FALSE;

	m_historicDamageTriggerId = 0;
}

//-------------------------------------------------------------------------------------------------
WeaponTemplate::~WeaponTemplate()
{
	deleteInstance(m_nextTemplate);

	// delete any extra-bonus that's present
	deleteInstance(m_extraBonus);
}

// ------------------------------------------------------------------------------------------------
void WeaponTemplate::reset()
{
	m_historicDamage.clear();
}

//-------------------------------------------------------------------------------------------------
/*static*/ void WeaponTemplate::parseWeaponBonusSet( INI* ini, void *instance, void * /*store*/, const void* /*userData*/ )
{
	WeaponTemplate* self = (WeaponTemplate*)instance;

	if (!self->m_extraBonus)
		self->m_extraBonus = newInstance(WeaponBonusSet);

	self->m_extraBonus->parseWeaponBonusSet(ini);
}

//-------------------------------------------------------------------------------------------------
/*static*/ void WeaponTemplate::parseScatterTarget( INI* ini, void *instance, void * /*store*/, const void* /*userData*/ )
{
	// Accept multiple listings of Coord2D's.
	WeaponTemplate* self = (WeaponTemplate*)instance;

	Coord2D target;
	target.x = 0;
	target.y = 0;
	INI::parseCoord2D( ini, nullptr, &target, nullptr );

	self->m_scatterTargets.push_back(target);
}

void WeaponTemplate::parseLinearTarget(INI* ini, void*, void* store, const void*)
{
	LinearTarget target{};
	INI::parseCoord2D(ini, nullptr, &target.offset, nullptr);
	target.delay = INI::scanInt(ini->getNextSubToken("T"));
	static_cast<std::vector<LinearTarget>*>(store)->push_back(target);
}

//-------------------------------------------------------------------------------------------------
static void parseRetailWeaponDelay(INI* ini, Int& minFrames, Int& maxFrames)
{
	const char* token = ini->getNextToken(ini->getSepsColon());
	const Bool isRange = stricmp(token, "Min") == 0;
	const Int minMsecs = INI::scanInt(isRange ? ini->getNextToken(ini->getSepsColon()) : token);
	Int maxMsecs = minMsecs;
	if (isRange)
	{
		const char* maxLabel = ini->getNextTokenOrNull(ini->getSepsColon());
		if (maxLabel && stricmp(maxLabel, "Max") == 0)
			maxMsecs = INI::scanInt(ini->getNextToken(ini->getSepsColon()));
	}
	if (minMsecs < 0 || maxMsecs < minMsecs)
		throw INI_INVALID_DATA;

	minFrames = RotwkDuration::retailFrames(minMsecs);
	maxFrames = RotwkDuration::retailFrames(maxMsecs);
}

void WeaponTemplate::parseClipReloadTime(INI* ini, void* instance, void* store, const void* userData)
{
	WeaponTemplate* self = static_cast<WeaponTemplate*>(instance);
	if (RetailFileLayers::enabled() || INIAudit::isEnabled())
	{
		parseRetailWeaponDelay(ini, self->m_clipReloadTime, self->m_maxClipReloadTime);
	}
	else
	{
		INI::parseDurationUnsignedInt(ini, instance, store, userData);
		self->m_maxClipReloadTime = self->m_clipReloadTime;
	}
}

/*static*/ void WeaponTemplate::parseShotDelay( INI* ini, void *instance, void * /*store*/, const void* /*userData*/ )
{
	// This smart parser allows both a single number for traditional delay, and a labeled pair of numbers for a delay range
	WeaponTemplate* self = (WeaponTemplate*)instance;
	if (RetailFileLayers::enabled() || INIAudit::isEnabled())
	{
		parseRetailWeaponDelay(ini, self->m_minDelayBetweenShots, self->m_maxDelayBetweenShots);
		return;
	}
	static const char *MIN_LABEL = "Min";
	static const char *MAX_LABEL = "Max";

	const char* token = ini->getNextToken(ini->getSepsColon());
	if( stricmp(token, MIN_LABEL) == 0 )
	{
		// Two entry min/max
		self->m_minDelayBetweenShots = INI::scanInt(ini->getNextToken(ini->getSepsColon()));
		token = ini->getNextToken(ini->getSepsColon());
		if( stricmp(token, MAX_LABEL) != 0 )
		{
			// Messed up double entry
			self->m_maxDelayBetweenShots = self->m_minDelayBetweenShots;
		}
		else
			self->m_maxDelayBetweenShots = INI::scanInt(ini->getNextToken(ini->getSepsColon()));
	}
	else
	{
		// single entry, as in no label so the first token is just a number
		self->m_minDelayBetweenShots = INI::scanInt(token);
		self->m_maxDelayBetweenShots = self->m_minDelayBetweenShots;
	}

	// No matter what we have now, we want to convert it to frames from msec.
	// ShotDelay used to use parseDurationUnsignedInt, and we are expanding on that.
	self->m_minDelayBetweenShots = WWMath::Ceil(ConvertDurationFromMsecsToFrames((Real)self->m_minDelayBetweenShots));
	self->m_maxDelayBetweenShots = WWMath::Ceil(ConvertDurationFromMsecsToFrames((Real)self->m_maxDelayBetweenShots));

}

void WeaponTemplate::parseHitPercentage(INI* ini, void*, void* store, const void*)
{
	*static_cast<Real*>(store) = INI::scanReal(ini->getNextToken(ini->getSepsPercent()));
}

//-------------------------------------------------------------------------------------------------
void WeaponTemplate::postProcessLoad()
{
	if (!TheThingFactory)
	{
		DEBUG_CRASH(("you must call this after TheThingFactory is inited"));
		return;
	}

	if (m_projectileName.isEmpty())
	{
		m_projectileTmpl = nullptr;
	}
	else
	{
		m_projectileTmpl = TheThingFactory->findTemplate(m_projectileName);
		DEBUG_ASSERTCRASH(m_projectileTmpl, ("projectile %s not found!",m_projectileName.str()));

#ifdef DEBUG_LOGGING
		if (m_projectileTmpl && m_primaryDamage > 0.0)
		{
			const Real projectileRadius = m_projectileTmpl->getTemplateGeometryInfo().getMajorRadius();
			if (m_primaryDamageRadius < projectileRadius)
				DEBUG_LOG(("Weapon template %s has a PrimaryDamageRadius (%f) smaller than its projectile object's GeometryMajorRadius (%f)! This may lead to inconsistent damage application.", getName().str(), m_primaryDamageRadius, projectileRadius));
		}
#endif
	}

	for (Int i = LEVEL_FIRST; i <= LEVEL_LAST; ++i)
	{
		// And the OCL if there is one
		if (m_fireOCLNames[i].isEmpty())
		{
			m_fireOCLs[i] = nullptr;
		}
		else
		{
			m_fireOCLs[i] = TheObjectCreationListStore->findObjectCreationList(m_fireOCLNames[i].str() );
			DEBUG_ASSERTCRASH(m_fireOCLs[i], ("OCL %s not found in a weapon!",m_fireOCLNames[i].str()));
		}
		m_fireOCLNames[i].clear();

		// And the other OCL if there is one
		if (m_projectileDetonationOCLNames[i].isEmpty() )
		{
			m_projectileDetonationOCLs[i] = nullptr;
		}
		else
		{
			m_projectileDetonationOCLs[i] = TheObjectCreationListStore->findObjectCreationList(m_projectileDetonationOCLNames[i].str() );
			DEBUG_ASSERTCRASH(m_projectileDetonationOCLs[i], ("OCL %s not found in a weapon!",m_projectileDetonationOCLNames[i].str()));
		}
		m_projectileDetonationOCLNames[i].clear();
	}

	for (WeaponNugget& nugget : m_nuggets)
		nugget.postProcessLoad();
}

//-------------------------------------------------------------------------------------------------
Real WeaponTemplate::getAttackRange(const WeaponBonus& bonus, Real heightAdvantage) const
{
	Real range = m_attackRange * bonus.getField(WeaponBonus::RANGE);
	if (heightAdvantage >= m_rangeBonusMinHeight && hasHeightRangeBonus())
		range += m_rangeBonus + (heightAdvantage - m_rangeBonusMinHeight) * m_rangeBonusPerFoot;
#ifdef RATIONALIZE_ATTACK_RANGE
	// Note - undersize by 1/4 of a pathfind cell, so that the goal is not teetering on the edge
	// of firing range.  jba.
	const Real UNDERSIZE = PATHFIND_CELL_SIZE_F*0.25f;
	Real r = range - UNDERSIZE;
	if (r < 0.0f) r = 0.0f;
	return r;
#else
// fudge this a little to account for pathfinding roundoff & such
	const Real ATTACK_RANGE_FUDGE = 1.05f;
	return range * ATTACK_RANGE_FUDGE;
#endif
}

Bool WeaponTemplate::getPreferredTargetPosition(const Object* target, Coord3D& position) const
{
	Coord3D bone;
	if (!target || m_preferredTargetBone.isEmpty() ||
		!target->getSingleLogicalBonePosition(m_preferredTargetBone.str(), &bone, nullptr))
		return FALSE;
	position = bone;
	return TRUE;
}

WeaponTemplate::TargetReach WeaponTemplate::getTargetReach(const Object* source, const Object* target)
{
	TargetReach reach{*target->getPosition(), 0.0f, FALSE};
	Coord3D offset;
	if (const auto* ai = target->getAI(); ai && ai->getSneakyTargetingOffset(&offset))
	{
		reach.position.x += offset.x;
		reach.position.y += offset.y;
		reach.position.z += offset.z;
		reach.distanceSquared = ThePartitionManager->getDistanceSquared(source, &reach.position, ATTACK_RANGE_CALC_TYPE);
		reach.targetsPosition = TRUE;
	}
	else if (target->isKindOf(KINDOF_BRIDGE))
	{
		TBridgeAttackInfo info;
		TheTerrainLogic->getBridgeAttackPoints(target, &info);
		const Real first = ThePartitionManager->getDistanceSquared(source, &info.attackPoint1, ATTACK_RANGE_CALC_TYPE);
		const Real second = ThePartitionManager->getDistanceSquared(source, &info.attackPoint2, ATTACK_RANGE_CALC_TYPE);
		reach.position = second < first ? info.attackPoint2 : info.attackPoint1;
		reach.distanceSquared = second < first ? second : first;
	}
	else
	{
		reach.distanceSquared = ThePartitionManager->getDistanceSquared(source, target, ATTACK_RANGE_CALC_TYPE);
	}
	return reach;
}

//-------------------------------------------------------------------------------------------------
Real WeaponTemplate::getMinimumAttackRange() const
{
#ifdef RATIONALIZE_ATTACK_RANGE
	// Note - undersize by 1/4 of a pathfind cell, so that the goal is not teetering on the edge
	// of firing range.  jba.
	const Real UNDERSIZE = PATHFIND_CELL_SIZE_F*0.25f;
	Real r = m_minimumAttackRange - UNDERSIZE;
	if (r < 0.0f) r = 0.0f;
	return r;
#else
	return m_minimumAttackRange;
#endif
}

//-------------------------------------------------------------------------------------------------
Real WeaponTemplate::getUnmodifiedAttackRange() const
{
	return m_attackRange;
}

static Int retailWeaponDelayInEngineFrames(Int frames, Real rateOfFire)
{
	const Int adjustedFrames = rateOfFire > 0.0f ? REAL_TO_INT_FLOOR(frames / rateOfFire) : frames;
	return RotwkDuration::engineFrames(adjustedFrames, LOGICFRAMES_PER_SECOND);
}

//-------------------------------------------------------------------------------------------------
Int WeaponTemplate::getDelayBetweenShots(const WeaponBonus& bonus) const
{
	// yes, divide, not multiply; the larger the rate-of-fire bonus, the shorter
	// we want the delay time to be.
	Int delayToUse;
	if( m_minDelayBetweenShots == m_maxDelayBetweenShots )
		delayToUse = m_minDelayBetweenShots; // Random number thing doesn't like this case
	else
		delayToUse = GameLogicRandomValue( m_minDelayBetweenShots, m_maxDelayBetweenShots );

	Real bonusROF = bonus.getField(WeaponBonus::RATE_OF_FIRE);
	//CRCDEBUG_LOG(("WeaponTemplate::getDelayBetweenShots() - min:%d max:%d val:%d, bonusROF=%g/%8.8X",
		//m_minDelayBetweenShots, m_maxDelayBetweenShots, delayToUse, bonusROF, AS_INT(bonusROF)));

	if (RetailFileLayers::enabled())
		return retailWeaponDelayInEngineFrames(delayToUse, bonusROF);
	return REAL_TO_INT_FLOOR(delayToUse / bonusROF);
}

//-------------------------------------------------------------------------------------------------
Int WeaponTemplate::getClipReloadTime(const WeaponBonus& bonus, Bool rollRange) const
{
	Int delay = m_clipReloadTime;
	if (rollRange && m_clipReloadTime < m_maxClipReloadTime)
		delay = GameLogicRandomValue(m_clipReloadTime, m_maxClipReloadTime);
	const Real bonusROF = bonus.getField(WeaponBonus::RATE_OF_FIRE);
	if (RetailFileLayers::enabled())
		return retailWeaponDelayInEngineFrames(RotwkDuration::clipReloadFrames(delay), bonusROF);
	return REAL_TO_INT_FLOOR(delay / bonusROF);
}

//-------------------------------------------------------------------------------------------------
Int WeaponTemplate::getPreAttackDelay( const WeaponBonus& bonus ) const
{
	return m_preAttackDelay * bonus.getField( WeaponBonus::PRE_ATTACK );
}

//-------------------------------------------------------------------------------------------------
Real WeaponTemplate::getPrimaryDamage(const WeaponBonus& bonus) const
{
	return m_primaryDamage * bonus.getField(WeaponBonus::DAMAGE);
}

//-------------------------------------------------------------------------------------------------
Real WeaponTemplate::getPrimaryDamageRadius(const WeaponBonus& bonus) const
{
	return m_primaryDamageRadius * bonus.getField(WeaponBonus::RADIUS);
}

//-------------------------------------------------------------------------------------------------
Real WeaponTemplate::getSecondaryDamage(const WeaponBonus& bonus) const
{
	return m_secondaryDamage * bonus.getField(WeaponBonus::DAMAGE);
}

//-------------------------------------------------------------------------------------------------
Real WeaponTemplate::getSecondaryDamageRadius(const WeaponBonus& bonus) const
{
	return m_secondaryDamageRadius * bonus.getField(WeaponBonus::RADIUS);
}

//-------------------------------------------------------------------------------------------------
Bool WeaponTemplate::isContactWeapon() const
{
	if (m_meleeWeapon)
		return TRUE;
#ifdef RATIONALIZE_ATTACK_RANGE
	// Note - undersize by 1/4 of a pathfind cell, so that the goal is not teetering on the edge
	// of firing range.  jba.
	const Real UNDERSIZE = PATHFIND_CELL_SIZE_F*0.25f;
	return (m_attackRange - UNDERSIZE) < PATHFIND_CELL_SIZE_F;
#else
// fudge this a little to account for pathfinding roundoff & such
	const Real ATTACK_RANGE_FUDGE = 1.05f;
	return m_attackRange * ATTACK_RANGE_FUDGE < PATHFIND_CELL_SIZE_F;
#endif
}

//-------------------------------------------------------------------------------------------------
Real WeaponTemplate::estimateWeaponTemplateDamage(
	const Object *sourceObj,
	const Object *victimObj,
	const Coord3D* victimPos,
	const WeaponBonus& bonus
) const
{
	if (sourceObj == nullptr || (victimObj == nullptr && victimPos == nullptr))
	{
		DEBUG_CRASH(("bad args to estimate"));
		return 0.0f;
	}

	if (hasNuggets())
		return estimateNuggetDamage(sourceObj, victimObj, bonus, 0);

	const Real damageAmount = getPrimaryDamage(bonus);
	if ( victimObj == nullptr )
	{
		return damageAmount;
	}

	DamageType damageType = getDamageType();
	DeathType deathType = getDeathType();

	if ( victimObj->isKindOf(KINDOF_SHRUBBERY) )
	{
		if (deathType == DEATH_BURNED)
		{
			// this is just a nonzero value, to ensure we can target shrubbery with flame weapons, regardless...
			return 1.0f;
		}
		else
		{
			return 0.0f;
		}
	}


  // hmm.. must be shooting a firebase or such, if there is noone home to take the bullet, return 0!
  if ( victimObj->isKindOf( KINDOF_STRUCTURE) && damageType == DAMAGE_SNIPER )
  {

#if RETAIL_COMPATIBLE_CRC || PRESERVE_SNIPING_EMPTY_STINGER_SITES
    if ( victimObj->getContain() )
    {
      if ( victimObj->getContain()->getContainCount() == 0 )
        return 0.0f;
    }
#else
		// TheSuperHackers @bugfix Stubbjax 22/06/2026 Only allow targeting Stinger Sites when they contain Soldiers.
		const Bool hasOccupants = victimObj->getContain() && victimObj->getContain()->getContainCount() > 0;
		const Bool hasSlaves = victimObj->getSpawnBehaviorInterface() && victimObj->getSpawnBehaviorInterface()->getSlaveCount() > 0;

		if (!hasOccupants && !hasSlaves)
			return 0.0f;
#endif
  }




	if ( damageType == DAMAGE_SURRENDER || m_allowAttackGarrisonedBldgs )
	{
		ContainModuleInterface* contain = victimObj->getContain();
		if( contain && contain->getContainCount() > 0 && contain->isGarrisonable() && !contain->isImmuneToClearBuildingAttacks() )
		{
			// this is just a nonzero value, to ensure we can target garrisoned things with surrender weapons, regardless...
			return 1.0f;
		}
	}

	if( damageType == DAMAGE_DISARM )
	{
		if( victimObj->isKindOf( KINDOF_MINE ) || victimObj->isKindOf( KINDOF_BOOBY_TRAP ) || victimObj->isKindOf( KINDOF_DEMOTRAP ) )
		{
			// this is just a nonzero value, to ensure we can target mines with disarm weapons, regardless...
			return 1.0f;
		}
		return 0.0f;
	}
	if( damageType == DAMAGE_DEPLOY && !victimObj->isAirborneTarget() )
	{
		return 1.0f;
	}

	//@todo Kris need to examine the DAMAGE_HACK type for damage estimation purposes.
	//Likely this damage type will have threat implications that won't properly be dealt with until resolved.

	DamageInfoInput damageInfo;
	damageInfo.m_damageType = damageType;
	damageInfo.m_deathType = deathType;
	damageInfo.m_sourceID = sourceObj->getID();
	damageInfo.m_amount = damageAmount;
	return victimObj->estimateDamage(damageInfo);
}

//-------------------------------------------------------------------------------------------------
Bool WeaponTemplate::shouldProjectileCollideWith(
	const Object* projectileLauncher,
	const Object* projectile,
	const Object* thingWeCollidedWith,
	ObjectID intendedVictimID	// could be INVALID_ID for a position-shot
) const
{
 	if (!projectile || !thingWeCollidedWith)
 		return false;

	if (HordeContain::get(thingWeCollidedWith))
		return false;

	// if it's our intended victim, we want to collide with it, regardless of any other considerations.
	if (intendedVictimID == thingWeCollidedWith->getID())
		return true;

 	if (projectileLauncher != nullptr)
 	{

 		// Don't hit your own launcher, ever.
 		if (projectileLauncher == thingWeCollidedWith)
 			return false;

 		// If our launcher is inside something, and that something is 'thingWeCollidedWith' we won't collide
 		const Object *launcherContainedBy = projectileLauncher->getContainedBy();
 		if( launcherContainedBy == thingWeCollidedWith )
 			return false;

 	}

	// never bother burning already-burned things. (srj)
	if (getDamageType() == DAMAGE_FLAME || getDamageType() == DAMAGE_PARTICLE_BEAM)
	{
		if (thingWeCollidedWith->testStatus(OBJECT_STATUS_BURNED))
		{
			return false;
		}
	}

	// horrible special case for airplanes sitting on airfields: the projectile might
	// "collide" with the airfield's (invisible) collision geometry when a resting plane
	// is targeted. we don't want this. special case it:
	if (thingWeCollidedWith->isKindOf(KINDOF_FS_AIRFIELD))
	{
		//
		// ok, so if we are an airfield, and our intended victim has a reserved space
		// with us, it "belongs" to us and collisions intended for it should not detonate
		// as a result of colliding with us.
		//
		// notes:
		//	-- we have already established that "thingWeCollidedWith" is not the intended victim (above)
		//	-- this does NOT verify that the plane is actually parked at the airfield; it might be elsewhere
		//			(but if it is, it's highly unlikely that this sort of collision could occur)
		//
		for (BehaviorModule** i = thingWeCollidedWith->getBehaviorModules(); *i; ++i)
		{
			ParkingPlaceBehaviorInterface* pp = (*i)->getParkingPlaceBehaviorInterface();
			if (pp != nullptr && pp->hasReservedSpace(intendedVictimID))
				return false;
		}
	}

	// if something has a Sneaky Target offset, it is momentarily immune to being hit...
	// normally this shouldn't happen, but occasionally can by accident. so avoid it. (srj)
	const AIUpdateInterface* ai = thingWeCollidedWith->getAI();
	if (ai != nullptr && ai->getSneakyTargetingOffset(nullptr))
	{
		return false;
	}

	Int requiredMask = 0;

	Relationship r = projectile->getRelationship(thingWeCollidedWith);
	if (r == ALLIES) requiredMask = WEAPON_COLLIDE_ALLIES;
	else if (r == ENEMIES) requiredMask = WEAPON_COLLIDE_ENEMIES;

	if (thingWeCollidedWith->isKindOf(KINDOF_STRUCTURE))
	{
		if (thingWeCollidedWith->getControllingPlayer() == projectile->getControllingPlayer())
			requiredMask |= WEAPON_COLLIDE_CONTROLLED_STRUCTURES;
		else
			requiredMask |= WEAPON_COLLIDE_STRUCTURES;
	}
	if (thingWeCollidedWith->isKindOf(KINDOF_SHRUBBERY))					requiredMask |= WEAPON_COLLIDE_SHRUBBERY;
	if (thingWeCollidedWith->isKindOf(KINDOF_PROJECTILE))					requiredMask |= WEAPON_COLLIDE_PROJECTILE;
	if (thingWeCollidedWith->getTemplate()->getFenceWidth() > 0)	requiredMask |= WEAPON_COLLIDE_WALLS;
	if (thingWeCollidedWith->isKindOf(KINDOF_SMALL_MISSILE))			requiredMask |= WEAPON_COLLIDE_SMALL_MISSILES;			//All missiles are also projectiles!
	if (thingWeCollidedWith->isKindOf(KINDOF_BALLISTIC_MISSILE))	requiredMask |= WEAPON_COLLIDE_BALLISTIC_MISSILES;	//All missiles are also projectiles!

	// if any in requiredMask are present in collidemask, do the collision. (srj)
	if ((getProjectileCollideMask() & requiredMask) != 0)
		return true;

	//DEBUG_LOG(("Rejecting projectile collision between %s and %s!",projectile->getTemplate()->getName().str(),thingWeCollidedWith->getTemplate()->getName().str()));
	return false;
}

//-------------------------------------------------------------------------------------------------
static Object* selectWeaponPassenger(Object* victim, Real chance)
{
	const auto* contain = victim->getContain();
	if (!contain || !contain->getContainCount() || !contain->isEnclosingContainerFor(nullptr))
		return nullptr;

	const auto* passengers = contain->getContainedItemsList();
	const auto isLiving = [](const Object* object) { return !object->isEffectivelyDead(); };
	const Int count = std::count_if(passengers->begin(), passengers->end(), isLiving);
	if (!count || (chance < 1.0f && GameLogicRandomValueReal(0.0f, 1.0f) >= chance))
		return nullptr;

	Int index = count == 1 ? 0 : GameLogicRandomValue(0, count - 1);
	for (Object* passenger : *passengers)
		if (isLiving(passenger) && index-- == 0)
			return passenger;
	return nullptr;
}

UnsignedInt WeaponTemplate::fireWeaponTemplate
(
	const Object *sourceObj,
	WeaponSlotType wslot,
	Int specificBarrelToUse,
	Object *victimObj,
	const Coord3D* victimPos,
	const WeaponBonus& bonus,
	Bool isProjectileDetonation,
	Bool ignoreRanges,
	Weapon *firingWeapon,
	ObjectID* projectileID,
	Bool inflictDamage,
	ObjectID detonationVictimID,
	Bool playFireFX
) const
{

	//-extraLogging
	#if defined(RTS_DEBUG)
		AsciiString targetStr;
		if( TheGlobalData->m_extraLogging )
		{
			if( victimObj )
				targetStr.format( "%s", victimObj->getTemplate()->getName().str() );
			else if( victimPos )
				targetStr.format( "%d,%d,%d", victimPos->x, victimPos->y, victimPos->z );
			else
				targetStr.format( "SELF." );

			DEBUG_LOG( ("%d - WeaponTemplate::fireWeaponTemplate() begin - %s attacking %s",
				TheGameLogic->getFrame(), sourceObj->getTemplate()->getName().str(), targetStr.str() ) );
		}
	#endif
	//end -extraLogging

	//CRCDEBUG_LOG(("WeaponTemplate::fireWeaponTemplate() from %s", DescribeObject(sourceObj).str()));
	DEBUG_ASSERTCRASH(specificBarrelToUse >= 0, ("specificBarrelToUse should no longer be -1"));

	if (sourceObj == nullptr || (victimObj == nullptr && victimPos == nullptr))
	{
		return 0;
	}

	if (victimObj)
	{
		if (const HordeContain* enemy = HordeContain::get(victimObj))
		{
			victimObj = enemy->findAttackTarget(sourceObj, ATTACK_NEW_TARGET, CMD_FROM_AI, wslot);
			if (!victimObj)
				victimObj = enemy->findNearestLivingMember(*sourceObj->getPosition());
			if (!victimObj)
				return 0;
		}
	}

	DEBUG_ASSERTCRASH((m_primaryDamage > 0) || hasNuggets() || (victimObj == nullptr), ("You can't really shoot a zero damage weapon at an Object.") );

	ObjectID sourceID = sourceObj->getID();
	const Coord3D* sourcePos = sourceObj->getPosition();

	Real distSqr;
	ObjectID victimID;
	Coord3D victimPosStorage;
	if (victimObj)
	{
		DEBUG_ASSERTLOG(sourceObj != victimObj, ("*** firing weapon at self -- is this really what you want?"));
		victimPos = victimObj->getPosition();
		victimID = victimObj->getID();

		const TargetReach reach = getTargetReach(sourceObj, victimObj);
		distSqr = reach.distanceSquared;
		if (reach.targetsPosition || victimObj->isKindOf(KINDOF_BRIDGE))
		{
			victimPosStorage = reach.position;
			victimPos = &victimPosStorage;
		}
		if (reach.targetsPosition)
		{
			victimObj = nullptr;
			victimID = INVALID_ID;
		}
	}
	else
	{
		victimID = INVALID_ID;
		distSqr = ThePartitionManager->getDistanceSquared(sourceObj, victimPos, ATTACK_RANGE_CALC_TYPE);
	}

//	DEBUG_LOG(("WeaponTemplate::fireWeaponTemplate: firing weapon %s (source=%s, victim=%s)",
//		m_name.str(),sourceObj->getTemplate()->getName().str(),victimObj?victimObj->getTemplate()->getName().str():"null"));

	//Only perform this check if the weapon isn't a leech range weapon (which can have unlimited range!)
	if( !ignoreRanges && !isLeechRangeWeapon() )
	{
		Real attackRangeSqr = sqr(getAttackRange(bonus, sourcePos->z - victimPos->z));
		if (distSqr > attackRangeSqr)
		{
			//DEBUG_ASSERTCRASH(distSqr < 5*5 || distSqr < attackRangeSqr*1.2f, ("*** victim is out of range (%f vs %f) of this weapon -- why did we attempt to fire?",WWMath::SqrtfOrigin(distSqr),WWMath::SqrtfOrigin(attackRangeSqr)));

			//-extraLogging
			#if defined(RTS_DEBUG)
				if( TheGlobalData->m_extraLogging )
					DEBUG_LOG( ("FAIL 2 (distSqr %.2f > attackRangeSqr %.2f)", distSqr, attackRangeSqr ) );
			#endif
			//end -extraLogging

			return 0;
		}
	}

	if (!ignoreRanges)
	{
		Real minAttackRangeSqr = sqr(getMinimumAttackRange());
#ifdef RATIONALIZE_ATTACK_RANGE
		if (distSqr < minAttackRangeSqr && !isProjectileDetonation)
#else
		if (distSqr < minAttackRangeSqr-0.5f && !isProjectileDetonation)
#endif
		{
			DEBUG_ASSERTCRASH(distSqr > minAttackRangeSqr*0.8f, ("*** victim is closer than min attack range (%f vs %f) of this weapon -- why did we attempt to fire?",WWMath::SqrtfOrigin(distSqr),WWMath::SqrtfOrigin(minAttackRangeSqr)));

			//-extraLogging
			#if defined(RTS_DEBUG)
				if( TheGlobalData->m_extraLogging )
					DEBUG_LOG( ("FAIL 3 (distSqr %.2f< minAttackRangeSqr %.2f - 0.5f && !isProjectileDetonation %d)", distSqr, minAttackRangeSqr, isProjectileDetonation ) );
			#endif
			//end -extraLogging

			return 0;
		}
	}

	if (!isProjectileDetonation && m_holdAfterFiringDelay)
		sourceObj->holdForWeaponUntil(TheGameLogic->getFrame() + m_holdAfterFiringDelay);

	const Bool flank = playFireFX && !isProjectileDetonation && m_fireFlankFX && HordeContain::isFlanked(victimObj, sourceObj, FALSE);
	// call this even if FXList is null, because this also handles stuff like Gun Barrel Recoil
	if (playFireFX && sourceObj && sourceObj->getDrawable())
	{
		Coord3D targetPos;
		if( victimObj )
		{
			if (!RetailFileLayers::enabled() || !getPreferredTargetPosition(victimObj, targetPos))
				victimObj->getGeometryInfo().getCenterPosition(*victimObj->getPosition(), targetPos);
			else if (RetailTestCommands::tracesCombat())
				std::fprintf(stderr, "TEST weapon-aim frame=%u weapon=%s target=%.2f,%.2f,%.2f\n",
					TheGameLogic->getFrame(), m_name.str(), targetPos.x, targetPos.y, targetPos.z);
		}
		else
		{
			targetPos.set( *victimPos );
		}
		Real reAngle = getWeaponRecoilAmount();
		Real reDir = reAngle != 0.0f ? (WWMath::Atan2Origin(victimPos->y - sourcePos->y, victimPos->x - sourcePos->x)) : 0.0f;
		VeterancyLevel v = sourceObj->getVeterancyLevel();
		const FXList* fx = isProjectileDetonation ? getProjectileDetonateFX(v) : getFireFX(v);

		if (flank)
			fx = m_fireFlankFX;
		const Bool suspended = TheGameLogic->getFrame() < firingWeapon->getSuspendFXFrame();
		if (suspended)
			fx = nullptr;

		Bool handled;

		// TheSuperHackers @todo: Remove hardcoded KINDOF_MINE check and apply PlayFXWhenStealthed = Yes to the mine weapons instead.

		if (!sourceObj->isLogicallyVisible() &&									// if user watching cannot see us
			!sourceObj->isKindOf(KINDOF_MINE) &&								// and not a mine (which always do the FX, even if hidden)...
			!isPlayFXWhenStealthed()													// and not a weapon marked to playwhenstealthed
			)
		{
			handled = TRUE;		// then let's just pretend like we did the fx by returning true
		}
		else
		{
			if (!isProjectileDetonation && !suspended && m_fxTrigger.isNotEmpty() && !m_fxTrigger.isNone())
				sourceObj->getDrawable()->handleWeaponFXTrigger(m_fxTrigger);
			const AIUpdateInterface* ai = sourceObj->getAIUpdateInterface();
			const Bool rotatingFX = RetailFileLayers::enabled() && m_rotatingTurret && fx && !isProjectileDetonation &&
				ai && ai->getWhichTurretForWeaponSlot(wslot, nullptr, nullptr) != TURRET_INVALID;
			if (rotatingFX)
			{
				Matrix3D muzzleTransform(true);
				Coord3D muzzle;
				Weapon::calcProjectileLaunchPosition(sourceObj, wslot, specificBarrelToUse, muzzleTransform, muzzle, FALSE);
				FXList::doFXPos(fx, &muzzle, &muzzleTransform, getWeaponSpeed(), &targetPos, getPrimaryDamageRadius(bonus),
					sourceObj, victimObj);
			}
			handled = sourceObj->getDrawable()->handleWeaponFireFX(wslot,
																															specificBarrelToUse,
																															rotatingFX ? nullptr : fx,
																															getWeaponSpeed(),
																															reAngle,
																															reDir,
																															&targetPos,
																															getPrimaryDamageRadius(bonus),
																															RetailFileLayers::enabled() ? victimObj : nullptr
																															);
			handled = handled || rotatingFX;
		}

		if (handled == false && fx != nullptr)
		{
			// bah. just play it at the drawable's pos.
			//DEBUG_LOG(("*** WeaponFireFX not fully handled by the client"));
			const Coord3D* where = isContactWeapon() ? &targetPos : sourceObj->getDrawable()->getPosition();
			FXList::doFXPos(fx, where, sourceObj->getDrawable()->getTransformMatrix(), getWeaponSpeed(), &targetPos, getPrimaryDamageRadius(bonus),
				RetailFileLayers::enabled() ? sourceObj : nullptr, RetailFileLayers::enabled() ? victimObj : nullptr);
		}
	}

	if (playFireFX && !isProjectileDetonation && RetailTestCommands::tracesCombat() &&
		(m_fxTrigger.isNotEmpty() || m_fireFlankFX))
	{
		std::fprintf(stderr, "TEST weapon-fx frame=%u weapon=%s trigger=%s flank=%d\n",
			TheGameLogic->getFrame(), m_name.str(), m_fxTrigger.str(), flank);
	}

	// Now do the FireOCL if there is one
	if( sourceObj )
	{
		VeterancyLevel v = sourceObj->getVeterancyLevel();
		const ObjectCreationList *oclToUse = isProjectileDetonation ? getProjectileDetonationOCL(v) : getFireOCL(v);
		if( oclToUse )
			ObjectCreationList::create( oclToUse, sourceObj, nullptr );
	}

	if (!isProjectileDetonation && inflictDamage && hasNuggets())
	{
		orderNuggetSlaves(sourceObj, victimObj, victimPos);
		orderNuggetHorde(sourceObj, victimObj, victimPos);
	}

	Bool nuggetHit = hasNuggets() && (isProjectileDetonation || !WeaponNuggetRules::needsHitRoll(m_hitPercentage) ||
		WeaponNuggetRules::rollHits(GameLogicRandomValue(0, 99), m_hitPercentage));
	if (hasNuggets() && !isProjectileDetonation && m_canBeDodged && victimObj && RetailFileLayers::enabled())
	{
		const auto* body = dynamic_cast<const ActiveBody*>(victimObj->getBodyModule());
		const Real dodge = body ? body->getRetailBodyData()->m_dodgePercent : 0.0f;
		if (dodge > 0.0f && GameLogicRandomValueReal(0.0f, 1.0f) <= dodge)
			nuggetHit = FALSE;
	}
	const Bool infantryScatter = m_infantryInaccuracyDist > 0.0f && victimObj && victimObj->isKindOf(KINDOF_INFANTRY);
	const Bool forceScatter = RetailFileLayers::enabled() && hasNuggets() && !isProjectileDetonation && infantryScatter &&
		(!m_disableScatterForTargetsOnWall || victimObj->getLayer() != LAYER_WALL);
	if (RetailFileLayers::enabled() && !isProjectileDetonation && inflictDamage && victimObj && m_hitPassengerPercentage > 0.0f &&
		(!hasNuggets() || (nuggetHit && !forceScatter)))
	{
		if (Object* passenger = selectWeaponPassenger(victimObj, m_hitPassengerPercentage))
		{
			victimObj = passenger;
			victimID = passenger->getID();
			victimPos = passenger->getPosition();
		}
	}
	Coord3D projectileDestination = *victimPos;
	getPreferredTargetPosition(victimObj, projectileDestination);
	const Bool scatter = (!nuggetHit || forceScatter) && (m_scatterRadius > 0.0f || infantryScatter);
	const Real scatterLimit = scatter ? m_scatterRadius + (infantryScatter ? m_infantryInaccuracyDist : 0.0f) : 0.0f;
	if (scatter && victimObj && victimObj->isKindOf(KINDOF_STRUCTURE))
		victimObj->getGeometryInfo().getCenterPosition(*victimObj->getPosition(), projectileDestination);

	const Bool independent = m_scatterIndependently && scatter && !isProjectileDetonation && hasProjectileNuggets();
	Real scatterRadius = scatter && !independent ? scatterProjectile(projectileDestination, scatterLimit, victimObj) : 0.0f;

	if (hasNuggets() && !isProjectileDetonation && hasProjectileNuggets())
	{
		launchNuggetProjectiles(sourceObj, wslot, specificBarrelToUse, nuggetHit && !forceScatter ? victimObj : nullptr,
			projectileDestination, firingWeapon, projectileID, independent ? scatterLimit : 0.0f, victimObj);
		if (!hasImpactNuggets())
			return 0;
		if (independent)
			scatterRadius = scatterProjectile(projectileDestination, scatterLimit, victimObj);
	}

	if (getProjectileTemplate() == nullptr || isProjectileDetonation)
	{
		// see if we need to be called back at a later point to deal the damage.
		Coord3D v;
		v.x = victimPos->x - sourcePos->x;
		v.y = victimPos->y - sourcePos->y;
		v.z = victimPos->z - sourcePos->z;
		// don't round the result; we WANT a fractional-frame-delay in this case.
		Real delayInFrames = WWMath::Div_FixNaN(v.length(), getWeaponSpeed());

		ObjectID damageID = getDamageDealtAtSelfPosition() ? INVALID_ID : victimID;
		const Bool nuggetMissed = hasNuggets() && (!nuggetHit || forceScatter);
		if (nuggetMissed)
			damageID = INVALID_ID;
		else if (damageID == INVALID_ID)
			damageID = detonationVictimID;

		if( firingWeapon->isLaser() )
		{
			if( scatterRadius <= getPrimaryDamageRadius( bonus ) || scatterRadius <= getSecondaryDamageRadius( bonus ) )
			{
				//The laser is close enough to damage the object, so just hit it directly. Some victim objects
				//adjust the laser's position to prevent it from hitting the ground.
				if( victimObj )
				{
					projectileDestination.set( *victimObj->getPosition() );
				}
				firingWeapon->createLaser( sourceObj, victimObj, &projectileDestination );
			}
			else
			{
				//We are missing our intended target, so now we want to aim at the ground at the projectile offset.
				damageID = INVALID_ID;
				firingWeapon->createLaser( sourceObj, nullptr, &projectileDestination );
			}
			if( inflictDamage )
			{
				dealDamageInternal( sourceID, damageID, &projectileDestination, bonus, isProjectileDetonation );
			}
			return TheGameLogic->getFrame();
		}

		const Coord3D* damagePos = getDamageDealtAtSelfPosition() ? sourcePos : (nuggetMissed ? &projectileDestination : victimPos);
		if (delayInFrames < 1.0f)
		{
			// go ahead and do it now
			//DEBUG_LOG(("WeaponTemplate::fireWeaponTemplate: firing weapon immediately!"));
			if( inflictDamage )
			{
				dealDamageInternal(sourceID, damageID, damagePos, bonus, isProjectileDetonation);
			}

			//-extraLogging
			#if defined(RTS_DEBUG)
				if( TheGlobalData->m_extraLogging )
					DEBUG_LOG( ("EARLY 4 (delayed damage applied now)") );
			#endif
			//end -extraLogging


			return TheGameLogic->getFrame();
		}
		else
		{
			UnsignedInt when = 0;
			if( TheWeaponStore && inflictDamage )
			{
				UnsignedInt delayInWholeFrames = REAL_TO_INT_CEIL(delayInFrames);
				when = TheGameLogic->getFrame() + delayInWholeFrames;
				//DEBUG_LOG(("WeaponTemplate::fireWeaponTemplate: firing weapon in %d frames (= %d)!", delayInWholeFrames,when));
				TheWeaponStore->setDelayedDamage(this, damagePos, when, sourceID, damageID, bonus);
			}

			//-extraLogging
			#if defined(RTS_DEBUG)
				if( TheGlobalData->m_extraLogging )
					DEBUG_LOG( ("EARLY 5 (delaying damage applied until frame %d)", when ) );
			#endif
			//end -extraLogging


			return when;
		}
	}
	else	// must be a projectile
	{
		Player *owningPlayer = sourceObj->getControllingPlayer(); //Need to know so missiles don't collide with firer
		Object *projectile = TheThingFactory->newObject( getProjectileTemplate(), owningPlayer->getDefaultTeam() );
		projectile->setProducer(sourceObj);

		//If the player has battle plans (America Strategy Center), then apply those bonuses
		//to this object if applicable. Internally it validates certain kinds of objects.
		//When projectiles are created, weapon bonuses such as damage may get applied.
		if( owningPlayer->getNumBattlePlansActive() > 0 )
		{
			owningPlayer->applyBattlePlanBonusesForObject( projectile );
		}


		//Store the project ID in the object as the last projectile fired!
		if (projectileID)
			*projectileID = projectile->getID();

		// Notify special power tracking
		SpecialPowerCompletionDie *die = sourceObj->findSpecialPowerCompletionDie();
		if (die)
		{
			die->notifyScriptEngine();
			die = projectile->findSpecialPowerCompletionDie();
			if (die)
			{
				die->setCreator(INVALID_ID);
			}
		}
		else
		{
			die = projectile->findSpecialPowerCompletionDie();
			if (die)
			{
				die->setCreator(sourceObj->getID());
			}
		}

		firingWeapon->newProjectileFired( sourceObj, projectile, victimObj, victimPos );//The actual logic weapon needs to know this was created.

		ProjectileUpdateInterface* pui = nullptr;
		for (BehaviorModule** u = projectile->getBehaviorModules(); *u; ++u)
		{
			if ((pui = (*u)->getProjectileUpdateInterface()) != nullptr)
				break;
		}
		if (pui)
		{
			VeterancyLevel v = sourceObj->getVeterancyLevel();
			if( scatterRadius > 0.0f )
			{
				//With a scatter radius, don't follow the victim (overriding the intent).
				pui->projectileLaunchAtObjectOrPosition( nullptr, &projectileDestination, sourceObj, wslot, specificBarrelToUse, this, m_projectileExhausts[v] );
			}
			else
			{
				pui->projectileLaunchAtObjectOrPosition(victimObj, &projectileDestination, sourceObj, wslot, specificBarrelToUse, this, m_projectileExhausts[v]);
			}
		}
		else
		{
			//DEBUG_CRASH(("Projectiles should implement ProjectileUpdateInterface!"));
			// actually, this is ok, for things like Firestorm.... (srj)
			projectile->setPosition(&projectileDestination);
		}

		//If we're launching a missile at a unit with valid countermeasures, then communicate it
		if( projectile->isKindOf( KINDOF_SMALL_MISSILE ) && victimObj && victimObj->hasCountermeasures() )
		{
			const AIUpdateInterface *ai = victimObj->getAI();
			//Only allow jets not currently supersonic to launch countermeasures
			if( ai && ai->getCurLocomotorSetType() != LOCOMOTORSET_SUPERSONIC )
			{
				//This function will determine now whether or not the fired projectile will be diverted to
				//an available decoy flare.
				victimObj->reportMissileForCountermeasures( projectile );
			}

		}
		//-extraLogging
		#if defined(RTS_DEBUG)
			if( TheGlobalData->m_extraLogging )
				DEBUG_LOG( ("DONE") );
		#endif
		//end -extraLogging

		return 0;
	}
}

//-------------------------------------------------------------------------------------------------
#if RETAIL_COMPATIBLE_CRC || PRESERVE_UNRELIABLE_FIRESTORMS
void WeaponTemplate::trimOldHistoricDamage() const
{
	UnsignedInt expirationDate = TheGameLogic->getFrame() - TheGlobalData->m_historicDamageLimit;
	while (!m_historicDamage.empty())
	{
		HistoricWeaponDamageInfo& h = m_historicDamage.front();
		if (h.frame <= expirationDate)
		{
			m_historicDamage.pop_front();
			continue;
		}
		else
		{
			// since they are in strict chronological order,
			// stop as soon as we get to a nonexpired one
			break;
		}
	}
}
#else
void WeaponTemplate::trimOldHistoricDamage() const
{
	if (m_historicDamage.empty())
		return;

	const UnsignedInt currentFrame = TheGameLogic->getFrame();
	const UnsignedInt expirationFrame = currentFrame - m_historicBonusTime;

	HistoricWeaponDamageList::iterator it = m_historicDamage.begin();

	while (it != m_historicDamage.end())
	{
		if (it->frame <= expirationFrame)
			it = m_historicDamage.erase(it);
		else
			break;
	}
}
#endif

//-------------------------------------------------------------------------------------------------
void WeaponTemplate::trimTriggeredHistoricDamage() const
{
	HistoricWeaponDamageList::iterator it = m_historicDamage.begin();

	while (it != m_historicDamage.end())
	{
		if (it->triggerId == m_historicDamageTriggerId)
			it = m_historicDamage.erase(it);
		else
			++it;
	}
}

//-------------------------------------------------------------------------------------------------
static Bool is2DDistSquaredLessThan(const Coord3D& a, const Coord3D& b, Real distSqr)
{
	Real da = sqr(a.x - b.x) + sqr(a.y - b.y);
	return da <= distSqr;
}

//-------------------------------------------------------------------------------------------------
#if RETAIL_COMPATIBLE_CRC || PRESERVE_UNRELIABLE_FIRESTORMS
void WeaponTemplate::processHistoricDamage(const Object* source, const Coord3D* pos) const
{
	//
	/** @todo We need to rewrite the historic stuff ... if you fire 5 missiles, and the 5th,
	// one creates a firestorm ... and then half a second later another volley of 5 missiles
	// come in, the second wave of 5 missiles would all do a historic weapon, making 5 more
	// firestorms (CBD) */
	//

	if( m_historicBonusCount > 0 && m_historicBonusWeapon != this )
	{
		trimOldHistoricDamage();

		Real radSqr = m_historicBonusRadius * m_historicBonusRadius;
		Int count = 0;
		UnsignedInt frameNow = TheGameLogic->getFrame();
		UnsignedInt oldestThatWillCount = frameNow - m_historicBonusTime; // Anything before this frame is "more than two seconds ago" eg
		for( HistoricWeaponDamageList::const_iterator it = m_historicDamage.begin(); it != m_historicDamage.end(); ++it )
		{
			if( it->frame >= oldestThatWillCount &&
					is2DDistSquaredLessThan( *pos, it->location, radSqr ) )
			{
				// This one is close enough in time and distance, so count it. This is tracked by template since it applies
				// across units, so don't try to clear historicDamage on success in here.
				++count;
			}
		}

		if( count >= m_historicBonusCount - 1 )	// minus 1 since we include ourselves implicitly
		{
		  TheWeaponStore->createAndFireTempWeapon(m_historicBonusWeapon, source, pos);

			/** @todo E3 hack! Clear the list for now to make sure we don't have multiple firestorms
				* remove this when the branches merge back into one.  What is causing the
				* multiple firestorms, who is to say ... this is a plug, not a fix! */
			m_historicDamage.clear();

		}
		else
		{

			// add AFTER checking for historic stuff
			m_historicDamage.push_back( HistoricWeaponDamageInfo(frameNow, *pos) );

		}

	}
}
#else
void WeaponTemplate::processHistoricDamage(const Object* source, const Coord3D* pos) const
{
	if (m_historicBonusCount > 0 && m_historicBonusWeapon != this)
	{
		trimOldHistoricDamage();

		++m_historicDamageTriggerId;

		const Int requiredCount = m_historicBonusCount - 1; // minus 1 since we include ourselves implicitly
		if (m_historicDamage.size() >= requiredCount)
		{
			const Real radSqr = m_historicBonusRadius * m_historicBonusRadius;
			Int count = 0;

			for (HistoricWeaponDamageList::iterator it = m_historicDamage.begin(); it != m_historicDamage.end(); ++it)
			{
				if (is2DDistSquaredLessThan(*pos, it->location, radSqr))
				{
					// This one is close enough in time and distance, so count it. This is tracked by template since it applies
					// across units, so don't try to clear historicDamage on success in here.
					it->triggerId = m_historicDamageTriggerId;

					if (++count == requiredCount)
					{
						TheWeaponStore->createAndFireTempWeapon(m_historicBonusWeapon, source, pos);
						trimTriggeredHistoricDamage();
						return;
					}
				}
			}
		}

		// add AFTER checking for historic stuff
		m_historicDamage.push_back(HistoricWeaponDamageInfo(TheGameLogic->getFrame(), *pos));
	}
}
#endif

WeaponTemplate::RadiusDamageVerdict WeaponTemplate::judgeRadiusDamageVictim(const Object* source, const Object* victim,
	const Object* primaryVictim, Int affects)
{
	if (source == nullptr || victim == primaryVictim)
		return RADIUS_DAMAGE_AFFECT;

	if ((affects & WEAPON_KILLS_SELF) && source == victim)
		return RADIUS_DAMAGE_KILL_SELF;

	if ((affects & WEAPON_AFFECTS_SELF) == 0 && (source == victim || source->getProducerID() == victim->getID()))
		return RADIUS_DAMAGE_SKIP;

	if ((affects & WEAPON_DOESNT_AFFECT_SIMILAR) && source->getTemplate()->isEquivalentTo(victim->getTemplate()) &&
		source->getRelationship(victim) == ALLIES)
		return RADIUS_DAMAGE_SKIP;

	if ((affects & WEAPON_DOESNT_AFFECT_AIRBORNE) != 0 && victim->isSignificantlyAboveTerrain())
		return RADIUS_DAMAGE_SKIP;

	const Relationship r = victim->getRelationship(source);
	Int requiredMask;
	if (r == ALLIES)
		requiredMask = WEAPON_AFFECTS_ALLIES;
	else if (r == ENEMIES)
		requiredMask = WEAPON_AFFECTS_ENEMIES;
	else
		requiredMask = WEAPON_AFFECTS_NEUTRALS;

	return (affects & requiredMask) ? RADIUS_DAMAGE_AFFECT : RADIUS_DAMAGE_SKIP;
}

//-------------------------------------------------------------------------------------------------
void WeaponTemplate::dealDamageInternal(ObjectID sourceID, ObjectID victimID, const Coord3D *pos, const WeaponBonus& bonus, Bool isProjectileDetonation) const
{
	if (sourceID == 0)	// must have a source
		return;

	if (victimID == 0 && pos == nullptr)	// must have some sort of destination
		return;

	Object *source = TheGameLogic->findObjectByID(sourceID);	// might be null...

	processHistoricDamage(source, pos);

//DEBUG_LOG(("WeaponTemplate::dealDamageInternal: dealing damage %s at frame %d",m_name.str(),TheGameLogic->getFrame()));

	Object *primaryVictim = victimID ? TheGameLogic->findObjectByID(victimID) : nullptr;
	if (primaryVictim)
	{
		if (const HordeContain* enemy = HordeContain::get(primaryVictim))
		{
			const Coord3D* fromPos = source ? source->getPosition() : pos;
			primaryVictim = fromPos ? enemy->findNearestLivingMember(*fromPos) : nullptr;
			victimID = primaryVictim ? primaryVictim->getID() : INVALID_ID;
		}
	}
	if (primaryVictim)
	{
		pos = primaryVictim->getPosition();
	}

	if (hasNuggets())
	{
		dealNuggetsAtImpact(sourceID, victimID, pos, bonus, !isProjectileDetonation && isContactWeapon());
		return;
	}

	DamageType damageType = getDamageType();
	DeathType deathType = getDeathType();
	ObjectStatusTypes damageStatusType = getDamageStatusType();
	const PhysicsBehavior* impactPhysics = RetailFileLayers::enabled() && source ? source->getPhysics() : nullptr;
	const Coord3D impactVelocity = impactPhysics ? *impactPhysics->getVelocity() : Coord3D{};
	if (getProjectileTemplate() == nullptr || isProjectileDetonation)
	{
		SimpleObjectIterator *iter;
		Object *curVictim;
		Real curVictimDistSqr;

		Real primaryRadius = getPrimaryDamageRadius(bonus);
		Real secondaryRadius = getSecondaryDamageRadius(bonus);
		Real primaryDamage = getPrimaryDamage(bonus);
		Real secondaryDamage = getSecondaryDamage(bonus);
		Int affects = getAffectsMask();

		DEBUG_ASSERTCRASH(secondaryRadius >= primaryRadius || secondaryRadius == 0.0f, ("secondary radius should be >= primary radius (or zero)"));

		Real primaryRadiusSqr = sqr(primaryRadius);
		Real radius = max(primaryRadius, secondaryRadius);
		if (radius > 0.0f)
		{
			iter = ThePartitionManager->iterateObjectsInRange(pos, radius, DAMAGE_RANGE_CALC_TYPE);
			curVictim = iter->firstWithNumeric(&curVictimDistSqr);
		}
		else
		{
			//DEBUG_ASSERTCRASH(primaryVictim != nullptr, ("weapons without radii should always pass in specific victims"));
			// check against victimID rather than primaryVictim, since we may have targeted a legitimate victim
			// that got killed before the damage was dealt... (srj)
			//DEBUG_ASSERTCRASH(victimID != 0, ("weapons without radii should always pass in specific victims"));
			iter = nullptr;
			curVictim = primaryVictim;
			curVictimDistSqr = 0.0f;

			if( affects & WEAPON_KILLS_SELF )
			{
				DamageInfo damageInfo;
				damageInfo.in.m_damageType = damageType;
				damageInfo.in.m_deathType = deathType;
				damageInfo.in.m_sourceID = sourceID;
				damageInfo.in.m_sourcePlayerMask = 0;
				damageInfo.in.m_damageStatusType = damageStatusType;
				damageInfo.in.m_amount = HUGE_DAMAGE_AMOUNT;
				source->attemptDamage( &damageInfo );
				return;
			}
		}
		MemoryPoolObjectHolder hold(iter);

		for (; curVictim != nullptr; curVictim = iter ? iter->nextWithNumeric(&curVictimDistSqr) : nullptr)
		{
			if (HordeContain::get(curVictim))
				continue;
			const RadiusDamageVerdict verdict = judgeRadiusDamageVictim(source, curVictim, primaryVictim, affects);
			if (verdict == RADIUS_DAMAGE_SKIP)
				continue;
			const Bool killSelf = verdict == RADIUS_DAMAGE_KILL_SELF;

			DamageInfo damageInfo;
			damageInfo.shouldPlayUnderAttackEvaEvent = m_shouldPlayUnderAttackEvaEvent;
			if (RetailFileLayers::enabled())
				damageInfo.retailWeaponName = m_name;
			damageInfo.in.m_damageType = damageType;
			damageInfo.in.m_deathType = deathType;
			damageInfo.in.m_sourceID = sourceID;
			damageInfo.in.m_sourcePlayerMask = 0;
			damageInfo.in.m_damageStatusType = damageStatusType;

			Coord3D damageDirection;
			damageDirection.zero();
			if( curVictim && source )
			{
				damageDirection.set( *curVictim->getPosition() );
				damageDirection.sub( *source->getPosition() );
			}

			Real allowedAngle = getRadiusDamageAngle();
			if( allowedAngle < PI )
			{
				if( curVictim == nullptr  ||  source == nullptr )
					continue; // We are directional damage, but can't figure out our direction.  Just bail.

				// People can only be hit in a cone oriented as the firer is oriented
				Vector3 sourceVector = source->getTransformMatrix()->Get_X_Vector();
				Vector3 damageVector(damageDirection.x, damageDirection.y, damageDirection.z);
				sourceVector.Normalize();
				damageVector.Normalize();

				// These are now normalized, so the dot productis actually the Cos of the angle they form
				// A smaller Cos would mean a more obtuse angle
				if( Vector3::Dot_Product(sourceVector, damageVector) < Cos(allowedAngle) )
					continue;// Too far to the side, can't hurt them.
			}

			// Grab the vector between the source object causing the damage and the victim in order that we can
			// simulate a shockwave pushing objects around
			damageInfo.in.m_shockWaveAmount = m_shockWaveAmount;
			if (damageInfo.in.m_shockWaveAmount > 0.0f)
			{
				// Calculate the vector of the shockwave
				Coord3D shockWaveVector = damageDirection;

				// Guard against zero vector. Make vector straight up if that is the case
				if (fabs(shockWaveVector.x) < WWMATH_EPSILON &&
						fabs(shockWaveVector.y) < WWMATH_EPSILON &&
						fabs(shockWaveVector.z) < WWMATH_EPSILON)
				{
					shockWaveVector.z = 1.0f;
				}

				// Populate the damage information with the shockwave information
				damageInfo.in.m_shockWaveVector = shockWaveVector;
				damageInfo.in.m_shockWaveRadius = m_shockWaveRadius;
				damageInfo.in.m_shockWaveTaperOff = m_shockWaveTaperOff;
			}

      if (source && source->getControllingPlayer()) {
				damageInfo.in.m_sourcePlayerMask = source->getControllingPlayer()->getPlayerMask();
			}
			// note, don't bother with damage multipliers here...
			// that's handled internally by the attemptDamage() method.
			damageInfo.in.m_amount = (curVictimDistSqr <= primaryRadiusSqr) ? primaryDamage : secondaryDamage;

			if( killSelf )
			{
				//Deal enough damage to kill yourself. I thought about getting the current health and applying
				//enough unresistable damage to die... however it's possible that we have different types of
				//deaths based on damage type and/or the possibility to resist certain damage types and
				//surviving -- so instead, I'm blindly inflicting a very high value of the intended damage type.
				damageInfo.in.m_amount = HUGE_DAMAGE_AMOUNT;
				//BodyModuleInterface* body = curVictim->getBodyModule();
				//if( body )
				//{
				//	Real curVictimHealth = curVictim->getBodyModule()->getHealth();
				//	damageInfo.in.m_amount = __max( damageInfo.in.m_amount, curVictimHealth );
				//}
			}

			damageInfo.impactVelocity = impactVelocity;

			// if the damage-dealer is a projectile, designate the damage as done by its launcher, not the projectile.
			// this is much more useful for the AI...
			if (source && source->isKindOf(KINDOF_PROJECTILE))
			{
				for (BehaviorModule** u = source->getBehaviorModules(); *u; ++u)
				{
					ProjectileUpdateInterface* pui = (*u)->getProjectileUpdateInterface();
					if (pui != nullptr)
					{
						damageInfo.in.m_sourceID = pui->projectileGetLauncherID();
						break;
					}
				}
			}

			HordeContain::applyWeaponDamage(curVictim, damageInfo, !isProjectileDetonation && isContactWeapon());
			//DEBUG_ASSERTLOG(damageInfo.out.m_noEffect, ("WeaponTemplate::dealDamageInternal: dealt to %s %08lx: attempted %f, actual %f (%f)",
			//	curVictim->getTemplate()->getName().str(),curVictim,
			//	damageInfo.in.m_amount, damageInfo.out.m_actualDamageDealt, damageInfo.out.m_actualDamageClipped));
		}
	}
	else
	{
		DEBUG_CRASH(("projectile weapons should never get dealDamage called directly"));
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
WeaponStore::WeaponStore()
{
}

//-------------------------------------------------------------------------------------------------
WeaponStore::~WeaponStore()
{
	deleteAllDelayedDamage();

	for (size_t i = 0; i < m_weaponTemplateVector.size(); i++)
	{
		WeaponTemplate* wt = m_weaponTemplateVector[i];
		deleteInstance(wt);
	}
	m_weaponTemplateVector.clear();
	m_weaponTemplateHashMap.clear();
}

//-------------------------------------------------------------------------------------------------
void WeaponStore::handleProjectileDetonation(const WeaponTemplate* wt, const Object *source, const Coord3D* pos, WeaponBonusConditionFlags extraBonusFlags, Bool inflictDamage, ObjectID detonationVictimID )
{
	Weapon* w = allocateNewWeapon(wt, PRIMARY_WEAPON);
	w->loadAmmoNow(source);
	w->fireProjectileDetonationWeapon( source, pos, extraBonusFlags, inflictDamage, detonationVictimID );
	deleteInstance(w);
}

//-------------------------------------------------------------------------------------------------
void WeaponStore::createAndFireTempWeapon(const WeaponTemplate* wt, const Object *source, const Coord3D* pos, Bool ignoreRanges)
{
	if (wt == nullptr)
		return;
	Weapon* w = allocateNewWeapon(wt, PRIMARY_WEAPON);
	w->loadAmmoNow(source);
	if (ignoreRanges)
		w->privateFireWeapon(source, nullptr, pos, FALSE, TRUE, 0, nullptr, TRUE);
	else
		w->fireWeapon(source, pos);
	deleteInstance(w);
}

//-------------------------------------------------------------------------------------------------
void WeaponStore::createAndFireTempWeapon(const WeaponTemplate* wt, const Object *source, Object *target, Bool ignoreRanges)
{
	if (wt == nullptr)
		return;
	Weapon* w = allocateNewWeapon(wt, PRIMARY_WEAPON);
	w->loadAmmoNow(source);
	w->privateFireWeapon(source, target, nullptr, FALSE, ignoreRanges, 0, nullptr, TRUE);
	deleteInstance(w);
}

//-------------------------------------------------------------------------------------------------
const WeaponTemplate *WeaponStore::findWeaponTemplate( const AsciiString& name ) const
{
	if (name.compareNoCase("None") == 0)
		return nullptr;
	const WeaponTemplate * wt = findWeaponTemplatePrivate( TheNameKeyGenerator->nameToKey( name ) );
	DEBUG_ASSERTCRASH(wt != nullptr, ("Weapon %s not found!", name.str()));
	return wt;
}

//-------------------------------------------------------------------------------------------------
const WeaponTemplate *WeaponStore::findWeaponTemplate( const char* name ) const
{
	if (stricmp(name, "None") == 0)
		return nullptr;
	const WeaponTemplate * wt = findWeaponTemplatePrivate( TheNameKeyGenerator->nameToKey( name ) );
	DEBUG_ASSERTCRASH(wt != nullptr, ("Weapon %s not found!",name));
	return wt;
}

//-------------------------------------------------------------------------------------------------
WeaponTemplate *WeaponStore::findWeaponTemplatePrivate( NameKeyType key ) const
{
	// search weapon list for name
	WeaponTemplateMap::const_iterator it = m_weaponTemplateHashMap.find(key);
	if(it != m_weaponTemplateHashMap.end())
		return it->second;

	return nullptr;

}

//-------------------------------------------------------------------------------------------------
WeaponTemplate *WeaponStore::newWeaponTemplate(AsciiString name)
{

	// sanity
	if(name.isEmpty())
		return nullptr;

	// allocate a new weapon
	WeaponTemplate *wt = newInstance(WeaponTemplate);
	wt->m_name = name;
	wt->m_nameKey = TheNameKeyGenerator->nameToKey( name );
	m_weaponTemplateVector.push_back(wt);
	m_weaponTemplateHashMap[wt->m_nameKey] = wt;

	return wt;
}

//-------------------------------------------------------------------------------------------------
WeaponTemplate *WeaponStore::newOverride(WeaponTemplate *weaponTemplate)
{
	if (!weaponTemplate)
		return nullptr;

	// allocate a new weapon
	WeaponTemplate *wt = newInstance(WeaponTemplate);
	(*wt) = (*weaponTemplate);
	(wt)->friend_setNextTemplate(weaponTemplate);

	return wt;
}

//-------------------------------------------------------------------------------------------------
void WeaponStore::update()
{
	for (std::list<WeaponDelayedDamageInfo>::iterator ddi = m_weaponDDI.begin(); ddi != m_weaponDDI.end(); )
	{
		UnsignedInt curFrame = TheGameLogic->getFrame();
		if (curFrame >= ddi->m_delayDamageFrame)
		{
			// we never do projectile-detonation-damage via this code path.
			const Bool isProjectileDetonation = false;
			if (ddi->m_nuggetIndex >= 0)
				ddi->m_delayedWeapon->dealNuggetDamage(ddi->m_nuggetIndex, ddi->m_delaySourceID, ddi->m_delayIntendedVictimID,
					&ddi->m_delayDamagePos, ddi->m_bonus, ddi->m_dotTick, ddi->m_contactHit,
					ddi->m_hasImpactVelocity ? &ddi->m_impactVelocity : nullptr, ddi->m_launchTrigger, ddi->m_launchWeapon);
			else
				ddi->m_delayedWeapon->dealDamageInternal(ddi->m_delaySourceID, ddi->m_delayIntendedVictimID, &ddi->m_delayDamagePos, ddi->m_bonus, isProjectileDetonation);
			ddi = m_weaponDDI.erase(ddi);
		}
		else
		{
			++ddi;
		}
	}
}

//-------------------------------------------------------------------------------------------------
void WeaponStore::deleteAllDelayedDamage()
{
	m_weaponDDI.clear();
}

// ------------------------------------------------------------------------------------------------
void WeaponStore::resetWeaponTemplates()
{

	for (size_t i = 0; i < m_weaponTemplateVector.size(); i++)
	{
		WeaponTemplate* wt = m_weaponTemplateVector[i];
		wt->reset();
	}

}

//-------------------------------------------------------------------------------------------------
void WeaponStore::reset()
{
	// clean up any overrides.
	for (size_t i = 0; i < m_weaponTemplateVector.size(); ++i)
	{
		WeaponTemplate *wt = m_weaponTemplateVector[i];
		if (wt->isOverride())
		{
			WeaponTemplate *overrideData = wt;
			wt = wt->friend_clearNextTemplate();
			deleteInstance(overrideData);
		}
	}

	deleteAllDelayedDamage();
	resetWeaponTemplates();
}

//-------------------------------------------------------------------------------------------------
void WeaponStore::setDelayedDamage(const WeaponTemplate *weapon, const Coord3D* pos, UnsignedInt whichFrame, ObjectID sourceID, ObjectID victimID,
	const WeaponBonus& bonus, Int nuggetIndex, Bool dotTick, Bool contactHit, RetailDamageFXTrigger launchTrigger,
	const Coord3D* impactVelocity, const WeaponTemplate* launchWeapon)
{
	WeaponDelayedDamageInfo wi;
	wi.m_delayedWeapon = weapon;
	wi.m_delayDamagePos = *pos;
	wi.m_delayDamageFrame = whichFrame;
	wi.m_delaySourceID = sourceID;
	wi.m_delayIntendedVictimID = victimID;
	wi.m_bonus = bonus;
	wi.m_nuggetIndex = nuggetIndex;
	wi.m_dotTick = dotTick;
	wi.m_contactHit = contactHit;
	wi.m_launchTrigger = launchTrigger;
	wi.m_launchWeapon = launchWeapon;
	if (impactVelocity)
	{
		wi.m_hasImpactVelocity = TRUE;
		wi.m_impactVelocity = *impactVelocity;
	}
	m_weaponDDI.push_back(wi);
}

void WeaponStore::xferRetailDelayedDamage(Xfer* xfer)
{
	const Bool loading = xfer->getXferMode() == XFER_LOAD;
	UnsignedInt count = loading ? 0 : static_cast<UnsignedInt>(m_weaponDDI.size());
	xfer->xferUnsignedInt(&count);

	const auto xferHit = [&](WeaponDelayedDamageInfo& hit) {
		AsciiString weapon = loading ? AsciiString() : hit.m_delayedWeapon->getName();
		xfer->xferAsciiString(&weapon);
		xfer->xferCoord3D(&hit.m_delayDamagePos);
		xfer->xferUnsignedInt(&hit.m_delayDamageFrame);
		xfer->xferObjectID(&hit.m_delaySourceID);
		xfer->xferObjectID(&hit.m_delayIntendedVictimID);
		hit.m_bonus.xfer(xfer);
		xfer->xferInt(&hit.m_nuggetIndex);
		xfer->xferBool(&hit.m_dotTick);
		xfer->xferBool(&hit.m_contactHit);
		Int trigger = hit.m_launchTrigger;
		xfer->xferInt(&trigger);
		xfer->xferBool(&hit.m_hasImpactVelocity);
		if (hit.m_hasImpactVelocity)
			xfer->xferCoord3D(&hit.m_impactVelocity);

		if (loading)
		{
			hit.m_delayedWeapon = findWeaponTemplatePrivate(TheNameKeyGenerator->nameToKey(weapon));
			if (!hit.m_delayedWeapon || hit.m_nuggetIndex < -1 ||
				(hit.m_nuggetIndex >= 0 && static_cast<size_t>(hit.m_nuggetIndex) >= hit.m_delayedWeapon->m_nuggets.size()) ||
				trigger < RETAIL_DAMAGE_FX_NONE || trigger > RETAIL_DAMAGE_FX_TREBUCHET_ROCK)
				throw SC_INVALID_DATA;
			hit.m_launchTrigger = static_cast<RetailDamageFXTrigger>(trigger);
		}
	};

	if (loading)
	{
		std::list<WeaponDelayedDamageInfo> restored;
		for (UnsignedInt i = 0; i < count; ++i)
			xferHit(restored.emplace_back());
		m_weaponDDI.swap(restored);
	}
	else
	{
		for (WeaponDelayedDamageInfo& hit : m_weaponDDI)
			xferHit(hit);
	}
}

void WeaponStore::xferRetailBlocks(Xfer* xfer, Bool present)
{
	const Bool loading = xfer->getXferMode() == XFER_LOAD;
	RetailSaveBlocks::transferSection(xfer, loading, present, {
		{ "delayed-launch-weapons", [this, loading](Xfer* x) {
			for (WeaponDelayedDamageInfo& hit : m_weaponDDI)
			{
				AsciiString name = hit.m_launchWeapon ? hit.m_launchWeapon->getName() : AsciiString();
				x->xferAsciiString(&name);
				if (loading && !name.isEmpty())
				{
					hit.m_launchWeapon = findWeaponTemplate(name);
					if (!hit.m_launchWeapon)
						throw SC_INVALID_DATA;
				}
			}
		}, [this] {
			for (WeaponDelayedDamageInfo& hit : m_weaponDDI)
				hit.m_launchWeapon = nullptr;
		}
		}
	});
}

//-------------------------------------------------------------------------------------------------
void WeaponStore::postProcessLoad()
{
	if (!TheThingFactory)
	{
		DEBUG_CRASH(("you must call this after TheThingFactory is inited"));
		return;
	}

	for (size_t i = 0; i < m_weaponTemplateVector.size(); i++)
	{
		WeaponTemplate* wt = m_weaponTemplateVector[i];
		if (wt)
			wt->postProcessLoad();
	}

}

//-------------------------------------------------------------------------------------------------
/*static*/ void WeaponStore::parseWeaponTemplateDefinition(INI* ini)
{
	AsciiString name;

	// read the weapon name
	const char* c = ini->getNextToken();
	name.set(c);

	// find existing item if present
	WeaponTemplate *weapon = TheWeaponStore->findWeaponTemplatePrivate( TheNameKeyGenerator->nameToKey( name ) );
	if (weapon)
	{
		if (ini->getLoadType() == INI_LOAD_CREATE_OVERRIDES)
			weapon = TheWeaponStore->newOverride(weapon);
		else
		{
			DEBUG_CRASH(("Weapon '%s' already exists, but OVERRIDE not specified", c));
			return;
		}

	}
	else
	{
		// no item is present, create a new one
		weapon = TheWeaponStore->newWeaponTemplate(name);
	}

	const Bool retail = RetailFileLayers::enabled();
	if (retail || INIAudit::isEnabled())
	{
		MultiIniFieldParse fields;
		fields.add(weapon->getFieldParse());
		fields.add(WeaponTemplate::TheRotwkWeaponFieldParseTable);
		ini->initFromINIMulti(weapon, fields);
	}
	else
		ini->initFromINI(weapon, weapon->getFieldParse());

	if (retail)
	{
		const Bool antiStructure = weapon->m_antiStructure < 0
			? (weapon->m_antiMask & WEAPON_ANTI_GROUND) != 0 : weapon->m_antiStructure;
		weapon->m_antiMask &= ~WEAPON_ANTI_STRUCTURE;
		if (antiStructure)
			weapon->m_antiMask |= WEAPON_ANTI_STRUCTURE;
	}

	if (weapon->m_projectileName.isNone())
		weapon->m_projectileName.clear();

#if defined(RTS_DEBUG)
	if (!weapon->getFireSound().getEventName().isEmpty() && weapon->getFireSound().getEventName().compareNoCase("NoSound") != 0)
	{
		DEBUG_ASSERTCRASH(TheAudio->isValidAudioEvent(&weapon->getFireSound()), ("Invalid FireSound %s in Weapon '%s'.", weapon->getFireSound().getEventName().str(), weapon->getName().str()));
	}
#endif

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
Weapon::Weapon(const WeaponTemplate* tmpl, WeaponSlotType wslot)
{
	// Weapons start empty; you must reload before use.
	// (however, there is no delay for reloading the first time.)
	m_template = tmpl;
	m_wslot = wslot;
	m_status = OUT_OF_AMMO;
	m_ammoInClip = 0;
	m_whenWeCanFireAgain = 0;
	m_whenPreAttackFinished = 0;
	m_whenLastReloadStarted = 0;
	m_projectileStreamID = INVALID_ID;
	m_leechWeaponRangeActive = false;
	m_pitchLimited = (m_template->getMinTargetPitch() > -PI || m_template->getMaxTargetPitch() < PI);
	m_maxShotCount = NO_MAX_SHOTS_LIMIT;
	m_curBarrel = 0;
	m_numShotsForCurBarrel = 	m_template->getShotsPerBarrel();
	m_lastFireFrame = 0;
	m_suspendFXFrame = TheGameLogic->getFrame() + m_template->getSuspendFXDelay();
}

//-------------------------------------------------------------------------------------------------
Weapon::Weapon(const Weapon& that)
{
	// Weapons lose all ammo when copied.
	this->m_template = that.m_template;
	this->m_wslot = that.m_wslot;
	this->m_status = OUT_OF_AMMO;
	this->m_ammoInClip = 0;
	this->m_whenPreAttackFinished = 0;
	this->m_whenLastReloadStarted = 0;
	this->m_whenWeCanFireAgain = 0;
	this->m_projectileStreamID = INVALID_ID;
	this->m_leechWeaponRangeActive = false;
	this->m_pitchLimited = (m_template->getMinTargetPitch() > -PI || m_template->getMaxTargetPitch() < PI);
	this->m_maxShotCount = NO_MAX_SHOTS_LIMIT;
	this->m_curBarrel = 0;
	this->m_numShotsForCurBarrel = m_template->getShotsPerBarrel();
	this->m_lastFireFrame = 0;
	this->m_suspendFXFrame = that.getSuspendFXFrame();
}

//-------------------------------------------------------------------------------------------------
Weapon& Weapon::operator=(const Weapon& that)
{
	if (this != &that)
	{
		// Weapons lose all ammo when copied.
		this->m_template = that.m_template;
		this->m_wslot = that.m_wslot;
		this->m_status = OUT_OF_AMMO;
		this->m_ammoInClip = 0;
		this->m_whenPreAttackFinished = 0;
		this->m_whenLastReloadStarted = 0;
		this->m_whenWeCanFireAgain = 0;
		this->m_leechWeaponRangeActive = false;
		this->m_pitchLimited = (m_template->getMinTargetPitch() > -PI || m_template->getMaxTargetPitch() < PI);
		this->m_maxShotCount = NO_MAX_SHOTS_LIMIT;
		this->m_curBarrel = 0;
		this->m_lastFireFrame = 0;
		this->m_suspendFXFrame = that.getSuspendFXFrame();
		this->m_numShotsForCurBarrel = m_template->getShotsPerBarrel();
		this->m_projectileStreamID = INVALID_ID;
		this->m_linearTargetIndex = 0;
	}
	return *this;
}

//-------------------------------------------------------------------------------------------------
Weapon::~Weapon()
{
}

//-------------------------------------------------------------------------------------------------
void Weapon::computeBonus(const Object *source, WeaponBonusConditionFlags extraBonusFlags, WeaponBonus& bonus) const
{
	bonus.clear();
	WeaponBonusConditionFlags flags = source->getWeaponBonusCondition();
	//CRCDEBUG_LOG(("Weapon::computeBonus() - flags are %X for %s", flags, DescribeObject(source).str()));
	flags |= extraBonusFlags;

	if( source->getContainedBy() )
	{
		// We may be able to add in our container's flags
		const ContainModuleInterface *theirContain = source->getContainedBy()->getContain();
		if( theirContain && theirContain->isWeaponBonusPassedToPassengers() )
			flags |= theirContain->getWeaponBonusPassedToPassengers();
	}

	if (TheGlobalData->m_weaponBonusSet)
		TheGlobalData->m_weaponBonusSet->appendBonuses(flags, bonus);
	const WeaponBonusSet* extra = m_template->getExtraBonus();
	if (extra)
		extra->appendBonuses(flags, bonus);

	if (RetailFileLayers::enabled() && m_template->m_passengerProportionalAttack)
	{
		const ContainModuleInterface* contain = source->getContain();
		const ContainedItemsList* passengers = contain ? contain->getContainedItemsList() : nullptr;
		const Int count = passengers ? std::count_if(passengers->begin(), passengers->end(), [&](const Object* passenger)
		{
			return !passenger->isEffectivelyDead() && m_template->m_projectileFilterInContainer.matchesObject(source, passenger);
		}) : 0;
		const Int cap = m_template->m_maxAttackPassengers > 0 ? m_template->m_maxAttackPassengers : contain ? contain->getContainMax() : 0;
		const Real fraction = cap > 0 ? static_cast<Real>(std::min(count, cap)) / cap : static_cast<Real>(count > 0);
		bonus.setField(WeaponBonus::DAMAGE, bonus.getField(WeaponBonus::DAMAGE) * fraction);
	}

	const Bool hasNuggets = m_template->hasNuggets();
	const Bool useInnate = m_template->usesInnateAttributes();
	const AttributeModifierRules::Totals* modifiers = AttributeModifiers::totalsOf(source);
	if (modifiers)
	{
		if (!hasNuggets)
			bonus.setField(WeaponBonus::DAMAGE, bonus.getField(WeaponBonus::DAMAGE) * modifiers->damageDealtFactor(useInnate));
		bonus.setField(WeaponBonus::RANGE, bonus.getField(WeaponBonus::RANGE) * modifiers->rangeFactor());
		bonus.setField(WeaponBonus::RATE_OF_FIRE, bonus.getField(WeaponBonus::RATE_OF_FIRE) * modifiers->value[AttributeModifierRules::RATE_OF_FIRE]);
	}

	if (hasNuggets)
	{
		const Object* attacker = WeaponTemplate::getNuggetAttacker(source);
		const AttributeModifierRules::Totals* damageModifiers = attacker == source ? modifiers : AttributeModifiers::totalsOf(attacker);
		if (damageModifiers)
		{
			bonus.setNuggetDamageModifiers(damageModifiers->value[AttributeModifierRules::DAMAGE_ADD],
				damageModifiers->damageDealtFactor(useInnate), damageModifiers->value[AttributeModifierRules::SPELL_DAMAGE]);
		}
	}
}

//-------------------------------------------------------------------------------------------------
void Weapon::activate(const Object* source)
{
	WeaponBonus bonus;
	computeBonus(source, 0, bonus);
	reloadWithBonus(source, bonus, m_template->instantLoadClipOnActivate(), FALSE);
}

void Weapon::loadAmmoNow(const Object *sourceObj)
{
	WeaponBonus bonus;
	computeBonus(sourceObj, 0, bonus);
	reloadWithBonus(sourceObj, bonus, true);
}

//-------------------------------------------------------------------------------------------------
void Weapon::reloadAmmo(const Object *sourceObj)
{

	WeaponBonus bonus;
	computeBonus(sourceObj, 0, bonus);
	reloadWithBonus(sourceObj, bonus, false);
}

//-------------------------------------------------------------------------------------------------
Int Weapon::getClipReloadTime(const Object *source) const
{
	WeaponBonus bonus;
	computeBonus(source, 0, bonus);
	return m_template->getClipReloadTime(bonus, FALSE);
}

//-------------------------------------------------------------------------------------------------
void Weapon::setClipPercentFull(Real percent, Bool allowReduction)
{
	if (m_template->getClipSize() == 0)
		return;

	Int ammo = REAL_TO_INT_FLOOR(m_template->getClipSize() * percent);
	if (ammo > m_ammoInClip || (allowReduction && ammo < m_ammoInClip))
	{
		m_ammoInClip = ammo;
		m_status = m_ammoInClip ? OUT_OF_AMMO : READY_TO_FIRE;
		//CRCDEBUG_LOG(("Weapon::setClipPercentFull() just set m_status to %d (ammo in clip is %d)", m_status, m_ammoInClip));
		m_whenLastReloadStarted = TheGameLogic->getFrame();
		m_whenWeCanFireAgain = m_whenLastReloadStarted;
		//CRCDEBUG_LOG(("Just set m_whenWeCanFireAgain to %d in Weapon::setClipPercentFull", m_whenWeCanFireAgain));
		rebuildScatterTargets();
	}
}

//-------------------------------------------------------------------------------------------------
void Weapon::rebuildScatterTargets()
{
	m_scatterTargetsUnused.clear();
	Int scatterTargetsCount = m_template->getScatterTargetsVector().size();
	if (scatterTargetsCount)
	{
		// When I reload, I need to rebuild the list of ScatterTargets to shoot at.
		for (Int targetIndex = 0; targetIndex < scatterTargetsCount; targetIndex++)
			m_scatterTargetsUnused.push_back( targetIndex );
	}
}

//-------------------------------------------------------------------------------------------------
void Weapon::reloadWithBonus(const Object *sourceObj, const WeaponBonus& bonus, Bool loadInstantly, Bool shareReload)
{
	if (m_template->getClipSize() > 0 &&
			m_ammoInClip == m_template->getClipSize() &&
			!sourceObj->isReloadTimeShared())
		return;	// don't restart our reload delay.

	m_ammoInClip = m_template->getClipSize();
	if (m_ammoInClip <= 0)
		m_ammoInClip = 0x7fffffff;	// 0 == unlimited (or effectively so)

	m_status = RELOADING_CLIP;
	Real reloadTime = loadInstantly ? 0 : m_template->getClipReloadTime(bonus, TRUE);
	m_whenLastReloadStarted = TheGameLogic->getFrame();
	m_whenWeCanFireAgain = m_whenLastReloadStarted + reloadTime;
	if (!loadInstantly && RetailTestCommands::tracesCombat())
		std::fprintf(stderr, "TEST cooldown frame=%u weapon=%s source=%u phase=reload frames=%u\n",
			m_whenLastReloadStarted, getName().str(), static_cast<UnsignedInt>(sourceObj->getID()),
			m_whenWeCanFireAgain - m_whenLastReloadStarted);
	//CRCDEBUG_LOG(("Just set m_whenWeCanFireAgain to %d in Weapon::reloadWithBonus 1", m_whenWeCanFireAgain));

			// if we are sharing reload times
			// go through other weapons in weapon set
			// set their m_whenWeCanFireAgain to this guy's delay
			// set their m_status to this guy's status
	if (shareReload && sourceObj->isReloadTimeShared())
	{
		for (Int wt = 0; wt<WEAPONSLOT_COUNT; wt++)
		{
			Weapon *weapon = sourceObj->getWeaponInWeaponSlot((WeaponSlotType)wt);
			if (weapon)
			{
				weapon->setPossibleNextShotFrame(m_whenWeCanFireAgain);
				//CRCDEBUG_LOG(("Just set m_whenWeCanFireAgain to %d in Weapon::reloadWithBonus 2", m_whenWeCanFireAgain));
				weapon->setStatus(RELOADING_CLIP);
			}
		}
	}

	rebuildScatterTargets();
}

//-------------------------------------------------------------------------------------------------
static void clipToTerrainExtent(Coord3D& approachTargetPos)
{
	Region3D bounds;
	TheTerrainLogic->getExtent(&bounds);
	if (approachTargetPos.x < bounds.lo.x+PATHFIND_CELL_SIZE_F) {
		approachTargetPos.x = bounds.lo.x+PATHFIND_CELL_SIZE_F;
	}
	if (approachTargetPos.y < bounds.lo.y+PATHFIND_CELL_SIZE_F) {
		approachTargetPos.y = bounds.lo.y+PATHFIND_CELL_SIZE_F;
	}
	if (approachTargetPos.x > bounds.hi.x-PATHFIND_CELL_SIZE_F) {
		approachTargetPos.x = bounds.hi.x-PATHFIND_CELL_SIZE_F;
	}
	if (approachTargetPos.y > bounds.hi.y-PATHFIND_CELL_SIZE_F) {
		approachTargetPos.y = bounds.hi.y-PATHFIND_CELL_SIZE_F;
	}
}

//-------------------------------------------------------------------------------------------------
void Weapon::onWeaponBonusChange(const Object *source)
{
	// We are concerned with our reload times being off if our ROF just changed.

	WeaponBonus bonus;
	computeBonus(source, 0, bonus); // The middle arg is for projectiles to inherit damage bonus from launcher

	Int newDelay;
	Bool needUpdate = FALSE;

	if( getStatus() == RELOADING_CLIP )
	{
		newDelay = m_template->getClipReloadTime(bonus, TRUE);
		needUpdate = TRUE;
	}
	else if( getStatus() == BETWEEN_FIRING_SHOTS )
	{
		newDelay = m_template->getDelayBetweenShots(bonus);
		needUpdate = TRUE;
	}

	if( needUpdate )
	{
		m_whenLastReloadStarted = TheGameLogic->getFrame();
		m_whenWeCanFireAgain = m_whenLastReloadStarted + newDelay;

		if (source->isReloadTimeShared())
		{
			for (Int wt = 0; wt<WEAPONSLOT_COUNT; wt++)
			{
				Weapon *weapon = source->getWeaponInWeaponSlot((WeaponSlotType)wt);
				if (weapon)
				{
					weapon->setPossibleNextShotFrame(m_whenWeCanFireAgain);
					weapon->setStatus(RELOADING_CLIP);
				}
			}
		}
	}
}

//-------------------------------------------------------------------------------------------------
Bool Weapon::computeApproachTarget(const Object *source, const Object *target, const Coord3D *pos, Real angleOffset, Coord3D& approachTargetPos) const
{
	// compute unit direction vector from us to our victim
	const Coord3D *targetPos;
	Coord3D dir;
	if (target)
	{
		targetPos = target->getPosition();
		ThePartitionManager->getVectorTo( target, source, ATTACK_RANGE_CALC_TYPE, dir );
	}
	else if (pos)
	{
		targetPos = pos;
		ThePartitionManager->getVectorTo( source, pos, ATTACK_RANGE_CALC_TYPE, dir );
		// Flip the vector to get from source to pos.
		dir.x = -dir.x;
		dir.y = -dir.y;
		dir.z = -dir.z;
	}
	else
	{
		DEBUG_CRASH(("error"));
		approachTargetPos.zero();
		return false;
	}

	Real dist = dir.length();
	Real minAttackRange = m_template->getMinimumAttackRange();
	if (minAttackRange > PATHFIND_CELL_SIZE_F && dist < minAttackRange)
	{
		// We aret too close, so move away from the target.
		DEBUG_ASSERTCRASH((minAttackRange<0.9f*getAttackRange(source, targetPos)), ("Min attack range is too near attack range."));
		// Recompute dir, cause if the bounding spheres touch, it will be 0.
		Coord3D srcPos = *source->getPosition();
		dir.x = srcPos.x-targetPos->x;
		dir.y = srcPos.y-targetPos->y;
#ifdef ATTACK_RANGE_IS_2D
		dir.z = 0.0f;
#else
		dir.z = srcPos.z-targetPos->z;
#endif
		dir.normalize();

		// if we're airborne and too close, just head for the opposite side.
		if (source->isAboveTerrain())
		{
			// Don't do a 180 degree turn.
			Real angle = WWMath::Atan2Origin(-dir.y, -dir.x);
			Real relAngle = source->getOrientation()- angle;
			if (relAngle>2*PI) relAngle -= 2*PI;
			if (relAngle<-2*PI) relAngle += 2*PI;
			if (fabs(relAngle)<PI/2) {
				dir.x = -dir.x;
				dir.y = -dir.y;
				dir.z = -dir.z;
			}
		}

		if (angleOffset != 0.0f)
		{
			Real angle = WWMath::Atan2Origin(dir.y, dir.x);
			dir.x = (Real)Cos(angle + angleOffset);
			dir.y = (Real)Sin(angle + angleOffset);
		}

		// select a spot along the line between us, halfway between the min & max range.
		Real attackRange = (getAttackRange(source, targetPos) + minAttackRange)/2.0f;
#ifdef ATTACK_RANGE_IS_2D
		if (target)
			attackRange += target->getGeometryInfo().getBoundingCircleRadius();
		attackRange += source->getGeometryInfo().getBoundingCircleRadius();
#else
		if (target)
			attackRange += target->getGeometryInfo().getBoundingSphereRadius();
		attackRange += source->getGeometryInfo().getBoundingSphereRadius();
#endif
		approachTargetPos.x = attackRange * dir.x + targetPos->x;
		approachTargetPos.y = attackRange * dir.y + targetPos->y;
		approachTargetPos.z = attackRange * dir.z + targetPos->z;
		///@todo - make sure we can get to the approach position.
		clipToTerrainExtent(approachTargetPos);
		return false;
	}

	const Real FUDGE = 0.001f;
	if (dist < FUDGE)
	{
		// we're close enough!
		approachTargetPos = *source->getPosition();
		return true;
	}
	else
	{
		if (isContactWeapon())
		{
			// Weapon is basically a contact weapon, like a car bomb.  The approach target logic
			// has been modified to let it approach the object, so just return the target position.	jba.
			approachTargetPos = *targetPos;
			return false;
		}

		dir.x /= dist;
		dir.y /= dist;
		dir.z /= dist;

		if (angleOffset != 0.0f)
		{
			Real angle = WWMath::Atan2Origin(dir.y, dir.x);
			dir.x = (Real)Cos(angle + angleOffset);
			dir.y = (Real)Sin(angle + angleOffset);
		}

		// select a spot along the line between us, in range of our weapon
		const Real ATTACK_RANGE_APPROACH_FUDGE = 0.9f;
		Real attackRange = getAttackRange(source, targetPos) * ATTACK_RANGE_APPROACH_FUDGE;
		approachTargetPos.x = attackRange * dir.x + targetPos->x;
		approachTargetPos.y = attackRange * dir.y + targetPos->y;
		approachTargetPos.z = attackRange * dir.z + targetPos->z;

		if (source->getAI() && source->getAI()->isAircraftThatAdjustsDestination()) {
			// Adjust the target so that we are not stacked atop another aircraft.
			TheAI->pathfinder()->adjustTargetDestination(source, target, pos, this, &approachTargetPos);
		}

		return false;
	}
}

//-------------------------------------------------------------------------------------------------
//Special case attack range calculate that fakes moving the object (to a garrisoned point) without
//actually moving the object. This is used to help determine if a garrisoned unit not yet
//positioned can attack someone.
//-------------------------------------------------------------------------------------------------
Bool Weapon::isSourceObjectWithGoalPositionWithinAttackRange( const Object *source, const Coord3D *goalPos, const Object *target, const Coord3D *targetPos ) const
{

	Real distSqr;
	if( target )
		distSqr = ThePartitionManager->getGoalDistanceSquared( source, goalPos, target, ATTACK_RANGE_CALC_TYPE );
	else if( targetPos )
		distSqr = ThePartitionManager->getGoalDistanceSquared( source, goalPos, targetPos, ATTACK_RANGE_CALC_TYPE );
	else
		return false;

	Real attackRangeSqr = sqr(getAttackRange(source, target ? target->getPosition() : targetPos, goalPos));
	Real minAttackRangeSqr = sqr(m_template->getMinimumAttackRange());
#ifdef RATIONALIZE_ATTACK_RANGE
	if (distSqr < minAttackRangeSqr)
#else
	if (distSqr < minAttackRangeSqr-0.5f)
#endif
	{
		return false;
	}
	return (distSqr <= attackRangeSqr);
}

//-------------------------------------------------------------------------------------------------
Bool Weapon::isWithinAttackRange(const Object *source, const Coord3D* pos) const
{
	Real distSqr = ThePartitionManager->getDistanceSquared( source, pos, ATTACK_RANGE_CALC_TYPE );
	Real attackRangeSqr = sqr(getAttackRange(source, pos));
	Real minAttackRangeSqr = sqr(m_template->getMinimumAttackRange());
#ifdef RATIONALIZE_ATTACK_RANGE
	if (distSqr < minAttackRangeSqr)
#else
	if (distSqr < minAttackRangeSqr-0.5f)
#endif
	{
		return false;
	}
	return (distSqr <= attackRangeSqr);
}

//-------------------------------------------------------------------------------------------------
Bool Weapon::isWithinAttackRange(const Object *source, const Object *target) const
{
	Real distSqr;
	Real attackRangeSqr;

	if (m_template->hasHeightRangeBonus())
	{
		const auto reach = WeaponTemplate::getTargetReach(source, target);
		distSqr = reach.distanceSquared;
		attackRangeSqr = sqr(getAttackRange(source, &reach.position));
	}
	else
	{
		attackRangeSqr = sqr(getAttackRange(source));
		if (!target->isKindOf(KINDOF_BRIDGE))
		{
			distSqr = ThePartitionManager->getDistanceSquared(source, target, ATTACK_RANGE_CALC_TYPE);
		}
		else
		{
			TBridgeAttackInfo info;
			TheTerrainLogic->getBridgeAttackPoints(target, &info);
			distSqr = ThePartitionManager->getDistanceSquared(source, &info.attackPoint1, ATTACK_RANGE_CALC_TYPE);
			if (distSqr > attackRangeSqr)
				distSqr = ThePartitionManager->getDistanceSquared(source, &info.attackPoint2, ATTACK_RANGE_CALC_TYPE);
		}
	}

	Real minAttackRangeSqr = sqr(m_template->getMinimumAttackRange());
#ifdef RATIONALIZE_ATTACK_RANGE
	if (distSqr < minAttackRangeSqr)
#else
	if (distSqr < minAttackRangeSqr-0.5f)
#endif
	{
		// too close. can't attack.
		return false;
	}

	if( distSqr <= attackRangeSqr )
	{
		// Note - only compare contact weapons with structures.  If you do the collision check
		// against vehicles, the attacker may get close enough to the vehicle to get crushed
		// before it fires its weapon.  jba.
		if( isContactWeapon() && target->isKindOf(KINDOF_STRUCTURE))
		{
			//We're close enough to fire off ranged weapons -- but in the case of contact weapons
			//we want to do a more detailed check to see if we're actually colliding with the target.
			ObjectIterator *iter = ThePartitionManager->iteratePotentialCollisions( source->getPosition(), source->getGeometryInfo(), 0.0f );
			MemoryPoolObjectHolder hold( iter );
			for( Object *them = iter->first(); them; them = iter->next() )
			{
				if( target == them )
				{
					return true;
				}
			}
			return false;
		}
		return true;
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
Bool Weapon::isTooClose(const Object *source, const Object *target) const
{
	Real minAttackRange = m_template->getMinimumAttackRange();
	if (minAttackRange == 0.0f)
		return false;

	Real distSqr = ThePartitionManager->getDistanceSquared( source, target, ATTACK_RANGE_CALC_TYPE );
	if (distSqr < sqr(minAttackRange))
	{
		return true;
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
Bool Weapon::isTooClose( const Object *source, const Coord3D *pos ) const
{
	Real minAttackRange = m_template->getMinimumAttackRange();
	if (minAttackRange == 0.0f)
		return false;

	Real distSqr = ThePartitionManager->getDistanceSquared( source, pos, ATTACK_RANGE_CALC_TYPE );
	if (distSqr < sqr(minAttackRange))
	{
		return true;
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
Bool Weapon::isGoalPosWithinAttackRange(const Object *source, const Coord3D* goalPos, const Object *target, const Coord3D* targetPos)	const
{
	Real distSqr;
	// Note - undersize by 1/4 of a pathfind cell, so that the goal is not teetering on the edge
	// of firing range.  jba.
	// Note 2 - even with RATIONALIZE_ATTACK_RANGE, we still need to subtract 1/4 of a pathfind cell,
	// otherwise if it teters on the edge, attacks can fail.  jba.
	Real attackRangeSqr = sqr(getAttackRange(source, target ? target->getPosition() : targetPos, goalPos) - (PATHFIND_CELL_SIZE_F*0.25f));

	if (target != nullptr)
	{
		if (target->isKindOf(KINDOF_BRIDGE))
		{
			// Special case - bridges have two attackable points at either end.
			TBridgeAttackInfo info;
			TheTerrainLogic->getBridgeAttackPoints(target, &info);
			distSqr = ThePartitionManager->getGoalDistanceSquared( source, goalPos, &info.attackPoint1, ATTACK_RANGE_CALC_TYPE );
			if (distSqr>attackRangeSqr)
			{
				// Try the other one.
				distSqr = ThePartitionManager->getGoalDistanceSquared( source, goalPos, &info.attackPoint2, ATTACK_RANGE_CALC_TYPE );
			}
		}
		else
		{
			distSqr = ThePartitionManager->getGoalDistanceSquared( source, goalPos, target, ATTACK_RANGE_CALC_TYPE );
		}
	}
	else
	{
		distSqr = ThePartitionManager->getGoalDistanceSquared( source, goalPos, targetPos, ATTACK_RANGE_CALC_TYPE );
	}

	// Note - oversize by 1/4 of a pathfind cell, so that the goal is not teetering on the edge
	// of firing range.  jba.
	// Note 2 - even with RATIONALIZE_ATTACK_RANGE, we still need to add 1/4 of a pathfind cell,
	// otherwise if it teters on the edge, attacks can fail.  jba.
	Real minAttackRangeSqr = sqr(m_template->getMinimumAttackRange()+(PATHFIND_CELL_SIZE_F*0.25f));
#ifdef RATIONALIZE_ATTACK_RANGE
	if (distSqr < minAttackRangeSqr)
#else
	if (distSqr < minAttackRangeSqr-0.5f)
#endif
	{
		return false;
	}
	return (distSqr <= attackRangeSqr);
}

//-------------------------------------------------------------------------------------------------
Real Weapon::getPercentReadyToFire() const
{
	switch (getStatus())
	{
		case OUT_OF_AMMO:
		case PRE_ATTACK:
			return 0.0f;

		case READY_TO_FIRE:
			return 1.0f;

		case BETWEEN_FIRING_SHOTS:
		case RELOADING_CLIP:
		{
			UnsignedInt now = TheGameLogic->getFrame();
			UnsignedInt nextShot = getPossibleNextShotFrame();
			DEBUG_ASSERTCRASH(now >= m_whenLastReloadStarted, ("now >= m_whenLastReloadStarted"));
			if (now >= nextShot)
				return 1.0f;

			DEBUG_ASSERTCRASH(nextShot >= m_whenLastReloadStarted, ("nextShot >= m_whenLastReloadStarted"));
			UnsignedInt totalTime = nextShot - m_whenLastReloadStarted;
			if (totalTime == 0)
			{
				return 1.0f;
			}

			UnsignedInt timeLeft = nextShot - now;
			DEBUG_ASSERTCRASH(timeLeft <= totalTime, ("timeLeft <= totalTime"));
			UnsignedInt timeSoFar = totalTime - timeLeft;
			if (timeSoFar >= totalTime)
			{
				return 1.0f;
			}
			else
			{
				return (Real)timeSoFar / (Real)totalTime;
			}
		}
	}
	DEBUG_CRASH(("should not get here"));
	return 0.0f;
}

//-------------------------------------------------------------------------------------------------
Real Weapon::getAttackRange(const Object* source, const Coord3D* target, const Coord3D* origin) const
{
	WeaponBonus bonus;
	computeBonus(source, 0, bonus);
	const Real heightAdvantage = target ? (origin ? origin : source->getPosition())->z - target->z : 0.0f;
	return m_template->getAttackRange(bonus, heightAdvantage);

	//Contained objects have longer ranges.
	//const Object *container = source->getContainedBy();
	//if( container )
	//{
	//	attackRange += container->getGeometryInfo().getBoundingCircleRadius();
	//}
	//return attackRange;
}

//-------------------------------------------------------------------------------------------------
Real Weapon::getAttackDistance(const Object *source, const Object *victimObj, const Coord3D* victimPos) const
{
	Real range = getAttackRange(source, victimObj ? victimObj->getPosition() : victimPos);

	if (victimObj != nullptr)
	{
	#ifdef ATTACK_RANGE_IS_2D
		range += source->getGeometryInfo().getBoundingCircleRadius();
		range += victimObj->getGeometryInfo().getBoundingCircleRadius();
	#else
		range += source->getGeometryInfo().getBoundingSphereRadius();
		range += victimObj->getGeometryInfo().getBoundingSphereRadius();
	#endif
	}

	return range;
}

//-------------------------------------------------------------------------------------------------
Real Weapon::estimateWeaponDamage(const Object *sourceObj, const Object *victimObj, const Coord3D* victimPos)
{
	if (!m_template)
		return 0.0f;

	// if the weapon is just reloading, it's ok. if it's out of ammo
	// (and won't autoreload), then we aren't gonna do any damage.
	if (getStatus() == OUT_OF_AMMO && !m_template->getAutoReloadsClip())
		return 0.0f;

	WeaponBonus bonus;
	computeBonus(sourceObj, 0, bonus);

	return m_template->estimateWeaponTemplateDamage(sourceObj, victimObj, victimPos, bonus);
}

//-------------------------------------------------------------------------------------------------
void Weapon::newProjectileFired(const Object *sourceObj, const Object *projectile, const Object *victimObj, const Coord3D *victimPos )
{
	// If I have a stream, I need to tell it about this new guy
	if( m_template->getProjectileStreamName().isEmpty() )
		return; // nope, no streak logic to do

	Object* projectileStream = TheGameLogic->findObjectByID(m_projectileStreamID);
	if( projectileStream == nullptr )
	{
		m_projectileStreamID = INVALID_ID;	// reset, since it might have been "valid" but deleted out from under us
		const ThingTemplate* pst = TheThingFactory->findTemplate(m_template->getProjectileStreamName());
		projectileStream = TheThingFactory->newObject( pst, sourceObj->getControllingPlayer()->getDefaultTeam() );
		if( projectileStream == nullptr )
			return;
		m_projectileStreamID = projectileStream->getID();
	}

	//Check for projectile stream update
	static NameKeyType key_ProjectileStreamUpdate = NAMEKEY("ProjectileStreamUpdate");
	ProjectileStreamUpdate* update = (ProjectileStreamUpdate*)projectileStream->findUpdateModule(key_ProjectileStreamUpdate);
	if( update )
	{
		update->setPosition( sourceObj->getPosition() );
		update->addProjectile( sourceObj->getID(), projectile->getID(), victimObj ? victimObj->getID() : INVALID_ID, victimPos );
		return;
	}

}

//-------------------------------------------------------------------------------------------------
void Weapon::createLaser( const Object *sourceObj, const Object *victimObj, const Coord3D *victimPos )
{
	const ThingTemplate* pst = TheThingFactory->findTemplate(m_template->getLaserName());
	if( !pst )
	{
		DEBUG_CRASH( ("Weapon::createLaser(). %s could not find template for its laser %s.",
			sourceObj->getTemplate()->getName().str(), m_template->getLaserName().str() ) );
		return;
	}
	Object* laser = TheThingFactory->newObject( pst, sourceObj->getControllingPlayer()->getDefaultTeam() );
	if( laser == nullptr )
		return;

	// Give it a good basis in reality to ensure it can draw when on screen.
	laser->setPosition(sourceObj->getPosition());

	//Check for laser update
	Drawable *draw = laser->getDrawable();
	if( draw )
	{
		static NameKeyType key_LaserUpdate = NAMEKEY( "LaserUpdate" );
		LaserUpdate *update = (LaserUpdate*)draw->findClientUpdateModule( key_LaserUpdate );
		if( update )
		{
			Coord3D pos = *victimPos;
			if( victimObj && !victimObj->isKindOf( KINDOF_PROJECTILE ) && !victimObj->isAirborneTarget() )
			{
				//Targets are positioned on the ground, so raise the beam up so we're not shooting their feet.
				//Projectiles are a different story, target their exact position.
				pos.z += 10.0f;
			}
			update->initLaser( sourceObj, victimObj, sourceObj->getPosition(), &pos, m_template->getLaserBoneName() );
		}
	}
}

//-------------------------------------------------------------------------------------------------
// return true if we auto-reloaded our clip after firing.
//DECLARE_PERF_TIMER(fireWeapon)
Bool Weapon::privateFireWeapon(
	const Object *sourceObj,
	Object *victimObj,
	const Coord3D* victimPos,
	Bool isProjectileDetonation,
	Bool ignoreRanges,
	WeaponBonusConditionFlags extraBonusFlags,
	ObjectID* projectileID,
	Bool inflictDamage,
	ObjectID detonationVictimID,
	Bool playFireFX
)
{
	//CRCDEBUG_LOG(("Weapon::privateFireWeapon() for %s", DescribeObject(sourceObj).str()));
	//USE_PERF_TIMER(fireWeapon)
	if (projectileID)
		*projectileID = INVALID_ID;

	if (!m_template)
		return false;

	// If we are a networked weapon, tell everyone nearby they might want to get in on this shot
	if( m_template->getRequestAssistRange()  &&  victimObj )
		processRequestAssistance( sourceObj, victimObj );

	//For weapon templates that have the leech range weapon flag set, it essentially grants
	//the weapon unlimited range for the remainder of the attack. While it's triggered here
	//it's the AIAttackState machine that actually uses and resets this value.
	//This makes the ASSUMPTION that it is IMPOSSIBLE TO FIRE A WEAPON WITHOUT BEING IN AN AIATTACKSTATE
	//
	// @todo srj -- this isn't a universally true assertion! eg, FireWeaponDie lets you do this easily.
	//
	if( m_template->isLeechRangeWeapon() )
	{
		setLeechRangeActive( TRUE );
	}

	//Special case damage type overrides requiring special handling.
	switch( m_template->getDamageType() )
	{
		case DAMAGE_DEPLOY:
		{
			const AIUpdateInterface *ai = sourceObj->getAI();
			if( ai )
			{
				const AssaultTransportAIInterface *atInterface = ai->getAssaultTransportAIInterface();
				if( atInterface )
				{
					atInterface->beginAssault( victimObj );
				}
			}
			break;
		}

		case DAMAGE_DISARM:
		{
			if (sourceObj && victimObj)
			{
				Bool found = false;
				for (BehaviorModule** bmi = victimObj->getBehaviorModules(); *bmi; ++bmi)
				{
					LandMineInterface* lmi = (*bmi)->getLandMineInterface();
					if (lmi)
					{
						VeterancyLevel v = sourceObj->getVeterancyLevel();
						FXList::doFXPos(m_template->getFireFX(v), victimObj->getPosition(), victimObj->getTransformMatrix(), 0, victimObj->getPosition(), 0);
						lmi->disarm();
						found = true;
						break;
					}
				}

				// it's a mine, but doesn't have LandMineInterface...
				if( (!found && victimObj->isKindOf( KINDOF_MINE )) || victimObj->isKindOf( KINDOF_BOOBY_TRAP ) || victimObj->isKindOf( KINDOF_DEMOTRAP ) )
				{
					VeterancyLevel v = sourceObj->getVeterancyLevel();
					FXList::doFXPos(m_template->getFireFX(v), victimObj->getPosition(), victimObj->getTransformMatrix(), 0, victimObj->getPosition(), 0);
					TheGameLogic->destroyObject( victimObj );// douse this thing before somebody gets hurt!
					found = true;
				}

				if( found )
				{
					sourceObj->getControllingPlayer()->getAcademyStats()->recordMineCleared();
				}
			}

			--m_maxShotCount;
			--m_ammoInClip;	// so we can use the delay between shots on the mine clearing weapon
			if (m_ammoInClip <= 0 && m_template->getAutoReloadsClip())
			{
				reloadAmmo(sourceObj);
				return TRUE;	// reloaded
			}
			else
			{
				return FALSE;	// did not reload
			}
		}

		case DAMAGE_HACK:
		{
			//We're using a hacker unit to hack a target. Hacking has various effects and
			//instead of inflicting damage, we are waiting for a period of time until the hack takes effect.
			//return FALSE;
		}
	}

	WeaponBonus bonus;
	computeBonus(sourceObj, extraBonusFlags, bonus);
	if (RetailFileLayers::enabled() &&
		((!isProjectileDetonation && !m_template->hasRequiredProjectile(sourceObj)) ||
			(m_template->m_passengerProportionalAttack && bonus.getField(WeaponBonus::DAMAGE) <= 0.0f)))
		return FALSE;

	DEBUG_ASSERTCRASH(getStatus() != OUT_OF_AMMO, ("Hmm, firing weapon that is OUT_OF_AMMO"));
	DEBUG_ASSERTCRASH(getStatus() == READY_TO_FIRE, ("Hmm, Weapon is firing more often than should be possible"));
	DEBUG_ASSERTCRASH(m_ammoInClip > 0, ("Hmm, firing an empty weapon"));

	if (getStatus() != READY_TO_FIRE)
		return false;

	UnsignedInt now = TheGameLogic->getFrame();
	Bool reloaded = false;
	if (m_ammoInClip > 0)
	{
		Int linearDelay = -1;
		// TheSuperHackers @logic-client-separation helmutbuhler 11/04/2025
		// barrelCount shouln't depend on Drawable, which belongs to client.
		Int barrelCount = sourceObj->getDrawable()->getBarrelCount(m_wslot);
		if (m_curBarrel >= barrelCount)
		{
			m_curBarrel = 0;
			m_numShotsForCurBarrel = m_template->getShotsPerBarrel();
		}

		if (RetailFileLayers::enabled() && !isProjectileDetonation && !m_template->m_linearTargets.empty())
		{
			const auto& targets = m_template->m_linearTargets;
			const Coord3D& origin = *sourceObj->getPosition();
			const Real facing = sourceObj->getOrientation();
			const Real sine = Sin(facing);
			const Real cosine = Cos(facing);
			m_template->getDelayBetweenShots(bonus);
			for (size_t step = 0; step < targets.size(); ++step)
			{
				const auto& target = targets[m_linearTargetIndex];
				const Bool skip = m_template->m_ignoreLinearFirstTarget && m_linearTargetIndex == 0;
				m_linearTargetIndex = (m_linearTargetIndex + 1) % targets.size();
				linearDelay = RotwkDuration::engineFrames(std::max(0, target.delay), LOGICFRAMES_PER_SECOND);
				if (!skip)
				{
					Coord3D pos;
					pos.x = origin.x + cosine * target.offset.x - sine * target.offset.y;
					pos.y = origin.y + sine * target.offset.x + cosine * target.offset.y;
					pos.z = TheTerrainLogic->getGroundHeight(pos.x, pos.y);
					m_template->fireWeaponTemplate(sourceObj, m_wslot, m_curBarrel, nullptr, &pos, bonus,
						FALSE, ignoreRanges, this, projectileID, inflictDamage, INVALID_ID, playFireFX);
				}
				if (linearDelay > 0)
					break;
			}
		}
		else if( !m_scatterTargetsUnused.empty() )
		{
			// If I have a set scatter pattern, I need to offset the target by a random pick from that pattern
			if( victimObj )
			{
				victimPos = victimObj->getPosition();
				victimObj = nullptr;
			}
			Coord3D targetPos = *victimPos; // need to copy, as this pointer is actually inside somebody potentially
			Int randomPick = GameLogicRandomValue( 0, m_scatterTargetsUnused.size() - 1 );
			Int targetIndex = m_scatterTargetsUnused[randomPick];

			Real scatterTargetScalar = getScatterTargetScalar();// essentially a radius, but operates only on this scatterTarget table
			Coord2D scatterOffset = m_template->getScatterTargetsVector().at( targetIndex );

			scatterOffset.x *= scatterTargetScalar;
			scatterOffset.y *= scatterTargetScalar;

			targetPos.x += scatterOffset.x;
			targetPos.y += scatterOffset.y;

			targetPos.z = TheTerrainLogic->getGroundHeight(targetPos.x, targetPos.y);

			// To erase from a vector, put the last on the one you used and pop the back.
			m_scatterTargetsUnused[randomPick] = m_scatterTargetsUnused.back();
			m_scatterTargetsUnused.pop_back();
			m_template->fireWeaponTemplate(sourceObj, m_wslot, m_curBarrel, victimObj, &targetPos, bonus,
				isProjectileDetonation, ignoreRanges, this, projectileID, inflictDamage, INVALID_ID, playFireFX);
		}
		else
		{
			m_template->fireWeaponTemplate(sourceObj, m_wslot, m_curBarrel, victimObj, victimPos, bonus,
				isProjectileDetonation, ignoreRanges, this, projectileID, inflictDamage, detonationVictimID, playFireFX);
		}

		m_lastFireFrame = now;
		--m_ammoInClip;
		--m_maxShotCount;
		--m_numShotsForCurBarrel;
		if (m_numShotsForCurBarrel <= 0)
		{
			++m_curBarrel;
			m_numShotsForCurBarrel = m_template->getShotsPerBarrel();
		}

		if (m_ammoInClip <= 0)
		{
			if (m_template->getAutoReloadsClip())
			{
				reloadAmmo(sourceObj);
				reloaded = true;
			}
			else
			{
				m_status = OUT_OF_AMMO;
				m_whenWeCanFireAgain = 0x7fffffff;
				//CRCDEBUG_LOG(("Just set m_whenWeCanFireAgain to %d in Weapon::privateFireWeapon 1", m_whenWeCanFireAgain));
			}
		}
		else
		{
			m_status = BETWEEN_FIRING_SHOTS;
			//CRCDEBUG_LOG(("Weapon::privateFireWeapon() just set m_status to BETWEEN_FIRING_SHOTS"));
			Int delay = linearDelay >= 0 ? linearDelay : m_template->getDelayBetweenShots(bonus);
			m_whenLastReloadStarted = now;
			m_whenWeCanFireAgain = now + delay;
			if (RetailTestCommands::tracesCombat())
				std::fprintf(stderr, "TEST cooldown frame=%u weapon=%s source=%u phase=between frames=%d\n",
					now, getName().str(), static_cast<UnsignedInt>(sourceObj->getID()), delay);
			//CRCDEBUG_LOG(("Just set m_whenWeCanFireAgain to %d (delay is %d) in Weapon::privateFireWeapon", m_whenWeCanFireAgain, delay));

			// if we are sharing reload times
			// go through other weapons in weapon set
			// set their m_whenWeCanFireAgain to this guy's delay
			// set their m_status to this guy's status

			if ( sourceObj->isReloadTimeShared() )
			{
				for (Int wt = 0; wt<WEAPONSLOT_COUNT; wt++)
				{
					Weapon *weapon = sourceObj->getWeaponInWeaponSlot((WeaponSlotType)wt);
					if (weapon)
					{
						weapon->setPossibleNextShotFrame(m_whenWeCanFireAgain);
						//CRCDEBUG_LOG(("Just set m_whenWeCanFireAgain to %d in Weapon::privateFireWeapon 3", m_whenWeCanFireAgain));
						weapon->setStatus(BETWEEN_FIRING_SHOTS);
					}
				}
			}

		}
	}

	return reloaded;
}

void Weapon::inheritSharedTimer(const Weapon& previous)
{
	if (!previous.m_template->m_shareTimers || previous.m_status == OUT_OF_AMMO || !m_ammoInClip ||
		previous.m_whenWeCanFireAgain <= TheGameLogic->getFrame() ||
		m_whenWeCanFireAgain >= previous.m_whenWeCanFireAgain)
		return;

	m_whenLastReloadStarted = previous.m_whenLastReloadStarted;
	m_whenWeCanFireAgain = previous.m_whenWeCanFireAgain;
	m_status = BETWEEN_FIRING_SHOTS;
}

//-------------------------------------------------------------------------------------------------
void WeaponTemplate::playPreAttackFX(const Object& source, const Object* target, const Coord3D* targetPosition) const
{
	if (!RetailFileLayers::enabled() || !m_preAttackFX)
		return;
	const ScopedVisualRandom random(TRUE);
	if (targetPosition && !target)
		FXList::doFXPos(m_preAttackFX, source.getPosition(), source.getTransformMatrix(), 0.0f,
			targetPosition, 0.0f, &source);
	else
		FXList::doFXObj(m_preAttackFX, &source, target);
}

void Weapon::preFireWeapon( const Object *source, const Object *victim )
{
	Int delay = getPreAttackDelay(source, victim);
	if (delay > 0 && RetailFileLayers::enabled() && m_template->m_preAttackRandomAmount)
	{
		constexpr Int framesPerTick = LOGICFRAMES_PER_SECOND / RotwkDuration::FRAMES_PER_SECOND;
		delay += GameLogicRandomValue(0, m_template->m_preAttackRandomAmount / framesPerTick) * framesPerTick;
	}
	if( delay > 0 )
	{
		const UnsignedInt frame = TheGameLogic->getFrame();
		if (!isHoldingMovementForReload(frame))
			setStatus(PRE_ATTACK);
		setPreAttackFinishedFrame(frame + delay);
		m_template->playPreAttackFX(*source, victim);
		if( m_template->isLeechRangeWeapon() )
		{
			setLeechRangeActive( TRUE );
		}
	}
}

//-------------------------------------------------------------------------------------------------
Bool Weapon::fireWeapon(const Object *source, Object *target, ObjectID* projectileID)
{
	//CRCDEBUG_LOG(("Weapon::fireWeapon() for %s at %s", DescribeObject(source).str(), DescribeObject(target).str()));
	return privateFireWeapon(source, target, nullptr, false, hasCommittedAttack(), 0, projectileID, TRUE);
}

//-------------------------------------------------------------------------------------------------
// return true if we auto-reloaded our clip after firing.
Bool Weapon::fireWeapon(const Object *source, const Coord3D* pos, ObjectID* projectileID)
{
	//CRCDEBUG_LOG(("Weapon::fireWeapon() for %s", DescribeObject(source).str()));
	return privateFireWeapon(source, nullptr, pos, false, hasCommittedAttack(), 0, projectileID, TRUE);
}

//-------------------------------------------------------------------------------------------------
void Weapon::fireProjectileDetonationWeapon(const Object *source, Object *target, WeaponBonusConditionFlags extraBonusFlags, Bool inflictDamage )
{
	//CRCDEBUG_LOG(("Weapon::fireProjectileDetonationWeapon() for %sat %s", DescribeObject(source).str(), DescribeObject(target).str()));
	privateFireWeapon( source, target, nullptr, true, false, extraBonusFlags, nullptr, inflictDamage );
}

//-------------------------------------------------------------------------------------------------
void Weapon::fireProjectileDetonationWeapon(const Object *source, const Coord3D* pos, WeaponBonusConditionFlags extraBonusFlags, Bool inflictDamage, ObjectID detonationVictimID )
{
	//CRCDEBUG_LOG(("Weapon::fireProjectileDetonationWeapon() for %s", DescribeObject(source).str()));
	privateFireWeapon( source, nullptr, pos, true, false, extraBonusFlags, nullptr, inflictDamage, detonationVictimID );
}

//-------------------------------------------------------------------------------------------------
//Currently, this function was added to allow a script to force fire a weapon,
//and immediately gain control of the weapon that was fired to give it special orders...
Object* Weapon::forceFireWeapon( const Object *source, const Coord3D *pos, Bool retailTimedFX )
{
	//CRCDEBUG_LOG(("Weapon::forceFireWeapon() for %s", DescribeObject(source).str()));
	//Force the ammo to load instantly.
	//loadAmmoNow( source );
	//Fire the weapon at the position. Internally, it'll store the weapon projectile ID if so created.
	ObjectID projectileID = INVALID_ID;
	const Bool ignoreRange = true;
	constexpr UnsignedInt framesPerTick = RotwkDuration::engineFrames(1, LOGICFRAMES_PER_SECOND);
	const Bool playFireFX = !retailTimedFX || !RetailFileLayers::enabled() || !m_lastFireFrame ||
		m_lastFireFrame / framesPerTick != TheGameLogic->getFrame() / framesPerTick;
	privateFireWeapon(source, nullptr, pos, false, ignoreRange, 0, &projectileID, TRUE, INVALID_ID, playFireFX);
	return TheGameLogic->findObjectByID( projectileID );
}

//-------------------------------------------------------------------------------------------------
UnsignedInt Weapon::getPossibleNextShotFrame() const
{
	if (m_lastFireFrame && m_template->getIdleAfterFiringDelay())
		return std::max(m_whenWeCanFireAgain, m_lastFireFrame + m_template->getIdleAfterFiringDelay());
	return m_whenWeCanFireAgain;
}

Bool Weapon::canFireInCurrentMovement(const Object* source) const
{
	if (!RetailFileLayers::enabled() || source->isKindOf(KINDOF_PROJECTILE))
		return TRUE;

	const auto& conditions = source->getLogicModelConditions();
	if (conditions.test(MODELCONDITION_CHARGING))
		return m_template->canFireWhileCharging();

	const auto* physics = source->getPhysics();
	if (!physics || m_template->canFireWhileMoving() || source->getEnclosingContainedBy())
		return TRUE;

	const auto* velocity = physics->getVelocity();
	return velocity->x * velocity->x + velocity->y * velocity->y < 0.01f;
}

Bool Weapon::isWaitingToIdle() const
{
	const UnsignedInt followThrough = RetailFileLayers::enabled() && m_template->m_requireFollowThru
		? m_template->getFiringDuration() : 0;
	return m_lastFireFrame &&
		TheGameLogic->getFrame() < m_lastFireFrame + std::max(followThrough, m_template->getIdleAfterFiringDelay());
}

Bool Weapon::isHoldingMovementForReload(UnsignedInt frame) const
{
	return m_template->holdsDuringReload() && m_status == RELOADING_CLIP && frame < m_whenWeCanFireAgain;
}

Bool Weapon::hasCommittedAttack() const
{
	return RetailFileLayers::enabled() && m_template->finishesAttackOnceStarted() &&
		m_whenPreAttackFinished > m_lastFireFrame;
}

WeaponStatus Weapon::getStatus() const
{
	UnsignedInt now = TheGameLogic->getFrame();
	if( now < m_whenPreAttackFinished )
	{
		return PRE_ATTACK;
	}
	if( now >= getPossibleNextShotFrame() )
	{
		if (m_ammoInClip > 0)
			m_status = READY_TO_FIRE;
		else
			m_status = OUT_OF_AMMO;
		//CRCDEBUG_LOG(("Weapon::getStatus() just set m_status to %d (ammo in clip is %d)", m_status, m_ammoInClip));
	}
	return m_status;
}

//-------------------------------------------------------------------------------------------------
Bool Weapon::isWithinTargetPitch(const Object *source, const Object *victim) const
{
	if (isContactWeapon() || !isPitchLimited())
		return true;

	const Coord3D* src = source->getPosition();
	const Coord3D* dst = victim->getPosition();

	const Real ACCEPTABLE_DZ = 10.0f;
	if (fabs(dst->z - src->z) < ACCEPTABLE_DZ)
		return true;	// always good enough if dz is small, regardless of pitch

	Real minPitch, maxPitch;
	source->getGeometryInfo().calcPitches(*src, victim->getGeometryInfo(), *dst, minPitch, maxPitch);

	// if there's any intersection between the the two pitch ranges, we're good to go.
	if ((minPitch >= m_template->getMinTargetPitch() && minPitch <= m_template->getMaxTargetPitch()) ||
			(maxPitch >= m_template->getMinTargetPitch() && maxPitch <= m_template->getMaxTargetPitch()) ||
			(minPitch <= m_template->getMinTargetPitch() && maxPitch >= m_template->getMaxTargetPitch()))
		return true;

	//DEBUG_LOG(("pitch %f-%f is out of range",rad2deg(minPitch),rad2deg(maxPitch),rad2deg(m_template->getMinTargetPitch()),rad2deg(m_template->getMaxTargetPitch())));
	return false;
}

//-------------------------------------------------------------------------------------------------
Real Weapon::getPrimaryDamageRadius(const Object *source) const
{
	WeaponBonus bonus;
	computeBonus(source, 0, bonus);
	return m_template->getPrimaryDamageRadius(bonus);
}

//-------------------------------------------------------------------------------------------------
Bool Weapon::isDamageWeapon() const
{
	//These damage types are special attacks that don't do damage directly, even
	//if they can indirectly. These are here to prevent the UI from allowing the
	//user to mouseover a target and think it can attack it using these types.
	switch( m_template->getDamageType() )
	{
		case DAMAGE_DEPLOY:
			//Kris @todo
			//Evaluate a better way to handle this weapon type... doesn't fit being a damage weapon.
			//May want to check if cargo can attack!
			return TRUE;

		case DAMAGE_DISARM:
			return TRUE;	// hmm, can only "damage" mines, but still...

		case DAMAGE_HACK:
			return FALSE;
	}

	if (m_template->hasNuggets())
		return m_template->nuggetsCanDamage();

	//Use no bonus
	WeaponBonus whoCares;
	if( m_template->getPrimaryDamage( whoCares ) > 0.0f || m_template->getSecondaryDamage( whoCares ) > 0.0f )
	{
		return TRUE;
	}

	return FALSE;
}

//-------------------------------------------------------------------------------------------------
Int Weapon::getPreAttackDelay( const Object *source, const Object *victim ) const
{
	// Look for a reason to return zero and have no delay.
	WeaponPrefireType type = m_template->getPrefireType();
	if( type == PREFIRE_PER_CLIP )
	{
		if( m_template->getClipSize() > 0  &&  m_ammoInClip < m_template->getClipSize() )
			return 0;// I only delay once a clip, and this is not the first shot
	}
	else if( type == PREFIRE_PER_ATTACK )
	{
		if( source->getNumConsecutiveShotsFiredAtTarget( victim ) > 0 )
			return 0;// I only delay once an attack, and I have already shot this guy
	}
	//else it is per shot, so it always applies

	WeaponBonus bonus;
	computeBonus(source, 0, bonus);
	return m_template->getPreAttackDelay( bonus );
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
class AssistanceRequestData
{
public:
	AssistanceRequestData();

	const Object *m_requestingObject;
	Object *m_victimObject;
	Real m_requestDistanceSquared;
};

//-------------------------------------------------------------------------------------------------
AssistanceRequestData::AssistanceRequestData()
{
	m_requestingObject = nullptr;
	m_victimObject = nullptr;
	m_requestDistanceSquared = 0.0f;
}

//-------------------------------------------------------------------------------------------------
static void makeAssistanceRequest( Object *requestOf, void *userData )
{
	AssistanceRequestData *requestData = (AssistanceRequestData *)userData;

	// Don't ask ourselves (can't believe I forgot this one)
	if( requestOf == requestData->m_requestingObject )
		return;

	// Only request of our kind of people
	if( !requestOf->getTemplate()->isEquivalentTo( requestData->m_requestingObject->getTemplate() ) )
		return;

	// Who are close enough
	Real distSq = ThePartitionManager->getDistanceSquared( requestOf, requestData->m_requestingObject, FROM_CENTER_2D );
	if( distSq > requestData->m_requestDistanceSquared )
		return;

	// and respond to requests
	static const NameKeyType key_assistUpdate = NAMEKEY("AssistedTargetingUpdate");
	AssistedTargetingUpdate *assistModule = (AssistedTargetingUpdate*)requestOf->findUpdateModule(key_assistUpdate);
	if( assistModule == nullptr )
		return;

	// and say yes
	if( !assistModule->isFreeToAssist() )
		return;

	assistModule->assistAttack( requestData->m_requestingObject, requestData->m_victimObject );
}

//-------------------------------------------------------------------------------------------------
void Weapon::processRequestAssistance( const Object *requestingObject, Object *victimObject )
{
	// Iterate through our player's objects, and tell everyone like us within our assistance range
	// who is free to do so to assist us on this shot.
	Player *ourPlayer = requestingObject->getControllingPlayer();
	if( !ourPlayer )
		return;

	AssistanceRequestData requestData;
	requestData.m_requestingObject = requestingObject;
	requestData.m_victimObject = victimObject;
	requestData.m_requestDistanceSquared = m_template->getRequestAssistRange() * m_template->getRequestAssistRange();

	ourPlayer->iterateObjects( makeAssistanceRequest, &requestData );
}

//-------------------------------------------------------------------------------------------------
/*static*/ void Weapon::calcProjectileLaunchPosition(
	const Object* launcher,
	WeaponSlotType wslot,
	Int specificBarrelToUse,
	Matrix3D& worldTransform,
	Coord3D& worldPos,
	Bool requireLaunchBone
)
{
	if( launcher->getContainedBy() )
	{
		// If we are in an enclosing container, our launch position is our actual position.  Yes, I am putting
		// a minor case and an oft used function, but the major case is huge and full of math.
		if(launcher->getContainedBy()->getContain()->isEnclosingContainerFor(launcher))
		{
			worldTransform = *launcher->getTransformMatrix();
			Vector3 tmp = worldTransform.Get_Translation();
			worldPos.x = tmp.X;
			worldPos.y = tmp.Y;
			worldPos.z = tmp.Z;
			return;
		}
	}

	Real turretAngle = 0.0f;
	Real turretPitch = 0.0f;
	const AIUpdateInterface* ai = launcher->getAIUpdateInterface();
	WhichTurretType tur = ai ? ai->getWhichTurretForWeaponSlot(wslot, &turretAngle, &turretPitch) : TURRET_INVALID;
	//CRCDEBUG_LOG(("calcProjectileLaunchPosition(): Turret %d, slot %d, barrel %d for %s", tur, wslot, specificBarrelToUse, DescribeObject(launcher).str()));

	Matrix3D attachTransform(true);
	Coord3D turretRotPos = {0.0f, 0.0f, 0.0f};
	Coord3D turretPitchPos = {0.0f, 0.0f, 0.0f};
	const Drawable* draw = launcher->getDrawable();
	//CRCDEBUG_LOG(("Do we have a drawable? %d", (draw != nullptr)));
	if (!draw || !draw->getProjectileLaunchOffset(wslot, specificBarrelToUse, &attachTransform, tur, &turretRotPos, &turretPitchPos))
	{
		//CRCDEBUG_LOG(("ProjectileLaunchPos %d %d not found!",wslot, specificBarrelToUse));
		if (requireLaunchBone)
			DEBUG_CRASH(("ProjectileLaunchPos %d %d not found!",wslot, specificBarrelToUse));
		attachTransform.Make_Identity();
		turretRotPos.zero();
		turretPitchPos.zero();
	}
	if (tur != TURRET_INVALID)
	{
		// The attach transform is the pristine front and center position of the fire point
		// We can't read from the client, so we need to reproduce the actual point that
		// takes turn and pitch into account.
		Matrix3D turnAdjustment(1);
		Matrix3D pitchAdjustment(1);

		// To rotate about a point, move that point to 0,0, rotate, then move it back.
		// Pre rotate will keep the first twist from screwing the angle of the second pitch
		pitchAdjustment.Translate( turretPitchPos.x, turretPitchPos.y, turretPitchPos.z );
		pitchAdjustment.In_Place_Pre_Rotate_Y(-turretPitch);
		pitchAdjustment.Translate( -turretPitchPos.x, -turretPitchPos.y, -turretPitchPos.z );

		turnAdjustment.Translate( turretRotPos.x, turretRotPos.y, turretRotPos.z );
		turnAdjustment.In_Place_Pre_Rotate_Z(turretAngle);
		turnAdjustment.Translate( -turretRotPos.x, -turretRotPos.y, -turretRotPos.z );

#ifdef ALLOW_TEMPORARIES
		attachTransform = turnAdjustment * pitchAdjustment * attachTransform;
#else
		Matrix3D tmp = attachTransform;
		attachTransform.mul(turnAdjustment, pitchAdjustment);
		attachTransform.postMul(tmp);
#endif
	}

//#if defined(RTS_DEBUG)
//  Real muzzleHeight = attachTransform.Get_Z_Translation();
//  DEBUG_ASSERTCRASH( muzzleHeight > 0.001f, ("YOUR TURRET HAS A VERY LOW PROJECTILE LAUNCH POSITION, BUT FOUND A VALID BONE. DID YOU PICK THE WRONG ONE? %s", launcher->getTemplate()->getName().str()));
//#endif

  launcher->convertBonePosToWorldPos(nullptr, &attachTransform, nullptr, &worldTransform);

	Vector3 tmp = worldTransform.Get_Translation();
	worldPos.x = tmp.X;
	worldPos.y = tmp.Y;
	worldPos.z = tmp.Z;
}

//-------------------------------------------------------------------------------------------------
/*static*/ void Weapon::positionProjectileForLaunch(
	Object* projectile,
	const Object* launcher,
	WeaponSlotType wslot,
	Int specificBarrelToUse,
	Bool redirectExperience
)
{
	//CRCDEBUG_LOG(("Weapon::positionProjectileForLaunch() for %s from %s",
		//DescribeObject(projectile).str(), DescribeObject(launcher).str()));

	// if our launch vehicle is gone, destroy ourselves
	if (launcher == nullptr)
	{
		TheGameLogic->destroyObject( projectile );
		return;
	}

	Matrix3D worldTransform(true);
	Coord3D worldPos;

	Weapon::calcProjectileLaunchPosition(launcher, wslot, specificBarrelToUse, worldTransform, worldPos);

	projectile->getDrawable()->setDrawableHidden(false);
	projectile->setTransformMatrix(&worldTransform);
	projectile->setPosition(&worldPos);
	if (redirectExperience)
		projectile->getExperienceTracker()->setExperienceSink(launcher->getID());

	const PhysicsBehavior* launcherPhys = launcher->getPhysics();
	PhysicsBehavior* missilePhys = projectile->getPhysics();
	if (launcherPhys && missilePhys)
	{
		launcherPhys->transferVelocityTo(missilePhys);
		missilePhys->setIgnoreCollisionsWith(launcher);
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void Weapon::getFiringLineOfSightOrigin(const Object* source, Coord3D& origin) const
{
	//GS 1-6-03
	// Sorry, but we have to simplify this.  If we take the actual projectile launch pos, then
	// that point can change. Take a Ranger with his gun on his shoulder.  His point is very high so
	// he clears this check and transitions to attacking.  This puts his gun at waist level and
	// now he fails this check so he transitions back.  Our height won't change.
	origin.z += source->getGeometryInfo().getMaxHeightAbovePosition();

/*
	if (m_template->getProjectileTemplate() == nullptr)
	{
		// note that we want to measure from the top of the collision
		// shape, not the bottom! (most objects have eyes a lot closer
		// to their head than their feet. if we have really odd critters
		// with eye-feet, we'll need to change this assumption.)
		origin.z += source->getGeometryInfo().getMaxHeightAbovePosition();
	}
	else
	{
		Matrix3D tmp(true);
		Coord3D launchPos = {0.0f, 0.0f, 0.0f};
		calcProjectileLaunchPosition(source, m_wslot, m_curBarrel, tmp, launchPos);
		origin.x += launchPos.x - source->getPosition()->x;
		origin.y += launchPos.y - source->getPosition()->y;
		origin.z += launchPos.z - source->getPosition()->z;
	}
*/
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
Bool Weapon::isClearFiringLineOfSightTerrain(const Object* source, const Object* victim) const
{
	Coord3D origin;
	origin = *source->getPosition();
	//CRCDEBUG_LOG(("Weapon::isClearFiringLineOfSightTerrain(Object) for %s", DescribeObject(source).str()));
	//DUMPCOORD3D(&origin);
	getFiringLineOfSightOrigin(source, origin);
	Coord3D victimPos;
	victim->getGeometryInfo().getCenterPosition( *victim->getPosition(), victimPos );
	//CRCDEBUG_LOG(("Weapon::isClearFiringLineOfSightTerrain() - victimPos is (%g,%g,%g) (%X,%X,%X)",
	//	victimPos.x, victimPos.y, victimPos.z,
	//	AS_INT(victimPos.x),AS_INT(victimPos.y),AS_INT(victimPos.z)));
	return ThePartitionManager->isClearLineOfSightTerrain(nullptr, origin, nullptr, victimPos);
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
Bool Weapon::isClearFiringLineOfSightTerrain(const Object* source, const Coord3D& victimPos) const
{
	Coord3D origin;
	origin = *source->getPosition();
	//CRCDEBUG_LOG(("Weapon::isClearFiringLineOfSightTerrain(Coord3D) for %s", DescribeObject(source).str()));
	//DUMPCOORD3D(&origin);
	getFiringLineOfSightOrigin(source, origin);
	return ThePartitionManager->isClearLineOfSightTerrain(nullptr, origin, nullptr, victimPos);
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
/** Determine whether if source was at goalPos whether it would have clear line of sight. */
Bool Weapon::isClearGoalFiringLineOfSightTerrain(const Object* source, const Coord3D& goalPos, const Object* victim) const
{
	Coord3D origin=goalPos;
	//CRCDEBUG_LOG(("Weapon::isClearGoalFiringLineOfSightTerrain(Object) for %s", DescribeObject(source).str()));
	//DUMPCOORD3D(&origin);
	getFiringLineOfSightOrigin(source, origin);
	Coord3D victimPos;
	victim->getGeometryInfo().getCenterPosition( *victim->getPosition(), victimPos );
	return ThePartitionManager->isClearLineOfSightTerrain(nullptr, origin, nullptr, victimPos);
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
/** Determine whether if source was at goalPos whether it would have clear line of sight. */
Bool Weapon::isClearGoalFiringLineOfSightTerrain(const Object* source, const Coord3D& goalPos, const Coord3D& victimPos) const
{
	Coord3D origin=goalPos;
	//CRCDEBUG_LOG(("Weapon::isClearGoalFiringLineOfSightTerrain(Coord3D) for %s", DescribeObject(source).str()));
	//DUMPCOORD3D(&origin);
	getFiringLineOfSightOrigin(source, origin);
	//CRCDEBUG_LOG(("Weapon::isClearFiringLineOfSightTerrain() - victimPos is (%g,%g,%g) (%X,%X,%X)",
	//	victimPos.x, victimPos.y, victimPos.z,
	//	AS_INT(victimPos.x),AS_INT(victimPos.y),AS_INT(victimPos.z)));
	return ThePartitionManager->isClearLineOfSightTerrain(nullptr, origin, nullptr, victimPos);
}

//-------------------------------------------------------------------------------------------------
//Kris: Patch 1.01 - November 10, 2003
//This function was added to transfer key weapon stats for Jarmen Kell to and from the bike for
//the sniper attack, so he can share the stats.
//-------------------------------------------------------------------------------------------------
void Weapon::transferNextShotStatsFrom( const Weapon &weapon )
{
	m_whenWeCanFireAgain = weapon.getPossibleNextShotFrame();
	m_whenLastReloadStarted = weapon.getLastReloadStartedFrame();
	m_status = weapon.getStatus();
}


// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void Weapon::crc( Xfer *xfer )
{
	if (RetailFileLayers::enabled())
		crcLinearTarget(xfer);
#ifdef DEBUG_CRC
	AsciiString logString;
	AsciiString tmp;
	Bool doLogging = g_logObjectCRCs;
	if (doLogging)
	{
		tmp.format("CRC of weapon %s: ", m_template->getName().str());
		logString.concat(tmp);
	}
#endif // DEBUG_CRC

	AsciiString tmplName = m_template->getName();
	xfer->xferAsciiString(&tmplName);

	// slot
	xfer->xferUser( &m_wslot, sizeof( WeaponSlotType ) );
#ifdef DEBUG_CRC
	if (doLogging)
	{
		tmp.format("m_wslot %d ", m_wslot);
		logString.concat(tmp);
	}
#endif // DEBUG_CRC

	// status
	/*
	xfer->xferUser( &m_status, sizeof( WeaponStatus ) );
#ifdef DEBUG_CRC
	if (doLogging)
	{
		tmp.format("m_status %d ", m_status);
		logString.concat(tmp);
	}
#endif // DEBUG_CRC
	*/

	// ammo
	xfer->xferUnsignedInt( &m_ammoInClip );
#ifdef DEBUG_CRC
	if (doLogging)
	{
		tmp.format("m_ammoInClip %d ", m_ammoInClip);
		logString.concat(tmp);
	}
#endif // DEBUG_CRC

	// when can fire again
	xfer->xferUnsignedInt( &m_whenWeCanFireAgain );
#ifdef DEBUG_CRC
	if (doLogging)
	{
		tmp.format("m_whenWeCanFireAgain %d ", m_whenWeCanFireAgain);
		logString.concat(tmp);
	}
#endif // DEBUG_CRC

	// when pre attack finished
	xfer->xferUnsignedInt( &m_whenPreAttackFinished );
#ifdef DEBUG_CRC
	if (doLogging)
	{
		tmp.format("m_whenPreAttackFinished %d ", m_whenPreAttackFinished);
		logString.concat(tmp);
	}
#endif // DEBUG_CRC

	// when last reload started
	xfer->xferUnsignedInt( &m_whenLastReloadStarted );
#ifdef DEBUG_CRC
	if (doLogging)
	{
		tmp.format("m_whenLastReloadStarted %d ", m_whenLastReloadStarted);
		logString.concat(tmp);
	}
#endif // DEBUG_CRC

	// last fire frame
	xfer->xferUnsignedInt( &m_lastFireFrame );
#ifdef DEBUG_CRC
	if (doLogging)
	{
		tmp.format("m_lastFireFrame %d ", m_lastFireFrame);
		logString.concat(tmp);
	}
#endif // DEBUG_CRC

	// projectile stream object
	xfer->xferObjectID( &m_projectileStreamID );
#ifdef DEBUG_CRC
	if (doLogging)
	{
		tmp.format("projectileStreamID %d ", m_projectileStreamID);
		logString.concat(tmp);
	}
#endif // DEBUG_CRC

	// laser object (defunct)
	ObjectID laserIDUnused = INVALID_ID;
	xfer->xferObjectID( &laserIDUnused );
#ifdef DEBUG_CRC
	if (doLogging)
	{
		tmp.format("laserID %d ", laserIDUnused);
		logString.concat(tmp);
	}
#endif // DEBUG_CRC

	// max shot count
	xfer->xferInt( &m_maxShotCount );
#ifdef DEBUG_CRC
	if (doLogging)
	{
		tmp.format("m_maxShotCount %d ", m_maxShotCount);
		logString.concat(tmp);
	}
#endif // DEBUG_CRC

	// current barrel
	xfer->xferInt( &m_curBarrel );
#ifdef DEBUG_CRC
	if (doLogging)
	{
		tmp.format("m_curBarrel %d ", m_curBarrel);
		logString.concat(tmp);
	}
#endif // DEBUG_CRC

	// num shots for current barrel
	xfer->xferInt( &m_numShotsForCurBarrel );
#ifdef DEBUG_CRC
	if (doLogging)
	{
		tmp.format("m_numShotsForCurBarrel %d ", m_numShotsForCurBarrel);
		logString.concat(tmp);
	}
#endif // DEBUG_CRC

	// scatter targets unused
	UnsignedShort scatterCount = m_scatterTargetsUnused.size();
	xfer->xferUnsignedShort( &scatterCount );
#ifdef DEBUG_CRC
	if (doLogging)
	{
		tmp.format("scatterCount %d ", scatterCount);
		logString.concat(tmp);
	}
#endif // DEBUG_CRC

	Int intData;

	std::vector< Int >::const_iterator it;

	for( it = m_scatterTargetsUnused.begin(); it != m_scatterTargetsUnused.end(); ++it )
	{

		intData = *it;
		xfer->xferInt( &intData );
#ifdef DEBUG_CRC
		if (doLogging)
		{
			tmp.format("%d ", intData);
			logString.concat(tmp);
		}
#endif // DEBUG_CRC

	}

	// pitch limited
	xfer->xferBool( &m_pitchLimited );
#ifdef DEBUG_CRC
	if (doLogging)
	{
		tmp.format("m_pitchLimited %d ", m_pitchLimited);
		logString.concat(tmp);
	}
#endif // DEBUG_CRC

	// leech weapon range active
	xfer->xferBool( &m_leechWeaponRangeActive );
#ifdef DEBUG_CRC
	if (doLogging)
	{
		tmp.format("m_leechWeaponRangeActive %d ", m_leechWeaponRangeActive);
		logString.concat(tmp);
	}
#endif // DEBUG_CRC


#ifdef DEBUG_CRC
	if (doLogging)
	{
		CRCDEBUG_LOG(("%s", logString.str()));
	}
#endif // DEBUG_CRC

}

// ------------------------------------------------------------------------------------------------
/** Xfer
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void Weapon::crcLinearTarget(Xfer* xfer) const
{
	if (!m_template->m_linearTargets.empty())
	{
		UnsignedInt index = m_linearTargetIndex;
		xfer->xferUnsignedInt(&index);
	}
}

void Weapon::xfer( Xfer *xfer )
{
	// version
	const XferVersion currentVersion = RetailFileLayers::enabled() ? 4 : 3;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	if (version >= 2)
	{
		AsciiString tmplName = m_template->getName();
		xfer->xferAsciiString(&tmplName);
		if (xfer->getXferMode() == XFER_LOAD)
		{
			m_template = TheWeaponStore->findWeaponTemplate(tmplName);
			if (m_template == nullptr)
				throw INI_INVALID_DATA;
		}
	}

	// slot
	xfer->xferUser( &m_wslot, sizeof( WeaponSlotType ) );

	// status
	xfer->xferUser( &m_status, sizeof( WeaponStatus ) );

	// ammo
	xfer->xferUnsignedInt( &m_ammoInClip );

	// when can fire again
	xfer->xferUnsignedInt( &m_whenWeCanFireAgain );

	// when pre attack finished
	xfer->xferUnsignedInt( &m_whenPreAttackFinished );

	// when last reload started
	xfer->xferUnsignedInt( &m_whenLastReloadStarted );

	// last fire frame
	xfer->xferUnsignedInt( &m_lastFireFrame );

	// suspendFXFrame, this affects client only
	if ( version >= 3 )
		xfer->xferUnsignedInt( &m_suspendFXFrame );
	else
		m_suspendFXFrame = 0;

	// projectile stream object
	xfer->xferObjectID( &m_projectileStreamID );

	// laser object
	ObjectID laserIDUnused = INVALID_ID;
	xfer->xferObjectID( &laserIDUnused );

	// max shot count
	xfer->xferInt( &m_maxShotCount );

	// current barrel
	xfer->xferInt( &m_curBarrel );

	// num shots for current barrel
	xfer->xferInt( &m_numShotsForCurBarrel );

	// scatter targets unused
	UnsignedShort scatterCount = m_scatterTargetsUnused.size();
	xfer->xferUnsignedShort( &scatterCount );
	Int intData;
	if( xfer->getXferMode() == XFER_SAVE )
	{
		std::vector< Int >::const_iterator it;

		for( it = m_scatterTargetsUnused.begin(); it != m_scatterTargetsUnused.end(); ++it )
		{

			intData = *it;
			xfer->xferInt( &intData );

		}

	}
	else
	{

		// sanity, the scatter targets must be empty
		m_scatterTargetsUnused.clear();

		for( UnsignedShort i = 0; i < scatterCount; ++i )
		{

			xfer->xferInt( &intData );
			m_scatterTargetsUnused.push_back( intData );

		}

	}

	// pitch limited
	xfer->xferBool( &m_pitchLimited );

	// leech weapon range active
	xfer->xferBool( &m_leechWeaponRangeActive );

	if (version >= 4)
		xfer->xferUnsignedInt(&m_linearTargetIndex);
	if (version < 4 || m_linearTargetIndex >= m_template->m_linearTargets.size())
		m_linearTargetIndex = 0;
}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void Weapon::loadPostProcess()
{
	if( m_projectileStreamID != INVALID_ID )
	{
		Object* projectileStream = TheGameLogic->findObjectByID( m_projectileStreamID );
		if( projectileStream == nullptr )
		{
			m_projectileStreamID = INVALID_ID;
		}
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
void WeaponBonus::xfer(Xfer* xfer)
{
	for (Real& field : m_field)
		xfer->xferReal(&field);
	xfer->xferReal(&m_nuggetDamageAdd);
	xfer->xferReal(&m_nuggetDamageMultiplier);
	xfer->xferReal(&m_nuggetSpellMultiplier);
}

void WeaponBonus::appendBonuses(WeaponBonus& bonus) const
{
	for (int f = 0; f < WeaponBonus::FIELD_COUNT; ++f)
	{
		bonus.m_field[f] += this->m_field[f] - 1.0f;
	}
}

//-------------------------------------------------------------------------------------------------
/*static*/ void WeaponBonusSet::parseWeaponBonusSet(INI* ini, void* /*instance*/, void* store, const void* /*userData*/)
{
	WeaponBonusSet* self = (WeaponBonusSet*)store;
	self->parseWeaponBonusSet(ini);
}

//-------------------------------------------------------------------------------------------------
/*static*/ void WeaponBonusSet::parseWeaponBonusSetPtr(INI* ini, void* /*instance*/, void* store, const void* /*userData*/)
{
	WeaponBonusSet** selfPtr = (WeaponBonusSet**)store;
	(*selfPtr)->parseWeaponBonusSet(ini);
}

//-------------------------------------------------------------------------------------------------
void WeaponBonusSet::parseWeaponBonusSet(INI* ini)
{
	WeaponBonusConditionType wb = (WeaponBonusConditionType)INI::scanIndexList(ini->getNextToken(), TheWeaponBonusNames);
	WeaponBonus::Field wf = (WeaponBonus::Field)INI::scanIndexList(ini->getNextToken(), TheWeaponBonusFieldNames);
	m_bonus[wb].setField(wf, INI::scanPercentToReal(ini->getNextToken()));
}

//-------------------------------------------------------------------------------------------------
void WeaponBonusSet::appendBonuses(WeaponBonusConditionFlags flags, WeaponBonus& bonus) const
{
	if (flags == 0)
		return;	// my, that was easy

	for (int i = 0; i < WEAPONBONUSCONDITION_COUNT; ++i)
	{
		if ((flags & (1 << i)) == 0)
			continue;

		this->m_bonus[i].appendBonuses(bonus);
	}
}

