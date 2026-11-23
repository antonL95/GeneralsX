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

// FILE: SpecialPowerModule.cpp ///////////////////////////////////////////////////////////////////
// Author: Colin Day, April 2002
// Desc:	 Special power module interface
///////////////////////////////////////////////////////////////////////////////////////////////////

// USER INCLUDES //////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine
#include "GameClient/RetailEva.h"

#include "GameLogic/ExperienceTracker.h"

#include <memory>

#include "Common/GameAudio.h"
#include "Common/GameUtility.h"
#include "Common/GlobalData.h"
#include "Common/INI.h"
#include "Common/INIException.h"
#include "Common/ModuleFactory.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/RetailFileLayers.h"
#include "GameLogic/Module/AttributeModifierHelper.h"
#include "GameLogic/Module/RotwkAbilityModules.h"
#include "Common/Science.h"
#include "Common/SpecialPower.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"

#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/Module/AutoPickUpUpdate.h"
#include "GameLogic/Module/DeletionUpdate.h"
#include "GameLogic/Module/SpecialAbilityUpdate.h"
#include "GameLogic/Module/UpdateModule.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Module/AutoHealBehavior.h"
#include "GameLogic/Module/WorkerAIUpdate.h"
#include "GameLogic/Module/SpecialPowerUpdateModule.h"
#include "GameLogic/ScriptEngine.h"

#include "GameClient/Eva.h"
#include "GameClient/InGameUI.h"
#include "GameClient/ControlBar.h"
#include "GameClient/FXList.h"



//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
namespace
{
const char* const modifierCategories[] =
{
	"NONE", "LEADERSHIP", "FORMATION", "SPELL", "WEAPON", "STRUCTURE", "LEVEL",
	"BUFF", "DEBUFF", "STUN", "INNATE_ARMOR", "INNATE_DAMAGEMULT", "INNATE_VISION",
	"INNATE_AUTOHEAL", "INNATE_HEALTH", nullptr
};

const char* const weatherTypes[] =
{
	"NONE", "CLOUDY", "RAINY", "CLOUDYRAINY", "SUNNY", nullptr
};
}

SpecialPowerModuleData::SpecialPowerModuleData()
{

	m_specialPowerTemplate = nullptr;
	m_updateModuleStartsAttack = false;
	m_startsPaused = FALSE;
	m_startsReady = FALSE;
	m_scriptedSpecialPowerOnly = FALSE;

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
/* static */ void SpecialPowerModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	BehaviorModuleData::buildFieldParse( p );

	static const FieldParse dataFieldParse[] =
	{
		{ "SpecialPowerTemplate",			INI::parseSpecialPowerTemplate, nullptr, offsetof( SpecialPowerModuleData, m_specialPowerTemplate ) },
		{ "RequiredLevel", INI::parseInt, nullptr, offsetof(SpecialPowerModuleData, m_requiredRank) },
		{ "UpdateModuleStartsAttack", INI::parseBool,									nullptr, offsetof( SpecialPowerModuleData, m_updateModuleStartsAttack ) },
		{ "StartsPaused",							INI::parseBool,									nullptr, offsetof( SpecialPowerModuleData, m_startsPaused ) },
		{ "StartsReady",							INI::parseBool,									nullptr, offsetof( SpecialPowerModuleData, m_startsReady ) },
		{ "InitiateSound",						INI::parseAudioEventRTS,				nullptr, offsetof( SpecialPowerModuleData, m_initiateSound ) },
		{ "ScriptedSpecialPowerOnly", INI::parseBool,									nullptr, offsetof( SpecialPowerModuleData, m_scriptedSpecialPowerOnly ) },
		{ "AffectAllies", INI::parseBool, nullptr, offsetof( SpecialPowerModuleData, m_affectAllies ) },
		{ "AntiCategory", INI::parseIndexList, modifierCategories, offsetof( SpecialPowerModuleData, m_antiCategory ) },
		{ "AttributeModifier", INI::parseAsciiString, nullptr, offsetof( SpecialPowerModuleData, m_attributeModifier ) },
		{ "AttributeModifierAffects", SpecialPowerObjectFilter::parse, nullptr, offsetof( SpecialPowerModuleData, m_attributeModifierAffects ) },
		{ "AttributeModifierAffectsSelf", INI::parseBool, nullptr, offsetof( SpecialPowerModuleData, m_attributeModifierAffectsSelf ) },
		{ "AttributeModifierFX", INI::parseAsciiString, nullptr, offsetof( SpecialPowerModuleData, m_attributeModifierFX ) },
		{ "AttributeModifierRange", INI::parseReal, nullptr, offsetof( SpecialPowerModuleData, m_attributeModifierRange ) },
		{ "AttributeModifierWeatherBased", INI::parseBool, nullptr, offsetof( SpecialPowerModuleData, m_attributeModifierWeatherBased ) },
		{ "AvailableAtStart", INI::parseBool, nullptr, offsetof( SpecialPowerModuleData, m_availableAtStart ) },
		{ "ChangeWeather", INI::parseIndexList, weatherTypes, offsetof( SpecialPowerModuleData, m_changeWeather ) },
		{ "DisableDuringAnimDuration", INI::parseBool, nullptr, offsetof( SpecialPowerModuleData, m_disableDuringAnimDuration ) },
		{ "DistanceFromCommandCenter", INI::parseReal, nullptr, offsetof( SpecialPowerModuleData, m_distanceFromCommandCenter ) },
		{ "GiveLevels", INI::parseInt, nullptr, offsetof( SpecialPowerModuleData, m_giveLevels ) },
		{ "IdleWhenStartingPower", INI::parseBool, nullptr, offsetof( SpecialPowerModuleData, m_idleWhenStartingPower ) },
		{ "InitiateFX", INI::parseAsciiString, nullptr, offsetof( SpecialPowerModuleData, m_initiateFX ) },
		{ "OnTriggerRechargeSpecialPower", INI::parseAsciiStringVector, nullptr, offsetof( SpecialPowerModuleData, m_onTriggerRechargeSpecialPower ) },
		{ "ReEnableAntiCategory", INI::parseBool, nullptr, offsetof( SpecialPowerModuleData, m_reEnableAntiCategory ) },
		{ "RequirementsFilterMPSkirmish", SpecialPowerObjectFilter::parse, nullptr, offsetof( SpecialPowerModuleData, m_requirementsFilterMPSkirmish ) },
		{ "RequirementsFilterStrategic", SpecialPowerObjectFilter::parse, nullptr, offsetof( SpecialPowerModuleData, m_requirementsFilterStrategic ) },
		{ "SetModelCondition", parseSpecialPowerModelCondition, nullptr, offsetof( SpecialPowerModuleData, m_setModelCondition ) },
		{ "SetModelConditionTime", INI::parseReal, nullptr, offsetof( SpecialPowerModuleData, m_setModelConditionTime ) },
		{ "TargetAllSides", INI::parseBool, nullptr, offsetof( SpecialPowerModuleData, m_targetAllSides ) },
		{ "TargetEnemy", INI::parseBool, nullptr, offsetof( SpecialPowerModuleData, m_targetEnemy ) },
		{ "TriggerFX", INI::parseAsciiString, nullptr, offsetof( SpecialPowerModuleData, m_triggerFX ) },
		{ "UseDistanceFromCommandCenter", INI::parseBool, nullptr, offsetof( SpecialPowerModuleData, m_useDistanceFromCommandCenter ) },
		{ "WeatherDuration", INI::parseDurationUnsignedInt, nullptr, offsetof( SpecialPowerModuleData, m_weatherDuration ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(dataFieldParse);

}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
ModuleData* SpecialPowerModule::friend_newModuleData(INI* ini)
{
	auto data = std::unique_ptr<SpecialPowerModuleData>(MSGNEW("AllModuleData") SpecialPowerModuleData);
	if (ini)
	{
		ini->initFromINIMultiProc(data.get(), SpecialPowerModuleData::buildFieldParse);
		if (!data->m_specialPowerTemplate)
			throw INIException("SpecialPowerModule requires a defined SpecialPowerTemplate");
	}
	return data.release();
}

SpecialPowerModule::SpecialPowerModule( Thing *thing, const ModuleData *moduleData )
									: UpdateModule( thing, moduleData )
{

	// TheSuperHackers @bugfix fbraz 13/07/2026: Always initialize to 0.
	// Disabling RETAIL_COMPATIBLE_CRC previously forced this to 0xFFFFFFFF, which caused unpaused
	// targetless special powers (like Spy Satellite and Battle Plans) to never become ready.
	m_availableOnFrame = 0;
	m_pausedCount = 0;
	m_pausedOnFrame = 0;
	m_pausedPercent = 0.0f;

	// we won't be able to use the power for X number of frames now

	// if we're pre-built, start counting down
	if( !getObject()->getStatusBits().test( OBJECT_STATUS_UNDER_CONSTRUCTION ) )
	{
		//A sharedNSync special only startPowerRecharges when first scienced or when executed,
		//Since a new module with same SPTemplates may construct at any time.
		if ( getSpecialPowerTemplate()->isSharedNSync() == FALSE )
		{
			startPowerRecharge();

			if (startsReady())
				m_availableOnFrame = TheGameLogic->getFrame();
			else if (RetailFileLayers::enabled() && static_cast<const SpecialPowerModuleData*>(moduleData)->m_availableAtStart)
				m_availableOnFrame = TheGameLogic->getFrame();
		}
	}
	// WE USED TO DO THE POLL-EVERYBODY-AND-VOTE-ON-WHO-TO-SYNC-TO THING HERE,
	// BUT NO MORE, NOW IT IS HANDLED IN PLAYER

	// Some Special powers need to be activated by an Upgrade, so prevent the timer from going until then
	const SpecialPowerModuleData *md = (const SpecialPowerModuleData *)moduleData;
	if( md->m_startsPaused )
		pauseCountdown( TRUE );

	resolveSpecialPower();

	// Now, if we find that we have just come into being,
	// but there is already a science granted for our shared superweapon,
	// lets make sure TheIngameUI knows about our public timer
	// add this weapon to the UI if it has a public timer for all to see
	if( m_pausedCount == 0 &&
			getSpecialPowerTemplate()->isSharedNSync() == TRUE &&
			getSpecialPowerTemplate()->hasPublicTimer() == TRUE &&
			TheInGameUI &&
			getObject()->getControllingPlayer() &&
			getObject()->isKindOf( KINDOF_STRUCTURE ) )
	{
		TheInGameUI->addSuperweapon( getObject()->getControllingPlayer()->getPlayerIndex(),
																 getPowerName(),
																 getObject()->getID(),
																 getSpecialPowerModuleData()->m_specialPowerTemplate );
	}


}

//-------------------------------------------------------------------------------------------------
const AudioEventRTS& SpecialPowerModule::getInitiateSound() const
{
	return getSpecialPowerModuleData()->m_initiateSound;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
SpecialPowerModule::~SpecialPowerModule()
{

	if( TheInGameUI && getSpecialPowerModuleData()->m_specialPowerTemplate->hasPublicTimer() == TRUE &&
			getObject()->getControllingPlayer() )
 		TheInGameUI->removeSuperweapon( getObject()->getControllingPlayer()->getPlayerIndex(),
																		getPowerName(),
																		getObject()->getID(),
																		getSpecialPowerModuleData()->m_specialPowerTemplate );

}

//-------------------------------------------------------------------------------------------------
void SpecialPowerModule::setReadyFrame( UnsignedInt frame )
{
	m_availableOnFrame = frame;

	//If a script should change the ready frame, we need to update the paused frame. This value isn't
	//used directly to determine if paused or not... it uses m_pausedCount.
	m_pausedOnFrame = TheGameLogic->getFrame();
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void SpecialPowerModule::resolveSpecialPower()
{
	/*

	// if we're pre-built, and not from a command center, and a building register us with the UI
	if( getSpecialPowerModuleData()->m_specialPowerTemplate->hasPublicTimer() == TRUE &&
			TheGameLogic->getFrame() == 0 && getObject()->getControllingPlayer() &&
		  getObject()->isKindOf( KINDOF_COMMANDCENTER ) == FALSE &&
			getObject()->isKindOf( KINDOF_STRUCTURE ) )
	{
		//KM: The KINDOF_STRUCTURE check was made to prevent scripted bombers from registering their
		//    special powers as public timers.
		TheInGameUI->addSuperweapon( getObject()->getControllingPlayer()->getPlayerIndex(),
																 getPowerName(),
																 getObject()->getID(),
																 getSpecialPowerModuleData()->m_specialPowerTemplate );
	}
	*/
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void SpecialPowerModule::onSpecialPowerCreation()
{
	// THIS gets called by addScience(), that is, when the General has purchased a new special power,
	// and this module is thus activated.

	// start a power recharge going
	startPowerRecharge();

	// Dustin wants these special powers to start ready to fire,
	// so here (and only here) we will expressly set them to ready-now.
	if ( getSpecialPowerTemplate()->isSharedNSync())
	{
		Player *player = getObject()->getControllingPlayer();
		if ( player )
		{
			player->expressSpecialPowerReadyFrame( getSpecialPowerTemplate(), TheGameLogic->getFrame() );
			m_availableOnFrame = player->getOrStartSpecialPowerReadyFrame( getSpecialPowerTemplate() );
		}
	}

	// Some Special powers need to be activated by an Upgrade, so prevent the timer from going until then
	const SpecialPowerModuleData *md = getSpecialPowerModuleData();
	if( md->m_startsPaused )
		pauseCountdown( TRUE );

	// add this weapon to the UI if it has a public timer for all to see
	if( TheInGameUI && getSpecialPowerModuleData()->m_specialPowerTemplate->hasPublicTimer() == TRUE &&
			getObject()->getControllingPlayer() &&
			getObject()->isKindOf( KINDOF_STRUCTURE ) )
	{
		TheInGameUI->addSuperweapon( getObject()->getControllingPlayer()->getPlayerIndex(),
																 getPowerName(),
																 getObject()->getID(),
																 getSpecialPowerModuleData()->m_specialPowerTemplate );
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
ScienceType SpecialPowerModule::getRequiredScience() const
{

	return getSpecialPowerModuleData()->m_specialPowerTemplate->getRequiredScience();
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
const SpecialPowerTemplate * SpecialPowerModule::getSpecialPowerTemplate() const
{

	return getSpecialPowerModuleData()->m_specialPowerTemplate;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
AsciiString SpecialPowerModule::getPowerName() const
{

	return getSpecialPowerModuleData()->m_specialPowerTemplate->getName();
}

//-------------------------------------------------------------------------------------------------
/** Is this module designed for the power identifier template passed in? */
//-------------------------------------------------------------------------------------------------
Bool SpecialPowerModule::isModuleForPower( const SpecialPowerTemplate *specialPowerTemplate ) const
{

	// get the module data
	const SpecialPowerModuleData *modData = getSpecialPowerModuleData();

	//
	// if the special power template defined in the module data matches the template we want
	// to check then we are for it!
	//
	if( modData->m_specialPowerTemplate == specialPowerTemplate )
	{
		//We match templates.
		return TRUE;
	}
	//We don't match templates.
	return FALSE;

}

//-------------------------------------------------------------------------------------------------
/** Is this special power ready to use */
//-------------------------------------------------------------------------------------------------
Int SpecialPowerModule::getRequiredRank() const
{
	const auto* data = getSpecialPowerModuleData();
	return data->m_specialPowerTemplate ? std::max(data->m_requiredRank, data->m_specialPowerTemplate->getRequiredRank()) : data->m_requiredRank;
}

Bool SpecialPowerModule::meetsRequiredRank() const
{
	if (!RetailFileLayers::enabled())
		return TRUE;
	const Int requiredRank = getRequiredRank();
	return requiredRank <= 0 || getObject()->getExperienceTracker()->getRank() >= requiredRank;
}

Bool SpecialPowerModule::isReady() const
{
	if (!meetsRequiredRank())
		return FALSE;
#if defined(RTS_DEBUG) || defined(_ALLOW_DEBUG_CHEATS_IN_RELEASE)
	// this is a cheat ... remove this for release!
	if( TheGlobalData->m_specialPowerUsesDelay == FALSE )
		return TRUE;
#endif

	const Object* obj = getObject();
	const SpecialPowerModuleData *modData = getSpecialPowerModuleData();

	if ( obj && modData )
	{
		Player *player = getObject()->getControllingPlayer();
		if ( player )
		{
			if ( modData->m_specialPowerTemplate->isSharedNSync())
				return (TheGameLogic->getFrame() >= player->getOrStartSpecialPowerReadyFrame( modData->m_specialPowerTemplate ) );
		}
	}

	return (m_pausedCount == 0) && (TheGameLogic->getFrame() >= m_availableOnFrame);

}

//-------------------------------------------------------------------------------------------------
/** Get the percentage ready a special power is to use
	* 1.0f = ready now
	* 0.5f = 50% ready
	* 0.2f = 20% ready
	* etc ... */
//-------------------------------------------------------------------------------------------------
Real SpecialPowerModule::getPercentReady() const
{
	if (!meetsRequiredRank())
		return 0.0f;
	if( m_pausedCount > 0 && m_pausedPercent == 1.0f )
	{
			//Don't consider it ready if paused.
		return 0.99999f;
	}

#if defined(RTS_DEBUG) || defined(_ALLOW_DEBUG_CHEATS_IN_RELEASE)
	if( TheGlobalData->m_specialPowerUsesDelay == FALSE )
		return 1.0f;
#endif

	// easy case ... is ready
	if( isReady() )
		return 1.0f;

	if( m_pausedCount > 0 )
	{
		return m_pausedPercent;
	}

	// get the module data
	const SpecialPowerModuleData *modData = getSpecialPowerModuleData();

	// sanity
	if( modData->m_specialPowerTemplate == nullptr )
		return 0.0f;

	UnsignedInt readyFrame = m_availableOnFrame;

	//unless
	const Object* obj = getObject();
	if ( obj )
	{
		Player *player = getObject()->getControllingPlayer();
		if ( player )
		{
			if ( modData->m_specialPowerTemplate->isSharedNSync())
			{
				readyFrame = player->getOrStartSpecialPowerReadyFrame( getSpecialPowerTemplate() );
			}
		}
	}

	// calculate the percent
	Real percent = 1.0f - ((readyFrame - TheGameLogic->getFrame()) /
												 (Real)modData->m_specialPowerTemplate->getReloadTime());

	return percent;
}

Bool SpecialPowerModule::startsReady() const
{
#if RETAIL_COMPATIBLE_CRC
	return false;
#endif

	const SpecialPowerModuleData* modData = getSpecialPowerModuleData();
	return modData->m_startsReady;
}

//-------------------------------------------------------------------------------------------------
// A special power module that is only supposed to be fired via scripts. An example of this
// are the various cargo plane units we have. Scripters can launch specials from them after
// specifying a waypoint path for them to follow them.
//-------------------------------------------------------------------------------------------------
Bool SpecialPowerModule::isScriptOnly() const
{
	const SpecialPowerModuleData *modData = getSpecialPowerModuleData();
	return modData->m_scriptedSpecialPowerOnly;
}


//-------------------------------------------------------------------------------------------------
/** A special power has been used ... start the recharge process by computing the frame
	* we will become fully available on in the future again */
//-------------------------------------------------------------------------------------------------
void SpecialPowerModule::startPowerRecharge()
{
#if defined(RTS_DEBUG) || defined(_ALLOW_DEBUG_CHEATS_IN_RELEASE)
	// this is a cheat ... remove this for release!
	if( TheGlobalData->m_specialPowerUsesDelay == FALSE )
		return;
#endif

	const SpecialPowerModuleData *modData = getSpecialPowerModuleData();

	// sanity
	if( modData->m_specialPowerTemplate == nullptr )
	{
		DEBUG_CRASH(("special power not found"));
		return;
	}

	Object* obj = getObject();
	if (!obj)
		return;

	Player* player = getObject()->getControllingPlayer();
	if (!player)
		return;

	//Here, we make sure that general specials work as one between command centers
	// only factory type faction buildings should do this, and only with generals powers (in general)
	// but there are no restrictions on the use of SharedNSync on specialPowerTemplates at large
	if ( modData->m_specialPowerTemplate->isSharedNSync() )
	{
		player->resetOrStartSpecialPowerReadyFrame( modData->m_specialPowerTemplate );
	}
	else
	{
		UnsignedInt reload = getSpecialPowerTemplate()->getReloadTime();
		if( obj->getAttributeModifierHelper() )
		{
			const Real factor = AttributeModifiers::rechargeFactor( obj );
			if( factor >= 0.0f && factor != 1.0f )
				reload = (UnsignedInt)REAL_TO_INT_CEIL( INT_TO_REAL( reload ) * factor );
		}
		m_availableOnFrame = TheGameLogic->getFrame() + reload;
	}
	RetailEva::watchRecharge(*obj, *modData->m_specialPowerTemplate);
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
static Bool startsAttackInUpdate(const SpecialPowerModule& module, const Object* source)
{
	return module.getPowerModuleData()->m_updateModuleStartsAttack ||
		(RetailFileLayers::enabled() && ReplaceObjectUpdate::find(source, module.getSpecialPowerTemplate()));
}

Bool SpecialPowerModule::initiateIntentToDoSpecialPower( const Object *targetObj, const Coord3D *targetPos, const Waypoint *way, UnsignedInt commandOptions )
{
	Bool valid = false;
	// tell our update modules that we intend to do this special power.
	for( BehaviorModule** u = getObject()->getBehaviorModules(); *u; ++u )
	{
		SpecialPowerUpdateInterface* spu = (*u)->getSpecialPowerUpdateInterface();
		if( spu )
		{
			//Validate that we are calling the correct module!
			if( isModuleForPower( getSpecialPowerModuleData()->m_specialPowerTemplate ) )
			{
				if( spu->doesSpecialPowerUpdatePassScienceTest() )
				{
					if( spu->initiateIntentToDoSpecialPower( getSpecialPowerModuleData()->m_specialPowerTemplate, targetObj, targetPos, way, commandOptions ) )
					{
						//Kris: Aug 2003
						//We have executed the special power, so don't try to execute any more. This logic
						//was changed for multi-level spectres. Before, multiple modules would get launched
						//causing 2 or 3 spectres to be created.
						valid = true;
						break;
					}
				}
			}
		}
	}

	// GeneralsX @bugfix Copilot 06/09/2026 Stop a rejected missile-launcher intent before recording or triggering it.
	// Upstream reference: Mauller, PR #1218
	// https://github.com/TheSuperHackers/GeneralsGameCode/pull/1218
	if (m_availableOnFrame == 0xFFFFFFFF)
	{
		DEBUG_ASSERTCRASH(!valid, ("Using MissileLauncherBuildingUpdate escape path when valid is set to true"));
		return false;
	}

	if (!valid && startsAttackInUpdate(*this, getObject()) &&
		ModuleFactory::registersRotwkModules() && getModuleNameKey() == NAMEKEY("SpecialPowerModule"))
		return FALSE;

	getObject()->getControllingPlayer()->getAcademyStats()->recordSpecialPowerUsed( getSpecialPowerModuleData()->m_specialPowerTemplate );

	//If we depend on our update module to trigger the special power, make sure we have the
	//appropriate update module!
	if( !valid && startsAttackInUpdate(*this, getObject()) )
	{
		DEBUG_CRASH( ("Object does not contain a special power module to execute.  Did you forget to add it to the object INI?"));
		//DEBUG_CRASH(( "Object does not contain special power module (%s) to execute.  Did you forget to add it to the object INI?",
		//							command->m_specialPower->getName().str() ));
	}

	return valid;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void SpecialPowerModule::refreshPowerTimer()
{
	const SpecialPowerTemplate* power = getSpecialPowerTemplate();
	if (power && !power->isSharedNSync() && m_pausedCount == 0 && m_availableOnFrame != 0xFFFFFFFF)
		setReadyFrame(TheGameLogic->getFrame());
}

void SpecialPowerModule::triggerSpecialPower( const Coord3D *location )
{
	if (RetailFileLayers::enabled() && getSpecialPowerModuleData()->m_specialPowerTemplate->getSpecialPowerType() == SPECIAL_HARVEST)
	{
		Object* object = getObject();
		auto* worker = static_cast<WorkerAIUpdate*>(object->findUpdateModule(NAMEKEY("WorkerAIUpdate")));
		if (worker)
			worker->startRetailHarvest(location);
	}

	aboutToDoSpecialPower( location );	// do BEFORE recharge

	if( RetailFileLayers::enabled() )
		applyRotwkTrigger( location );

	applyPowerEffect(location);
	const SpecialPowerType power = getSpecialPowerTemplate()->getSpecialPowerType();
	if (RetailFileLayers::enabled() && (power == SPECIAL_ATHELAS || power == SPECIAL_ELVEN_GRACE))
		AutoHealBehavior::triggerRetailButtonHealing(getObject());
	createViewObject(location);

	// we won't be able to use the power for X number of frames now
	startPowerRecharge();
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void SpecialPowerModule::createViewObject( const Coord3D *location )
{
	if (!location)
		return;

	const SpecialPowerModuleData *modData = getSpecialPowerModuleData();
	const SpecialPowerTemplate *powerTemplate = modData->m_specialPowerTemplate;

	if( modData == nullptr  ||  powerTemplate == nullptr )
		return;

	Real visionRange = powerTemplate->getViewObjectRange();
	UnsignedInt visionDuration = powerTemplate->getViewObjectDuration();

	if( visionRange == 0 || visionDuration == 0 )
		return; // We don't want a view object at all.

	AsciiString objectName = TheGlobalData->m_specialPowerViewObjectName;
	if( objectName.isEmpty() )
		return;

	const ThingTemplate *viewObjectTemplate = TheThingFactory->findTemplate( objectName );
	if( viewObjectTemplate == nullptr )
		return;

	Object *viewObject = TheThingFactory->newObject( viewObjectTemplate, getObject()->getControllingPlayer()->getDefaultTeam() );

	if( viewObject == nullptr )
		return;

	viewObject->setPosition( location );
	viewObject->setShroudClearingRange( visionRange );

	static NameKeyType key_DeletionUpdate = NAMEKEY("DeletionUpdate");
	DeletionUpdate* dup = (DeletionUpdate*)viewObject->findUpdateModule(key_DeletionUpdate);
	if( dup )
	{
		dup->setLifetimeRange( visionDuration, visionDuration );
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void SpecialPowerModule::markSpecialPowerTriggered( const Coord3D *location )
{
	triggerSpecialPower( location );
}


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void SpecialPowerModule::aboutToDoSpecialPower( const Coord3D *location )
{
	// Tell the scripting engine!
	TheScriptEngine->notifyOfTriggeredSpecialPower(
		getObject()->getControllingPlayer()->getPlayerIndex(),
		getSpecialPowerModuleData()->m_specialPowerTemplate->getName(),
		getObject()->getID());

	// Let EVA do her thing
	SpecialPowerType type = getSpecialPowerModuleData()->m_specialPowerTemplate->getSpecialPowerType();

	Player *localPlayer = rts::getObservedOrLocalPlayer();
	Relationship relationship = localPlayer->getRelationship(getObject()->getTeam());

  // Only play the EVA sounds if this is not the local player, and the local player doesn't consider the
	// person an enemy.
	// Kris: Actually, all players need to hear these warnings.
  // Ian: But now there are different Eva messages depending on who launched
	//if (localPlayer != getObject()->getControllingPlayer() && localPlayer->getRelationship(getObject()->getTeam()) != ENEMIES)
  {
		if( type == SPECIAL_PARTICLE_UPLINK_CANNON || type == SUPW_SPECIAL_PARTICLE_UPLINK_CANNON || type == LAZR_SPECIAL_PARTICLE_UPLINK_CANNON )
    {
      if ( localPlayer == getObject()->getControllingPlayer() )
      {
        TheEva->setShouldPlay(EVA_SuperweaponLaunched_Own_ParticleCannon);
      }
      else if (relationship != ENEMIES)
      {
        // Note: counting relationship NEUTRAL as ally. Not sure if this makes a difference???
        TheEva->setShouldPlay(EVA_SuperweaponLaunched_Ally_ParticleCannon);
      }
      else
      {
        TheEva->setShouldPlay(EVA_SuperweaponLaunched_Enemy_ParticleCannon);
      }
    }
    else if( type == SPECIAL_NEUTRON_MISSILE || type == NUKE_SPECIAL_NEUTRON_MISSILE || type == SUPW_SPECIAL_NEUTRON_MISSILE )
    {
      if ( localPlayer == getObject()->getControllingPlayer() )
      {
        TheEva->setShouldPlay(EVA_SuperweaponLaunched_Own_Nuke);
      }
      else if (relationship != ENEMIES)
      {
        // Note: counting relationship NEUTRAL as ally. Not sure if this makes a difference???
        TheEva->setShouldPlay(EVA_SuperweaponLaunched_Ally_Nuke);
      }
      else
      {
        TheEva->setShouldPlay(EVA_SuperweaponLaunched_Enemy_Nuke);
      }
    }
		else if (type == SPECIAL_SCUD_STORM)
    {
      if ( localPlayer == getObject()->getControllingPlayer() )
      {
        TheEva->setShouldPlay(EVA_SuperweaponLaunched_Own_ScudStorm);
      }
      else if (relationship != ENEMIES)
      {
        // Note: counting relationship NEUTRAL as ally. Not sure if this makes a difference???
        TheEva->setShouldPlay(EVA_SuperweaponLaunched_Ally_ScudStorm);
      }
      else
      {
        TheEva->setShouldPlay(EVA_SuperweaponLaunched_Enemy_ScudStorm);
      }
    }
		else if (type == SPECIAL_GPS_SCRAMBLER || type == SLTH_SPECIAL_GPS_SCRAMBLER )
    {
			// This is Ghetto.  Voices should be ini lines in the special power entry.  You shouldn't have to
			// add to an enum to get a new voice
      if ( localPlayer == getObject()->getControllingPlayer() )
      {
        TheEva->setShouldPlay(EVA_SuperweaponLaunched_Own_GPS_Scrambler);
      }
      else if (relationship != ENEMIES)
      {
        // Note: counting relationship NEUTRAL as ally. Not sure if this makes a difference???
        TheEva->setShouldPlay(EVA_SuperweaponLaunched_Ally_GPS_Scrambler);
      }
      else
      {
        TheEva->setShouldPlay(EVA_SuperweaponLaunched_Enemy_GPS_Scrambler);
      }
    }
		else if (type == SPECIAL_SNEAK_ATTACK)
    {
      if ( localPlayer == getObject()->getControllingPlayer() )
      {
        TheEva->setShouldPlay(EVA_SuperweaponLaunched_Own_Sneak_Attack);
      }
      else if (relationship != ENEMIES)
      {
        // Note: counting relationship NEUTRAL as ally. Not sure if this makes a difference???
        TheEva->setShouldPlay(EVA_SuperweaponLaunched_Ally_Sneak_Attack);
      }
      else
      {
        TheEva->setShouldPlay(EVA_SuperweaponLaunched_Enemy_Sneak_Attack);
      }
    }
	}

	// get module data
	const SpecialPowerModuleData *modData = getSpecialPowerModuleData();

	// play our initiate sound if we have one
	AudioEventRTS audioEvent = *modData->m_specialPowerTemplate->getInitiateSound();
	audioEvent.setObjectID(getObject()->getID());
	TheAudio->addAudioEvent( &audioEvent );

	// play sound at target location if specified
	if( location )
	{

		AudioEventRTS soundAtLocation = *modData->m_specialPowerTemplate->getInitiateAtTargetSound();
		soundAtLocation.setPosition( location );
		soundAtLocation.setPlayerIndex(getObject()->getControllingPlayer()->getPlayerIndex());
		TheAudio->addAudioEvent( &soundAtLocation );

	}

}

//-------------------------------------------------------------------------------------------------
//By default, special powers are not triggered by it's update module -- in which case
//it triggers it and resets its timer immediately. When the update module triggers it,
//then all we do is initiate the special power, and trust that the update module will
//do the rest.
//-------------------------------------------------------------------------------------------------
Bool SpecialPowerModule::isRetailCastAllowed(UnsignedInt commandOptions) const
{
	if (!RetailFileLayers::enabled() || !TheRetailCommandCatalog)
		return TRUE;
	const Object* obj = getObject();
	const SpecialPowerTemplate* power = getSpecialPowerTemplate();
	if (!obj || !power)
		return TRUE;
	const CommandButton* button = TheRetailCommandCatalog->findPowerButton(obj, power);
	if (!button)
		return TRUE;
	return button->isModelConditionAllowed(obj) && button->isValidContainerForCommand(obj) &&
		((commandOptions & COMMAND_OPTION_INTERNAL_AUTO) != 0 || button->isRetailButtonUsable());
}

void SpecialPowerModule::doSpecialPower( UnsignedInt commandOptions )
{
	m_lastCastAccepted = FALSE;
	if (m_pausedCount > 0 || getObject()->isDisabled() || !meetsRequiredRank() || !isRetailCastAllowed(commandOptions)) {
		return;
	}

	const Bool initiated = initiateIntentToDoSpecialPower( nullptr, nullptr, nullptr, commandOptions );

	if( !startsAttackInUpdate(*this, getObject()) )
	{
		triggerSpecialPower( nullptr );
		m_lastCastAccepted = TRUE;
	}
	else
	{
		m_lastCastAccepted = initiated;
	}
}

void SpecialPowerModule::doSpecialPowerAtObject( Object *obj, UnsignedInt commandOptions )
{
	m_lastCastAccepted = FALSE;
	if (m_pausedCount > 0 || getObject()->isDisabled() || !meetsRequiredRank() || !isRetailCastAllowed(commandOptions)) {
		return;
	}
	if (!CommandButton::isRetailPowerObjectTargetAllowed(getObject(), getSpecialPowerTemplate(), obj))
		return;

	const Bool initiated = initiateIntentToDoSpecialPower( obj, nullptr, nullptr, commandOptions );

	if( !startsAttackInUpdate(*this, getObject()) )
	{
		triggerSpecialPower( obj->getPosition() );
		m_lastCastAccepted = TRUE;
	}
	else
	{
		m_lastCastAccepted = initiated;
	}
}

void SpecialPowerModule::doSpecialPowerAtLocation( const Coord3D *loc, Real angle, UnsignedInt commandOptions )
{
	m_lastCastAccepted = FALSE;
	if (m_pausedCount > 0 || getObject()->isDisabled() || !meetsRequiredRank() || !isRetailCastAllowed(commandOptions)) {
		return;
	}

	const Bool initiated = initiateIntentToDoSpecialPower( nullptr, loc, nullptr, commandOptions );

	if (m_availableOnFrame == 0xFFFFFFFF)
		return;

	if( !startsAttackInUpdate(*this, getObject()) )
	{
		triggerSpecialPower( loc );
		m_lastCastAccepted = TRUE;
	}
	else
	{
		m_lastCastAccepted = initiated;
	}
}

void SpecialPowerModule::doSpecialPowerUsingWaypoints( const Waypoint *way, UnsignedInt commandOptions )
{
	m_lastCastAccepted = FALSE;
	if (m_pausedCount > 0 || getObject()->isDisabled() || !meetsRequiredRank() || !isRetailCastAllowed(commandOptions)) {
		return;
	}

	const Bool initiated = initiateIntentToDoSpecialPower( nullptr, nullptr, way, commandOptions );

	if( !startsAttackInUpdate(*this, getObject()) )
	{
		triggerSpecialPower( nullptr );
		m_lastCastAccepted = TRUE;
	}
	else
	{
		m_lastCastAccepted = initiated;
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void SpecialPowerModule::pauseCountdown( Bool pause )
{
	if (pause)// If pausing
	{
		if( m_pausedCount == 0 )
		{
			// Only record this with the first pausing, otherwise upon final unpausing you would get credited the time
			// between pauses as time served.
			m_pausedOnFrame = TheGameLogic->getFrame();
			m_pausedPercent = getPercentReady();
		}

		++m_pausedCount;
	}
	else if( m_pausedCount > 0 )//Else if unpausing, but only if I am in fact paused, so multiple unpauses don't break.
	{
		--m_pausedCount;

		// And only update the ready time if we are fully unpaused now.
		if( m_pausedCount == 0 )
		{
			m_availableOnFrame += (TheGameLogic->getFrame() - m_pausedOnFrame);
		}
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
UnsignedInt SpecialPowerModule::getReadyFrame() const
{
	if ( getSpecialPowerTemplate()->isSharedNSync() )
	{
		const Object* obj = getObject();
		if ( obj )
		{
			Player *player = getObject()->getControllingPlayer();
			if ( player )
				return player->getOrStartSpecialPowerReadyFrame( getSpecialPowerTemplate() );
		}
	}

	if (m_pausedCount > 0 || getObject()->isDisabled())
	{
		Int pausedFrames = TheGameLogic->getFrame() - m_pausedOnFrame;
		return m_availableOnFrame + pausedFrames;
	}
	else
	{
		return m_availableOnFrame;
	}
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void SpecialPowerModule::crc( Xfer *xfer )
{

	// extend base class
	BehaviorModule::crc( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void SpecialPowerModule::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	BehaviorModule::xfer( xfer );

	// available on frame
	xfer->xferUnsignedInt( &m_availableOnFrame );

	// paused by script
	xfer->xferInt( &m_pausedCount );

	// paused on frame
	xfer->xferUnsignedInt( &m_pausedOnFrame );

	// paused percent
	xfer->xferReal( &m_pausedPercent );

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void SpecialPowerModule::loadPostProcess()
{

	// extend base class
	BehaviorModule::loadPostProcess();



	// Now, if we find that we have just come into being,
	// but there is already a science granted for our shared superweapon,
	// lets make sure TheIngameUI knows about our public timer
	// add this weapon to the UI if it has a public timer for all to see
	if( m_pausedCount == 0 &&
			getSpecialPowerTemplate()->isSharedNSync() == TRUE &&
			getSpecialPowerTemplate()->hasPublicTimer() == TRUE &&
			TheInGameUI &&
			getObject()->getControllingPlayer() &&
			getObject()->isKindOf( KINDOF_STRUCTURE ) )
	{
		TheInGameUI->addSuperweapon( getObject()->getControllingPlayer()->getPlayerIndex(),
																 getPowerName(),
																 getObject()->getID(),
																 getSpecialPowerModuleData()->m_specialPowerTemplate );
	}






}

void SpecialPowerModule::applyRotwkTrigger( const Coord3D *location )
{
	const SpecialPowerModuleData *data = getSpecialPowerModuleData();
	Object *self = getObject();
	if (!data->m_updateModuleStartsAttack && data->m_setModelCondition == MODELCONDITION_USER_6 &&
		data->m_specialPowerTemplate->getSpecialPowerType() == SPECIAL_GENERAL_TARGETLESS &&
		AutoPickUpUpdate::hasEatingButton(self))
		self->setPowerModelCondition(data->m_setModelCondition,
			std::max(1u, SpecialAbilityUpdate::secondsToFrames(data->m_setModelConditionTime)));
	if (data->m_changeWeather != GameLogic::WEATHER_NONE)
		TheGameLogic->changeRetailWeather(data->m_changeWeather, data->m_weatherDuration);
	if( data->m_triggerFX.isNotEmpty() && TheFXListStore )
	{
		const FXList *fx = TheFXListStore->findFXList( data->m_triggerFX.str() );
		FXList::doFXPos( fx, location ? location : self->getPosition() );
	}
	if( data->m_attributeModifier.isNotEmpty() )
	{
		const AttributeModifiers::Sides sides = data->m_targetAllSides ? AttributeModifiers::SIDES_ALL
			: data->m_targetEnemy ? AttributeModifiers::SIDES_ENEMIES : AttributeModifiers::SIDES_ALLIES;
		AttributeModifiers::applyInRange( self, location ? location : self->getPosition(), data->m_attributeModifierRange,
			&data->m_attributeModifierAffects, data->m_attributeModifierAffectsSelf, AttributeModifiers::findList( data->m_attributeModifier ),
			AttributeModifierHelper::USE_LIST_DURATION, AttributeModifierRules::categoryBit( data->m_antiCategory ), data->m_attributeModifierFX, sides,
			data->m_affectAllies );
	}
	for( const AsciiString &name : data->m_onTriggerRechargeSpecialPower )
	{
		const SpecialPowerTemplate *other = TheSpecialPowerStore->findSpecialPowerTemplate( name );
		SpecialPowerModuleInterface *module = other ? self->getSpecialPowerModule( other ) : nullptr;
		if( module && module != this )
			module->startPowerRecharge();
	}
}
