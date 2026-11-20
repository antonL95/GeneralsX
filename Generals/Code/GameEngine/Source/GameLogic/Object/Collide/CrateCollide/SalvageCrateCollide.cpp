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

	if( eligibleForWeaponSet( other ) && testWeaponChance() )
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


	return TRUE;
}

// ------------------------------------------------------------------------------------------------

static Bool rollRetailCrateChance(Real chance)
{
	return chance >= 1.0f || (chance > 0.0f && GameLogicRandomValueReal(0.0f, 1.0f) < chance);
}

Bool SalvageCrateCollide::executeRetailPickup(Object* other)
{
	const auto* data = getSalvageCrateCollideModuleData();
	Player* player = other->getControllingPlayer();
	Object* anchor = HordeContain::getAnchor(other);
	if (!anchor)
		anchor = other;
	Int money = 0;
	Int levels = 0;
	const char* reward = nullptr;

	if (HordeContain::get(anchor) && rollRetailCrateChance(data->m_bannerChance) && HordeContain::addCrateBanner(anchor))
		reward = "banner";
	else if (rollRetailCrateChance(data->m_porterChance) && player->getPlayerTemplate())
	{
		const auto* porter = TheThingFactory->findTemplate(player->getPlayerTemplate()->getStartingUnit(0));
		if (porter && porter->isKindOf(KINDOF_DOZER))
		{
			Object* unit = TheThingFactory->newObject(porter, player->getDefaultTeam());
			unit->setPosition(getObject()->getPosition());
			unit->setOrientation(other->getOrientation());
			reward = "porter";
		}
	}
	if (!reward)
	{
		if (rollRetailCrateChance(data->m_resourceChance))
		{
			const Int low = std::min(data->m_minResource, data->m_maxResource);
			const Int high = std::max(data->m_minResource, data->m_maxResource);
			money = low < high ? GameLogicRandomValue(low, high) : low;
			doMoney(other, money);
			reward = "resources";
		}
		else if (rollRetailCrateChance(data->m_levelUpChance))
		{
			const auto levelUp = [&levels](Object* unit)
			{
				ExperienceTracker* tracker = unit->getExperienceTracker();
				if (!unit->isEffectivelyDead() && tracker && tracker->isTrainable())
				{
					const auto previous = tracker->getVeterancyLevel();
					tracker->gainExpForLevel(1, FALSE);
					levels += tracker->getVeterancyLevel() > previous;
				}
			};
			if (data->m_levelUpRadius > 0.0f)
			{
				PartitionFilterSamePlayer filter(player);
				PartitionFilter* filters[] = { &filter, nullptr };
				ObjectIterator* iter = ThePartitionManager->iterateObjectsInRange(other,
					data->m_levelUpRadius, FROM_CENTER_2D, filters, ITER_FASTEST);
				MemoryPoolObjectHolder hold(iter);
				std::vector<Object*> units;
				for (Object* unit = iter->first(); unit; unit = iter->next())
					units.push_back(unit);
				std::sort(units.begin(), units.end(), [](const Object* a, const Object* b) { return a->getID() < b->getID(); });
				for (Object* unit : units)
					levelUp(unit);
			}
			else
			{
				levelUp(anchor);
			}
			reward = "experience";
		}
		else if (data->m_pickupUpgrade.isNotEmpty())
		{
			const UpgradeTemplate* upgrade = TheUpgradeCenter->findUpgrade(data->m_pickupUpgrade);
			if (upgrade && upgrade->getUpgradeType() == UPGRADE_TYPE_PLAYER)
			{
				player->addUpgrade(upgrade, UPGRADE_STATUS_COMPLETE);
				reward = "upgrade";
			}
		}
	}

	if (std::getenv("GENERALSX_TEST_COMMANDS"))
		fprintf(stderr, "TEST crate frame=%u crate=%s id=%u collector=%s id=%u reward=%s resources=%d levels=%d\n",
			TheGameLogic->getFrame(), getObject()->getTemplate()->getName().str(), static_cast<unsigned>(getObject()->getID()),
			other->getTemplate()->getName().str(), static_cast<unsigned>(other->getID()), reward ? reward : "none", money, levels);
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

void SalvageCrateCollide::doLevelGain( Object *other )
{
	other->getExperienceTracker()->gainExpForLevel( 1 );
}

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
