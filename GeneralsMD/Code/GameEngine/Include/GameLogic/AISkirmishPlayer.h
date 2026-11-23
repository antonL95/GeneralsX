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

// AISkirmishPlayer.h
// Computerized opponent
// Author: Michael S. Booth, January 2002

#pragma once

#include "Common/GameMemory.h"
#include "GameLogic/AIPlayer.h"

#include <array>
#include <map>
#include <tuple>
#include <set>

class BuildListInfo;
class SpecialPowerTemplate;


/**
 * The computer-controlled opponent.
 */
class AISkirmishPlayer : public AIPlayer
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( AISkirmishPlayer, "AISkirmishPlayer"  )

public:	 // AISkirmish specific methods.

	AISkirmishPlayer( Player *p );							///< constructor
	static void loadRetailData();
	static void parseRetailArmy(INI* ini);
	static void parseRetailTuning(INI* ini);
	static Bool rollRetailSpecialPowerActivation(const Player& player);
	static Bool hasRetailCombatChain(const ThingTemplate& attacker);
	static Real getRetailCombatChainModifier(const ThingTemplate& attacker, const ThingTemplate& target);
	virtual Bool computeSuperweaponTarget(const SpecialPowerTemplate *power, Coord3D *pos, Int playerNdx, Real weaponRadius) override; ///< Calculates best pos for weapon given radius.

public:	// AIPlayer interface methods.

	virtual void update() override;											///< simulates the behavior of a player

	virtual void newMap() override;											///< New map loaded call.

	/// Invoked when a unit I am training comes into existence
	virtual void onUnitProduced( Object *factory, Object *unit ) override;

	virtual void buildSpecificAITeam(TeamPrototype *teamProto, Bool priorityBuild) override; ///< Builds this team immediately.

	virtual void buildSpecificAIBuilding(const AsciiString &thingName) override; ///< Builds this building as soon as possible.

	virtual void buildAIBaseDefense(Bool flank) override; ///< Builds base defense on front or flank of base.

	virtual void buildAIBaseDefenseStructure(const AsciiString &thingName, Bool flank) override; ///< Builds base defense on front or flank of base.

	virtual void recruitSpecificAITeam(TeamPrototype *teamProto, Real recruitRadius) override; ///< Builds this team immediately.

	virtual Bool isSkirmishAI() override {return true;}

	virtual Bool checkBridges(Object *unit, Waypoint *way) override;

	virtual Player *getAiEnemy() override;	///< Solo AI attacks based on scripting.  Only skirmish auto-acquires an enemy at this point.  jba.

protected:

	// snapshot methods
	virtual void crc( Xfer *xfer ) override;
	virtual void xfer( Xfer *xfer ) override;
	virtual void loadPostProcess() override;

	virtual void doBaseBuilding() override;
	virtual void checkReadyTeams() override;
	virtual void checkQueuedTeams() override;
	virtual void doTeamBuilding() override;
	virtual Object *findDozer(const Coord3D *pos) override;
	virtual void queueDozer() override;

protected:

	virtual Bool selectTeamToBuild() override;			///< determine the next team to build
	virtual Bool selectTeamToReinforce( Int minPriority ) override;			///< determine the next team to reinforce
	virtual Bool startTraining( WorkOrder *order, Bool busyOK, AsciiString teamName) override;	///< find a production building that can handle the order, and start building

	virtual Bool isAGoodIdeaToBuildTeam( TeamPrototype *proto ) override;		///< return true if team should be built
	virtual void processBaseBuilding() override;		///< do base-building behaviors
	virtual void processTeamBuilding() override;		///< do team-building behaviors

protected:
	void adjustBuildList(BuildListInfo *list);
	void updateRetail();
	void xferRetailObjectives(Xfer* xfer, Bool hasRouteHistory);
	Int getMyEnemyPlayerIndex();
	void acquireEnemy();

protected:
	Int m_curFrontBaseDefense; // First is 0.
	Int m_curFlankBaseDefense; // First is 0.
	Real m_curFrontLeftDefenseAngle;
	Real m_curFrontRightDefenseAngle;
	Real m_curLeftFlankLeftDefenseAngle;
	Real m_curLeftFlankRightDefenseAngle;
	Real m_curRightFlankLeftDefenseAngle;
	Real m_curRightFlankRightDefenseAngle;

	UnsignedInt m_frameToCheckEnemy;
	Player			*m_currentEnemy;
	struct RetailObjective
	{
		UnsignedInt issuedFrame = 0;
		UnsignedInt blockedRevision = 0;
	};
	std::map<std::tuple<ObjectID, ObjectID, UnsignedInt>, RetailObjective> m_retailObjectives;
	struct RetailTacticalTarget
	{
		ObjectID object = INVALID_ID;
		ObjectID defendedStructure = INVALID_ID;
		UnsignedInt owner = 0;
		UnsignedInt selectedFrame = 0;
		Bool retiring = FALSE;
	};
	std::vector<RetailTacticalTarget> m_retailTargets;
	struct RetailWallPlan
	{
		ObjectID defendedStructure = INVALID_ID;
		Coord3D center{};
		Coord2D forward{};
		std::array<ObjectID, 8> nodes{};
	};
	RetailWallPlan m_retailWallPlan;
	void xferRetailBlocks(Xfer* xfer, Bool present);
	UnsignedInt m_retailLastFarmFrame = 0;
	UnsignedInt m_retailSavings = 0;
	Int m_retailScavengedBuilding = -1;
	Real m_retailFarmPriority = 0.0f;
	std::set<AsciiString> m_retailBuiltStructures;
	std::map<AsciiString, Real> m_retailUnitPriorities;
	struct RetailUpgradePlan
	{
		Real priority = 0.0f;
		Bool admitted = FALSE;
	};
	std::map<std::pair<ObjectID, AsciiString>, RetailUpgradePlan> m_retailUpgradePlans;

};
