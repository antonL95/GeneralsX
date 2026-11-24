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


#include "Common/ActionManager.h"
#include "Common/GameMemory.h"
#include "Common/GlobalData.h"
#include "Common/INI.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/RandomValue.h"
#include "Common/RetailAIKindOf.h"
#include "Common/RetailFileLayers.h"
#include "Common/RetailINIStartup.h"
#include "Common/RetailSaveBlocks.h"
#include "Common/Team.h"
#include "Common/ThingFactory.h"
#include "Common/BuildAssistant.h"
#include "Common/SpecialPower.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "Common/WellKnownKeys.h"
#include "Common/Xfer.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/LogicRandomValue.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/Object.h"
#include "GameLogic/AISkirmishPlayer.h"
#include "GameLogic/RetailSkirmishSpending.h"
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
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Module/RebuildHoleBehavior.h"
#include "GameLogic/Module/ReplaceSelfUpgrade.h"
#include "GameLogic/Module/SlaughterHordeContain.h"
#include "GameLogic/Module/SpecialAbilityUpdate.h"
#include "GameLogic/Module/UpdateModule.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Module/ProductionUpdate.h"
#include "GameClient/TerrainVisual.h"
#include "GameClient/ControlBar.h"

#include <cmath>
#include <limits>

namespace
{
struct RetailArmyMember
{
	AsciiString type;
	Real share[3] = {};
};

constexpr Int RETAIL_ATTACK_WAVE_MIN_UNITS = 3;
constexpr Real RETAIL_DEFENDED_TARGET_DISTANCE = 350.0f;

enum RetailTacticalKind
{
	RETAIL_TARGET_ENEMY_STRUCTURE,
	RETAIL_TARGET_DEFENSIVE,
	RETAIL_TARGET_OPPORTUNITY,
	RETAIL_TARGET_EXPANSION,
	RETAIL_TARGET_TARGETLESS
};

const char* const retailTacticalKindNames[] = {
	"ENEMY_STRUCTURE", "DEFENSIVE", "OPPORTUNITY", "EXPANSION", "TARGETLESS", nullptr
};

Bool screensRetailOpportunity(const Object& object)
{
	if (!object.isKindOf(KINDOF_STRUCTURE) || !object.isKindOf(KINDOF_IMMOBILE) ||
		object.isEffectivelyDead() || object.getContainedBy() || object.isKindOf(KINDOF_INERT))
		return FALSE;
	if (object.isKindOf(KINDOF_DOZER_FACTORY))
		return TRUE;
	return !object.isKindOf(KINDOF_LINKED_TO_FLAG) && !object.isKindOf(KINDOF_WALK_ON_TOP_OF_WALL) &&
		!object.isKindOf(KINDOF_DEFENSIVE_WALL) && !object.isKindOf(KINDOF_BASE_FOUNDATION) &&
		!object.isKindOf(KINDOF_IGNORE_FOR_VICTORY) && !object.isKindOf(KINDOF_WALL_UPGRADE) &&
		!object.isKindOf(KINDOF_WALL_HUB) && !object.isKindOf(KINDOF_WALL_SEGMENT) &&
		!object.isKindOf(KINDOF_WALL_GATE);
}

struct RetailPriorityRange
{
	Real low;
	Real high;
};

Bool usesRetailUpgradePriority(const UpgradeTemplate& upgrade, Bool research)
{
	const auto heuristic = upgrade.getSkirmishAIHeuristic();
	return heuristic == SkirmishUpgradeHeuristic::Normal ||
		(research && heuristic == SkirmishUpgradeHeuristic::Important);
}

Real rollRetailUpgradePriority(const RetailPriorityRange& range, Bool research)
{
	const auto [low, high] = std::minmax(range.low, range.high);
	if (!std::isfinite(low) || !std::isfinite(high))
		return 0.0f;
	if (research)
		return GameLogicRandomValueReal(low, high);
	const Int maxBound = std::numeric_limits<Int>::max() / 2;
	return GameLogicRandomValue(static_cast<Int>(std::clamp<double>(low, -maxBound, maxBound)),
		static_cast<Int>(std::clamp<double>(high, -maxBound, maxBound)));
}

struct RetailArmy
{
	AsciiString name;
	AsciiString side;
	AsciiString farm;
	AsciiString wallNode;
	std::vector<RetailArmyMember> members;
	std::vector<AsciiString> heroes;
	std::vector<AsciiString> scavengedBuildings;
	std::vector<AsciiString> offensiveBuildings;
	std::vector<Int> tacticalTargets;
	std::vector<Int> maxTeamsPerTarget;
	Real targetLifetimeSeconds = 90.0f;
	Real targetRetentionChance = 0.5f;
	Real defenseRecruitAllChance[3] = {0.0f, 0.0f, 0.5f};
	Int smallDefenseBuildings = 1;
	Int mediumDefenseBuildings = 4;
	Int maxRecruitsPerDefenseType = 0;
	Int maxOpportunityThreat = 0;
	Real rushSeconds = 120.0f;
	Real midgameSeconds = 420.0f;
	Int minimumFarms = 2;
	Int minimumFarmMoney = 300;
	Int perFarmValue = 70;
	Real farmPriorityPerSecond = 3.0f;
	Real rushFarmInterval = 20.0f;
	Real percentToSave[3] = {};
	Real fortressRebuildPriority = 1950.0f;
	Real structureRebuildModifier = 0.5f;
	Real defaultPriority = 100.0f;
	Real unitUpgradeChance = 0.3f;
	RetailPriorityRange unitUpgradePriority = {100.0f, 200.0f};
	RetailPriorityRange normalResearchPriority = {85.0f, 150.0f};
	RetailPriorityRange importantResearchPriority = {200.0f, 250.0f};
	Real lowUnitPriority[3] = {};
	Real mustUseCommandPoints[3] = {};
	Int attackRecruitDepth = 0;
	Int defenseRecruitDepth = 0;
	Int exploreRecruitDepth = 0;
};

struct RetailDifficulty
{
	AsciiString definition;
	AsciiString name;
	Int farmLimit = -1;
	Int farmChance[2] = {1, 1};
	Int offensiveTacticChance[2] = {1, 1};
	Int specialPowerChance[2] = {1, 1};
};

struct RetailBrutalCheats
{
	Real costReduction = 0.0f;
	Real timeReduction = 0.1f;
};

struct RetailCombatChain
{
	Int unit = -1;
	std::vector<Int> targets;
	std::vector<Real> modifiers;
};

struct RetailAITuning
{
	std::vector<RetailDifficulty> difficulties;
	std::array<RetailCombatChain, std::size(RetailAIKindOf::names) - 1> combatChains;
	RetailBrutalCheats brutal;
	Bool disableBaseBuilding = FALSE;
	Bool disableEconomyBuilding = FALSE;
	Bool disableUnitBuilding = FALSE;
	Bool disableUnitUpgrading = FALSE;
	Bool disableScienceUpgrading = FALSE;
	Bool disableTacticalAI = FALSE;
	Bool disableWallBuilding = FALSE;
	Real defenseTreeNodeRadius = 300.0f;
};

std::vector<RetailArmy> retailArmies;
RetailAITuning retailTuning;
std::vector<RetailArmy> baseRetailArmies;
RetailAITuning baseRetailTuning;

struct RetailSkirmishPolicy
{
	const RetailArmy* army = nullptr;
	const RetailDifficulty* difficulty = nullptr;
};

RetailSkirmishPolicy retailPolicy(const Player& player)
{
	RetailSkirmishPolicy policy;
	for (const RetailArmy& army : retailArmies)
	{
		if (army.side.compareNoCase(player.getSide()) == 0)
		{
			policy.army = &army;
			break;
		}
	}
	const GameDifficulty level = player.getPlayerDifficulty();
	const char* const names[] = {"EASY", "NORMAL", "HARD", "BRUTAL"};
	if (level >= DIFFICULTY_EASY && level <= DIFFICULTY_BRUTAL)
	{
		for (const RetailDifficulty& difficulty : retailTuning.difficulties)
			if (difficulty.name.compareNoCase(names[level]) == 0)
				policy.difficulty = &difficulty;
	}
	return policy;
}

void parseCombatChainTargets(INI* ini, void*, void* store, const void*)
{
	auto& targets = *static_cast<std::vector<Int>*>(store);
	for (const char* token = ini->getNextTokenOrNull(); token; token = ini->getNextTokenOrNull())
		targets.push_back(INI::scanIndexList(token, RetailAIKindOf::names));
}

void parseCombatChainModifiers(INI* ini, void*, void* store, const void*)
{
	auto& modifiers = *static_cast<std::vector<Real>*>(store);
	for (const char* token = ini->getNextTokenOrNull(); token; token = ini->getNextTokenOrNull())
		modifiers.push_back(INI::scanReal(token));
}

void parseCombatChain(INI* ini, void* instance, void*, const void*)
{
	ini->getNextToken();
	RetailCombatChain chain;
	static const FieldParse fields[] = {
		{ "Unit", INI::parseIndexList, RetailAIKindOf::names, offsetof(RetailCombatChain, unit) },
		{ "TargetTypes", parseCombatChainTargets, nullptr, offsetof(RetailCombatChain, targets) },
		{ "TargetPriorityModifiers", parseCombatChainModifiers, nullptr, offsetof(RetailCombatChain, modifiers) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	ini->initFromINI(&chain, fields);
	auto& chains = static_cast<RetailAITuning*>(instance)->combatChains;
	if (chain.unit >= 0 && static_cast<size_t>(chain.unit) < chains.size())
		chains[chain.unit] = std::move(chain);
}

Int combatKind(const ThingTemplate& type)
{
	const auto& threats = type.getRotWKObjectData().threatBreakdowns;
	return threats.empty() ? -1 : threats.back().aiKindOf;
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

void parseTacticalTargetKinds(INI* ini, void*, void* store, const void*)
{
	std::vector<Int> targets;
	for (const char* token = ini->getNextTokenOrNull(); token; token = ini->getNextTokenOrNull())
		targets.push_back(INI::scanIndexList(token, retailTacticalKindNames));
	*static_cast<std::vector<Int>*>(store) = std::move(targets);
}

void parseTacticalTeamLimits(INI* ini, void*, void* store, const void*)
{
	std::vector<Int> limits;
	for (const char* token = ini->getNextTokenOrNull(); token; token = ini->getNextTokenOrNull())
		limits.push_back(INI::scanInt(token));
	*static_cast<std::vector<Int>*>(store) = std::move(limits);
}

void parseRetailTemplateAssignment(INI* ini, void*, void* store, const void*)
{
	ini->getNextToken();
	static const FieldParse fields[] = {
		{ "TemplateName", INI::parseAsciiString, nullptr, 0 },
		{ nullptr, nullptr, nullptr, 0 }
	};
	ini->initFromINI(store, fields);
}

void parseDifficultyProbability(INI* ini, void*, void* store, const void*)
{
	const Int numerator = INI::scanInt(ini->getNextToken(ini->getSepsColon()));
	const Int denominator = INI::scanInt(ini->getNextToken(ini->getSepsColon()));
	if (denominator < 1)
		throw INI_INVALID_DATA;
	auto* chance = static_cast<Int*>(store);
	chance[0] = numerator;
	chance[1] = denominator;
}

void parseDifficultyTuning(INI* ini, void* instance, void*, const void*)
{
	const AsciiString definition = ini->getNextToken();
	auto& difficulties = static_cast<RetailAITuning*>(instance)->difficulties;
	const auto existing = std::find_if(difficulties.begin(), difficulties.end(), [&](const RetailDifficulty& current) {
		return current.definition.compareNoCase(definition) == 0;
	});
	RetailDifficulty difficulty = existing == difficulties.end() ? RetailDifficulty{} : *existing;
	difficulty.definition = definition;
	static const FieldParse fields[] = {
		{ "Difficulty", INI::parseAsciiString, nullptr, offsetof(RetailDifficulty, name) },
		{ "EconomyMaxFarms", INI::parseInt, nullptr, offsetof(RetailDifficulty, farmLimit) },
		{ "EconomyUpgradeProbability", parseDifficultyProbability, nullptr, offsetof(RetailDifficulty, farmChance) },
		{ "OffensiveTacticActivationProbability", parseDifficultyProbability, nullptr, offsetof(RetailDifficulty, offensiveTacticChance) },
		{ "SpecialPowerActivationProbability", parseDifficultyProbability, nullptr, offsetof(RetailDifficulty, specialPowerChance) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	ini->initFromINI(&difficulty, fields);
	if (existing != difficulties.end())
		*existing = std::move(difficulty);
	else if (difficulty.name.isNotEmpty())
		difficulties.push_back(std::move(difficulty));
}

void parseBrutalCheats(INI* ini, void*, void* store, const void*)
{
	ini->getNextToken();
	static const FieldParse fields[] = {
		{ "BuildCostReduction", INI::parsePercentToReal, nullptr, offsetof(RetailBrutalCheats, costReduction) },
		{ "BuildTimeReduction", INI::parsePercentToReal, nullptr, offsetof(RetailBrutalCheats, timeReduction) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	ini->initFromINI(store, fields);
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

Real tacticalDistanceSquared(const Coord3D& a, const Coord3D& b)
{
	const Coord3D delta = a - b;
	return sqr(delta.x) + sqr(delta.y) + sqr(delta.z);
}

const AttachUpdate* retailRingAttachment(const Object& object)
{
	return object.isKindOf(KINDOF_ONE_RING)
		? static_cast<const AttachUpdate*>(object.findUpdateModule(NAMEKEY("AttachUpdate"))) : nullptr;
}

Bool canCollectRetailRing(const Object& unit, const AttachUpdate& ring)
{
	if (ring.canAttachTo(unit))
		return TRUE;
	const auto* horde = HordeContain::get(&unit);
	return horde && std::any_of(horde->getContainedItemsList()->begin(), horde->getContainedItemsList()->end(),
		[&](const Object* member) { return ring.canAttachTo(*member); });
}

Object* findRetailRingCitadel(Object& courier, const std::vector<Object*>& owned)
{
	std::vector<Object*> destinations;
	for (Object* building : owned)
	{
		const auto* citadel = dynamic_cast<const CitadelSlaughterHordeContain*>(building->getContain());
		if (citadel && citadel->canAcceptRingCarrier(&courier) &&
			TheActionManager->canEnterObject(&courier, building, CMD_FROM_AI, CHECK_CAPACITY))
			destinations.push_back(building);
	}
	if (destinations.size() < 2)
		return destinations.empty() ? nullptr : destinations.front();

	const auto rank = [&](const Object* object) {
		return std::make_pair(tacticalDistanceSquared(*object->getPosition(), *courier.getPosition()), object->getID());
	};
	std::sort(destinations.begin(), destinations.end(), [&](const Object* a, const Object* b) { return rank(a) < rank(b); });
	const auto reachable = std::find_if(destinations.begin(), destinations.end(), [&](const Object* destination) {
		return TheAI->pathfinder()->slowDoesPathExist(&courier, courier.getPosition(), destination->getPosition(), destination->getID());
	});
	return reachable != destinations.end() ? *reachable : destinations.front();
}

Bool isRetailDefenseRecruit(const Object& object)
{
	return !object.isKindOf(KINDOF_UNATTACKABLE) && !object.isKindOf(KINDOF_STRUCTURE) &&
		(object.isKindOf(KINDOF_SHIP) || (!object.isKindOf(KINDOF_IMMOBILE) &&
			(object.isKindOf(KINDOF_CAN_ATTACK) || object.isKindOf(KINDOF_HERO) || object.isKindOf(KINDOF_SUPPORT))));
}

Bool updateRetailFlagCapture(const std::vector<Object*>& owned, UnsignedInt frame, Int playerIndex, Bool trace)
{
	Object* capturer = nullptr;
	Object* flag = nullptr;
	const CommandButton* capture = nullptr;
	Real closest = 0.0f;
	for (Object* unit : owned)
	{
		const auto* ability = unit->findSpecialAbilityUpdate(SPECIAL_INFANTRY_CAPTURE_BUILDING);
		if (!ability)
			continue;
		if (ability->isActive())
			return TRUE;
		if (!unit->getAI() || !unit->getAI()->isIdle() || !unit->isMobile() || unit->isDisabled() || unit->getContainedBy())
			continue;
		const CommandSet* commands = retailCommands(*unit);
		if (!commands)
			continue;
		for (Int slot = 0; slot < CommandButtonRules::RETAIL_COMMANDS_PER_SET; ++slot)
		{
			const CommandButton* button = commands->getCommandButton(slot);
			const SpecialPowerTemplate* power = button ? button->getSpecialPowerTemplate() : nullptr;
			if (!power || button->getCommandType() != GUI_COMMAND_SPECIAL_POWER ||
				power->getSpecialPowerType() != SPECIAL_INFANTRY_CAPTURE_BUILDING ||
				button->getRequiredRank() > unit->getExperienceTracker()->getRank() ||
				!HeroRoster::buttonUpgradesMet(button, unit->getControllingPlayer(), unit))
				continue;
			for (Object* target = TheGameLogic->getFirstObject(); target; target = target->getNextObject())
			{
				if (!target->isKindOf(KINDOF_CAPTUREFLAG) || target->isDestroyed() || unit->getRelationship(target) == ALLIES)
					continue;
				const Coord3D delta = *target->getPosition() - *unit->getPosition();
				const Real distance = sqr(delta.x) + sqr(delta.y);
				if (flag && std::make_tuple(distance, target->getID(), unit->getID()) >=
					std::make_tuple(closest, flag->getID(), capturer->getID()))
					continue;
				if (!TheActionManager->canDoSpecialPowerAtObject(unit, target, CMD_FROM_AI, power, button->getOptions()))
					continue;
				capturer = unit;
				flag = target;
				capture = button;
				closest = distance;
			}
		}
	}
	if (!capturer)
		return FALSE;
	capturer->doCommandButtonAtObject(capture, flag, CMD_FROM_AI);
	const auto* ability = capturer->findSpecialAbilityUpdate(SPECIAL_INFANTRY_CAPTURE_BUILDING);
	const Bool active = ability && ability->isActive();
	if (active && trace)
		std::fprintf(stderr, "AI_FLAG_CAPTURE\t%u\t%d\t%d\t%d\n", frame, playerIndex, capturer->getID(), flag->getID());
	return active;
}

struct RetailDefenseCluster
{
	Coord3D center{};
	Real radius = 0.0f;
	std::vector<Object*> buildings;

	void add(Object& building, Real nodeRadius)
	{
		const Coord3D& point = *building.getPosition();
		if (buildings.empty())
		{
			center = point;
			radius = nodeRadius;
		}
		else
		{
			const Real distance = WWMath::SqrtfOrigin(tacticalDistanceSquared(point, center));
			if (distance > radius - nodeRadius)
			{
				const Real nextRadius = (radius + distance + nodeRadius) * 0.5f;
				const Real fraction = (nextRadius - radius) / distance;
				center.x += (point.x - center.x) * fraction;
				center.y += (point.y - center.y) * fraction;
				center.z += (point.z - center.z) * fraction;
				radius = nextRadius;
			}
		}
		buildings.push_back(&building);
	}
};

std::vector<RetailDefenseCluster> makeDefenseClusters(std::vector<Object*> structures, Real nodeRadius)
{
	std::sort(structures.begin(), structures.end(), [](const Object* a, const Object* b) {
		return a->getID() < b->getID();
	});
	std::vector<RetailDefenseCluster> clusters;
	const Real joinDistanceSquared = 2.0f * sqr(nodeRadius);
	for (Object* building : structures)
	{
		const auto connected = [&](const RetailDefenseCluster& cluster) {
			return std::any_of(cluster.buildings.begin(), cluster.buildings.end(), [&](const Object* member) {
				return tacticalDistanceSquared(*building->getPosition(), *member->getPosition()) < joinDistanceSquared;
			});
		};
		const auto first = std::find_if(clusters.begin(), clusters.end(), connected);
		if (first == clusters.end())
		{
			clusters.emplace_back().add(*building, nodeRadius);
			continue;
		}
		first->add(*building, nodeRadius);
		for (auto it = first + 1; it != clusters.end();)
		{
			if (!connected(*it))
			{
				++it;
				continue;
			}
			for (Object* member : it->buildings)
				first->add(*member, nodeRadius);
			it = clusters.erase(it);
		}
	}
	return clusters;
}

Bool isRetailFortressKeep(const ThingTemplate& type)
{
	return type.isKindOf(KINDOF_COMMANDCENTER) && type.isKindOf(KINDOF_CASTLE_KEEP);
}

Bool isRetailFortress(const ThingTemplate& type)
{
	if (isRetailFortressKeep(type))
		return TRUE;
	if (!type.isKindOf(KINDOF_BASE_SITE))
		return FALSE;
	const ModuleInfo& modules = type.getBehaviorModuleInfo();
	for (Int i = 0; i < modules.getCount(); ++i)
		if (modules.getNthName(i) == "CastleBehavior")
			return TRUE;
	return FALSE;
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

Object* constructRetailBuildingAt(Object& builder, const ThingTemplate& type, Coord3D point)
{
	Player* player = builder.getControllingPlayer();
	const Real angle = type.getPlacementViewAngle();
	const UnsignedInt flags = BuildAssistant::TERRAIN_RESTRICTIONS | BuildAssistant::NO_OBJECT_OVERLAP |
		BuildAssistant::CLEAR_PATH | BuildAssistant::USE_QUICK_PATHFIND | BuildAssistant::SUPPRESS_BUILD_BIBS;
	if (TheBuildAssistant->isLocationLegalToBuild(&point, &type, angle, flags, &builder, player) != LBC_OK)
		return nullptr;
	point.z = TheTerrainLogic->getGroundHeight(point.x, point.y);
	return builder.getAI()->getDozerAIInterface()->construct(&type, &point, angle, player, FALSE);
}

Bool placeRetailBuilding(Object& builder, const ThingTemplate& type, const Coord3D& center)
{
	static const Coord2D directions[] = {
		{1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1}
	};
	for (Real distance = 100.0f; distance <= 700.0f; distance += 25.0f)
	{
		for (const Coord2D& direction : directions)
		{
			const Coord3D point = {center.x + direction.x * distance, center.y + direction.y * distance, 0.0f};
			if (constructRetailBuildingAt(builder, type, point))
				return TRUE;
		}
	}
	return FALSE;
}

Coord2D retailWallFacing(const Player& player, const Coord3D& center)
{
	const Object* nearest = nullptr;
	Real distance = 0.0f;
	for (const Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
	{
		const Player* owner = object->getControllingPlayer();
		if (!owner || !owner->isPlayableSide() || !owner->isPlayerActive() || object->isEffectivelyDead() ||
			!isRetailFortressKeep(*object->getTemplate()) || player.getRelationship(object->getTeam()) != ENEMIES)
			continue;
		const Real candidate = tacticalDistanceSquared(*object->getPosition(), center);
		if (!nearest || std::make_pair(candidate, object->getID()) < std::make_pair(distance, nearest->getID()))
		{
			nearest = object;
			distance = candidate;
		}
	}
	if (!nearest)
		return {};
	const Coord3D delta = *nearest->getPosition() - center;
	const Real length = WWMath::SqrtfOrigin(sqr(delta.x) + sqr(delta.y));
	return length > 0.0f ? Coord2D{delta.x / length, delta.y / length} : Coord2D{};
}

Object* placeRetailWallNode(Object& builder, const ThingTemplate& type, const Coord3D& center, const Coord2D& forward, size_t node)
{
	static const Real diagonal = WWMath::SqrtfOrigin(0.5f);
	static const Coord2D angles[] = {
		{1, 0}, {diagonal, -diagonal}, {diagonal, diagonal}, {0, -1},
		{0, 1}, {-diagonal, -diagonal}, {-diagonal, diagonal}, {-1, 0}
	};
	const Coord2D direction{
		forward.x * angles[node].x - forward.y * angles[node].y,
		forward.x * angles[node].y + forward.y * angles[node].x
	};
	for (Real distance = 400.0f; distance <= 660.0f; distance += 20.0f)
	{
		const Coord3D point{center.x + direction.x * distance, center.y + direction.y * distance, 0.0f};
		if (Object* hub = constructRetailBuildingAt(builder, type, point))
			return hub;
	}
	return nullptr;
}
}

void AISkirmishPlayer::loadRetailData()
{
	retailArmies.clear();
	retailTuning = {};
	if (!RetailINIStartup::loadSubsystem("TheBaseTemplateLibrary"))
	{
		INI ini;
		ini.load("Data\\INI\\Default\\SkirmishAIData.ini", INI_LOAD_OVERWRITE, nullptr);
	}
	baseRetailArmies = retailArmies;
	baseRetailTuning = retailTuning;
}

void RetailSkirmishSpending::resetMapPolicies()
{
	retailArmies = baseRetailArmies;
	retailTuning = baseRetailTuning;
}

Bool AISkirmishPlayer::rollRetailSpecialPowerActivation(const Player& player)
{
	if (!RetailFileLayers::enabled())
		return TRUE;
	const RetailDifficulty* difficulty = retailPolicy(player).difficulty;
	if (!difficulty)
		return TRUE;
	const Int* chance = difficulty->specialPowerChance;
	return chance[0] >= chance[1] || GameLogicRandomValue(0, chance[1] - 1) < chance[0];
}

void AISkirmishPlayer::parseRetailArmy(INI* ini)
{
	const AsciiString name = ini->getNextToken();
	const auto existing = std::find_if(retailArmies.begin(), retailArmies.end(), [&](const RetailArmy& army) {
		return army.name.compareNoCase(name) == 0;
	});
	RetailArmy army = existing == retailArmies.end() ? RetailArmy{} : *existing;
	army.name = name;
	static const FieldParse fields[] = {
		{ "Side", INI::parseAsciiString, nullptr, offsetof(RetailArmy, side) },
		{ "AIEconomyAssigment", parseRetailTemplateAssignment, nullptr, offsetof(RetailArmy, farm) },
		{ "AIWallNodeAssignment", parseRetailTemplateAssignment, nullptr, offsetof(RetailArmy, wallNode) },
		{ "ArmyMemberDefinition", parseArmyMember, nullptr, 0 },
		{ "HeroBuildOrder", INI::parseAsciiStringVector, nullptr, offsetof(RetailArmy, heroes) },
		{ "PhaseDuration_Rush", INI::parseReal, nullptr, offsetof(RetailArmy, rushSeconds) },
		{ "PhaseDuration_MidGame", INI::parseReal, nullptr, offsetof(RetailArmy, midgameSeconds) },
		{ "EconomyBuilderMinFarmsOwned", INI::parseInt, nullptr, offsetof(RetailArmy, minimumFarms) },
		{ "EconomyBuilderMinMoney", INI::parseInt, nullptr, offsetof(RetailArmy, minimumFarmMoney) },
		{ "EconomyBuilderPerFarmValue", INI::parseInt, nullptr, offsetof(RetailArmy, perFarmValue) },
		{ "EconomyBuilderPerSecPriorityIncreaseBase", INI::parseReal, nullptr, offsetof(RetailArmy, farmPriorityPerSecond) },
		{ "EconomyBuilderMinTimeBetweenFarms_Rush", INI::parseReal, nullptr, offsetof(RetailArmy, rushFarmInterval) },
		{ "PercentToSave_Rush", INI::parsePercentToReal, nullptr, offsetof(RetailArmy, percentToSave) },
		{ "PercentToSave_MidGame", INI::parsePercentToReal, nullptr, offsetof(RetailArmy, percentToSave) + sizeof(Real) },
		{ "PercentToSave_EndGame", INI::parsePercentToReal, nullptr, offsetof(RetailArmy, percentToSave) + 2 * sizeof(Real) },
		{ "FortressRebuildPriority", INI::parseReal, nullptr, offsetof(RetailArmy, fortressRebuildPriority) },
		{ "StructureRebuildPriorityModifier", INI::parsePercentToReal, nullptr, offsetof(RetailArmy, structureRebuildModifier) },
		{ "DefaultUnitPriority", INI::parseReal, nullptr, offsetof(RetailArmy, defaultPriority) },
		{ "ChanceForUnitsToUpgrade", INI::parsePercentToReal, nullptr, offsetof(RetailArmy, unitUpgradeChance) },
		{ "UnitUpgradePriorityLow", INI::parseReal, nullptr, offsetof(RetailArmy, unitUpgradePriority) + offsetof(RetailPriorityRange, low) },
		{ "UnitUpgradePriorityHigh", INI::parseReal, nullptr, offsetof(RetailArmy, unitUpgradePriority) + offsetof(RetailPriorityRange, high) },
		{ "UpgradeSciencePriorityNormalLow", INI::parseReal, nullptr, offsetof(RetailArmy, normalResearchPriority) + offsetof(RetailPriorityRange, low) },
		{ "UpgradeSciencePriorityNormalHigh", INI::parseReal, nullptr, offsetof(RetailArmy, normalResearchPriority) + offsetof(RetailPriorityRange, high) },
		{ "UpgradeSciencePriorityImportantLow", INI::parseReal, nullptr, offsetof(RetailArmy, importantResearchPriority) + offsetof(RetailPriorityRange, low) },
		{ "UpgradeSciencePriorityImportantHigh", INI::parseReal, nullptr, offsetof(RetailArmy, importantResearchPriority) + offsetof(RetailPriorityRange, high) },
		{ "LowUnitPriorityModifier_Rush", INI::parseReal, nullptr, offsetof(RetailArmy, lowUnitPriority) },
		{ "LowUnitPriorityModifier_MidGame", INI::parseReal, nullptr, offsetof(RetailArmy, lowUnitPriority) + sizeof(Real) },
		{ "LowUnitPriorityModifier_EndGame", INI::parseReal, nullptr, offsetof(RetailArmy, lowUnitPriority) + 2 * sizeof(Real) },
		{ "MustUseCommandPointPercentage_Phase1", INI::parsePercentToReal, nullptr, offsetof(RetailArmy, mustUseCommandPoints) },
		{ "MustUseCommandPointPercentage_Phase2", INI::parsePercentToReal, nullptr, offsetof(RetailArmy, mustUseCommandPoints) + sizeof(Real) },
		{ "MustUseCommandPointPercentage_Phase3", INI::parsePercentToReal, nullptr, offsetof(RetailArmy, mustUseCommandPoints) + 2 * sizeof(Real) },
		{ "CombatChainSearchDepthForTeamRecruits_AttackTeams", INI::parseInt, nullptr, offsetof(RetailArmy, attackRecruitDepth) },
		{ "CombatChainSearchDepthForTeamRecruits_DefenseTeams", INI::parseInt, nullptr, offsetof(RetailArmy, defenseRecruitDepth) },
		{ "CombatChainSearchDepthForTeamRecruits_ExploreTeams", INI::parseInt, nullptr, offsetof(RetailArmy, exploreRecruitDepth) },
		{ "OffensiveBuildings", INI::parseAsciiStringVector, nullptr, offsetof(RetailArmy, offensiveBuildings) },
		{ "ScavangedResourceBuildings", INI::parseAsciiStringVector, nullptr, offsetof(RetailArmy, scavengedBuildings) },
		{ "TacticalAITargets", parseTacticalTargetKinds, nullptr, offsetof(RetailArmy, tacticalTargets) },
		{ "MaxTeamsPerTarget", parseTacticalTeamLimits, nullptr, offsetof(RetailArmy, maxTeamsPerTarget) },
		{ "SecondsTillTargetsCanExpire", INI::parseReal, nullptr, offsetof(RetailArmy, targetLifetimeSeconds) },
		{ "ChanceForTargetToExpire", INI::parsePercentToReal, nullptr, offsetof(RetailArmy, targetRetentionChance) },
		{ "MaxBuildingsToBeDefensiveTarget_Small", INI::parseInt, nullptr, offsetof(RetailArmy, smallDefenseBuildings) },
		{ "MaxBuildingsToBeDefensiveTarget_Med", INI::parseInt, nullptr, offsetof(RetailArmy, mediumDefenseBuildings) },
		{ "ChanceToUseAllUnitsForDefenseTarget_Small", INI::parsePercentToReal, nullptr, offsetof(RetailArmy, defenseRecruitAllChance) },
		{ "ChanceToUseAllUnitsForDefenseTarget_Med", INI::parsePercentToReal, nullptr, offsetof(RetailArmy, defenseRecruitAllChance) + sizeof(Real) },
		{ "ChanceToUseAllUnitsForDefenseTarget_Large", INI::parsePercentToReal, nullptr, offsetof(RetailArmy, defenseRecruitAllChance) + 2 * sizeof(Real) },
		{ "ValueToSetForMaxOnDefenseTeam", INI::parseInt, nullptr, offsetof(RetailArmy, maxRecruitsPerDefenseType) },
		{ "MaxThreatForOpportunityTargets", INI::parseInt, nullptr, offsetof(RetailArmy, maxOpportunityThreat) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	ini->initFromINI(&army, fields);
	if (existing != retailArmies.end())
		*existing = std::move(army);
	else if (army.side.isNotEmpty())
		retailArmies.push_back(std::move(army));
}

void AISkirmishPlayer::parseRetailTuning(INI* ini)
{
	ini->getNextToken();
	static const FieldParse fields[] = {
		{ "DifficultyTuning", parseDifficultyTuning, nullptr, 0 },
		{ "CombatChainDefinition", parseCombatChain, nullptr, 0 },
		{ "BrutalDifficultyCheats", parseBrutalCheats, nullptr, offsetof(RetailAITuning, brutal) },
		{ "DisableBaseBuilding", INI::parseBool, nullptr, offsetof(RetailAITuning, disableBaseBuilding) },
		{ "DisableEconomyBuilding", INI::parseBool, nullptr, offsetof(RetailAITuning, disableEconomyBuilding) },
		{ "DisableUnitBuilding", INI::parseBool, nullptr, offsetof(RetailAITuning, disableUnitBuilding) },
		{ "DisableUnitUpgrading", INI::parseBool, nullptr, offsetof(RetailAITuning, disableUnitUpgrading) },
		{ "DisableScienceUpgrading", INI::parseBool, nullptr, offsetof(RetailAITuning, disableScienceUpgrading) },
		{ "DisableTacticalAI", INI::parseBool, nullptr, offsetof(RetailAITuning, disableTacticalAI) },
		{ "DisableWallBuilding", INI::parseBool, nullptr, offsetof(RetailAITuning, disableWallBuilding) },
		{ "DefenseTreeNodeRadius", INI::parseReal, nullptr, offsetof(RetailAITuning, defenseTreeNodeRadius) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	ini->initFromINI(&retailTuning, fields);
}

Bool AISkirmishPlayer::hasRetailCombatChain(const ThingTemplate& attacker)
{
	if (!RetailFileLayers::enabled())
		return FALSE;
	const Int kind = combatKind(attacker);
	return kind >= 0 && static_cast<size_t>(kind) < retailTuning.combatChains.size() &&
		retailTuning.combatChains[kind].unit >= 0;
}

Real AISkirmishPlayer::getRetailCombatChainModifier(const ThingTemplate& attacker, const ThingTemplate& target)
{
	if (!hasRetailCombatChain(attacker))
		return 0.0f;
	const Int kind = combatKind(target);
	if (kind < 0)
		return 0.0f;
	const RetailCombatChain& chain = retailTuning.combatChains[combatKind(attacker)];
	const size_t count = std::min<size_t>(17, std::min(chain.targets.size(), chain.modifiers.size()));
	for (size_t slot = 0; slot < count; ++slot)
		if (chain.targets[slot] == kind)
			return chain.modifiers[slot];
	return 0.0f;
}

Real RetailSkirmishSpending::brutalFactor(const Player& player, Bool time)
{
	if (!RetailFileLayers::enabled() || player.getPlayerType() != PLAYER_COMPUTER ||
		!player.isPlayableSide() || player.getPlayerDifficulty() != DIFFICULTY_BRUTAL)
		return 1.0f;
	const Real reduction = time ? retailTuning.brutal.timeReduction : retailTuning.brutal.costReduction;
	return 1.0f - std::min(reduction, 1.0f);
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
	const RetailSkirmishPolicy policy = retailPolicy(*m_player);
	const RetailArmy* rules = policy.army;
	if (!rules)
		return;

	const Real elapsed = INT_TO_REAL(frame) / LOGICFRAMES_PER_SECOND;
	const auto phaseAt = [rules](Real seconds) {
		return seconds < rules->rushSeconds ? 0 : seconds < rules->rushSeconds + rules->midgameSeconds ? 1 : 2;
	};
	const Int phase = phaseAt(elapsed);
	const ThingTemplate* farm = TheThingFactory->findTemplate(rules->farm, FALSE);
	const auto isScavengedBuilding = [&](const ThingTemplate& type) {
		return std::any_of(rules->scavengedBuildings.begin(), rules->scavengedBuildings.end(), [&](const AsciiString& building) {
			const ThingTemplate* scavenged = TheThingFactory->findTemplate(building, FALSE);
			return scavenged && type.isEquivalentTo(scavenged);
		});
	};
	std::vector<const ThingTemplate*> offensiveBuildings;
	for (const AsciiString& name : rules->offensiveBuildings)
		if (const ThingTemplate* type = TheThingFactory->findTemplate(name, FALSE); type && type->isKindOf(KINDOF_STRUCTURE))
			offensiveBuildings.push_back(type);
	const auto isOffensiveBuilding = [&](const ThingTemplate& type) {
		return std::any_of(offensiveBuildings.begin(), offensiveBuildings.end(), [&](const ThingTemplate* offensive) {
			return type.isEquivalentTo(offensive);
		});
	};
	const auto isFarm = [&](const ThingTemplate& type) -> Bool {
		if (isScavengedBuilding(type))
			return FALSE;
		return type.isKindOf(KINDOF_ECONOMY_STRUCTURE) || (farm && type.isEquivalentTo(farm));
	};
	static const Bool trace = std::getenv("GENERALSX_SKIRMISH_AI_TRACE") != nullptr;
	const Int playerIndex = m_player->getPlayerIndex();
	Int farms = 0;
	const GameDifficulty difficulty = m_player->getPlayerDifficulty();
	const Int farmLimit = policy.difficulty && policy.difficulty->farmLimit >= 0
		? policy.difficulty->farmLimit : std::numeric_limits<Int>::max();

	Object* keep = nullptr;
	Bool fortressPresent = FALSE;
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
		else
		{
			fortressPresent = fortressPresent || isRetailFortressKeep(*object->getTemplate()) ||
				(object->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION) && isRetailFortress(*object->getTemplate()));
			if (object->isKindOf(KINDOF_STRUCTURE) && !isScavengedBuilding(*object->getTemplate()))
				m_retailBuiltStructures.insert(object->getTemplate()->getName());
		}
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
	Money* money = m_player->getMoney();
	const Int savingsDifficulty = std::clamp<Int>(difficulty, DIFFICULTY_EASY, DIFFICULTY_HARD);
	UnsignedInt savingsLimit = 5000 * savingsDifficulty;
	for (const AsciiString& name : rules->heroes)
	{
		const ThingTemplate* hero = TheThingFactory->findTemplate(name, FALSE);
		if (hero && !HeroRoster::isAlive(m_player, hero) && !HeroRoster::isQueued(m_player, hero))
			savingsLimit = std::max(savingsLimit, static_cast<UnsignedInt>(std::max(0, HeroRoster::price(m_player, hero).cost)));
	}
	if (farms > 2 && m_retailSavings < savingsLimit)
	{
		const Real saving = money->countMoney() * rules->percentToSave[phase] / (3 - savingsDifficulty);
		if (saving > 1.0f)
			m_retailSavings += money->withdraw(static_cast<UnsignedInt>(std::min<Real>(saving, money->countMoney())), FALSE);
	}
	if (trace && (frame < 2 * LOGICFRAMES_PER_SECOND || phase != phaseAt(elapsed - 2.0f)))
	{
		std::fprintf(stderr, "AI_PHASE\t%u\t%d\t%d\t%u\t%u\t%d\n",
			frame, playerIndex, phase, money->countMoney(), m_retailSavings, farms);
	}
	const auto spendWithSavings = [&](Int cost, auto purchase) {
		const UnsignedInt cash = money->countMoney();
		const UnsignedInt price = static_cast<UnsignedInt>(std::max(0, cost));
		const UnsignedInt needed = price > cash ? price - cash : 0;
		if (needed > m_retailSavings)
			return FALSE;
		money->deposit(needed, FALSE, FALSE);
		if (purchase())
		{
			m_retailSavings -= needed;
			return TRUE;
		}
		money->withdraw(needed, FALSE);
		return FALSE;
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
	const Int commandPointTotal = m_player->getCommandPointsTotal();
	const Real commandPointUsage = commandPointTotal > 0
		? INT_TO_REAL(m_player->getCommandPointsUsed()) / INT_TO_REAL(commandPointTotal) : 0.0f;
	const Real baseUnitPriority = phase == 0 ? 500.0f : rules->defaultPriority;
	const Real priorityGrowth = (rules->mustUseCommandPoints[phase] - commandPointUsage) * rules->lowUnitPriority[phase];
	const Real unitPriorityCap = 1800.0f + (1.0f - commandPointUsage) * 300.0f;
	const auto adjustUnitPriority = [&](Real current) {
		return std::min(std::max(rules->defaultPriority, current + priorityGrowth), unitPriorityCap);
	};
	const auto isOrdinaryArmyUnit = [](const ThingTemplate& type) {
		return !type.isKindOf(KINDOF_HERO) && !type.isKindOf(KINDOF_SHIP);
	};
	const auto unitPriority = [&](const ThingTemplate& unit) {
		const auto pending = m_retailUnitPriorities.find(unit.getName());
		return pending == m_retailUnitPriorities.end() ? baseUnitPriority : std::max(baseUnitPriority, pending->second);
	};
	const auto armyRecipe = [&](Object& producer, const CommandButton* button) -> const ThingTemplate* {
		if (!button || button->getCommandType() != GUI_COMMAND_UNIT_BUILD ||
			button->getRequiredRank() > producer.getExperienceTracker()->getRank() ||
			!HeroRoster::buttonUpgradesMet(button, m_player, &producer))
			return nullptr;
		return button->getThingTemplate();
	};
	const auto chooseArmyUnit = [&](Object& producer, const CommandSet& set, Bool request) {
		const CommandButton* selected = nullptr;
		Real bestPriority = baseUnitPriority;
		Real bestWeight = 0.0f;
		for (Int slot = 0; slot < CommandButtonRules::RETAIL_COMMANDS_PER_SET; ++slot)
		{
			const CommandButton* button = set.getCommandButton(slot);
			const ThingTemplate* unit = armyRecipe(producer, button);
			if (!unit)
				continue;
			const Real weight = armyWeight(*unit, FALSE);
			if (weight <= 0.0f)
				continue;
			const Real score = weight / INT_TO_REAL(1 + countType(unit));
			const Real priority = request ? baseUnitPriority : unitPriority(*unit);
			if (priority < bestPriority || (priority == bestPriority && score <= bestWeight))
				continue;
			const CanMakeType status = TheBuildAssistant->canMakeUnit(&producer, unit);
			if (status != CANMAKE_OK && (!request || (status != CANMAKE_NO_MONEY && status != CANMAKE_NO_COMMAND_POINTS)))
				continue;
			bestPriority = priority;
			bestWeight = score;
			selected = button;
		}
		return selected;
	};
	const Bool canBuildArmy = !retailTuning.disableUnitBuilding && m_player->getCanBuildUnits();
	std::set<AsciiString> supportedUnitRequests;
	if (canBuildArmy)
	{
		for (Object* producer : producers)
		{
			const CommandSet* set = retailCommands(*producer);
			if (!set)
				continue;
			for (Int slot = 0; slot < CommandButtonRules::RETAIL_COMMANDS_PER_SET; ++slot)
			{
				const ThingTemplate* unit = armyRecipe(*producer, set->getCommandButton(slot));
				if (unit && isOrdinaryArmyUnit(*unit) && armyWeight(*unit, TRUE) > 0.0f)
					supportedUnitRequests.insert(unit->getName());
			}
			if (producer->getProductionUpdateInterface()->getProductionCount() != 0)
				continue;
			for (Bool request : {TRUE, FALSE})
			{
				const CommandButton* button = chooseArmyUnit(*producer, *set, request);
				const ThingTemplate* unit = button ? button->getThingTemplate() : nullptr;
				if (unit && isOrdinaryArmyUnit(*unit))
					m_retailUnitPriorities.try_emplace(unit->getName(), 0.0f);
			}
		}
	}
	std::erase_if(m_retailUnitPriorities, [&](const auto& pending) {
		return !supportedUnitRequests.contains(pending.first);
	});
	for (auto& [name, adjustment] : m_retailUnitPriorities)
	{
		const Real previous = std::max(baseUnitPriority, adjustment);
		if (owned.size() > 1)
			adjustment = adjustUnitPriority(previous);
		if (trace)
			std::fprintf(stderr, "AI_UNIT_REQUEST\t%u\t%d\t%s\t%.9g\t%.9g\t%.9g\n",
				frame, playerIndex, name.str(), baseUnitPriority, previous, std::max(baseUnitPriority, adjustment));
	}
	struct ArmyOrder
	{
		Object* producer = nullptr;
		const CommandButton* button = nullptr;
		Real priority = -std::numeric_limits<Real>::infinity();
	};
	const auto prioritizedArmyOrder = [&] {
		ArmyOrder order;
		if (!canBuildArmy)
			return order;
		for (Object* producer : producers)
		{
			const CommandSet* set = retailCommands(*producer);
			if (!set || producer->getProductionUpdateInterface()->getProductionCount() != 0)
				continue;
			const CommandButton* button = chooseArmyUnit(*producer, *set, FALSE);
			const ThingTemplate* unit = button ? button->getThingTemplate() : nullptr;
			if (unit)
			{
				const Real priority = unitPriority(*unit);
				if (priority > order.priority)
					order = {producer, button, priority};
			}
		}
		return order;
	};

	struct UpgradeOrder
	{
		Object* producer = nullptr;
		const UpgradeTemplate* upgrade = nullptr;
		Real priority = 0.0f;
		UnsignedInt cost = 0;
	};
	std::vector<UpgradeOrder> upgrades;
	Int researchSlots = 0;
	if (phase > 0)
	{
		const auto upgradeComplete = [&](const Object& producer, const UpgradeTemplate& upgrade) {
			return upgrade.getUpgradeType() == UPGRADE_TYPE_PLAYER ? m_player->hasUpgradeComplete(&upgrade) :
				producer.hasUpgrade(&upgrade);
		};
		std::erase_if(m_retailUpgradePlans, [&](const auto& entry) {
			const Object* producer = TheGameLogic->findObjectByID(entry.first.first);
			const UpgradeTemplate* upgrade = TheUpgradeCenter->findUpgrade(entry.first.second);
			return !producer || producer->isEffectivelyDead() || producer->getControllingPlayer() != m_player ||
				!upgrade || upgradeComplete(*producer, *upgrade);
		});
		auto orderedProducers = producers;
		std::sort(orderedProducers.begin(), orderedProducers.end(), [](const Object* a, const Object* b) {
			return a->getID() < b->getID();
		});
		Int activeResearch = 0;
		for (Object* producer : orderedProducers)
		{
			if (!producer->isKindOf(KINDOF_STRUCTURE))
				continue;
			const ProductionUpdateInterface* production = producer->getProductionUpdateInterface();
			for (const ProductionEntry* entry = production->firstProduction(); entry; entry = production->nextProduction(entry))
				if (entry->getProductionType() == PRODUCTION_UPGRADE && usesRetailUpgradePriority(*entry->getProductionUpgrade(), TRUE))
					++activeResearch;
		}
		researchSlots = (phase == 1 ? 1 : 3) - activeResearch;
		UpgradeOrder researchRequest;
		Int pendingResearch = 0;
		for (Object* producer : orderedProducers)
		{
			const Bool research = producer->isKindOf(KINDOF_STRUCTURE);
			if (research ? (retailTuning.disableScienceUpgrading || researchSlots <= 0) :
				retailTuning.disableUnitUpgrading)
				continue;
			const CommandSet* set = retailCommands(*producer);
			if (!set || producer->getProductionUpdateInterface()->getProductionCount() != 0)
				continue;
			for (Int slot = 0; slot < CommandButtonRules::RETAIL_COMMANDS_PER_SET; ++slot)
			{
				const CommandButton* button = set->getCommandButton(slot);
				const UpgradeTemplate* upgrade = button ? button->getUpgradeTemplate() : nullptr;
				if (!upgrade || !usesRetailUpgradePriority(*upgrade, research) ||
					(button->getCommandType() != GUI_COMMAND_OBJECT_UPGRADE &&
						(!research || button->getCommandType() != GUI_COMMAND_PLAYER_UPGRADE)) ||
					button->getRequiredRank() > producer->getExperienceTracker()->getRank() ||
					!HeroRoster::buttonUpgradesMet(button, m_player, producer) || upgradeComplete(*producer, *upgrade))
					continue;
				if (upgrade->getUpgradeType() == UPGRADE_TYPE_OBJECT && !producer->affectedByUpgrade(upgrade))
					continue;
				if (upgrade->getUpgradeType() == UPGRADE_TYPE_PLAYER && m_player->hasUpgradeInProduction(upgrade))
					continue;
				const auto cost = static_cast<UnsignedInt>(std::max(0, upgrade->calcCostToBuild(m_player)));
				const auto key = std::make_pair(producer->getID(), upgrade->getUpgradeName());
				if (research)
				{
					if (const auto plan = m_retailUpgradePlans.find(key); plan != m_retailUpgradePlans.end())
					{
						++pendingResearch;
						upgrades.push_back({producer, upgrade, plan->second.priority, cost});
					}
					else if (cost <= money->countMoney() / (activeResearch + 1) &&
						(!researchRequest.producer || cost < researchRequest.cost))
						researchRequest = {producer, upgrade, 0.0f, cost};
				}
				else
				{
					auto [plan, inserted] = m_retailUpgradePlans.try_emplace(key);
					if (inserted)
					{
						plan->second.priority = rollRetailUpgradePriority(rules->unitUpgradePriority, FALSE);
						const Real roll = GameLogicRandomValueReal(0.0f, 1.0f);
						plan->second.admitted = rules->unitUpgradeChance > 0.0f && roll <= rules->unitUpgradeChance;
					}
					if (plan->second.admitted)
						upgrades.push_back({producer, upgrade, plan->second.priority, cost});
				}
			}
		}
		if (researchRequest.producer && pendingResearch < researchSlots)
		{
			const auto* upgrade = researchRequest.upgrade;
			const auto& range = upgrade->getSkirmishAIHeuristic() == SkirmishUpgradeHeuristic::Important
				? rules->importantResearchPriority : rules->normalResearchPriority;
			researchRequest.priority = rollRetailUpgradePriority(range, TRUE);
			m_retailUpgradePlans.emplace(std::make_pair(researchRequest.producer->getID(), upgrade->getUpgradeName()),
				RetailUpgradePlan{researchRequest.priority, TRUE});
			upgrades.push_back(researchRequest);
		}
		std::stable_sort(upgrades.begin(), upgrades.end(), [](const UpgradeOrder& a, const UpgradeOrder& b) {
			return a.priority > b.priority;
		});
	}
	const auto buyUpgradesAbove = [&](Real priority) {
		for (const UpgradeOrder& order : upgrades)
		{
			if (order.priority <= priority)
				break;
			const Bool research = order.producer->isKindOf(KINDOF_STRUCTURE);
			auto* production = order.producer->getProductionUpdateInterface();
			if ((research && researchSlots <= 0) || production->getProductionCount() != 0 || money->countMoney() < order.cost ||
				!production->queueUpgrade(order.upgrade))
				continue;
			if (research)
				--researchSlots;
			if (trace)
				std::fprintf(stderr, "AI_UPGRADE\t%u\t%d\t%s\t%s\t%s\t%g\n", frame, playerIndex,
					research ? "research" : "unit", order.producer->getTemplate()->getName().str(),
					order.upgrade->getUpgradeName().str(), order.priority);
		}
	};
	if (trace && !producers.empty())
		std::fprintf(stderr, "AI_UNIT_PRIORITY\t%u\t%d\t%d\t%g\t%g\t%g\t%g\t%g\n", frame, playerIndex, phase,
			commandPointUsage, rules->mustUseCommandPoints[phase], rules->lowUnitPriority[phase], rules->defaultPriority,
			owned.size() > 1 ? std::max(baseUnitPriority, adjustUnitPriority(baseUnitPriority)) : baseUnitPriority);
	Bool deferBuildingForUnit = FALSE;
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
		const Bool replaceFortress = !fortressPresent && std::any_of(m_retailBuiltStructures.begin(),
			m_retailBuiltStructures.end(), [](const AsciiString& name) {
				const ThingTemplate* type = TheThingFactory->findTemplate(name, FALSE);
				return type && isRetailFortressKeep(*type);
			});
		Int maximumCommandPoints = 0;
		const Int commandPoints = m_player->getCommandPointsTotal(&maximumCommandPoints);
		const Real sinceFarm = INT_TO_REAL(frame - m_retailLastFarmFrame) / LOGICFRAMES_PER_SECOND;
		const Bool belowFarmFloor = farms < rules->minimumFarms;
		const Bool farmDue = phase == 0
			? belowFarmFloor && (m_retailLastFarmFrame == 0 || sinceFarm > rules->rushFarmInterval)
			: belowFarmFloor || (farms < phase * 3 + 4 && sinceFarm > 30.0f);
		const Bool seekFarm = !retailTuning.disableEconomyBuilding && farmDue &&
			farms < farmLimit && commandPoints < maximumCommandPoints;
		const RetailDifficulty defaultDifficulty;
		const Int* farmChance = policy.difficulty ? policy.difficulty->farmChance : defaultDifficulty.farmChance;
		const Bool planFarm = seekFarm && (farmChance[0] >= farmChance[1] ||
			(farmChance[0] > 0 && GameLogicRandomValue(0, farmChance[1] - 1) < farmChance[0]));
		const Real initialFarmPriority = belowFarmFloor ? 2000.0f : 100.0f;
		if (!seekFarm)
			m_retailFarmPriority = 0.0f;
		else if (planFarm && m_retailFarmPriority == 0.0f)
			m_retailFarmPriority = initialFarmPriority;
		if (seekFarm && m_retailFarmPriority > 0.0f && phase > 0)
		{
			const Real rate = rules->farmPriorityPerSecond / std::max(1, farms);
			const Real usage = static_cast<Real>(m_player->getCommandPointsUsed()) / std::max(1, commandPoints);
			m_retailFarmPriority = std::min(2000.0f, m_retailFarmPriority + 2.0f * rate * (1.0f + 10.0f * usage));
		}
		const ThingTemplate* scavenged = nullptr;
		const Int scavengedCount = static_cast<Int>(rules->scavengedBuildings.size());
		if (!retailTuning.disableEconomyBuilding && scavengedCount > 0 &&
			money->countMoney() + m_retailSavings > 599 && !builders.empty())
		{
			if (m_retailScavengedBuilding < 0 || m_retailScavengedBuilding >= scavengedCount)
				m_retailScavengedBuilding = scavengedCount == 1 ? 0 : GameLogicRandomValue(0, scavengedCount - 1);
			scavenged = TheThingFactory->findTemplate(rules->scavengedBuildings[m_retailScavengedBuilding], FALSE);
		}
		const auto buildFrom = [&](Object& builder, FoundationAIUpdate* plot) -> Bool {
			const CommandSet* set = retailCommands(builder);
			if (!set)
				return FALSE;
			const CommandButton* selected = nullptr;
			Bool selectedUsesSavings = FALSE;
			Real priority = 0.0f;
			for (Int slot = 0; slot < CommandButtonRules::RETAIL_COMMANDS_PER_SET; ++slot)
			{
				const CommandButton* button = set->getCommandButton(slot);
				if (!button || button->getCommandType() != (plot ? GUI_COMMAND_FOUNDATION_CONSTRUCT : GUI_COMMAND_DOZER_CONSTRUCT))
					continue;
				const ThingTemplate* type = button->getThingTemplate();
				if (!type || button->getRequiredRank() > builder.getExperienceTracker()->getRank() ||
					!HeroRoster::buttonUpgradesMet(button, m_player, &builder))
					continue;
				const Bool economy = isFarm(*type);
				Bool usesSavings = FALSE;
				Real score = 0.0f;
				if (economy)
				{
					const auto cost = static_cast<UnsignedInt>(std::max(0, type->calcCostToBuild(m_player)));
					const UnsignedInt cash = money->countMoney();
					if (seekFarm && (belowFarmFloor || (cash >= cost && cash - cost >= openingUnitReserve)))
						score = m_retailFarmPriority;
				}
				else if (countType(type) == 0)
				{
					if (replaceFortress && isRetailFortress(*type))
					{
						usesSavings = TRUE;
						score = rules->fortressRebuildPriority;
					}
					else if (scavenged && type->isEquivalentTo(scavenged))
					{
						usesSavings = TRUE;
						score = rules->defaultPriority;
					}
					else
					{
						const ThingTemplate* replacement = automaticReplacement(*type);
						if (producesArmy(*type) || (replacement && countType(replacement) == 0 && producesArmy(*replacement)) ||
							(plot && farms >= 2 && soldiers.size() >= 2 && type->isKindOf(KINDOF_FS_BASE_DEFENSE)))
							score = rules->defaultPriority;
						if (m_retailBuiltStructures.contains(type->getName()))
						{
							score *= rules->structureRebuildModifier;
							usesSavings = TRUE;
						}
					}
				}
				if (score > priority)
				{
					const auto cost = static_cast<UnsignedInt>(std::max(0, type->calcCostToBuild(m_player)));
					const auto cash = money->countMoney();
					if (cost > cash && (!usesSavings || cost - cash > m_retailSavings))
						continue;
					if (plot)
					{
						if (!m_player->canBuild(type))
							continue;
					}
					else
					{
						const CanMakeType canMake = TheBuildAssistant->canMakeUnit(&builder, type);
						if (canMake != CANMAKE_OK && !(usesSavings && canMake == CANMAKE_NO_MONEY))
							continue;
					}
					priority = score;
					selected = button;
					selectedUsesSavings = usesSavings;
				}
			}
			if (!selected)
				return FALSE;
			if (!upgrades.empty())
				buyUpgradesAbove(std::max(prioritizedArmyOrder().priority, priority));
			if (prioritizedArmyOrder().priority > priority)
			{
				deferBuildingForUnit = TRUE;
				return FALSE;
			}
			const ThingTemplate* type = selected->getThingTemplate();
			const auto construct = [&]() {
				return plot ? plot->construct(type, m_player) != nullptr
					: placeRetailBuilding(builder, *type, keep ? *keep->getPosition() : *builder.getPosition());
			};
			const Bool built = selectedUsesSavings ? spendWithSavings(type->calcCostToBuild(m_player), construct) : construct();
			if (built)
			{
				if (isFarm(*type))
				{
					++farms;
					m_retailLastFarmFrame = frame;
					m_retailFarmPriority = 0.0f;
				}
				else if (scavenged && type->isEquivalentTo(scavenged))
					m_retailScavengedBuilding = (m_retailScavengedBuilding + 1) % scavengedCount;
				else
					m_retailBuiltStructures.insert(type->getName());
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

	if (!upgrades.empty())
		buyUpgradesAbove(prioritizedArmyOrder().priority);
	ArmyOrder deferredArmy = deferBuildingForUnit ? prioritizedArmyOrder() : ArmyOrder{};
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
			m_retailUnitPriorities.erase(unit->getName());
			if (trace)
			{
				std::fprintf(stderr, "AI_TRAIN\t%u\t%d\t%s\t%s\t%d\n", frame, playerIndex,
					button ? button->getName().str() : "hero-roster", unit->getName().str(),
					button ? HeroRoster::buttonUpgradesMet(button, m_player, producer) : TRUE);
				std::fflush(stderr);
			}
			return TRUE;
		};
		if (deferredArmy.producer)
		{
			if (deferredArmy.producer == producer)
			{
				train(deferredArmy.button->getThingTemplate(), deferredArmy.button);
				deferredArmy = {};
			}
			continue;
		}
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
				const Bool research = producer->isKindOf(KINDOF_STRUCTURE);
				const Bool unlock = unlocksArmy(upgrade);
				if ((research && retailTuning.disableScienceUpgrading) ||
					(usesRetailUpgradePriority(*upgrade, research) && (!research || phase != 0 || !unlock)) ||
					(needsArmy && !unlock))
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
				if (hero && !HeroRoster::isAlive(m_player, hero))
				{
					HeroRules::Price price;
					if (!production->getUnitPriceOverride(hero, price))
						price = HeroRoster::price(m_player, hero);
					if (spendWithSavings(price.cost, [&] { return train(hero, nullptr); }))
						break;
				}
			}
		if (production->getProductionCount() != 0)
			continue;
		if (const CommandButton* selected = chooseArmyUnit(*producer, *set, FALSE))
			train(selected->getThingTemplate(), selected);
	}

	buyUpgradesAbove(-std::numeric_limits<Real>::infinity());
	if (retailTuning.disableTacticalAI)
		return;
	if (keep && !retailTuning.disableWallBuilding && phase == 2 && m_player->getCanBuildBase() &&
		rules->wallNode.isNotEmpty() && money->countMoney() + m_retailSavings >= 2500)
	{
		const ThingTemplate* type = TheThingFactory->findTemplate(rules->wallNode, FALSE);
		const auto builder = std::find_if(builders.begin(), builders.end(), [](Object* object) {
			return object->getAI()->isIdle() && !object->getAI()->getDozerAIInterface()->isAnyTaskPending();
		});
		if (type && type->isKindOf(KINDOF_WALL_HUB) && builder != builders.end())
		{
			auto& plan = m_retailWallPlan;
			const auto stillOwned = [&](ObjectID id) {
				const Object* object = TheGameLogic->findObjectByID(id);
				return object && !object->isEffectivelyDead() && object->getControllingPlayer() == m_player;
			};
			if (plan.defendedStructure != INVALID_ID && !stillOwned(plan.defendedStructure))
				plan = {};
			const Bool newPlan = plan.defendedStructure == INVALID_ID;
			const Coord3D center = newPlan ? *keep->getPosition() : plan.center;
			const Coord2D forward = newPlan ? retailWallFacing(*m_player, center) : plan.forward;
			ObjectID defended = plan.defendedStructure;
			if (newPlan)
			{
				const auto target = std::find_if(m_retailTargets.begin(), m_retailTargets.end(), [&](const auto& target) {
					return !target.retiring && stillOwned(target.defendedStructure);
				});
				defended = target == m_retailTargets.end() ? keep->getID() : target->defendedStructure;
			}
			Bool attempted = FALSE;
			Object* hub = nullptr;
			for (size_t i = 0; i < plan.nodes.size(); ++i)
			{
				if (stillOwned(plan.nodes[i]))
					continue;
				attempted = TRUE;
				if ((forward.x != 0.0f || forward.y != 0.0f) &&
					spendWithSavings(type->calcCostToBuild(m_player), [&] {
						hub = placeRetailWallNode(**builder, *type, center, forward, i);
						return hub != nullptr;
					}))
				{
					plan.defendedStructure = defended;
					plan.center = center;
					plan.forward = forward;
					plan.nodes[i] = hub->getID();
					break;
				}
			}
			if (trace && attempted)
				std::fprintf(stderr, "AI_WALL_NODE\t%u\t%d\t%d\t%d\t%s\n", frame, playerIndex,
					defended, hub ? hub->getID() : INVALID_ID, hub ? "assigned" : "unavailable");
		}
	}
	const UnsignedInt routeRevision = TheAI->pathfinder()->getRetailRouteRevision();
	const auto liveObjective = [&](const Object* target, UnsignedInt owner) {
		if (!target || target->isEffectivelyDead() || target->isDestroyed() || target->getTeam()->getID() != owner)
			return false;
		if (const AttachUpdate* pickup = retailRingAttachment(*target))
			return pickup->getParentID() == INVALID_ID;
		return m_player->getRelationship(target->getTeam()) == ENEMIES;
	};
	using ObjectiveKey = std::pair<ObjectID, UnsignedInt>;
	std::map<ObjectiveKey, std::set<UnsignedInt>> activeTeams;
	std::set<ObjectID> collectingRing;
	for (auto it = m_retailObjectives.begin(); it != m_retailObjectives.end();)
	{
		const auto [unitID, targetID, owner] = it->first;
		const Object* unit = TheGameLogic->findObjectByID(unitID);
		const Object* target = TheGameLogic->findObjectByID(targetID);
		if (!unit || unit->isEffectivelyDead() || unit->getControllingPlayer() != m_player ||
			(targetID != INVALID_ID && !liveObjective(target, owner)))
		{
			it = m_retailObjectives.erase(it);
			continue;
		}
		auto& order = it->second;
		const auto* horde = static_cast<const HordeAIUpdate*>(unit->findUpdateModule(NAMEKEY("HordeAIUpdate")));
		const AIUpdateInterface* ai = unit->getAI();
		const Bool ring = target && retailRingAttachment(*target);
		const Bool active = horde ? (ring ? horde->isOnPlannerAdvance(order.issuedFrame)
			: horde->isExecutingPlannerOrder(targetID, order.issuedFrame))
			: ai && ai->isExecutingRetailPlannerOrder(ring ? INVALID_ID : targetID, order.issuedFrame);
		if (order.issuedFrame && !order.blockedRevision)
		{
			if (horde)
				order.blockedRevision = horde->blockedPlannerRevision(order.issuedFrame);
			else if (ai)
				order.blockedRevision = ai->blockedRetailPlannerRevision(order.issuedFrame);
		}
		if (order.blockedRevision ? order.blockedRevision != routeRevision : ring && !active)
			it = m_retailObjectives.erase(it);
		else
		{
			if (active)
			{
				activeTeams[{targetID, owner}].insert(order.issuedFrame);
				if (ring)
					collectingRing.insert(unitID);
			}
			++it;
		}
	}
	const auto teamCount = [&](const RetailTacticalTarget& target) {
		const auto teams = activeTeams.find({target.object, target.owner});
		return teams == activeTeams.end() ? 0 : static_cast<Int>(teams->second.size());
	};
	m_retailTargets.resize(rules->tacticalTargets.size());
	for (size_t i = 0; i < m_retailTargets.size(); ++i)
	{
		auto& target = m_retailTargets[i];
		if (target.selectedFrame == 0)
			continue;
		const Object* object = TheGameLogic->findObjectByID(target.object);
		const Object* defended = TheGameLogic->findObjectByID(target.defendedStructure);
		const Bool lostStructure = target.defendedStructure != INVALID_ID &&
			(!defended || defended->isEffectivelyDead() || defended->getControllingPlayer() != m_player);
		const Player* scoutOpponent = target.object == INVALID_ID && target.owner < static_cast<UnsignedInt>(ThePlayerList->getPlayerCount())
			? ThePlayerList->getNthPlayer(target.owner) : nullptr;
		const Bool lostOpponent = target.object == INVALID_ID && (!scoutOpponent || !scoutOpponent->isPlayerActive() ||
			m_player->getRelationship(scoutOpponent->getDefaultTeam()) != ENEMIES);
		const Bool missing = lostStructure || lostOpponent || (target.object != INVALID_ID && !liveObjective(object, target.owner));
		if (!target.retiring && !missing &&
			INT_TO_REAL(frame - target.selectedFrame) >= rules->targetLifetimeSeconds * LOGICFRAMES_PER_SECOND)
		{
			if (GameLogicRandomValueReal(0.0f, 1.0f) < rules->targetRetentionChance)
				target.selectedFrame = frame;
			else
			{
				target.retiring = TRUE;
				if (trace)
					std::fprintf(stderr, "AI_TARGET_EXPIRE\t%u\t%d\t%zu\t%d\n", frame, playerIndex, i, target.object);
			}
		}
		if (missing || (target.retiring && teamCount(target) == 0))
			target = {};
		else if (trace && rules->tacticalTargets[i] == RETAIL_TARGET_OPPORTUNITY && teamCount(target) > 0)
			std::fprintf(stderr, "AI_OPPORTUNITY_HELD\t%u\t%d\t%d\t%d\n", frame, playerIndex, target.object, teamCount(target));
	}
	const auto slotLimit = [&](size_t i) {
		return i < rules->maxTeamsPerTarget.size() ? rules->maxTeamsPerTarget[i] : 1;
	};
	const auto hasKind = [&](RetailTacticalKind kind) {
		return std::find(rules->tacticalTargets.begin(), rules->tacticalTargets.end(), kind) != rules->tacticalTargets.end();
	};
	Object* ringOpportunity = nullptr;
	const AttachUpdate* ringPickup = nullptr;
	const Bool hasOpportunity = hasKind(RETAIL_TARGET_OPPORTUNITY);
	const auto targetless = std::find(rules->tacticalTargets.begin(), rules->tacticalTargets.end(), RETAIL_TARGET_TARGETLESS);
	const Bool canRunTargetless = targetless != rules->tacticalTargets.end() &&
		slotLimit(targetless - rules->tacticalTargets.begin()) > 0;
	const Bool canSeekRing = phase == 2 && hasOpportunity &&
		std::none_of(owned.begin(), owned.end(), [&](const Object* object) {
			const auto* citadel = dynamic_cast<const CitadelSlaughterHordeContain*>(object->getContain());
			return citadel && citadel->hasPlayerRingUpgrade(*m_player);
		});
	if (canSeekRing || canRunTargetless)
	{
		for (Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
		{
			if (object->isEffectivelyDead() || object->isDestroyed())
				continue;
			const AttachUpdate* pickup = retailRingAttachment(*object);
			Object* carrier = pickup ? TheGameLogic->findObjectByID(pickup->getParentID()) : nullptr;
			if (carrier && carrier->getControllingPlayer() == m_player)
			{
				if (!canRunTargetless)
					continue;
				Object* courier = HordeContain::getAnchor(carrier);
				if (!courier)
					courier = carrier;
				if (AIUpdateInterface* ai = courier->getAI(); ai && ai->getEnterTarget() == nullptr)
				{
					if (Object* citadel = findRetailRingCitadel(*courier, owned))
					{
						if (ai->issueRetailRingReturn(citadel) && trace)
							std::fprintf(stderr, "AI_RING_RETURN\t%u\t%d\t%d\t%d\n", frame, playerIndex, courier->getID(), citadel->getID());
					}
				}
				continue;
			}
			if (canSeekRing && !ringOpportunity && (pickup || object->isKindOf(KINDOF_NEUTRALGOLLUM)))
			{
				ringOpportunity = carrier ? carrier : object;
				if (Object* horde = HordeContain::getAnchor(carrier))
					ringOpportunity = horde;
				ringPickup = carrier ? nullptr : pickup;
			}
		}
	}
	const auto buildForward = [&] {
		if (!keep || offensiveBuildings.empty() || builders.empty() || retailTuning.disableBaseBuilding ||
			!m_player->getCanBuildBase() || money->countMoney() + m_retailSavings < 1000)
			return;
		const auto defense = std::find_if(m_retailTargets.begin(), m_retailTargets.end(), [&](const auto& target) {
			return target.defendedStructure != INVALID_ID && teamCount(target) > 0;
		});
		if (defense == m_retailTargets.end())
			return;
		const Object* threat = TheGameLogic->findObjectByID(defense->object);
		if (!threat)
			return;
		const Coord3D center = *threat->getPosition();
		const Coord3D home = *keep->getPosition();
		const Coord3D towardHome = home - center;
		const Real distance = sqr(towardHome.x) + sqr(towardHome.y) + sqr(towardHome.z);
		if (distance > sqr(1500.0f))
			return;
		Int nearby = 0;
		for (const Object* object : owned)
		{
			if (!isOffensiveBuilding(*object->getTemplate()))
				continue;
			if (object->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION))
				return;
			if (tacticalDistanceSquared(*object->getPosition(), center) <= sqr(600.0f) && ++nearby >= 2)
				return;
		}
		const Real scale = distance > 0.0f ? 200.0f / WWMath::SqrtfOrigin(distance) : 0.0f;
		const Coord3D position{center.x + towardHome.x * scale, center.y + towardHome.y * scale, 0.0f};
		for (const ThingTemplate* type : offensiveBuildings)
		{
			Object* selected = nullptr;
			for (Object* builder : builders)
			{
				if (!builder->getAI()->isIdle() || builder->getAI()->getDozerAIInterface()->isAnyTaskPending())
					continue;
				const CommandSet* set = retailCommands(*builder);
				if (!set)
					continue;
				const auto rank = [&](const Object& object) {
					return std::make_pair(tacticalDistanceSquared(*object.getPosition(), center), object.getID());
				};
				if (selected && rank(*selected) <= rank(*builder))
					continue;
				for (Int slot = 0; slot < CommandButtonRules::RETAIL_COMMANDS_PER_SET; ++slot)
				{
					const CommandButton* button = set->getCommandButton(slot);
					if (button && button->getCommandType() == GUI_COMMAND_DOZER_CONSTRUCT && button->getThingTemplate() == type &&
						button->getRequiredRank() <= builder->getExperienceTracker()->getRank() &&
						HeroRoster::buttonUpgradesMet(button, m_player, builder))
					{
						selected = builder;
						break;
					}
				}
			}
			if (!selected || GameLogicRandomValueReal(0.0f, 1.0f) >= 0.75f)
				continue;
			const CanMakeType canMake = TheBuildAssistant->canMakeUnit(selected, type);
			if ((canMake == CANMAKE_OK || canMake == CANMAKE_NO_MONEY) &&
				spendWithSavings(type->calcCostToBuild(m_player), [&] {
					return constructRetailBuildingAt(*selected, *type, position) != nullptr;
				}) && trace)
				std::fprintf(stderr, "AI_FORWARD_BUILD\t%u\t%d\t%s\t%d\t%d\t%.9g\t%.9g\t%.9g\t%.9g\t%.9g\t%.9g\t%.9g\t%.9g\n",
					frame, playerIndex, type->getName().str(), defense->defendedStructure, selected->getID(),
					home.x, home.y, home.z, center.x, center.y, center.z, position.x, position.y);
			return;
		}
	};
	if (canRunTargetless && !updateRetailFlagCapture(owned, frame, playerIndex, trace))
		buildForward();
	std::vector<std::pair<Object*, Bool>> wave;
	for (Object* unit : soldiers)
	{
		if (unit->getTeam() != m_player->getDefaultTeam() || !unit->isMobile() || !unit->isAbleToAttack() ||
			collectingRing.contains(unit->getID()))
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
	if (wave.empty() && !ringPickup)
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
	const auto objectiveDistance = [&](const Object* objective, UnsignedInt owner, const Coord3D& point, Int minimum, Real& result) {
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
		if (count < minimum)
			return FALSE;
		result = total / INT_TO_REAL(count);
		return TRUE;
	};
	struct TacticalCandidate
	{
		Object* object = nullptr;
		const Coord3D* position = nullptr;
		UnsignedInt owner = 0;
		ObjectShroudStatus shroud = OBJECTSHROUD_INVALID;
		Real distance = 0.0f;
		const RetailDefenseCluster* defense = nullptr;
		Object* collector = nullptr;
		Bool ring = FALSE;
		ObjectID id() const { return object ? object->getID() : INVALID_ID; }
		ObjectID defendedID() const { return defense ? defense->buildings.front()->getID() : INVALID_ID; }
	};
	const auto visibleToPlayer = [&](const Object& object) {
		const ObjectShroudStatus shroud = object.getShroudedStatus(playerIndex);
		return (shroud == OBJECTSHROUD_CLEAR || shroud == OBJECTSHROUD_PARTIAL_CLEAR) &&
			(!object.testStatus(OBJECT_STATUS_STEALTHED) || object.testStatus(OBJECT_STATUS_DETECTED));
	};
	const auto visibleEnemy = [&](const Object& object) {
		const Player* owner = object.getControllingPlayer();
		if (!owner || !owner->isPlayableSide() || !owner->isPlayerActive() || object.isEffectivelyDead() ||
			object.isKindOf(KINDOF_INERT) || object.isKindOf(KINDOF_UNATTACKABLE) ||
			object.isKindOf(KINDOF_BASE_FOUNDATION) || m_player->getRelationship(object.getTeam()) != ENEMIES)
			return false;
		return visibleToPlayer(object);
	};
	std::vector<TacticalCandidate> structures;
	std::vector<TacticalCandidate> expansions;
	std::vector<TacticalCandidate> defenses;
	std::vector<TacticalCandidate> opportunities;
	std::vector<RetailDefenseCluster> defenseClusters;
	if (hasKind(RETAIL_TARGET_DEFENSIVE) && retailTuning.defenseTreeNodeRadius > 0.0f)
	{
		std::vector<Object*> defensesToRegister;
		for (Object* object : owned)
		{
			if (!object->isKindOf(KINDOF_STRUCTURE) || object->isKindOf(KINDOF_INERT) ||
				object->isKindOf(KINDOF_BASE_FOUNDATION) || object->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION))
				continue;
			if (!isOffensiveBuilding(*object->getTemplate()))
				defensesToRegister.push_back(object);
		}
		defenseClusters = makeDefenseClusters(std::move(defensesToRegister), retailTuning.defenseTreeNodeRadius);
		for (const RetailDefenseCluster& cluster : defenseClusters)
		{
			TacticalCandidate nearest;
			Real nearestDistance = 0.0f;
			for (Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
			{
				if (object->getContainedBy() || !object->isMobile() || !object->isAbleToAttack() || !visibleEnemy(*object))
					continue;
				const Real distance = tacticalDistanceSquared(*object->getPosition(), cluster.center);
				if (distance > sqr(cluster.radius) ||
					(nearest.object && std::make_pair(distance, object->getID()) >= std::make_pair(nearestDistance, nearest.id())))
					continue;
				TacticalCandidate candidate{object, object->getPosition(), object->getTeam()->getID(),
					object->getShroudedStatus(playerIndex), 0.0f, &cluster};
				if (objectiveDistance(object, candidate.owner, *candidate.position, 1, candidate.distance))
				{
					nearest = candidate;
					nearestDistance = distance;
				}
			}
			if (nearest.object)
				defenses.push_back(nearest);
		}
	}
	if (hasKind(RETAIL_TARGET_ENEMY_STRUCTURE))
		for (Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
		{
			if (!object->isKindOf(KINDOF_STRUCTURE) || !object->isKindOf(KINDOF_IMMOBILE) || !visibleEnemy(*object))
				continue;
			TacticalCandidate candidate{object, object->getPosition(), object->getTeam()->getID(), object->getShroudedStatus(playerIndex)};
			if (!objectiveDistance(object, candidate.owner, *candidate.position, RETAIL_ATTACK_WAVE_MIN_UNITS, candidate.distance))
				continue;
			if (object->isKindOf(KINDOF_COMMANDCENTER) || object->isKindOf(KINDOF_FS_BASE_DEFENSE))
				candidate.distance += sqr(RETAIL_DEFENDED_TARGET_DISTANCE);
			structures.push_back(candidate);
		}
	if (hasOpportunity)
	{
		if (ringOpportunity && visibleToPlayer(*ringOpportunity) &&
			(ringPickup || m_player->getRelationship(ringOpportunity->getTeam()) == ENEMIES))
		{
			TacticalCandidate candidate{ringOpportunity, ringOpportunity->getPosition(), ringOpportunity->getTeam()->getID(),
				ringOpportunity->getShroudedStatus(playerIndex), 0.0f, nullptr, nullptr, TRUE};
			if (ringPickup)
			{
				for (Object* unit : owned)
				{
					const AIUpdateInterface* ai = unit->getAI();
					if (!ai || !ai->isIdle() || !ai->canAcceptRetailPlannerOrder(FALSE) || !unit->isMobile() ||
						unit->isDisabled() || unit->getContainedBy() || unit->getTeam() != m_player->getDefaultTeam() ||
						m_retailObjectives.contains({unit->getID(), candidate.id(), candidate.owner}) ||
						collectingRing.contains(unit->getID()) || !canCollectRetailRing(*unit, *ringPickup))
						continue;
					const Real distance = tacticalDistanceSquared(*unit->getPosition(), *candidate.position);
					if (!candidate.collector || std::make_pair(distance, unit->getID()) <
						std::make_pair(candidate.distance, candidate.collector->getID()))
					{
						candidate.collector = unit;
						candidate.distance = distance;
					}
				}
			}
			if (candidate.collector || (!ringPickup && objectiveDistance(candidate.object, candidate.owner, *candidate.position,
				RETAIL_ATTACK_WAVE_MIN_UNITS, candidate.distance)))
				opportunities.push_back(candidate);
		}
		std::set<Int> screenedOpponents;
		std::vector<Object*> enemyBuilders;
		for (Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
		{
			const Player* owner = object->getControllingPlayer();
			if (!owner || !owner->isPlayableSide() || !owner->isPlayerActive() ||
				m_player->getRelationship(object->getTeam()) != ENEMIES)
				continue;
			if (screensRetailOpportunity(*object))
				screenedOpponents.insert(owner->getPlayerIndex());
			else if (object->isKindOf(KINDOF_DOZER) && !object->getContainedBy() && visibleEnemy(*object))
				enemyBuilders.push_back(object);
		}
		std::map<Int, TacticalCandidate> available;
		for (Object* object : enemyBuilders)
		{
			TacticalCandidate candidate{object, object->getPosition(), object->getTeam()->getID(), object->getShroudedStatus(playerIndex)};
			const Bool held = std::any_of(m_retailTargets.begin(), m_retailTargets.end(), [&](const auto& target) {
				return target.selectedFrame != 0 && target.object == candidate.id() && target.owner == candidate.owner;
			});
			const Int opponent = object->getControllingPlayer()->getPlayerIndex();
			if ((!held && screenedOpponents.contains(opponent)) ||
				!objectiveDistance(object, candidate.owner, *candidate.position, RETAIL_ATTACK_WAVE_MIN_UNITS, candidate.distance))
				continue;
			if (held)
				opportunities.push_back(candidate);
			else
			{
				auto& newest = available[opponent];
				if (!newest.object || candidate.id() > newest.id())
					newest = candidate;
			}
		}
		for (const auto& [opponent, candidate] : available)
			opportunities.push_back(candidate);
	}
	if (hasKind(RETAIL_TARGET_EXPANSION))
		for (UnsignedInt i = 0; i < static_cast<UnsignedInt>(ThePlayerList->getPlayerCount()); ++i)
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
			TacticalCandidate candidate{nullptr, start->getLocation(), i};
			if (objectiveDistance(nullptr, i, *candidate.position, RETAIL_ATTACK_WAVE_MIN_UNITS, candidate.distance))
				expansions.push_back(candidate);
		}
	const TacticalCandidate* chosen = nullptr;
	size_t chosenSlot = 0;
	for (Bool requireEmpty : {TRUE, FALSE})
	{
		for (size_t i = 0; i < m_retailTargets.size(); ++i)
		{
			const Int kind = rules->tacticalTargets[i];
			const auto* candidates = kind == RETAIL_TARGET_ENEMY_STRUCTURE ? &structures
				: kind == RETAIL_TARGET_DEFENSIVE ? &defenses
				: kind == RETAIL_TARGET_OPPORTUNITY ? &opportunities
				: kind == RETAIL_TARGET_EXPANSION ? &expansions : nullptr;
			auto& target = m_retailTargets[i];
			const Int limit = slotLimit(i);
			const Int teams = teamCount(target);
			if (!candidates || target.retiring || teams >= limit || (teams == 0) != requireEmpty)
				continue;
			std::vector<const TacticalCandidate*> defenseChoices;
			for (const auto& candidate : *candidates)
			{
				if (target.selectedFrame != 0)
				{
					if (target.object != candidate.id() || target.owner != candidate.owner ||
						target.defendedStructure != candidate.defendedID())
						continue;
				}
				else if (std::any_of(m_retailTargets.begin(), m_retailTargets.end(), [&](const RetailTacticalTarget& other) {
					return other.selectedFrame != 0 && other.object == candidate.id() && other.owner == candidate.owner;
				}))
					continue;
				if (candidate.defense && target.selectedFrame == 0)
				{
					defenseChoices.push_back(&candidate);
					continue;
				}
				if (!chosen || std::make_tuple(!candidate.ring, candidate.distance, candidate.id(), candidate.owner) <
					std::make_tuple(!chosen->ring, chosen->distance, chosen->id(), chosen->owner))
					chosen = &candidate;
			}
			if (!defenseChoices.empty())
			{
				chosen = defenseChoices[GameLogicRandomValue(0, static_cast<Int>(defenseChoices.size()) - 1)];
				if (chosen->defense->buildings.size() == 1)
				{
					const Int roll = GameLogicRandomValue(0, 100);
					if (trace)
						std::fprintf(stderr, "AI_DEFENSE_ROLL\t%u\t%d\t%d\t%d\n", frame, playerIndex, chosen->defendedID(), roll);
					if (roll >= 30)
						chosen = nullptr;
				}
			}
			if (chosen)
			{
				chosenSlot = i;
				break;
			}
		}
		if (chosen)
			break;
	}
	if (!chosen)
		return;
	const Int chosenKind = rules->tacticalTargets[chosenSlot];
	if (chosenKind == RETAIL_TARGET_ENEMY_STRUCTURE || chosenKind == RETAIL_TARGET_OPPORTUNITY)
	{
		const Int* chance = policy.difficulty ? policy.difficulty->offensiveTacticChance : nullptr;
		if ((difficulty == DIFFICULTY_EASY && phase == 0) ||
			(chance && chance[0] < chance[1] && GameLogicRandomValue(0, chance[1] - 1) >= chance[0]))
			return;
	}
	Object* target = chosen->object;
	const Coord3D* destination = chosen->position;
	const UnsignedInt objectiveOwner = chosen->owner;
	size_t defenseSize = 0;
	size_t recruitLimit = wave.size();
	Bool recruitAll = FALSE;
	if (chosen->defense)
	{
		defenseSize = chosen->defense->buildings.size();
		const size_t category = std::min<size_t>(defenseSize, 3) - 1;
		recruitAll = GameLogicRandomValueReal(0.0f, 1.0f) < rules->defenseRecruitAllChance[category];
		if (recruitAll)
			recruitLimit = 4;
		else if (category < 2)
			recruitLimit = category == 0 ? 1 : 3;
		else
		{
			const size_t roster = std::count_if(owned.begin(), owned.end(),
				[](const Object* object) { return isRetailDefenseRecruit(*object); });
			if (roster > 1)
				recruitLimit = roster / 2;
		}
		std::sort(wave.begin(), wave.end(), [&](const auto& a, const auto& b) {
			const auto rank = [&](const Object* unit) {
				return std::make_pair(tacticalDistanceSquared(*unit->getPosition(), *destination), unit->getID());
			};
			return rank(a.first) < rank(b.first);
		});
	}
	Int committed = 0;
	const auto issueOrder = [&](Object* unit, Bool collect) {
		AIUpdateInterface* ai = unit->getAI();
		Bool accepted;
		if (auto* horde = static_cast<HordeAIUpdate*>(unit->findUpdateModule(NAMEKEY("HordeAIUpdate"))))
			accepted = horde->issuePlannerOrder(collect ? nullptr : target, *destination);
		else if (target && !collect)
			accepted = ai->issueRetailPlannerAttack(target);
		else
			accepted = ai->issueRetailPlannerAdvance(*destination);
		if (!accepted)
			return;
		m_retailObjectives.emplace(std::make_tuple(unit->getID(), target ? target->getID() : INVALID_ID, objectiveOwner),
			RetailObjective{frame, 0});
		++committed;
		if (trace)
			std::fprintf(stderr, "AI_TARGET\t%u\t%d\t%s\t%d\t%d\t%d\n", frame, playerIndex,
				chosen->defense ? "defend" : collect ? "collect" : target ? "attack" : "scout",
				unit->getID(), target ? target->getID() : 0, chosen->shroud);
	};
	if (chosen->collector)
		issueOrder(chosen->collector, TRUE);
	else
	{
		for (const auto& [unit, ready] : wave)
		{
			if ((!ready && !target) || !eligible(*unit, target, objectiveOwner, *destination))
				continue;
			issueOrder(unit, FALSE);
			if (static_cast<size_t>(committed) >= recruitLimit)
				break;
		}
	}
	auto& assigned = m_retailTargets[chosenSlot];
	if (committed > 0 && assigned.selectedFrame == 0)
		assigned = {chosen->id(), chosen->defendedID(), objectiveOwner, frame, FALSE};
	if (trace && committed > 0 && chosen->defense)
		std::fprintf(stderr, "AI_DEFENSE\t%u\t%d\t%d\t%d\t%zu\t%d\t%d\n", frame, playerIndex,
			chosen->defendedID(), target->getID(), defenseSize, recruitAll, committed);
	if (trace && !chosen->defense && committed >= RETAIL_ATTACK_WAVE_MIN_UNITS)
	{
		const char* tactic = chosenKind == RETAIL_TARGET_OPPORTUNITY ? "opportunity" : target ? "structure" : "enemy-start";
		std::fprintf(stderr, "AI_WAVE\t%u\t%d\t%s\t%d\t%d\t%a\t%a\n", frame, playerIndex,
			tactic, committed, target ? target->getID() : 0, destination->x, destination->y);
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
	m_retailTargets.clear();
	m_retailWallPlan = {};
	m_retailLastFarmFrame = 0;
	m_retailSavings = 0;
	m_retailScavengedBuilding = -1;
	m_retailFarmPriority = 0.0f;
	m_retailBuiltStructures.clear();
	m_retailUnitPriorities.clear();
	m_retailUpgradePlans.clear();

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
	{
		xferRetailObjectives(xfer, TRUE);
		xferRetailBlocks(xfer, TRUE);
	}
}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info;
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void AISkirmishPlayer::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = RetailFileLayers::enabled() ? 4 : 1;
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
	if (RetailFileLayers::enabled())
		xferRetailBlocks(xfer, version >= 4);
}

void AISkirmishPlayer::xferRetailBlocks(Xfer* xfer, Bool present)
{
	const Bool loading = xfer->getXferMode() == XFER_LOAD;
	RetailSaveBlocks::transferSection(xfer, loading, present, {
		{"skirmish-spending", [this, loading](Xfer* x) {
			x->xferUnsignedInt(&m_retailLastFarmFrame);
			x->xferUnsignedInt(&m_retailSavings);
			x->xferInt(&m_retailScavengedBuilding);
			x->xferReal(&m_retailFarmPriority);
			UnsignedInt count = static_cast<UnsignedInt>(m_retailBuiltStructures.size());
			x->xferUnsignedInt(&count);
			if (loading)
			{
				m_retailBuiltStructures.clear();
				for (UnsignedInt i = 0; i < count; ++i)
				{
					AsciiString name;
					x->xferAsciiString(&name);
					m_retailBuiltStructures.insert(name);
				}
			}
			else
			{
				for (AsciiString name : m_retailBuiltStructures)
					x->xferAsciiString(&name);
			}
		}, [this] {
			m_retailLastFarmFrame = 0;
			m_retailSavings = 0;
			m_retailScavengedBuilding = -1;
			m_retailFarmPriority = 0.0f;
			m_retailBuiltStructures.clear();
		}},
		{"skirmish-targets", [this, loading](Xfer* x) {
			UnsignedInt count = static_cast<UnsignedInt>(m_retailTargets.size());
			x->xferUnsignedInt(&count);
			if (loading)
				m_retailTargets.clear();
			for (UnsignedInt i = 0; i < count; ++i)
			{
				auto& target = loading ? m_retailTargets.emplace_back() : m_retailTargets[i];
				x->xferObjectID(&target.object);
				x->xferObjectID(&target.defendedStructure);
				x->xferUnsignedInt(&target.owner);
				x->xferUnsignedInt(&target.selectedFrame);
				x->xferBool(&target.retiring);
			}
		}, [this] {
			m_retailTargets.clear();
		}},
		{"skirmish-unit-requests", [this, loading](Xfer* x) {
			UnsignedInt count = static_cast<UnsignedInt>(m_retailUnitPriorities.size());
			x->xferUnsignedInt(&count);
			if (loading)
			{
				m_retailUnitPriorities.clear();
				for (UnsignedInt i = 0; i < count; ++i)
				{
					AsciiString name;
					Real priority = 0.0f;
					x->xferAsciiString(&name);
					x->xferReal(&priority);
					m_retailUnitPriorities.emplace(name, priority);
				}
			}
			else
			{
				for (const auto& [name, adjustment] : m_retailUnitPriorities)
				{
					AsciiString type = name;
					Real priority = adjustment;
					x->xferAsciiString(&type);
					x->xferReal(&priority);
				}
			}
		}, [this] {
			m_retailUnitPriorities.clear();
		}},
		{"skirmish-wall-nodes", [this](Xfer* x) {
			x->xferObjectID(&m_retailWallPlan.defendedStructure);
			x->xferCoord3D(&m_retailWallPlan.center);
			x->xferCoord2D(&m_retailWallPlan.forward);
			for (ObjectID& node : m_retailWallPlan.nodes)
				x->xferObjectID(&node);
		}, [this] {
			m_retailWallPlan = {};
		}},
		{"skirmish-upgrade-requests", [this, loading](Xfer* x) {
			UnsignedInt count = static_cast<UnsignedInt>(m_retailUpgradePlans.size());
			x->xferUnsignedInt(&count);
			const auto transfer = [x](ObjectID& object, AsciiString& name, RetailUpgradePlan& plan) {
				x->xferObjectID(&object);
				x->xferAsciiString(&name);
				x->xferReal(&plan.priority);
				x->xferBool(&plan.admitted);
			};
			if (loading)
			{
				m_retailUpgradePlans.clear();
				for (UnsignedInt i = 0; i < count; ++i)
				{
					std::pair<ObjectID, AsciiString> key;
					RetailUpgradePlan plan;
					transfer(key.first, key.second, plan);
					m_retailUpgradePlans.emplace(std::move(key), plan);
				}
			}
			else
			{
				for (auto [key, plan] : m_retailUpgradePlans)
				{
					ObjectID object = key.first;
					AsciiString name = key.second;
					transfer(object, name, plan);
				}
			}
		}, [this] {
			m_retailUpgradePlans.clear();
		}}
	});
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

