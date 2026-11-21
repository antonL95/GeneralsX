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

// AISkirmishPlayer.cpp
// Computerized opponent
// Author: Michael S. Booth, January 2002

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine


#include "Common/GameMemory.h"
#include "Common/GlobalData.h"
#include "Common/INI.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/RetailFileLayers.h"
#include "Common/Team.h"
#include "Common/ThingFactory.h"
#include "Common/BuildAssistant.h"
#include "Common/SpecialPower.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "Common/WellKnownKeys.h"
#include "Common/Xfer.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/Object.h"
#include "GameLogic/AISkirmishPlayer.h"
#include "GameLogic/HeroRoster.h"
#include "GameLogic/SidesList.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/DoCommandUpgrade.h"
#include "GameLogic/Module/DozerAIUpdate.h"
#include "GameLogic/Module/FoundationAIUpdate.h"
#include "GameLogic/Module/GrantUpgradeCreate.h"
#include "GameLogic/Module/HordeAIUpdate.h"
#include "GameLogic/Module/RebuildHoleBehavior.h"
#include "GameLogic/Module/ReplaceSelfUpgrade.h"
#include "GameLogic/Module/UpdateModule.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Module/ProductionUpdate.h"
#include "GameClient/TerrainVisual.h"
#include "GameClient/ControlBar.h"

namespace
{
struct RetailArmyMember
{
	AsciiString type;
	Real share[3] = {};
};

constexpr Int RETAIL_ATTACK_WAVE_MIN_UNITS = 3;
constexpr Real RETAIL_DEFENDED_TARGET_DISTANCE = 350.0f;

struct RetailArmy
{
	AsciiString side;
	AsciiString farm;
	std::vector<RetailArmyMember> members;
	std::vector<AsciiString> heroes;
	Real rushSeconds = 0.0f;
	Real midgameSeconds = 0.0f;
	Int minimumFarms = 0;
};

struct RetailDifficulty
{
	AsciiString name;
	Int farmLimit = 0;
};

struct RetailAITuning
{
	std::vector<RetailDifficulty> difficulties;
	Bool disableBaseBuilding = FALSE;
	Bool disableEconomyBuilding = FALSE;
	Bool disableUnitBuilding = FALSE;
	Bool disableUnitUpgrading = FALSE;
	Bool disableTacticalAI = FALSE;
};

std::vector<RetailArmy> retailArmies;
RetailAITuning retailTuning;

void skipRetailAISection(INI* ini, void*, void*, const void* section)
{
	ini->recordSkipped("subblock", static_cast<const char*>(section));
	ini->readRawBlock(INI::getEndToken());
}

void parseArmyMember(INI* ini, void* instance, void*, const void*)
{
	ini->getNextToken();
	auto& army = *static_cast<RetailArmy*>(instance);
	RetailArmyMember member;
	static const FieldParse fields[] = {
		{ "Unit", INI::parseAsciiString, nullptr, offsetof(RetailArmyMember, type) },
		{ "PercentageOfArmyPhase1", INI::parseReal, nullptr, offsetof(RetailArmyMember, share) },
		{ "PercentageOfArmyPhase2", INI::parseReal, nullptr, offsetof(RetailArmyMember, share) + sizeof(Real) },
		{ "PercentageOfArmyPhase3", INI::parseReal, nullptr, offsetof(RetailArmyMember, share) + 2 * sizeof(Real) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	ini->initFromINI(&member, fields);
	if (member.type.isNotEmpty())
		army.members.push_back(member);
}

void parseFarmAssignment(INI* ini, void*, void* store, const void*)
{
	ini->getNextToken();
	static const FieldParse fields[] = {
		{ "TemplateName", INI::parseAsciiString, nullptr, 0 },
		{ nullptr, nullptr, nullptr, 0 }
	};
	ini->initFromINI(store, fields);
}

void parseDifficultyTuning(INI* ini, void* instance, void*, const void*)
{
	ini->getNextToken();
	auto& tuning = *static_cast<RetailAITuning*>(instance);
	RetailDifficulty difficulty;
	static const FieldParse fields[] = {
		{ "Difficulty", INI::parseAsciiString, nullptr, offsetof(RetailDifficulty, name) },
		{ "EconomyMaxFarms", INI::parseInt, nullptr, offsetof(RetailDifficulty, farmLimit) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	ini->initFromINI(&difficulty, fields);
	if (difficulty.name.isNotEmpty())
		tuning.difficulties.push_back(difficulty);
}

const CommandSet* retailCommands(const Object& object)
{
	return TheRetailCommandCatalog->findCommandSet(object.getCommandSetString());
}

Bool matchesArmyType(const ThingTemplate& actual, const ThingTemplate& planned)
{
	const auto names = [](const ThingTemplate& type, const AsciiString& name) {
		const auto& equivalents = type.getRotWKObjectData().equivalentTo;
		return std::any_of(equivalents.begin(), equivalents.end(), [&](const AsciiString& equivalent) {
			return equivalent.compareNoCase(name) == 0;
		});
	};
	return actual.isEquivalentTo(&planned) || names(actual, planned.getName()) || names(planned, actual.getName());
}

const ThingTemplate* automaticReplacement(const ThingTemplate& building)
{
	const auto triggerNames = [](const UpgradeModuleData& data) -> const std::vector<AsciiString>& {
		const auto& mux = data.m_upgradeMuxData;
		return mux.m_activationUpgradeNames.empty() ? mux.m_triggerUpgradeNames : mux.m_activationUpgradeNames;
	};
	const ModuleInfo& modules = building.getBehaviorModuleInfo();
	for (Int i = 0; i < modules.getCount(); ++i)
	{
		if (modules.getNthName(i) != "DoCommandUpgrade")
			continue;
		const auto* command = static_cast<const DoCommandUpgradeModuleData*>(modules.getNthData(i));
		const CommandButton* button = TheRetailCommandCatalog->findCommandButton(command->m_grantCommand);
		const UpgradeTemplate* upgrade = button ? button->getUpgradeTemplate() : nullptr;
		const auto& triggers = triggerNames(*command);
		if (!upgrade || button->getCommandType() != GUI_COMMAND_OBJECT_UPGRADE || triggers.size() != 1)
			continue;
		Bool automatic = FALSE;
		for (Int j = 0; j < modules.getCount(); ++j)
		{
			if (modules.getNthName(j) == "GrantUpgradeCreate" &&
				static_cast<const GrantUpgradeCreateModuleData*>(modules.getNthData(j))->m_upgradeName == triggers.front())
				automatic = TRUE;
		}
		if (!automatic)
			continue;
		for (Int j = 0; j < modules.getCount(); ++j)
		{
			if (modules.getNthName(j) != "ReplaceSelfUpgrade")
				continue;
			const auto* replacement = static_cast<const ReplaceSelfUpgradeModuleData*>(modules.getNthData(j));
			const auto& activators = triggerNames(*replacement);
			if (activators.size() == 1 && activators.front() == upgrade->getUpgradeName() && !replacement->m_replacements.empty())
				return TheThingFactory->findTemplate(replacement->m_replacements.front(), FALSE);
		}
	}
	return nullptr;
}

void chooseRetailDwarvenEra(Player& player, Object& keep)
{
	const UpgradeTemplate* lotr = TheUpgradeCenter->findUpgrade("Upgrade_LotRErebor");
	const UpgradeTemplate* hobbit = TheUpgradeCenter->findUpgrade("Upgrade_HobbitErebor");
	const UpgradeTemplate* lotrAI = TheUpgradeCenter->findUpgrade("Upgrade_LotREreborAI");
	const UpgradeTemplate* hobbitAI = TheUpgradeCenter->findUpgrade("Upgrade_HobbitEreborAI");
	if (!lotr || !hobbit || !lotrAI || !hobbitAI ||
		player.hasUpgradeComplete(lotrAI) || player.hasUpgradeComplete(hobbitAI))
		return;
	if (player.hasUpgradeComplete(lotr))
		player.addUpgrade(lotrAI, UPGRADE_STATUS_COMPLETE);
	else if (player.hasUpgradeComplete(hobbit))
		player.addUpgrade(hobbitAI, UPGRADE_STATUS_COMPLETE);
	else if (!player.hasUpgradeInProduction(lotr) && !player.hasUpgradeInProduction(hobbit))
	{
		const CommandSet* set = retailCommands(keep);
		ProductionUpdateInterface* production = keep.getProductionUpdateInterface();
		if (!set || !production || production->getProductionCount() != 0)
			return;
		for (Int slot = 0; slot < CommandButtonRules::RETAIL_COMMANDS_PER_SET; ++slot)
		{
			const CommandButton* button = set->getCommandButton(slot);
			const UpgradeTemplate* upgrade = button ? button->getUpgradeTemplate() : nullptr;
			if (upgrade && (upgrade == lotr || upgrade == hobbit) && button->getCommandType() == GUI_COMMAND_PLAYER_UPGRADE &&
				HeroRoster::buttonUpgradesMet(button, &player, &keep) && production->queueUpgrade(upgrade))
				break;
		}
	}
}

Bool placeRetailBuilding(Object& builder, const ThingTemplate& type, const Coord3D& center)
{
	DozerAIInterface* dozer = builder.getAI()->getDozerAIInterface();
	Player* player = builder.getControllingPlayer();
	const Real angle = type.getPlacementViewAngle();
	const UnsignedInt flags = BuildAssistant::TERRAIN_RESTRICTIONS | BuildAssistant::NO_OBJECT_OVERLAP |
		BuildAssistant::CLEAR_PATH | BuildAssistant::USE_QUICK_PATHFIND | BuildAssistant::SUPPRESS_BUILD_BIBS;
	static const Coord2D directions[] = {
		{1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1}
	};
	for (Real distance = 100.0f; distance <= 700.0f; distance += 25.0f)
	{
		for (const Coord2D& direction : directions)
		{
			Coord3D point = {center.x + direction.x * distance, center.y + direction.y * distance, 0.0f};
			if (TheBuildAssistant->isLocationLegalToBuild(&point, &type, angle, flags, &builder, player) != LBC_OK)
				continue;
			point.z = TheTerrainLogic->getGroundHeight(point.x, point.y);
			if (dozer->construct(&type, &point, angle, player, FALSE))
				return TRUE;
		}
	}
	return FALSE;
}
}

void AISkirmishPlayer::loadRetailData()
{
	retailArmies.clear();
	retailTuning = {};
	INI ini;
	ini.load("Data\\INI\\Default\\SkirmishAIData.ini", INI_LOAD_OVERWRITE, nullptr);
}

void AISkirmishPlayer::parseRetailArmy(INI* ini)
{
	ini->getNextToken();
	RetailArmy army;
	static const FieldParse fields[] = {
		{ "Side", INI::parseAsciiString, nullptr, offsetof(RetailArmy, side) },
		{ "AIEconomyAssigment", parseFarmAssignment, nullptr, offsetof(RetailArmy, farm) },
		{ "AIWallNodeAssignment", skipRetailAISection, "AIWallNodeAssignment", 0 },
		{ "ArmyMemberDefinition", parseArmyMember, nullptr, 0 },
		{ "HeroBuildOrder", INI::parseAsciiStringVector, nullptr, offsetof(RetailArmy, heroes) },
		{ "PhaseDuration_Rush", INI::parseReal, nullptr, offsetof(RetailArmy, rushSeconds) },
		{ "PhaseDuration_MidGame", INI::parseReal, nullptr, offsetof(RetailArmy, midgameSeconds) },
		{ "EconomyBuilderMinFarmsOwned", INI::parseInt, nullptr, offsetof(RetailArmy, minimumFarms) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	ini->initFromINI(&army, fields);
	if (army.side.isNotEmpty())
		retailArmies.push_back(std::move(army));
}

void AISkirmishPlayer::parseRetailTuning(INI* ini)
{
	ini->getNextToken();
	static const FieldParse fields[] = {
		{ "DifficultyTuning", parseDifficultyTuning, nullptr, 0 },
		{ "CombatChainDefinition", skipRetailAISection, "CombatChainDefinition", 0 },
		{ "BrutalDifficultyCheats", skipRetailAISection, "BrutalDifficultyCheats", 0 },
		{ "DisableBaseBuilding", INI::parseBool, nullptr, offsetof(RetailAITuning, disableBaseBuilding) },
		{ "DisableEconomyBuilding", INI::parseBool, nullptr, offsetof(RetailAITuning, disableEconomyBuilding) },
		{ "DisableUnitBuilding", INI::parseBool, nullptr, offsetof(RetailAITuning, disableUnitBuilding) },
		{ "DisableUnitUpgrading", INI::parseBool, nullptr, offsetof(RetailAITuning, disableUnitUpgrading) },
		{ "DisableTacticalAI", INI::parseBool, nullptr, offsetof(RetailAITuning, disableTacticalAI) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	ini->initFromINI(&retailTuning, fields);
}

#define USE_DOZER 1



///////////////////////////////////////////////////////////////////////////////////////////////////
// PRIVATE DATA ///////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

AISkirmishPlayer::AISkirmishPlayer( Player *p ) :	AIPlayer(p),
m_curFlankBaseDefense(0),
m_curFrontBaseDefense(0),
m_curFrontLeftDefenseAngle(0),
m_curFrontRightDefenseAngle(0),
m_curLeftFlankLeftDefenseAngle(0),
m_curLeftFlankRightDefenseAngle(0),
m_curRightFlankLeftDefenseAngle(0),
m_curRightFlankRightDefenseAngle(0),
m_frameToCheckEnemy(0),
m_currentEnemy(nullptr)

{
	m_frameLastBuildingBuilt = TheGameLogic->getFrame();
	p->setCanBuildUnits(true); // turn on ai production by default.
}

AISkirmishPlayer::~AISkirmishPlayer()
{
	clearTeamsInQueue();
}


/**
 * Build our base.
 */
void AISkirmishPlayer::processBaseBuilding()
{
	//
	// Refresh base buildings. Scan through list, if a building is missing,
	// rebuild it, unless it's rebuild count is zero.
	//
	if (m_readyToBuildStructure)
	{
		const ThingTemplate *bldgPlan=nullptr;
		BuildListInfo	*bldgInfo = nullptr;
		Bool isPriority = false;
		Object *bldg = nullptr;
		const ThingTemplate *powerPlan=nullptr;
		BuildListInfo	*powerInfo = nullptr;
		Bool isUnderPowered = !m_player->getEnergy()->hasSufficientPower();
		Bool powerUnderConstruction = false;
		for( BuildListInfo *info = m_player->getBuildList(); info; info = info->getNext() )
		{
			AsciiString name = info->getTemplateName();
			if (name.isEmpty()) continue;
			const ThingTemplate *curPlan = TheThingFactory->findTemplate( name );
			if (!curPlan) {
				DEBUG_LOG(("*** ERROR - Build list building '%s' doesn't exist.", name.str()));
				continue;
			}
			bldg = TheGameLogic->findObjectByID( info->getObjectID() );
			// check for hole.
			if (info->getObjectID() != INVALID_ID) {
				// used to have a building.
				Object *bldg = TheGameLogic->findObjectByID( info->getObjectID() );
				if (bldg==nullptr) {
					// got destroyed.
					ObjectID priorID;
					priorID = info->getObjectID();
					info->setObjectID(INVALID_ID);
					info->setObjectTimestamp(TheGameLogic->getFrame()+1);
					// Scan for a GLA hole.	KINDOF_REBUILD_HOLE
					Object *obj;
					for( obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject() ) {
						if (!obj->isKindOf(KINDOF_REBUILD_HOLE)) continue;
						RebuildHoleBehaviorInterface *rhbi = RebuildHoleBehavior::getRebuildHoleBehaviorInterfaceFromObject( obj );
						if( rhbi ) {
							ObjectID spawnerID = rhbi->getSpawnerID();
							if (priorID == spawnerID) {
								DEBUG_LOG(("AI Found hole to rebuild %s", curPlan->getName().str()));
								info->setObjectID(obj->getID());
							}
						}
 					}
				}	else {
					if (bldg->getControllingPlayer() == m_player) {
						// Check for built or dozer missing.
						if( bldg->getStatusBits().test( OBJECT_STATUS_UNDER_CONSTRUCTION ) )
						{
							if (bldg->isKindOf(KINDOF_FS_POWER) && !bldg->isKindOf(KINDOF_CASH_GENERATOR))
							{
								powerUnderConstruction = true;
							}
							// make sure dozer is working on him.
							ObjectID builder = bldg->getBuilderID();
							Object* myDozer = TheGameLogic->findObjectByID(builder);

              if (myDozer && ( myDozer->getControllingPlayer() != m_player || myDozer->isDisabledByType( DISABLED_UNMANNED ) ) )
              {
                //I don't expect this dozer to work well with me.
                myDozer = nullptr;
                bldg->setBuilder( nullptr );
              }

							if (myDozer==nullptr) {
								DEBUG_LOG(("AI's Dozer got killed (or captured).  Find another dozer."));
								queueDozer();
 								myDozer = findDozer(bldg->getPosition());
								if (myDozer==nullptr || myDozer->getAI()==nullptr) {
									continue;
								}
								myDozer->getAI()->aiResumeConstruction(bldg, CMD_FROM_AI);
							}	else {
								// make sure he is building.
								myDozer->getAI()->aiResumeConstruction(bldg, CMD_FROM_AI);
							}
						}
					} else {
						// oops, got captured.
						info->setObjectID(INVALID_ID);
						info->setObjectTimestamp(TheGameLogic->getFrame()+1);
					}
				}
			}
			if (info->getObjectID()==INVALID_ID && info->getObjectTimestamp()>0) {
				// this object was built at some time, and got destroyed at or near objectTimestamp.
				// Wait a few seconds before initiating a rebuild.
				if (info->getObjectTimestamp()+TheAI->getAiData()->m_rebuildDelaySeconds*LOGICFRAMES_PER_SECOND > TheGameLogic->getFrame()) {
					continue;
				}	else {
					DEBUG_LOG(("Enabling rebuild for %s", info->getTemplateName().str()));
					info->setObjectTimestamp(0); // ready to build.
				}
			}
			if (bldg) {
				continue; // already built.
			}
			// Make sure it is safe to build here.
			if (!isLocationSafe(info->getLocation(), curPlan)) {
				continue;
			}
			if (info->isPriorityBuild()) {
				// Always take priority build, unless we already have priority build.
				if (!isPriority) {
					bldgPlan = curPlan;
					bldgInfo = info;
					isPriority = true;
				}
			}
			if (curPlan->isKindOf(KINDOF_FS_POWER)) {
				if (powerPlan==nullptr && !curPlan->isKindOf(KINDOF_CASH_GENERATOR)) {
					if (isUnderPowered || info->isAutomaticBuild()) {
						powerPlan = curPlan;
						powerInfo = info;
					}
				}
			}
			if (!info->isAutomaticBuild()) {
				continue; // marked to not build automatically.
			}
			Object *dozer = findDozer(info->getLocation());
			if (dozer==nullptr) {
				if (isUnderPowered) {
					queueDozer();
				}
				continue;
			}
			if (TheBuildAssistant->canMakeUnit(dozer, bldgPlan)!=CANMAKE_OK) {
				if (info->isBuildable()) {
					AsciiString bldgName = info->getTemplateName();
					bldgName.concat(" - Dozer unable to build - money or technology missing.");
					TheScriptEngine->AppendDebugMessage(bldgName, false);
				}
				continue;
			}
			// check if this building has any "rebuilds" left
			if (info->isBuildable())
			{
				if (bldgPlan == nullptr) {
					bldgPlan = curPlan;
					bldgInfo = info;
				}
			}
		}
		if (powerInfo && powerPlan && !powerPlan->isEquivalentTo(bldgPlan)) {
			if (!powerUnderConstruction) {
				bldgPlan = powerPlan;
				bldgInfo = powerInfo;
				DEBUG_LOG(("Forcing build of power plant."));
			}
		}
		if (bldgPlan && bldgInfo) {
#ifdef USE_DOZER
			// dozer-construct the building
			bldg = buildStructureWithDozer(bldgPlan, bldgInfo);
			// store the object with the build order
			if (bldg)
			{
				bldgInfo->setObjectID( bldg->getID() );
				bldgInfo->decrementNumRebuilds();

				m_readyToBuildStructure = false;
				m_structureTimer = TheAI->getAiData()->m_structureSeconds*static_cast<float>(LOGICFRAMES_PER_SECOND);
				if (m_player->getMoney()->countMoney() < TheAI->getAiData()->m_resourcesPoor) {
					m_structureTimer = m_structureTimer/TheAI->getAiData()->m_structuresPoorMod;
				}	else if (m_player->getMoney()->countMoney() > TheAI->getAiData()->m_resourcesWealthy) {
					m_structureTimer = m_structureTimer/TheAI->getAiData()->m_structuresWealthyMod;
				}
				m_frameLastBuildingBuilt = TheGameLogic->getFrame();
				// only build one building per delay loop
			}

#else
			// force delay between rebuilds
			Int framesToBuild = bldgPlan->calcTimeToBuild(m_player);
			if (TheGameLogic->getFrame() - m_frameLastBuildingBuilt < framesToBuild)
			{
				m_buildDelay = framesToBuild - (TheGameLogic->getFrame() - m_frameLastBuildingBuilt);
				return;
			}	else {
				// building is missing, (re)build it
				// deduct money to build, if we have it
				Int cost = bldgPlan->calcCostToBuild( m_player );
				if (m_player->getMoney()->countMoney() >= cost)
				{
					// we have the money, deduct it
					m_player->getMoney()->withdraw( cost );

					// inst-construct the building
					bldg = buildStructureNow(bldgPlan, bldgInfo);
					// store the object with the build order
					if (bldg)
					{
						bldgInfo->setObjectID( bldg->getID() );
						bldgInfo->decrementNumRebuilds();

						m_readyToBuildStructure = false;
						m_structureTimer = TheAI->getAiData()->m_structureSeconds*LOGICFRAMES_PER_SECOND;
						if (m_player->getMoney()->countMoney() < TheAI->getAiData()->m_resourcesPoor) {
							m_structureTimer = m_structureTimer/TheAI->getAiData()->m_structuresPoorMod;
						}	else if (m_player->getMoney()->countMoney() > TheAI->getAiData()->m_resourcesWealthy) {
							m_structureTimer = m_structureTimer/TheAI->getAiData()->m_structuresWealthyMod;
						}
						m_frameLastBuildingBuilt = TheGameLogic->getFrame();
					}
				}
			}
#endif
		}
	}
}

/**
 * Invoked when a unit I am training comes into existence
 */
void AISkirmishPlayer::onUnitProduced( Object *factory, Object *unit )
{
	AIPlayer::onUnitProduced(factory, unit);
}

/**
 * Search the computer player's buildings for one that can build the given request
 * and start training the unit.
 * If busyOK is true, it will queue a unit even if one is building.  This lets
 * script invoked teams "push" to the front of the queue.
 */
Bool AISkirmishPlayer::startTraining( WorkOrder *order, Bool busyOK, AsciiString teamName)
{
	Object *factory = findFactory(order->m_thing, busyOK);
	if( factory )
	{
		ProductionUpdateInterface *pu = factory->getProductionUpdateInterface();
		if (pu && pu->queueCreateUnit( order->m_thing, pu->requestUniqueUnitID() )) {
			order->m_factoryID = factory->getID();
			if (TheGlobalData->m_debugAI) {
				AsciiString teamStr = "Queuing ";
				teamStr.concat(order->m_thing->getName());
				teamStr.concat(" for ");
				teamStr.concat(teamName);
				TheScriptEngine->AppendDebugMessage(teamStr, false);
			}
			return true;
		}
	}

	return FALSE;

}


/**
 * Check if this team is buildable, doesn't exceed maximum limits, meets conditions, and isn't under construction.
 */
Bool AISkirmishPlayer::isAGoodIdeaToBuildTeam( TeamPrototype *proto )
{
	// Check condition.
	if (!proto->evaluateProductionCondition()) {
		return false;
	}
	// check build limit
	if (proto->countTeamInstances() >= proto->getTemplateInfo()->m_maxInstances){
		if (TheGlobalData->m_debugAI) {
			AsciiString str;
			str.format("Team %s not chosen - %d already exist.", proto->getName().str(), proto->countTeamInstances());
			TheScriptEngine->AppendDebugMessage(str, false);
		}
		return false;	// Max already built.
	}

	for ( DLINK_ITERATOR<TeamInQueue> iter = iterate_TeamBuildQueue(); !iter.done(); iter.advance())
	{
		TeamInQueue *team = iter.cur();
		if (team->m_team->getPrototype() == proto) {
			return false; // currently building one of these.
		}
	}
	Bool needMoney;
	if (!isPossibleToBuildTeam( proto, true, needMoney)) {
		if (TheGlobalData->m_debugAI) {
			AsciiString str;
			if (needMoney) {
				str.format("Team %s not chosen - Not enough money.", proto->getName().str());
			} else {
				str.format("Team %s not chosen - Factory/tech missing or busy.", proto->getName().str());
			}
			TheScriptEngine->AppendDebugMessage(str, false);
		}
		return false;
	}
	return true;
}

/**
 * See if any existing teams need reinforcements, and have higher priority.
 */
Bool AISkirmishPlayer::selectTeamToReinforce( Int minPriority )
{
	return AIPlayer::selectTeamToReinforce(minPriority);
}

/**
 * Determine the next team to build.  Return true if one was selected.
 */
Bool AISkirmishPlayer::selectTeamToBuild()
{
	return AIPlayer::selectTeamToBuild();
}

/**
	Build a specific building.
	*/
void AISkirmishPlayer::buildSpecificAIBuilding(const AsciiString &thingName)
{
	//
	Bool found = false;
	Bool foundUnbuilt = false;
	for( BuildListInfo *info = m_player->getBuildList(); info; info = info->getNext() )
	{
		if (info->getTemplateName()==thingName)
		{
			AsciiString name = info->getTemplateName();
			if (name.isEmpty()) continue;
			const ThingTemplate *bldgPlan = TheThingFactory->findTemplate( name );
			if (!bldgPlan) {
				DEBUG_LOG(("*** ERROR - Build list building '%s' doesn't exist.", name.str()));
				continue;
			}
			Object *bldg = TheGameLogic->findObjectByID( info->getObjectID() );
			found = true;
			if (bldg) {
				continue; // already built.
			}
			if (info->isPriorityBuild()) {
				continue; // already marked for priority build.
			}
			foundUnbuilt = true;
			info->markPriorityBuild();
			break;
		}
	}
	if (foundUnbuilt) {
		m_buildDelay = 0;
		AsciiString buildingStr = "Queueing building '";
		buildingStr.concat(thingName);
		buildingStr.concat("' for construction.");
		TheScriptEngine->AppendDebugMessage(buildingStr, false);
	}	else if (found) {
		AsciiString buildingStr = "Warning - all instances of building '";
		buildingStr.concat(thingName);
		buildingStr.concat("' are already built or queued for build, not queueing.");
		TheScriptEngine->AppendDebugMessage(buildingStr, false);
	}	else {
		AsciiString buildingStr = "Error - could not find building '";
		buildingStr.concat(thingName);
		buildingStr.concat("' in the building template list.");
		TheScriptEngine->AppendDebugMessage(buildingStr, false);
	}
}



/**
	Gets the player index of my enemy.
	*/
Int AISkirmishPlayer::getMyEnemyPlayerIndex() {
	Int playerNdx;
	if (m_currentEnemy) {
		return m_currentEnemy->getPlayerIndex();
	}
	// For now, return first human player, as there should only be one. jba
	for (playerNdx=0; playerNdx<ThePlayerList->getPlayerCount(); playerNdx++) {
		if (ThePlayerList->getNthPlayer(playerNdx)->getPlayerType() == PLAYER_HUMAN) {
			break;
		}
	}
	return playerNdx;
}

/**
	Get the AI's enemy.  Recalc if it has been a while (5 seconds.)
*/
void AISkirmishPlayer::acquireEnemy()
{
	Player *bestEnemy = nullptr;
	Real bestDistanceSqr = HUGE_DIST*HUGE_DIST;

	if (m_currentEnemy) {
		Bool inBadShape = !m_currentEnemy->hasAnyUnits() || !m_currentEnemy->hasAnyBuildFacility();
		if (!inBadShape) return;
	}

	// look for the closest enemy.
	Int i;
	for (i=0; i<ThePlayerList->getPlayerCount(); i++) {
		Player *curPlayer = ThePlayerList->getNthPlayer(i);
		if (m_player->getRelationship(curPlayer->getDefaultTeam()) == ENEMIES) {
			if (curPlayer->hasAnyObjects()==false) continue; // not much of an enemy.
			// ok, we got an enemy;
			// If a player is out of units, or out of build facilities, we can lower his priority.
			Bool inBadShape = !curPlayer->hasAnyUnits() || !curPlayer->hasAnyBuildFacility();

			Coord3D enemyPos = m_baseCenter;
			Region2D bounds;
			getPlayerStructureBounds(&bounds, i);
			enemyPos.x = bounds.lo.x + bounds.width()/2;
			enemyPos.y = bounds.lo.y + bounds.height()/2;
			Real curDistSqr = sqr(enemyPos.x-m_baseCenter.x) + sqr(enemyPos.y-m_baseCenter.y);

			//Fudge for in bad shape.	 If an enemy is crippled, concentrate on the other ones.
			if (inBadShape) {
				curDistSqr = HUGE_DIST*HUGE_DIST*0.5f;
			}
			// See if other ai's are attacking this target.
			// We don't want the ai's to gang up on one enemy.
			Int k;
			for (k=0; k<ThePlayerList->getPlayerCount(); k++) {
				if (k==i) continue;  // don't count self.
				Player *somePlayer = ThePlayerList->getNthPlayer(k);
				if (somePlayer->isSkirmishAIPlayer() && (somePlayer->getCurrentEnemy()==curPlayer)) {
					// Some ai is already targeting this guy.  Add a distance penalty.
					curDistSqr += (500*500);
				}
				if (somePlayer->isSkirmishAIPlayer() && (somePlayer->getCurrentEnemy()==m_player)) {
					// he is attacking me.  So I will (gently) prefer to attack him.
					curDistSqr -= (25*25);
					if (curDistSqr<0) curDistSqr = 0;
				}
			}

			// Ai enemy - will take if we don't get a better offer.
			if (curDistSqr<bestDistanceSqr) {
				bestEnemy = curPlayer;
				bestDistanceSqr = curDistSqr;
			}
		}
	}
	if (bestEnemy!=nullptr && (bestEnemy!=m_currentEnemy)) {
		m_currentEnemy = bestEnemy;
		AsciiString msg = TheNameKeyGenerator->keyToName(m_player->getPlayerNameKey());
		msg.concat(" acquiring target enemy player: ");
		msg.concat(TheNameKeyGenerator->keyToName(m_currentEnemy->getPlayerNameKey()));
		TheScriptEngine->AppendDebugMessage( msg, false);
	}
}



/**
	Get the AI's enemy.  Recalc if it has been a while (20 seconds.)
*/
Player *AISkirmishPlayer::getAiEnemy()
{
	if (TheGameLogic->getFrame()>=m_frameToCheckEnemy) {
		m_frameToCheckEnemy = TheGameLogic->getFrame() + 5*LOGICFRAMES_PER_SECOND;
		acquireEnemy();
	}
	return m_currentEnemy;
}

/**
	Build base defense structures on the front or flank of the base.
*/
void AISkirmishPlayer::buildAIBaseDefense(Bool flank)
{
	const AISideInfo *resInfo = TheAI->getAiData()->m_sideInfo;
	AsciiString defenseTemplateName;
	while (resInfo) {
		if (resInfo->m_side == m_player->getSide()) {
			defenseTemplateName = resInfo->m_baseDefenseStructure1;
			break;
		}
		resInfo = resInfo->m_next;
	}
	if (resInfo) {
		buildAIBaseDefenseStructure(resInfo->m_baseDefenseStructure1, flank);
	}
}

/**
	Build base defense structures on the front or flank of the base.
	Base defenses are placed as follows:
	m_baseCenter and m_baseRadius are calculated on map load.
	Defenses are placed along the this circle.
	Front defenses (!flank) are placed starting at the "Center" approach path.
	The first front defense is placed towards th Center path.  Number 2 is placed
	to the left of #1, #3 is placed to the right of #1, #4 is placed to the left of
	#2 and so on.  So it looks like:

												#1
									 #2 			#3
					#6  #4								 #5	  #7
		  #8																	#9

	The flank base defenses cover the "Flank" approach, and the "Backdoor" approach.
	They alternate between these two, so the first flank defense covers flank, and the second
	covers backdoor, and continue to alternate.  They cover the approach using the same
	pattern as front above.
	John A.

	*/
void AISkirmishPlayer::buildAIBaseDefenseStructure(const AsciiString &thingName, Bool flank)
{
	const ThingTemplate *tTemplate = TheThingFactory->findTemplate(thingName);
	if (tTemplate==nullptr) {
		DEBUG_CRASH(("Couldn't find base defense structure '%s' for side %s", thingName.str(), m_player->getSide().str()));
		return;
	}
	do {
		AsciiString pathLabel;
		if (flank) {
			if (m_curFlankBaseDefense&1) {
				pathLabel.format("%s%d", SKIRMISH_FLANK, m_player->getMpStartIndex()+1);
			}	else {
				pathLabel.format("%s%d", SKIRMISH_BACKDOOR, m_player->getMpStartIndex()+1);
			}
		}	else {
			pathLabel.format("%s%d", SKIRMISH_CENTER, m_player->getMpStartIndex()+1);
		}

		Coord3D goalPos = m_baseCenter;
		Waypoint *way = TheTerrainLogic->getClosestWaypointOnPath( &goalPos, pathLabel );
		if (way) {
			goalPos = *way->getLocation();
		} else {
			if (flank) return;
			Region2D bounds;
			getPlayerStructureBounds(&bounds, getMyEnemyPlayerIndex());
			goalPos.x = bounds.lo.x + bounds.width()/2;
			goalPos.y = bounds.lo.y + bounds.height()/2;
		}
		Coord2D offset;
		offset.x = goalPos.x-m_baseCenter.x;
		offset.y = goalPos.y-m_baseCenter.y;
		offset.normalize();
		Real defenseDistance = m_baseRadius;
		defenseDistance += TheAI->getAiData()->m_skirmishBaseDefenseExtraDistance;
		offset.x *= defenseDistance;
		offset.y *= defenseDistance;

		Real structureRadius = tTemplate->getTemplateGeometryInfo().getBoundingCircleRadius();
		Real baseCircumference = 2*PI*defenseDistance;
		Real angleOffset = 2*PI*WWMath::Div_FixNaN(structureRadius*4, baseCircumference, 0.0f);

		Int selector;
		Real angle;
		if (flank) {
			selector = m_curFlankBaseDefense>>1;
			if (m_curFlankBaseDefense&1) {
				if (selector&1) {
					m_curLeftFlankRightDefenseAngle -= angleOffset;
					angle = m_curLeftFlankRightDefenseAngle;
				}	else {
					angle = m_curLeftFlankLeftDefenseAngle;
					m_curLeftFlankLeftDefenseAngle += angleOffset;
				}
			}	else {
				if (selector&1) {
					m_curRightFlankRightDefenseAngle -= angleOffset;
					angle = m_curRightFlankRightDefenseAngle;
				}	else {
					angle = m_curRightFlankLeftDefenseAngle;
					m_curRightFlankLeftDefenseAngle += angleOffset;
				}
			}

		} else {
			selector = m_curFrontBaseDefense;
			if (selector&1) {
				m_curFrontRightDefenseAngle -= angleOffset;
				angle = m_curFrontRightDefenseAngle;
			}	else {
				angle = m_curFrontLeftDefenseAngle;
				m_curFrontLeftDefenseAngle += angleOffset;
			}
		}

		if (angle > PI/3) break;
		Real s = WWMath::SinTrig(angle);
		Real c = WWMath::CosTrig(angle);

// TheSuperHackers @info helmutbuhler 21/04/2025 This debug mutates the code to become CRC incompatible
#if defined(RTS_DEBUG) || !RETAIL_COMPATIBLE_CRC
		DEBUG_LOG(("buildAIBaseDefenseStructure -- Angle is %f sin %f, cos %f", 180*angle/PI, s, c));
		DEBUG_LOG(("buildAIBaseDefenseStructure -- Offset is %f  %f, Final Position is %f, %f",
			offset.x, offset.y,
			offset.x*c - offset.y*s,
			offset.y*c + offset.x*s
			));
#endif
		Coord3D buildPos = m_baseCenter;
		buildPos.x += offset.x*c - offset.y*s;
		buildPos.y += offset.y*c + offset.x*s;

		/* See if we can build there. */
		Bool canBuild;
		Real placeAngle = tTemplate->getPlacementViewAngle();
		canBuild = LBC_OK == TheBuildAssistant->isLocationLegalToBuild(&buildPos, tTemplate, placeAngle,
			BuildAssistant::TERRAIN_RESTRICTIONS|BuildAssistant::NO_OBJECT_OVERLAP, nullptr, m_player);
		TheTerrainVisual->removeAllBibs();	// isLocationLegalToBuild adds bib feedback, turn it off.  jba.
		if (flank) {
			m_curFlankBaseDefense++;
		} else {
			m_curFrontBaseDefense++;
		}
		if (canBuild) {
			m_player->addToPriorityBuildList(thingName, &buildPos, placeAngle);
			break;
		}
	}	while (true);

}


/**
	Checks bridges along a waypoint path.  If any are destroyed, sends a dozer to fix, and returns true.
	If there is no bridge problem, returns false.
	*/
Bool AISkirmishPlayer::checkBridges(Object *unit, Waypoint *way)
{
	Coord3D unitPos = *unit->getPosition();
	AIUpdateInterface *ai = unit->getAI();
	if (!ai) return false; // no ai
	const LocomotorSet& locoSet = ai->getLocomotorSet();
	Waypoint *curWay;
	for (curWay = way; curWay; curWay = curWay->getNext()) {
		if (TheAI->pathfinder()->clientSafeQuickDoesPathExist(locoSet, &unitPos, curWay->getLocation())) {
			continue;
		}
		ObjectID brokenBridge = INVALID_ID;
		if (TheAI->pathfinder()->findBrokenBridge(locoSet, &unitPos, curWay->getLocation(), &brokenBridge)) {
			repairStructure(brokenBridge);
			return true;
		}
	}
	return false;

}


/**
	Build a specific team.  If priorityBuild, put at front of queue with priority set.
	*/
void AISkirmishPlayer::buildSpecificAITeam( TeamPrototype *teamProto, Bool priorityBuild)
{
	AIPlayer::buildSpecificAITeam(teamProto, priorityBuild);
}


/**
	Recruit a specific team, within the specific radius of the home position.
	*/
void AISkirmishPlayer::recruitSpecificAITeam(TeamPrototype *teamProto, Real recruitRadius)
{
	if (recruitRadius < 1) recruitRadius = 99999.0f;
	//
	// Create "Team in queue" based on team population
	//
	if (teamProto)
	{
		if (teamProto->getIsSingleton()) {
			Team *singletonTeam = TheTeamFactory->findTeam( teamProto->getName() );
			if (singletonTeam && singletonTeam->hasAnyObjects()) {
				AsciiString teamStr = "Unable to recruit singleton team '";
				teamStr.concat("' because team already exists.");
				TheScriptEngine->AppendDebugMessage(teamStr, false);
				return;
			}
		}
		if (!teamProto->getTemplateInfo()->m_hasHomeLocation)
		{
			AsciiString teamStr = "Error : team '";
			teamStr.concat(teamProto->getName());
			teamStr.concat("' has no Home Position (or Origin).");
			TheScriptEngine->AppendDebugMessage(teamStr, false);
		}
		// create inactive team to place members into as they are built
		// when team is complete, the team is activated
		Team *theTeam = TheTeamFactory->createInactiveTeam( teamProto->getName() );
		AsciiString teamName = teamProto->getName();
		teamName.concat(" - Recruiting.");
		TheScriptEngine->AppendDebugMessage(teamName, false);
		const TCreateUnitsInfo *unitInfo = &teamProto->getTemplateInfo()->m_unitsInfo[0];
//		WorkOrder *orders = nullptr;
		Int i;
		Int unitsRecruited = 0;
		// Recruit.
		for( i=0; i<teamProto->getTemplateInfo()->m_numUnitsInfo; i++ )
		{
			const ThingTemplate *thing = TheThingFactory->findTemplate( unitInfo[i].unitThingName );
			if (thing)
			{
				int count = unitInfo[i].maxUnits;
				while (count>0) {
					Object *unit = theTeam->tryToRecruit(thing, &teamProto->getTemplateInfo()->m_homeLocation, recruitRadius);
					if (unit)
					{
						unitsRecruited++;

						AsciiString teamStr = "Team '";
						teamStr.concat(theTeam->getPrototype()->getName());
						teamStr.concat("' recruits ");
						teamStr.concat(thing->getName());
						teamStr.concat(" from team '");
						teamStr.concat(unit->getTeam()->getPrototype()->getName());
						teamStr.concat("'");
						TheScriptEngine->AppendDebugMessage(teamStr, false);

						unit->setTeam(theTeam);

						AIUpdateInterface *ai = unit->getAIUpdateInterface();
						if (ai)
						{
#ifdef DEBUG_LOGGING
							Coord3D pos = *unit->getPosition();
							Coord3D to = teamProto->getTemplateInfo()->m_homeLocation;
							DEBUG_LOG(("Moving unit from %f,%f to %f,%f", pos.x, pos.y , to.x, to.y ));
#endif
							ai->aiMoveToPosition( &teamProto->getTemplateInfo()->m_homeLocation, CMD_FROM_AI);
						}
					} else {
						break;
					}
					count--;
				}
			}
		}
		if (unitsRecruited>0)
		{
			/* We have something to build. */
			TeamInQueue *team = newInstance(TeamInQueue);
			// Put in front of queue.
			prependTo_TeamReadyQueue(team);
			team->m_priorityBuild = false;
			team->m_workOrders = nullptr;
			team->m_frameStarted = TheGameLogic->getFrame();
			team->m_team = theTeam;
			AsciiString teamName = teamProto->getName();
			teamName.concat(" - Finished recruiting.");
			TheScriptEngine->AppendDebugMessage(teamName, false);
		}	else {
			//disband.
			if (!theTeam->getPrototype()->getIsSingleton()) {
				deleteInstance(theTeam);
				theTeam = nullptr;
			}
			AsciiString teamName = teamProto->getName();
			teamName.concat(" - Recruited 0 units, disbanding.");
			TheScriptEngine->AppendDebugMessage(teamName, false);
		}
	}
}




/**
 * Train our teams.
 */
void AISkirmishPlayer::processTeamBuilding()
{
	// select a new team
	if (selectTeamToBuild()) {
		queueUnits();
	}
}

//----------------------------------------------------------------------------------------------------------
/**
 * See if it's time to build another base building.
 */
void AISkirmishPlayer::doBaseBuilding()
{
	if (m_player->getCanBuildBase()) {
		// See if we are ready to start trying a structure.
		if (!m_readyToBuildStructure) {
			m_structureTimer--;
			if (m_structureTimer<=0) {
				m_readyToBuildStructure = true;
				m_buildDelay = 0;
			}
			if (m_structureTimer > 3*LOGICFRAMES_PER_SECOND) {
				m_structureTimer = 3*LOGICFRAMES_PER_SECOND;
			}
		}
		// This timer is to keep from banging on the logic each frame.  If something interesting
		// happens, like a building is added or a unit finished, the timers are shortcut.
		m_buildDelay--;
		if (m_buildDelay<1) {
			if (m_readyToBuildStructure) {
				processBaseBuilding();
			}
			if (m_buildDelay<1) {
				// processBaseBuilding may reset m_buildDelay.
				m_buildDelay = 2*LOGICFRAMES_PER_SECOND; // check again in 2 seconds.
			}
			// Note that this timer gets shortcut when a building is completed.
		}
	}
}

//----------------------------------------------------------------------------------------------------------
/**
 * See if any ready teams have finished moving to the rally point.
 */
void AISkirmishPlayer::checkReadyTeams()
{
	AIPlayer::checkReadyTeams();
}

//----------------------------------------------------------------------------------------------------------
/**
 * See if any queued teams have finished building, or have run out of time.
 */
void AISkirmishPlayer::checkQueuedTeams()
{
	AIPlayer::checkQueuedTeams();
}

//----------------------------------------------------------------------------------------------------------
/**
 * See if it is time to start another ai team building.
 */
void AISkirmishPlayer::doTeamBuilding()
{
	// See if any teams are expired.
	if (m_player->getCanBuildUnits()) {
		// See if we are ready to start trying a team.
		if (!m_readyToBuildTeam) {
			m_teamTimer--;
			if (m_teamTimer<=0) {
				m_readyToBuildTeam = true;
				m_teamDelay = 0;
			}
			if (m_teamTimer > 3*LOGICFRAMES_PER_SECOND) {
				m_teamTimer = 3*LOGICFRAMES_PER_SECOND;
			}
		}

		// This timer is to keep from banging on the logic each frame.  If something interesting
		// happens, like a building is added or a unit finished, the timers are shortcut.
		m_teamDelay--;
		if (m_teamDelay<1) {
			queueUnits(); // update the queues.
			if (m_readyToBuildTeam) {
				processTeamBuilding();
			}
			m_teamDelay = 2*LOGICFRAMES_PER_SECOND; // check again in 5 seconds.
			// Note that this timer gets shortcut when a unit or building is completed.
		}
	}
}

//----------------------------------------------------------------------------------------------------------
/**
 * Perform computer-controlled player AI
 */
void AISkirmishPlayer::update()
{
	if (RetailFileLayers::enabled())
	{
		updateRetail();
		return;
	}
	AIPlayer::update();
}

void AISkirmishPlayer::updateRetail()
{
	const UnsignedInt frame = TheGameLogic->getFrame();
	if (!TheRetailCommandCatalog || !m_player->isPlayableSide() || !m_player->isPlayerActive() || frame == 0 ||
		frame % (2 * LOGICFRAMES_PER_SECOND) != static_cast<UnsignedInt>(m_player->getPlayerIndex()))
		return;
	const auto rules = std::find_if(retailArmies.begin(), retailArmies.end(), [this](const RetailArmy& army) {
		return army.side.compareNoCase(m_player->getSide().str()) == 0;
	});
	if (rules == retailArmies.end())
		return;

	const Real elapsed = INT_TO_REAL(frame) / LOGICFRAMES_PER_SECOND;
	const Int phase = elapsed < rules->rushSeconds ? 0 : elapsed < rules->rushSeconds + rules->midgameSeconds ? 1 : 2;
	const ThingTemplate* farm = TheThingFactory->findTemplate(rules->farm, FALSE);
	const auto isFarm = [farm](const ThingTemplate& type) {
		return type.isKindOf(KINDOF_ECONOMY_STRUCTURE) || (farm && type.isEquivalentTo(farm));
	};
	static const Bool trace = std::getenv("GENERALSX_SKIRMISH_AI_TRACE") != nullptr;
	const Int playerIndex = m_player->getPlayerIndex();
	Int farms = 0;
	Int farmLimit = rules->minimumFarms;
	const GameDifficulty difficulty = m_player->getPlayerDifficulty();
	const char* const names[] = { "EASY", "NORMAL", "HARD" };
	if (difficulty >= DIFFICULTY_EASY && difficulty <= DIFFICULTY_HARD)
		for (const RetailDifficulty& tuning : retailTuning.difficulties)
			if (tuning.name == names[difficulty])
				farmLimit = tuning.farmLimit;

	Object* keep = nullptr;
	std::vector<Object*> builders;
	std::vector<Object*> plots;
	std::vector<Object*> producers;
	std::vector<Object*> soldiers;
	std::vector<Object*> owned;
	for (Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
	{
		if (object->getControllingPlayer() != m_player || object->isEffectivelyDead())
			continue;
		owned.push_back(object);
		if (isFarm(*object->getTemplate()))
			++farms;
		if (object->isDisabled() || object->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION) || object->getContainedBy())
			continue;
		if (!keep && object->isKindOf(KINDOF_COMMANDCENTER))
			keep = object;
		if (auto* foundation = FoundationAIUpdate::find(*object); foundation && foundation->canBuildOn(m_player))
			plots.push_back(object);
		if (object->getProductionUpdateInterface())
			producers.push_back(object);
		if (AIUpdateInterface* ai = object->getAI())
		{
			if (DozerAIInterface* dozer = ai->getDozerAIInterface())
			{
				if (!dozer->isAnyTaskPending() && ai->isIdle())
					builders.push_back(object);
			}
			else if (!object->isKindOf(KINDOF_IMMOBILE) && !object->isKindOf(KINDOF_INERT) &&
				(object->isKindOf(KINDOF_HORDE) || object->isKindOf(KINDOF_CAN_ATTACK)))
				soldiers.push_back(object);
		}
	}

	if (keep && m_player->getSide() == "Dwarves" && !retailTuning.disableUnitUpgrading)
		chooseRetailDwarvenEra(*m_player, *keep);

	const auto countType = [&](const ThingTemplate* type) {
		return static_cast<Int>(std::count_if(owned.begin(), owned.end(), [type](const Object* object) {
			return object->getTemplate()->isEquivalentTo(type) || object->getHeroIdentityTemplate()->isEquivalentTo(type);
		}));
	};
	std::vector<std::pair<const ThingTemplate*, const RetailArmyMember*>> army;
	for (const RetailArmyMember& member : rules->members)
		if (const ThingTemplate* unit = TheThingFactory->findTemplate(member.type, FALSE))
			army.emplace_back(unit, &member);
	const auto armyWeight = [&](const ThingTemplate& unit, Bool anyPhase) {
		Real weight = 0.0f;
		for (const auto& [planned, member] : army)
		{
			if (matchesArmyType(unit, *planned))
				weight = std::max(weight, anyPhase ? std::max({member->share[0], member->share[1], member->share[2]}) : member->share[phase]);
		}
		return weight;
	};
	const auto producesArmy = [&](const ThingTemplate& building) {
		const CommandSet* set = TheRetailCommandCatalog->findCommandSet(building.friend_getCommandSetString());
		if (!set)
			return FALSE;
		for (Int slot = 0; slot < CommandButtonRules::RETAIL_COMMANDS_PER_SET; ++slot)
		{
			const CommandButton* button = set->getCommandButton(slot);
			const ThingTemplate* unit = button ? static_cast<const ThingTemplate*>(button->getThingTemplate()) : nullptr;
			if (unit && button->getCommandType() == GUI_COMMAND_UNIT_BUILD && armyWeight(*unit, FALSE) > 0.0f)
				return TRUE;
		}
		return FALSE;
	};

	const Int armySize = std::count_if(soldiers.begin(), soldiers.end(), [&](const Object* unit) {
		return armyWeight(*unit->getTemplate(), TRUE) > 0.0f ||
			armyWeight(*unit->getHeroIdentityTemplate(), TRUE) > 0.0f;
	});
	const Bool needsArmy = armySize < RETAIL_ATTACK_WAVE_MIN_UNITS;
	UnsignedInt openingUnitReserve = 0;
	if (needsArmy && farms >= 2 && farmLimit > farms &&
		!retailTuning.disableUnitBuilding && m_player->getCanBuildUnits())
	{
		for (Object* producer : producers)
		{
			const CommandSet* set = retailCommands(*producer);
			const auto* production = producer->getProductionUpdateInterface();
			if (!set || production->getProductionCount() != 0)
				continue;
			for (Int slot = 0; slot < CommandButtonRules::RETAIL_COMMANDS_PER_SET; ++slot)
			{
				const CommandButton* button = set->getCommandButton(slot);
				const ThingTemplate* unit = button ? button->getThingTemplate() : nullptr;
				if (!unit || button->getCommandType() != GUI_COMMAND_UNIT_BUILD || armyWeight(*unit, FALSE) <= 0.0f ||
					button->getRequiredRank() > producer->getExperienceTracker()->getRank() ||
					!HeroRoster::buttonUpgradesMet(button, m_player, producer) || !m_player->hasEnoughCommandPoints(unit))
					continue;
				const CanMakeType result = TheBuildAssistant->canMakeUnit(producer, unit);
				HeroRules::Price price;
				const Int cost = production->getUnitPriceOverride(unit, price) ? price.cost : unit->calcCostToBuild(m_player);
				if (cost > 0 && (result == CANMAKE_OK || result == CANMAKE_NO_MONEY) &&
					(openingUnitReserve == 0 || static_cast<UnsignedInt>(cost) < openingUnitReserve))
					openingUnitReserve = static_cast<UnsignedInt>(cost);
			}
		}
	}

	if (!retailTuning.disableBaseBuilding && m_player->getCanBuildBase())
	{
		const auto buildFrom = [&](Object& builder, FoundationAIUpdate* plot) -> Bool {
			const CommandSet* set = retailCommands(builder);
			if (!set)
				return FALSE;
			const CommandButton* selected = nullptr;
			Int priority = 0;
			for (Int slot = 0; slot < CommandButtonRules::RETAIL_COMMANDS_PER_SET; ++slot)
			{
				const CommandButton* button = set->getCommandButton(slot);
				if (!button || button->getCommandType() != (plot ? GUI_COMMAND_FOUNDATION_CONSTRUCT : GUI_COMMAND_DOZER_CONSTRUCT))
					continue;
				const ThingTemplate* type = button->getThingTemplate();
				if (!type || button->getRequiredRank() > builder.getExperienceTracker()->getRank() ||
					!HeroRoster::buttonUpgradesMet(button, m_player, &builder))
					continue;
				if (plot ? (!m_player->canBuild(type) || !m_player->canAffordBuild(type))
					: TheBuildAssistant->canMakeUnit(&builder, type) != CANMAKE_OK)
					continue;
				const Bool economy = isFarm(*type);
				Int score = 0;
				if (economy)
				{
					const auto money = m_player->getMoney()->countMoney();
					const auto cost = static_cast<UnsignedInt>(std::max(0, type->calcCostToBuild(m_player)));
					if (!retailTuning.disableEconomyBuilding && farms < farmLimit &&
						(farms < 2 || (money >= cost && money - cost >= openingUnitReserve)))
						score = farms < 2 ? 3 : 1;
				}
				else if (countType(type) == 0)
				{
					const ThingTemplate* replacement = automaticReplacement(*type);
					if (producesArmy(*type) || (replacement && countType(replacement) == 0 && producesArmy(*replacement)))
						score = 2;
					else if (plot && farms >= 2 && soldiers.size() >= 2 && type->isKindOf(KINDOF_FS_BASE_DEFENSE))
						score = 1;
				}
				if (score > priority)
				{
					priority = score;
					selected = button;
				}
			}
			if (!selected)
				return FALSE;
			const ThingTemplate* type = selected->getThingTemplate();
			const Bool built = plot ? plot->construct(type, m_player) != nullptr
				: placeRetailBuilding(builder, *type, keep ? *keep->getPosition() : *builder.getPosition());
			if (built)
			{
				if (isFarm(*type))
					++farms;
				if (trace)
				{
					std::fprintf(stderr, "AI_BUILD\t%u\t%d\t%s\t%s\t%d\t%d\t%d\n", frame, playerIndex,
						selected->getName().str(), type->getName().str(),
						HeroRoster::buttonUpgradesMet(selected, m_player, &builder), farms, farmLimit);
					std::fflush(stderr);
				}
			}
			return built;
		};
		for (Object* plot : plots)
			if (buildFrom(*plot, FoundationAIUpdate::find(*plot)))
				break;
		for (Object* builder : builders)
			if (buildFrom(*builder, nullptr))
				break;
	}

	for (Object* producer : producers)
	{
		ProductionUpdateInterface* production = producer->getProductionUpdateInterface();
		const CommandSet* set = retailCommands(*producer);
		if (!set || production->getProductionCount() != 0)
			continue;
		const auto train = [&](const ThingTemplate* unit, const CommandButton* button) {
			if (!unit || TheBuildAssistant->canMakeUnit(producer, unit) != CANMAKE_OK ||
				!production->queueCreateUnit(unit, production->requestUniqueUnitID()))
				return FALSE;
			if (trace)
			{
				std::fprintf(stderr, "AI_TRAIN\t%u\t%d\t%s\t%s\t%d\n", frame, playerIndex,
					button ? button->getName().str() : "hero-roster", unit->getName().str(),
					button ? HeroRoster::buttonUpgradesMet(button, m_player, producer) : TRUE);
				std::fflush(stderr);
			}
			return TRUE;
		};
		const auto unlocksArmy = [&](const UpgradeTemplate* upgrade) {
			for (Int slot = 0; slot < CommandButtonRules::RETAIL_COMMANDS_PER_SET; ++slot)
			{
				const CommandButton* button = set->getCommandButton(slot);
				const ThingTemplate* unit = button ? button->getThingTemplate() : nullptr;
				if (!unit || button->getCommandType() != GUI_COMMAND_UNIT_BUILD || armyWeight(*unit, TRUE) <= 0.0f)
					continue;
				const auto& needed = button->getNeededUpgrades();
				if (std::find(needed.begin(), needed.end(), upgrade) != needed.end() &&
					!HeroRoster::buttonUpgradesMet(button, m_player, producer))
					return TRUE;
			}
			return FALSE;
		};
		if (!retailTuning.disableUnitUpgrading && farms >= 2)
		{
			for (Int slot = 0; slot < CommandButtonRules::RETAIL_COMMANDS_PER_SET; ++slot)
			{
				const CommandButton* button = set->getCommandButton(slot);
				const UpgradeTemplate* upgrade = button ? button->getUpgradeTemplate() : nullptr;
				if (!upgrade || (button->getCommandType() != GUI_COMMAND_PLAYER_UPGRADE &&
					button->getCommandType() != GUI_COMMAND_OBJECT_UPGRADE))
					continue;
				if (needsArmy && !unlocksArmy(upgrade))
					continue;
				if (m_player->getMoney()->countMoney() >= static_cast<UnsignedInt>(std::max(0, upgrade->calcCostToBuild(m_player))) &&
					button->getRequiredRank() <= producer->getExperienceTracker()->getRank() &&
					HeroRoster::buttonUpgradesMet(button, m_player, producer) && production->queueUpgrade(upgrade))
					break;
			}
		}
		if (retailTuning.disableUnitBuilding || !m_player->getCanBuildUnits() || production->getProductionCount() != 0)
			continue;
		if (!needsArmy)
			for (const AsciiString& name : rules->heroes)
			{
				const ThingTemplate* hero = TheThingFactory->findTemplate(name, FALSE);
				if (hero && !HeroRoster::isAlive(m_player, hero) && train(hero, nullptr))
					break;
			}
		if (production->getProductionCount() != 0)
			continue;
		const CommandButton* selected = nullptr;
		Real priority = 0.0f;
		for (Int slot = 0; slot < CommandButtonRules::RETAIL_COMMANDS_PER_SET; ++slot)
		{
			const CommandButton* button = set->getCommandButton(slot);
			const ThingTemplate* unit = button ? button->getThingTemplate() : nullptr;
			if (!unit || button->getCommandType() != GUI_COMMAND_UNIT_BUILD)
				continue;
			const Real weight = armyWeight(*unit, FALSE);
			if (weight <= 0.0f)
				continue;
			const Real score = weight / INT_TO_REAL(1 + countType(unit));
			if (score > priority && button->getRequiredRank() <= producer->getExperienceTracker()->getRank() &&
				HeroRoster::buttonUpgradesMet(button, m_player, producer) && TheBuildAssistant->canMakeUnit(producer, unit) == CANMAKE_OK)
			{
				priority = score;
				selected = button;
			}
		}
		if (selected)
			train(selected->getThingTemplate(), selected);
	}

	if (retailTuning.disableTacticalAI)
		return;
	const UnsignedInt routeRevision = TheAI->pathfinder()->getRetailRouteRevision();
	for (auto it = m_retailObjectives.begin(); it != m_retailObjectives.end();)
	{
		const auto [unitID, targetID, owner] = it->first;
		const Object* unit = TheGameLogic->findObjectByID(unitID);
		const Object* target = TheGameLogic->findObjectByID(targetID);
		if (!unit || unit->isEffectivelyDead() || unit->getControllingPlayer() != m_player ||
			(targetID != INVALID_ID && (!target || target->isEffectivelyDead() ||
				target->getTeam()->getID() != owner || m_player->getRelationship(target->getTeam()) != ENEMIES)))
		{
			it = m_retailObjectives.erase(it);
			continue;
		}
		auto& order = it->second;
		if (order.issuedFrame && !order.blockedRevision)
		{
			if (const auto* horde = static_cast<const HordeAIUpdate*>(unit->findUpdateModule(NAMEKEY("HordeAIUpdate"))))
				order.blockedRevision = horde->blockedPlannerRevision(order.issuedFrame);
			else if (const auto* ai = unit->getAI())
				order.blockedRevision = ai->blockedRetailPlannerRevision(order.issuedFrame);
		}
		if (order.blockedRevision && order.blockedRevision != routeRevision)
			it = m_retailObjectives.erase(it);
		else
			++it;
	}
	std::vector<std::pair<Object*, Bool>> wave;
	for (Object* unit : soldiers)
	{
		if (unit->getTeam() != m_player->getDefaultTeam() || !unit->isMobile() || !unit->isAbleToAttack())
			continue;
		const AIUpdateInterface* ai = unit->getAI();
		const auto* horde = static_cast<const HordeAIUpdate*>(unit->findUpdateModule(NAMEKEY("HordeAIUpdate")));
		const Bool ready = ai->isIdle() && (!horde || !horde->hasAttackOrder());
		Bool scouting = horde && horde->isOnPlannerAdvance() && !horde->isEngaging();
		if (!horde && ai->getCurrentStateID() == AI_ATTACK_MOVE_TO && ai->getLastCommandSource() == CMD_FROM_AI)
		{
			const auto former = m_retailObjectives.lower_bound({unit->getID(), INVALID_ID, 0});
			scouting = former != m_retailObjectives.end() && std::get<0>(former->first) == unit->getID() &&
				std::get<1>(former->first) == INVALID_ID;
		}
		if (ready || scouting)
			wave.emplace_back(unit, ready);
	}
	if (wave.size() < RETAIL_ATTACK_WAVE_MIN_UNITS)
		return;
	const auto eligible = [&](const Object& unit, const Object* objective, UnsignedInt owner, const Coord3D& point) -> Bool {
		if (!unit.getAI()->canAcceptRetailPlannerOrder(objective != nullptr) ||
			m_retailObjectives.contains({unit.getID(), objective ? objective->getID() : INVALID_ID, owner}))
			return FALSE;
		if (objective)
		{
			const CanAttackResult result = unit.getAbleToAttackSpecificObject(ATTACK_NEW_TARGET, objective, CMD_FROM_AI);
			return result == ATTACKRESULT_POSSIBLE || result == ATTACKRESULT_POSSIBLE_AFTER_MOVING;
		}
		const Coord3D delta = point - *unit.getPosition();
		return delta.x * delta.x + delta.y * delta.y > sqr(unit.getGeometryInfo().getBoundingCircleRadius());
	};
	const auto objectiveDistance = [&](const Object* objective, UnsignedInt owner, const Coord3D& point, Real& result) {
		Int count = 0;
		Real total = 0.0f;
		for (const auto& [unit, ready] : wave)
		{
			if ((!ready && !objective) || !eligible(*unit, objective, owner, point))
				continue;
			const Coord3D delta = point - *unit->getPosition();
			total += delta.x * delta.x + delta.y * delta.y;
			++count;
		}
		if (count < RETAIL_ATTACK_WAVE_MIN_UNITS)
			return FALSE;
		result = total / INT_TO_REAL(count);
		return TRUE;
	};
	Object* target = nullptr;
	ObjectShroudStatus targetShroud = OBJECTSHROUD_INVALID;
	UnsignedInt objectiveOwner = 0;
	Real distance = 0.0f;
	for (Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
	{
		const Player* owner = object->getControllingPlayer();
		if (!owner || !owner->isPlayableSide() || !owner->isPlayerActive() || !object->isKindOf(KINDOF_STRUCTURE) ||
			!object->isKindOf(KINDOF_IMMOBILE) ||
			object->isEffectivelyDead() || object->isKindOf(KINDOF_INERT) || object->isKindOf(KINDOF_UNATTACKABLE) ||
			object->isKindOf(KINDOF_BASE_FOUNDATION) ||
			m_player->getRelationship(object->getTeam()) != ENEMIES)
			continue;
		const ObjectShroudStatus shroud = object->getShroudedStatus(playerIndex);
		if (shroud != OBJECTSHROUD_CLEAR && shroud != OBJECTSHROUD_PARTIAL_CLEAR)
			continue;
		if (object->testStatus(OBJECT_STATUS_STEALTHED) && !object->testStatus(OBJECT_STATUS_DETECTED))
			continue;
		Real candidateDistance;
		const UnsignedInt team = object->getTeam()->getID();
		if (!objectiveDistance(object, team, *object->getPosition(), candidateDistance))
			continue;
		if (object->isKindOf(KINDOF_COMMANDCENTER) || object->isKindOf(KINDOF_FS_BASE_DEFENSE))
			candidateDistance += sqr(RETAIL_DEFENDED_TARGET_DISTANCE);
		if (!target || candidateDistance < distance || (candidateDistance == distance && object->getID() < target->getID()))
		{
			target = object;
			targetShroud = shroud;
			objectiveOwner = team;
			distance = candidateDistance;
		}
	}
	const Coord3D* destination = target ? target->getPosition() : nullptr;
	if (!destination)
	{
		for (Int i = 0; i < ThePlayerList->getPlayerCount(); ++i)
		{
			Player* opponent = ThePlayerList->getNthPlayer(i);
			if (!opponent || !opponent->isPlayableSide() || !opponent->isPlayerActive() ||
				m_player->getRelationship(opponent->getDefaultTeam()) != ENEMIES)
				continue;
			const Int index = opponent->getMpStartIndex();
			if (index < 0 || index >= MAX_PLAYER_COUNT)
				continue;
			AsciiString name;
			name.format("Player_%d_Start", index + 1);
			const Waypoint* start = TheTerrainLogic->getWaypointByName(name);
			if (!start)
				continue;
			Real candidateDistance;
			if (!objectiveDistance(nullptr, i, *start->getLocation(), candidateDistance))
				continue;
			if (!destination || candidateDistance < distance)
			{
				destination = start->getLocation();
				objectiveOwner = i;
				distance = candidateDistance;
			}
		}
	}
	if (!destination)
		return;
	Int committed = 0;
	for (const auto& [unit, ready] : wave)
	{
		if ((!ready && !target) || !eligible(*unit, target, objectiveOwner, *destination))
			continue;
		AIUpdateInterface* ai = unit->getAI();
		Bool accepted;
		if (auto* horde = static_cast<HordeAIUpdate*>(unit->findUpdateModule(NAMEKEY("HordeAIUpdate"))))
			accepted = horde->issuePlannerOrder(target, *destination);
		else if (target)
			accepted = ai->issueRetailPlannerAttack(target);
		else
			accepted = ai->issueRetailPlannerAdvance(*destination);
		if (!accepted)
			continue;
		m_retailObjectives.emplace(std::make_tuple(unit->getID(), target ? target->getID() : INVALID_ID, objectiveOwner),
			RetailObjective{frame, 0});
		++committed;
		if (trace)
			std::fprintf(stderr, "AI_TARGET\t%u\t%d\t%s\t%d\t%d\t%d\n", frame, playerIndex,
				target ? "attack" : "scout", unit->getID(), target ? target->getID() : 0, targetShroud);
	}
	if (trace && committed >= RETAIL_ATTACK_WAVE_MIN_UNITS)
	{
		std::fprintf(stderr, "AI_WAVE\t%u\t%d\t%s\t%d\t%d\t%a\t%a\n", frame, playerIndex,
			target ? "structure" : "enemy-start", committed, target ? target->getID() : 0, destination->x, destination->y);
		std::fflush(stderr);
	}
}

//----------------------------------------------------------------------------------------------------------
/**
 * Adjusts the build list to match the starting position.
 */
void AISkirmishPlayer::adjustBuildList(BuildListInfo *list)
{
	Bool foundStart = false;
	Coord3D startPos;

	// Find our command center location.
	Object *obj;
	for( obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject() )
	{

		Player *owner = obj->getControllingPlayer();
		if (owner==m_player) {
			// See if it's a command center.
			if (obj->isKindOf(KINDOF_COMMANDCENTER)) {
				foundStart = true;
				startPos = *obj->getPosition();
				m_player->onStructureUndone(obj);
				TheAI->pathfinder()->removeObjectFromPathfindMap(obj);
				TheGameLogic->destroyObject(obj);
				break;
			}
		}
	}
	if (!foundStart) {
		DEBUG_LOG(("Couldn't find starting command center for ai player."));
		return;
	}
	// Find the location of the command center in the build list.
	Bool foundInBuildList = false;
	Coord3D buildPos;
	BuildListInfo *cur = list;
	while (cur) {
		const ThingTemplate *tTemplate = TheThingFactory->findTemplate(cur->getTemplateName());
		if (tTemplate && tTemplate->isKindOf(KINDOF_COMMANDCENTER)) {
			foundInBuildList = true;
			buildPos = *cur->getLocation();
			cur->setInitiallyBuilt(true);
		}
		cur = cur->getNext();
	}
	Region3D bounds;
	TheTerrainLogic->getMaximumPathfindExtent(&bounds);
	/* calculate section of 3x3 grid:
		6 7 8
		3 4 5
		0 1 2 */

	Int gridIndex = 0;
	if (startPos.x > bounds.lo.x + bounds.width()/3) {
		gridIndex++;
	}
	if (startPos.x > bounds.lo.x + 2*bounds.width()/3) {
		gridIndex++;
	}

	if (startPos.y > bounds.lo.y + bounds.height()/3) {
		gridIndex+=3;
	}
	if (startPos.y > bounds.lo.y + 2*bounds.height()/3) {
		gridIndex+=3;
	}

	Real angle = 0;
	if (TheAI->getAiData()->m_rotateSkirmishBases) {
		switch (gridIndex) {
			case 0 : angle = 0; break;
			case 1 : angle = PI/4; break;// 45 degrees.
			case 2 : angle = PI/2; break; // 90 degrees;
			case 3 : angle = -PI/4; break; // -45 degrees.
			case 4 : angle = 0; break;
			case 5 : angle = 3*PI/4; break; // 135 degrees.
			case 6 : angle = -PI/2; break; // -90 degrees;
			case 7 : angle = -3*PI/4; break; // -135 degrees.
			case 8 : angle = PI; break; // 180 degrees.
		}
	}

	angle += 3*PI/4;

	Real s = WWMath::SinTrig(angle);
	Real c = WWMath::CosTrig(angle);

	cur = list;
	while (cur) {
		const ThingTemplate *tTemplate = TheThingFactory->findTemplate(list->getTemplateName());
		if (tTemplate && tTemplate->isKindOf(KINDOF_COMMANDCENTER)) {
			foundInBuildList = true;
			Coord3D curPos = *cur->getLocation();
			// Transform to new coords.
			curPos.x -= buildPos.x;
			curPos.y -= buildPos.y;
			Real newX = curPos.x*c - curPos.y*s;
			Real newY = curPos.y*c + curPos.x*s;
			curPos.x = newX + startPos.x;
			curPos.y = newY + startPos.y;
			cur->setLocation(curPos);
			cur->setAngle(cur->getAngle());
		}
		cur = cur->getNext();
	}

}



//----------------------------------------------------------------------------------------------------------
/**
 * Find any things that build stuff & add them to the build list.  Then build any initially built
 * buildings.
 */
void AISkirmishPlayer::newMap()
{
	m_retailObjectives.clear();

	/* Get our proper build list. */
	AsciiString mySide = m_player->getSide();
	DEBUG_LOG(("AI Player side is %s", mySide.str()));
	const AISideBuildList *build = TheAI->getAiData()->m_sideBuildLists;
	while (build) {
		if (build->m_side == mySide) {
			BuildListInfo *buildList = build->m_buildList->duplicate();
			adjustBuildList(buildList); // adjust to  our start position.
			m_player->setBuildList(buildList);
			computeCenterAndRadiusOfBase(&m_baseCenter, &m_baseRadius);
			break;
		}
		build = build->m_next;
	}
	DEBUG_ASSERTLOG(build!=nullptr, ("Couldn't find build list for skirmish player."));

	// Build any with the initially built flag.
	for( BuildListInfo *info = m_player->getBuildList(); info; info = info->getNext() )
	{
		AsciiString name = info->getTemplateName();
		if (name.isEmpty()) continue;
		const ThingTemplate *bldgPlan = TheThingFactory->findTemplate( name );
		if (!bldgPlan) {
			DEBUG_LOG(("*** ERROR - Build list building '%s' doesn't exist.", name.str()));
			continue;
		}
		if (info->isInitiallyBuilt()) {
			buildStructureNow(bldgPlan, info);
		} else {
			info->incrementNumRebuilds(); // the initial build in the normal build list consumes a rebuild, so add one.
		}
	}
}

//----------------------------------------------------------------------------------------------------------
/**
 * Queues up a dozer.
 */
void AISkirmishPlayer::queueDozer()
{
	AIPlayer::queueDozer();
}

//----------------------------------------------------------------------------------------------------------
/**
 * Finds a dozer that isn't building or collecting resources.
 */
Object * AISkirmishPlayer::findDozer( const Coord3D *pos )
{
	return AIPlayer::findDozer(pos);
}


//----------------------------------------------------------------------------------------------------------
/**
 * Find a good spot to fire a superweapon.
 */
Bool AISkirmishPlayer::computeSuperweaponTarget(const SpecialPowerTemplate *power, Coord3D *retPos, Int playerNdx, Real weaponRadius)
{

	Region2D bounds;
	getPlayerStructureBounds(&bounds, playerNdx);

	if( power->getSpecialPowerType() == SPECIAL_CLUSTER_MINES || power->getSpecialPowerType() == NUKE_SPECIAL_CLUSTER_MINES )
	{
		// hackus brutus - mine the entrances to our base.
		AsciiString pathLabel;
		Int mode = GameLogicRandomValue(0, 2);
		if (mode==1) {
				pathLabel.format("%s%d", SKIRMISH_FLANK, m_player->getMpStartIndex()+1);
		}	else if (mode==2) {
				pathLabel.format("%s%d", SKIRMISH_BACKDOOR, m_player->getMpStartIndex()+1);
		}	else {
			pathLabel.format("%s%d", SKIRMISH_CENTER, m_player->getMpStartIndex()+1);
		}

		Coord3D goalPos = m_baseCenter;
		Waypoint *way = TheTerrainLogic->getClosestWaypointOnPath( &goalPos, pathLabel );
		if (way) {
			goalPos = *way->getLocation();
		} else {
			Region2D bounds;
			getPlayerStructureBounds(&bounds, getMyEnemyPlayerIndex());
			goalPos.x = bounds.lo.x + bounds.width()/2;
			goalPos.y = bounds.lo.y + bounds.height()/2;
		}
		Coord2D offset;
		offset.x = goalPos.x-m_baseCenter.x;
		offset.y = goalPos.y-m_baseCenter.y;
		offset.normalize();
		offset.x *= m_baseRadius;
		offset.y *= m_baseRadius;
		*retPos = m_baseCenter;
		retPos->x += offset.x;
		retPos->y += offset.y;
		retPos->z = TheTerrainLogic->getGroundHeight(retPos->x, retPos->y);
		return TRUE;
	}

	return AIPlayer::computeSuperweaponTarget(power, retPos, playerNdx, weaponRadius);

}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void AISkirmishPlayer::crc( Xfer *xfer )
{
	if (RetailFileLayers::enabled())
		xferRetailObjectives(xfer, TRUE);
}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info;
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void AISkirmishPlayer::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = RetailFileLayers::enabled() ? 3 : 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// xfer base class info
	AIPlayer::xfer( xfer );

	// front base defense
	xfer->xferInt( &m_curFrontBaseDefense );

	// flank base defense
	xfer->xferInt( &m_curFlankBaseDefense );

	// front left defense angle
	xfer->xferReal( &m_curFrontLeftDefenseAngle );

	// front right defense angle
	xfer->xferReal( &m_curFrontRightDefenseAngle );

	// left flank left defense angle
	xfer->xferReal( &m_curLeftFlankLeftDefenseAngle );

	// left flank right defense angle
	xfer->xferReal( &m_curLeftFlankRightDefenseAngle );

	// right flank left defense angle
	xfer->xferReal( &m_curRightFlankLeftDefenseAngle );

	// right flank right defense angle
	xfer->xferReal( &m_curRightFlankRightDefenseAngle );

	if (version >= 2)
		xferRetailObjectives(xfer, version >= 3);
}

void AISkirmishPlayer::xferRetailObjectives(Xfer* xfer, Bool hasRouteHistory)
{
	UnsignedInt count = static_cast<UnsignedInt>(m_retailObjectives.size());
	xfer->xferUnsignedInt(&count);
	if (xfer->getXferMode() == XFER_LOAD)
	{
		m_retailObjectives.clear();
		for (UnsignedInt i = 0; i < count; ++i)
		{
			ObjectID unit = INVALID_ID;
			ObjectID target = INVALID_ID;
			UnsignedInt owner = 0;
			xfer->xferObjectID(&unit);
			xfer->xferObjectID(&target);
			xfer->xferUnsignedInt(&owner);
			if (!hasRouteHistory)
				continue;
			RetailObjective order;
			xfer->xferUnsignedInt(&order.issuedFrame);
			xfer->xferUnsignedInt(&order.blockedRevision);
			m_retailObjectives.emplace(std::make_tuple(unit, target, owner), order);
		}
	}
	else
		for (const auto& [key, history] : m_retailObjectives)
		{
			auto [unit, target, owner] = key;
			auto order = history;
			xfer->xferObjectID(&unit);
			xfer->xferObjectID(&target);
			xfer->xferUnsignedInt(&owner);
			xfer->xferUnsignedInt(&order.issuedFrame);
			xfer->xferUnsignedInt(&order.blockedRevision);
		}
}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void AISkirmishPlayer::loadPostProcess()
{

}

