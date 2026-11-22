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

// FILE: SalvageCrateCollide.cpp ///////////////////////////////////////////////////////////////////////
// Author: Graham Smallwood, March 2002
// Desc:   The Savlage system can give a Weaponset bonus, a level, or money.  Salvagers create them
//					by killing marked units, and only WeaponSalvagers can get the WeaponSet bonus
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine
#include "GameLogic/Module/SalvageCrateCollide.h"

#include "Common/AudioEventRTS.h"
#include "Common/MiscAudio.h"
#include "Common/KindOf.h"
#include "Common/RandomValue.h"
#include "Common/RetailFileLayers.h"
#include "Common/Player.h"
#include "Common/PlayerTemplate.h"
#include "Common/ThingFactory.h"
#include "Common/Upgrade.h"
#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"
#include "GameClient/GameText.h"
#include "GameClient/InGameUI.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Object.h"

#include <algorithm>
#include <cstdlib>

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
SalvageCrateCollide::SalvageCrateCollide( Thing *thing, const ModuleData* moduleData ) : CrateCollide( thing, moduleData )
{

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
SalvageCrateCollide::~SalvageCrateCollide()
{

}

//-------------------------------------------------------------------------------------------------
Bool SalvageCrateCollide::isValidToExecute( const Object *other ) const
{
	if( ! CrateCollide::isValidToExecute( other ) )
		return FALSE;

	if (RetailFileLayers::enabled())
		return !other->isKindOf(KINDOF_STRUCTURE);

	if( ! other->getTemplate()->isKindOf( KINDOF_SALVAGER ) )
		return FALSE;

	return TRUE;
}

//-------------------------------------------------------------------------------------------------
Bool SalvageCrateCollide::executeCrateBehavior( Object *other )
{
	if (RetailFileLayers::enabled())
		return executeRetailPickup(other);

	if( eligibleForArmorSet(other) )// No percent chance on this one, if you can get it, you get it.
	{
		doArmorSet(other);

		//Play the salvage installation crate pickup sound.
		AudioEventRTS soundToPlay = TheAudio->getMiscAudio()->m_crateSalvage;
		soundToPlay.setObjectID( other->getID() );
		TheAudio->addAudioEvent( &soundToPlay );
	}
	else if( eligibleForWeaponSet( other ) && testWeaponChance() )
	{
		doWeaponSet( other );

		//Play the salvage installation crate pickup sound.
		AudioEventRTS soundToPlay = TheAudio->getMiscAudio()->m_crateSalvage;
		soundToPlay.setObjectID( other->getID() );
		TheAudio->addAudioEvent( &soundToPlay );

		//Play the unit voice acknowledgement for upgrading weapons.
		//Already handled by the "move order"
		//const AudioEventRTS *soundToPlayPtr = other->getTemplate()->getPerUnitSound( "VoiceSalvage" );
		//soundToPlay = *soundToPlayPtr;
		//soundToPlay.setObjectID( other->getID() );
		//TheAudio->addAudioEvent( &soundToPlay );
	}
	else if( eligibleForLevel( other ) && testLevelChance() )
	{
		doLevelGain( other );

		//Sound will play in
		//soundToPlay = TheAudio->getMiscAudio()->m_unitPromoted;
	}
	else // just assume the testMoneyChance
	{
		const auto* data = getSalvageCrateCollideModuleData();
		const Int money = data->m_minimumMoney == data->m_maximumMoney ? data->m_minimumMoney :
			GameLogicRandomValue(data->m_minimumMoney, data->m_maximumMoney);
		doMoney(other, money);
		AudioEventRTS soundToPlay = TheAudio->getMiscAudio()->m_crateMoney;
		soundToPlay.setObjectID( other->getID() );
		TheAudio->addAudioEvent(&soundToPlay);
	}

	other->getControllingPlayer()->getAcademyStats()->recordSalvageCollected();

	return TRUE;
}

// ------------------------------------------------------------------------------------------------

Bool SalvageCrateCollide::executeRetailPickup(Object* other)
{
	const auto* data = getSalvageCrateCollideModuleData();
	Player* player = other->getControllingPlayer();
	Object* anchor = HordeContain::get(other) ? other : HordeContain::getAnchor(other);
	Int money = 0;
	Int levels = 0;
	Int banner = 0;
	const char* reward = nullptr;

	if (data->m_bannerChance > 0.0f && anchor && HordeContain::canAddCrateBanner(anchor) &&
		(data->m_bannerChance >= 1.0f || GameLogicRandomValueReal(0.0f, 1.0f) < data->m_bannerChance) &&
		HordeContain::addCrateBanner(anchor))
	{
		banner = 1;
		reward = "banner";
	}
	else if (data->m_levelUpChance > 0.0f &&
		(data->m_levelUpChance >= 1.0f || GameLogicRandomValueReal(0.0f, 1.0f) < data->m_levelUpChance))
	{
		std::vector<ObjectID> units{anchor ? anchor->getID() : other->getID()};
		if (data->m_levelUpRadius > 0.0f)
		{
			PartitionFilterSamePlayer filter(player);
			PartitionFilter* filters[] = { &filter, nullptr };
			ObjectIterator* iter = ThePartitionManager->iterateObjectsInRange(other->getPosition(),
				data->m_levelUpRadius, FROM_CENTER_2D, filters, ITER_FASTEST);
			MemoryPoolObjectHolder hold(iter);
			for (Object* unit = iter->first(); unit; unit = iter->next())
			{
				if (unit->isEffectivelyDead() || unit->isKindOf(KINDOF_STRUCTURE))
					continue;
				Object* horde = HordeContain::getAnchor(unit);
				units.push_back(horde ? horde->getID() : unit->getID());
			}
		}
		std::sort(units.begin(), units.end());
		units.erase(std::unique(units.begin(), units.end()), units.end());
		for (ObjectID id : units)
		{
			Object* unit = TheGameLogic->findObjectByID(id);
			if (unit && !unit->isEffectivelyDead() && unit->getControllingPlayer() == player)
			{
				if (ExperienceTracker* tracker = unit->getExperienceTracker())
					levels += tracker->gainRetailLevels(1);
			}
		}
		reward = "experience";
	}
	else if (data->m_pickupUpgrade.isNotEmpty())
	{
		const UpgradeTemplate* upgrade = TheUpgradeCenter->findUpgrade(data->m_pickupUpgrade);
		if (upgrade)
		{
			if (upgrade->getUpgradeType() == UPGRADE_TYPE_PLAYER)
			{
				player->addUpgrade(upgrade, UPGRADE_STATUS_COMPLETE);
				reward = "upgrade";
			}
			else if (Object* recipient = anchor ? anchor : other)
			{
				recipient->giveUpgrade(upgrade);
				reward = "upgrade";
			}
		}
	}
	else if (data->m_resourceChance > 0.0f &&
		(data->m_resourceChance >= 1.0f || GameLogicRandomValueReal(0.0f, 1.0f) < data->m_resourceChance))
	{
		const Int low = std::min(data->m_minResource, data->m_maxResource);
		const Int high = std::max(data->m_minResource, data->m_maxResource);
		money = low < high ? GameLogicRandomValue(low, high) : low;
		doMoney(other, money);
		reward = "resources";
	}

	if (std::getenv("GENERALSX_TEST_COMMANDS"))
		fprintf(stderr, "TEST crate frame=%u crate=%s id=%u collector=%s id=%u reward=%s resources=%d levels=%d banner=%d\n",
			TheGameLogic->getFrame(), getObject()->getTemplate()->getName().str(), static_cast<unsigned>(getObject()->getID()),
			other->getTemplate()->getName().str(), static_cast<unsigned>(other->getID()), reward ? reward : "none", money, levels, banner);
	return TRUE;
}

Bool SalvageCrateCollide::eligibleForWeaponSet( Object *other )
{
	if( other == nullptr )
		return FALSE;

	// A kindof marks eligibility, and you must not be fully upgraded
	if( !other->isKindOf(KINDOF_WEAPON_SALVAGER) )
		return FALSE;
	if( other->testWeaponSetFlag(WEAPONSET_CRATEUPGRADE_TWO) )
		return FALSE;

	return TRUE;
}

// ------------------------------------------------------------------------------------------------
Bool SalvageCrateCollide::eligibleForArmorSet( Object *other )
{
	if( other == nullptr )
		return FALSE;

	// A kindof marks eligibility, and you must not be fully upgraded
	if( !other->isKindOf(KINDOF_ARMOR_SALVAGER) )
		return FALSE;
	if( other->testArmorSetFlag(ARMORSET_CRATE_UPGRADE_TWO) )
		return FALSE;

	return TRUE;
}

// ------------------------------------------------------------------------------------------------
Bool SalvageCrateCollide::eligibleForLevel( Object *other )
{
	if( other == nullptr )
		return FALSE;

	// Sorry, you are max level
	if( other->getExperienceTracker()->getVeterancyLevel() == LEVEL_HEROIC )
		return FALSE;

	// Sorry, you can't gain levels
	if( !other->getExperienceTracker()->isTrainable() )
		return FALSE;

	return TRUE;
}

// ------------------------------------------------------------------------------------------------
Bool SalvageCrateCollide::testWeaponChance()
{
	const SalvageCrateCollideModuleData *md = getSalvageCrateCollideModuleData();
	if( md->m_weaponChance == 1.0f )
		return TRUE; // don't waste a random number for a 100%

	Real randomNumber = GameLogicRandomValueReal( 0, 1 );
	if( randomNumber < md->m_weaponChance )
		return TRUE;

	return FALSE;
}

// ------------------------------------------------------------------------------------------------
Bool SalvageCrateCollide::testLevelChance()
{
	const SalvageCrateCollideModuleData *md = getSalvageCrateCollideModuleData();
	if( md->m_levelChance == 1.0f )
		return TRUE; // don't waste a random number for a 100%

	Real randomNumber = GameLogicRandomValueReal( 0, 1 );
	if( randomNumber < md->m_levelChance )
		return TRUE;

	return FALSE;
}

// ------------------------------------------------------------------------------------------------
void SalvageCrateCollide::doWeaponSet( Object *other )
{
	if( other->testWeaponSetFlag( WEAPONSET_CRATEUPGRADE_ONE ) )
	{
		other->clearWeaponSetFlag( WEAPONSET_CRATEUPGRADE_ONE );
		other->setWeaponSetFlag( WEAPONSET_CRATEUPGRADE_TWO );
	}
	else
	{
		other->setWeaponSetFlag( WEAPONSET_CRATEUPGRADE_ONE );
	}
}

// ------------------------------------------------------------------------------------------------
void SalvageCrateCollide::doArmorSet( Object *other )
{
	if( other->testArmorSetFlag( ARMORSET_CRATE_UPGRADE_ONE ) )
	{
		other->clearArmorSetFlag( ARMORSET_CRATE_UPGRADE_ONE );
		other->setArmorSetFlag( ARMORSET_CRATE_UPGRADE_TWO );

		other->clearAndSetModelConditionState(MODELCONDITION_ARMORSET_CRATEUPGRADE_ONE, MODELCONDITION_ARMORSET_CRATEUPGRADE_TWO);
	}
	else
	{
		other->setArmorSetFlag( ARMORSET_CRATE_UPGRADE_ONE );

		other->setModelConditionState(MODELCONDITION_ARMORSET_CRATEUPGRADE_ONE);
	}
}

// ------------------------------------------------------------------------------------------------
void SalvageCrateCollide::doLevelGain( Object *other )
{
	other->getExperienceTracker()->gainExpForLevel( 1 );
}

// ------------------------------------------------------------------------------------------------
void SalvageCrateCollide::doMoney( Object *other, Int money )
{
	if( money > 0 )
	{
		other->getControllingPlayer()->getMoney()->deposit( money );
		other->getControllingPlayer()->getScoreKeeper()->addMoneyEarned( money );
		if (!TheInGameUI || !TheGameText)
			return;

		//Display cash income floating over the crate.  Position is me, everything else is them.
		UnicodeString moneyString;
		moneyString.format( TheGameText->fetch( "GUI:AddCash" ), money );
		Coord3D pos;
		pos.set( *getObject()->getPosition() );
		pos.z += 10.0f; //add a little z to make it show up above the unit.
		Color color = other->getControllingPlayer()->getPlayerColor() | GameMakeColor( 0, 0, 0, 230 );
		TheInGameUI->addFloatingText( moneyString, &pos, color );
	}
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void SalvageCrateCollide::crc( Xfer *xfer )
{

	// extend base class
	CrateCollide::crc( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void SalvageCrateCollide::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	CrateCollide::xfer( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void SalvageCrateCollide::loadPostProcess()
{

	// extend base class
	CrateCollide::loadPostProcess();

}
