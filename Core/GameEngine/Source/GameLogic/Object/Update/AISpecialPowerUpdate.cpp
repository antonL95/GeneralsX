#include "PreRTS.h"

#include "GameLogic/Module/AISpecialPowerUpdate.h"

#include "Common/ActionManager.h"
#include "Common/AttributeModifierRules.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/RandomValue.h"
#include "Common/RetailFileLayers.h"
#include "Common/RotwkDuration.h"
#include "Common/SpecialPower.h"
#include "Common/ThingTemplate.h"
#include "GameClient/ControlBar.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#if RTS_ZEROHOUR
#include "Common/Team.h"
#include "GameLogic/AISkirmishPlayer.h"
#include "GameLogic/HeroRoster.h"
#include "GameLogic/Module/AutoPickUpUpdate.h"
#include "GameLogic/Module/RotwkParseOnlyModules.h"
#include "GameLogic/Module/SpecialAbilityUpdate.h"
#include "GameLogic/Weapon.h"
#endif
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/HordeAIUpdate.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "WWMath/wwmath.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <vector>

namespace
{
const char* const specialPowerAITypes[] =
{
	"AI_SPECIAL_POWER_BASIC_SELF_BUFF",
	"AI_SPECIAL_POWER_CAPTURE_BUILDING",
	"AI_SPECIAL_POWER_ELENDIL",
	"AI_SPECIAL_POWER_ENEMY_TYPE_KILLER",
	"AI_SPECIAL_POWER_ENEMY_TYPE_KILLER_RANGED",
	"AI_SPECIAL_POWER_ENEMY_TYPE_KILLER_STRUCTURES",
	"AI_SPECIAL_POWER_GANDALF_WIZARD_BLAST",
	"AI_SPECIAL_POWER_GIVEXP_AOE",
	"AI_SPECIAL_POWER_RANGED_AOE_ATTACK",
	"AI_SPECIAL_POWER_TOGGLE_MOUNTED",
	"AI_SPECIAL_POWER_SELFAOEHEALHEROS",
	"AI_SPECIAL_POWER_TARGETAOE_SUMMON",
	"AI_SPECIAL_POWER_LEGOLAS_ARROWWIND",
	"AI_SPECIAL_POWER_LEGOLAS_TRAINARCHERS",
	"AI_SPECIAL_POWER_GOBLINKING_BATTLEFRENZY",
	"AI_SPECIAL_POWER_GOBLINKING_CALLOFTHEDEEP",
	"AI_SPECIAL_POWER_GOBLINKING_MOUNTED",
	"AI_SPECIAL_POWER_HEAL_AOE",
	"AI_SPECIAL_POWER_TOGGLE_MELEE_AND_RANGE",
	"AI_SPECIAL_POWER_TOGGLE_SIEGE",
	"AI_SPECIAL_POWER_CHARGE",
	"AI_SPELLBOOK_ALWAYS_FIRE",
	"AI_SPELLBOOK_ASSIST_BATTLE_BUFF",
	"AI_SPELLBOOK_ASSIST_BATTLE_DEBUFF",
	"AI_SPELLBOOK_ARMY_BREAKER",
	"AI_SPELLBOOK_CAPTURE_CREEP",
	"AI_SPELLBOOK_HEAL",
	"AI_SPELLBOOK_STRUCTURE_BREAKER",
	"AI_SPELLBOOK_STRUCTURE_BREAKER_PREF_WALLS",
	"AI_SPELLBOOK_ENSHROUDINGMIST",
	"AI_SPELLBOOK_BUFFTERRAIN",
	"AI_SPELLBOOK_REBUILD",
	"AI_SPELLBOOK_BUFFECONOMYBUILDING",
	"AI_SPELLBOOK_CALLTHEHORDE",
	"AI_SPELLBOOK_SHROUD_REVEAL",
	"AI_SPELLBOOK_TREE_KILLER",
	"AI_SPELLBOOK_STRUCTURE_BASEKILL",
	"AI_SPELLBOOK_CITADEL",
	"AI_SPECIAL_POWER_STANCEBATTLE",
	"AI_SPECIAL_POWER_STANCEAGGRESSIVE",
	"AI_SPECIAL_POWER_STANCEHOLDGROUND",
	"AI_SPELLBOOK_DEBUFFECONOMYBUILDING",
	"AI_SPELLBOOK_DEBUFFPRODUCTIONBUILDING",
	"AI_SPECIAL_POWER_AOE_AND_BUFF",
	"AI_SPECIAL_POWER_SOUL_FREEZE",
	"AI_SPECIAL_POWER_DOMINATE_ENEMY",
	"AI_SPECIAL_POWER_DOMINATE_TROLL",
	"AI_SPECIAL_POWER_TAME_THE_BEAST",
	"AI_SPECIAL_POWER_BASIC_SELF_DEBUFF",
	"AI_SPECIAL_POWER_ATTACK_HEAL_AOE",
	"AI_SPECIAL_POWER_RANGED_AOE_ATTACK_UNITS",
	"AI_SPECIAL_POWER_MORGUL_BLADE",
	"AI_SPECIAL_POWER_GOBLIN_POISON",
	nullptr
};

void parseSpecialPowerAIType(INI* ini, void*, void* store, const void*)
{
	const char* token = ini->getNextToken();
	if (stricmp(token, "AI_SPECIAL_POWER_SELFAOE_HEALHEROS") == 0)
		token = "AI_SPECIAL_POWER_SELFAOEHEALHEROS";
	*static_cast<Int*>(store) = INI::scanIndexList(token, specialPowerAITypes);
}

#if RTS_ZEROHOUR
constexpr UpdateSleepTime retailPolicyCooldown = UPDATE_SLEEP(LOGICFRAMES_PER_SECOND * 5);
constexpr UpdateSleepTime retailPolicyPoll = UPDATE_SLEEP(RotwkDuration::engineFrames(1, LOGICFRAMES_PER_SECOND));

enum class CombatPowerTargets
{
	None,
	Enemies,
	EnemyHeroes
};

CombatPowerTargets combatPowerTargets(std::string_view policy)
{
	if (policy == "AI_SPECIAL_POWER_CHARGE")
		return CombatPowerTargets::Enemies;
	if (policy == "AI_SPECIAL_POWER_MORGUL_BLADE")
		return CombatPowerTargets::EnemyHeroes;
	return CombatPowerTargets::None;
}

Int stancePolicy(std::string_view policy)
{
	using namespace AttributeModifierRules;
	if (policy == "AI_SPECIAL_POWER_STANCEBATTLE")
		return STANCE_BATTLE;
	if (policy == "AI_SPECIAL_POWER_STANCEAGGRESSIVE")
		return STANCE_AGGRESSIVE;
	if (policy == "AI_SPECIAL_POWER_STANCEHOLDGROUND")
		return STANCE_HOLD_GROUND;
	return STANCE_UNINITIALIZED;
}

Bool rollRetailPolicyActivation(const Player& player)
{
	return GameLogicRandomValueReal(0.0f, 1.0f) > 0.5f && AISkirmishPlayer::rollRetailSpecialPowerActivation(player);
}

Bool isActiveCommand(const Object& source, const CommandButton& button)
{
	const CommandSet* commands = TheRetailCommandCatalog->findCommandSet(source.getCommandSetString());
	for (Int slot = 0; commands && slot < CommandButtonRules::RETAIL_COMMANDS_PER_SET; ++slot)
		if (commands->getCommandButton(slot) == &button)
			return TRUE;
	return FALSE;
}

const Object* combatVictim(const Object& source)
{
	const auto* horde = static_cast<const HordeAIUpdate*>(source.findUpdateModule(NAMEKEY("HordeAIUpdate")));
	const AIUpdateInterface* ai = source.getAI();
	const Object* victim = horde ? horde->getAttackTarget() : (ai ? ai->getCurrentVictim() : nullptr);
	return victim && !victim->isEffectivelyDead() && source.getRelationship(victim) == ENEMIES ? victim : nullptr;
}

Bool canPoisonVictim(const Object& source, const Object& victim)
{
	constexpr Real poisonDistanceSquared = 10000.0f;
	return !victim.isKindOf(KINDOF_STRUCTURE) && !victim.isKindOf(KINDOF_MACHINE) &&
		!victim.isKindOf(KINDOF_SHIP) &&
		ThePartitionManager->getDistanceSquared(&source, &victim, FROM_CENTER_2D) < poisonDistanceSquared;
}

Bool canToggleWeapon(const Object& source, const Object& victim, Bool siege)
{
	const Weapon* weapon = source.getCurrentWeapon();
	if (!weapon)
		return FALSE;

	constexpr Real meleeDistanceSquared = 50.0f * 50.0f;
	constexpr Real rangedDistanceSquared = 150.0f * 150.0f;
	const Bool melee = weapon->getTemplate()->isMeleeWeapon();
	const Bool structure = victim.isKindOf(KINDOF_STRUCTURE);
	const Real distance = ThePartitionManager->getDistanceSquared(&source, &victim, FROM_CENTER_3D);
	if (siege)
		return melee ? structure : !structure && distance <= rangedDistanceSquared;
	return melee ? distance >= rangedDistanceSquared : distance <= meleeDistanceSquared;
}

Bool shouldToggleMount(Object& source, const Player& player)
{
	Team* team = source.getTeam();
	if (!team || team == player.getDefaultTeam())
		return FALSE;

	Int cavalryBalance = 0;
	team->iterateObjects([](Object* member, void* state) {
		const Object* container = member->getContainedBy();
		if (container && container->isKindOf(KINDOF_HORDE))
			return;
		constexpr Int cavalryThreatKind = 3;
		const auto& threats = member->getTemplate()->getRotWKObjectData().threatBreakdowns;
		const Bool cavalry = !threats.empty() && threats.back().aiKindOf == cavalryThreatKind;
		*static_cast<Int*>(state) += cavalry ? 1 : -1;
	}, &cavalryBalance);

	return source.getLogicModelConditions().test(MODELCONDITION_MOUNTED) ? cavalryBalance < 0 : cavalryBalance > 0;
}

Real bodyHealthFraction(const Object& object)
{
	const auto* body = object.getBodyModule();
	const Real maximum = body->getMaxHealth();
	return maximum > 0.0f ? body->getHealth() / maximum : 1.0f;
}

Bool shouldHealHeroes(const Object& source, Real radius)
{
	const Real threshold = 0.5f + GameLogicRandomValueReal(-0.1f, 0.1f);
	if (bodyHealthFraction(source) < threshold)
		return TRUE;
	if (!(radius > 0.0f) || !std::isfinite(radius))
		return FALSE;

	SimpleObjectIterator* nearby = ThePartitionManager->iterateObjectsInRange(&source, radius, FROM_CENTER_2D,
		nullptr, ITER_SORTED_NEAR_TO_FAR);
	MemoryPoolObjectHolder hold(nearby);
	for (const Object* hero = nearby->first(); hero; hero = nearby->next())
		if (hero != &source && !hero->isEffectivelyDead() && hero->isKindOf(KINDOF_HERO) &&
			source.getRelationship(hero) == ALLIES && bodyHealthFraction(*hero) < threshold)
			return TRUE;
	return FALSE;
}

Real combatHealthFraction(const Object& source)
{
	const auto* horde = HordeContain::get(&source);
	if (!horde)
		return bodyHealthFraction(source);

	const Int capacity = horde->getContainMax();
	if (capacity <= 0)
		return 1.0f;
	Real total = 0.0f;
	for (const Object* member : *horde->getContainedItemsList())
		if (member && !member->isEffectivelyDead())
			total += bodyHealthFraction(*member);
	return total / static_cast<Real>(capacity);
}

Bool canUseSelfBuff(const Object& source)
{
	constexpr Real selfBuffHealthLimit = 0.8f;
	return combatHealthFraction(source) < selfBuffHealthLimit;
}

UpdateSleepTime updateStancePolicy(Object& source, const Player& player, const CommandButton& button, Int policyStance)
{
	using namespace AttributeModifierRules;
	if (policyStance == STANCE_HOLD_GROUND)
		return UPDATE_SLEEP_FOREVER;

	const StancesBehavior* stances = StancesBehavior::find(&source);
	if (!stances || button.getCommandType() != GUI_COMMAND_SET_STANCE || button.getStances().empty() ||
		(button.getRequiredRank() > 0 && source.getExperienceTracker()->getRank() < button.getRequiredRank()) ||
		(BitIsSet(button.getOptions(), NEED_UPGRADE) && !HeroRoster::buttonUpgradesMet(&button, &player, &source)) ||
		(policyStance == STANCE_AGGRESSIVE && stances->getStance() == STANCE_AGGRESSIVE))
		return retailPolicyPoll;

	constexpr Real battleHealthLimit = 0.35f;
	const Bool battle = combatHealthFraction(source) <= battleHealthLimit;
	if ((policyStance == STANCE_BATTLE) != battle || !button.isModelConditionAllowed(&source) ||
		!button.isValidContainerForCommand(&source) || !rollRetailPolicyActivation(player))
		return retailPolicyPoll;

	source.doCommandButton(&button, CMD_FROM_AI);
	return retailPolicyCooldown;
}

Real specialPowerSearchRange(const Object& source, const SpecialPowerTemplate& power, Real authoredRange)
{
	if (authoredRange > 0.0f)
		return authoredRange;
	for (BehaviorModule** mod = source.getBehaviorModules(); *mod; ++mod)
	{
		const auto* update = (*mod)->getSpecialPowerUpdateInterface();
		if (!update || !update->isSpecialAbility())
			continue;
		const Real range = static_cast<const SpecialAbilityUpdate*>(update)->getStartAbilityRangeFor(&power);
		if (range > 0.0f)
			return range;
	}
	return 0.0f;
}

void tracePowerTarget(const Object& source, const SpecialPowerTemplate& power, const Object& target, ObjectShroudStatus shroud)
{
	static const Bool trace = std::getenv("GENERALSX_SKIRMISH_AI_TRACE") != nullptr;
	if (trace)
	{
		std::fprintf(stderr, "AI_TARGET\t%u\t%d\t%s\t%d\t%d\t%d\n", TheGameLogic->getFrame(),
			source.getControllingPlayer()->getPlayerIndex(), power.getName().str(), source.getID(), target.getID(), shroud);
		std::fflush(stderr);
	}
}

Object* selectCombatPowerTarget(const Object& source, const Player& player, const CommandButton& button,
	const SpecialPowerTemplate& power, const SpecialPowerModuleInterface& module, CombatPowerTargets targets, Real range)
{
	if (!(range > 0.0f) || !std::isfinite(range))
		return nullptr;

	SimpleObjectIterator* nearby = ThePartitionManager->iterateObjectsInRange(&source, range, FROM_CENTER_2D);
	MemoryPoolObjectHolder hold(nearby);
	std::vector<Object*> candidates;
	for (Object* target = nearby->first(); target; target = nearby->next())
	{
		if (target->isEffectivelyDead() || target->getContainedBy() || target->isKindOf(KINDOF_INERT) ||
			source.getRelationship(target) != ENEMIES ||
			(targets == CombatPowerTargets::EnemyHeroes && !target->isKindOf(KINDOF_HERO) && !target->isKindOf(KINDOF_CREATE_A_HERO)))
			continue;
		candidates.push_back(target);
	}
	std::sort(candidates.begin(), candidates.end(), [](const Object* left, const Object* right) {
		return left->getID() < right->getID();
	});

	Object* best = nullptr;
	Real bestHealth = 0.0f;
	for (Object* target : candidates)
	{
		const auto shroud = target->getShroudedStatus(player.getPlayerIndex());
		if ((shroud != OBJECTSHROUD_CLEAR && shroud != OBJECTSHROUD_PARTIAL_CLEAR) ||
			(target->testStatus(OBJECT_STATUS_STEALTHED) && !target->testStatus(OBJECT_STATUS_DETECTED)))
			continue;
		const Real health = bodyHealthFraction(*target);
		if ((best && health >= bestHealth) || !module.isValidPowerTarget(target) ||
			!button.isValidToUseOn(&source, target, target->getPosition(), CMD_FROM_AI) ||
			!power.isValidRetailTarget(&source, target, nullptr, CMD_FROM_AI))
			continue;
		best = target;
		bestHealth = health;
	}
	return best;
}

Bool canSummonAt(const Object& source, const CommandButton& button, const SpecialPowerTemplate& power,
	const AISpecialPowerUpdateModuleData& data, const Coord3D& location)
{
	if (!button.isValidToUseOn(&source, nullptr, &location, CMD_FROM_AI) ||
		!power.isValidRetailTarget(&source, nullptr, &location, CMD_FROM_AI))
		return FALSE;
	if (data.m_spellMakesAStructure)
	{
		constexpr Real structureClearance = 150.0f;
		SimpleObjectIterator* nearby = ThePartitionManager->iterateObjectsInRange(&location, structureClearance, FROM_CENTER_2D);
		MemoryPoolObjectHolder hold(nearby);
		for (const Object* object = nearby->first(); object; object = nearby->next())
			if (!object->isEffectivelyDead() && object->isKindOf(KINDOF_STRUCTURE))
				return FALSE;
	}
	return TRUE;
}

std::array<Coord3D, 4> diagonalPowerLocations(const Object& source, Real radius)
{
	const Real offset = radius / WWMath::Sqrt(2.0f);
	const Coord3D& center = *source.getPosition();
	return {{
		{center.x + offset, center.y + offset, center.z},
		{center.x - offset, center.y + offset, center.z},
		{center.x - offset, center.y - offset, center.z},
		{center.x + offset, center.y - offset, center.z}
	}};
}

Int countVisiblePowerEnemies(const Object& source, const Player& player, const Coord3D& center, Real radius,
	Bool includeHordeMembers)
{
	SimpleObjectIterator* nearby = ThePartitionManager->iterateObjectsInRange(&center, radius, FROM_CENTER_2D);
	MemoryPoolObjectHolder hold(nearby);
	Int count = 0;
	for (const Object* object = nearby->first(); object; object = nearby->next())
	{
		const Object* container = object->getContainedBy();
		if (source.getRelationship(object) != ENEMIES || object->isEffectivelyDead() ||
			(container && (!includeHordeMembers || !container->isKindOf(KINDOF_HORDE))) || object->isKindOf(KINDOF_INERT) ||
			(object->testStatus(OBJECT_STATUS_STEALTHED) && !object->testStatus(OBJECT_STATUS_DETECTED)))
			continue;
		const auto shroud = object->getShroudedStatus(player.getPlayerIndex());
		if (shroud == OBJECTSHROUD_CLEAR || shroud == OBJECTSHROUD_PARTIAL_CLEAR)
			++count;
	}
	return count;
}

Bool canCastPolicyAtPosition(const Object& source, const CommandButton& button, const SpecialPowerTemplate& power,
	const Coord3D& location)
{
	if (!button.isMountStateAllowed(&source) ||
		(button.getRequiredRank() > 0 && source.getExperienceTracker()->getRank() < button.getRequiredRank()))
		return FALSE;
	return power.isValidRetailTarget(&source, nullptr, &location, CMD_FROM_AI) &&
		(BitIsSet(button.getOptions(), COMMAND_OPTION_NEED_OBJECT_TARGET) ||
			button.isValidToUseOn(&source, nullptr, &location, CMD_FROM_AI));
}

std::optional<Coord3D> selectWizardBlastLocation(const Object& source, const Player& player, const CommandButton& button,
	const SpecialPowerTemplate& power, Real radius)
{
	if (!(radius >= 0.0f) || !std::isfinite(radius))
		return std::nullopt;
	constexpr Real emergencyHealth = 0.3f;
	constexpr Int minimumEnemies = 21;
	Int bestCount = bodyHealthFraction(source) < emergencyHealth ? 0 : minimumEnemies - 1;
	std::optional<Coord3D> best;
	for (const Coord3D& location : diagonalPowerLocations(source, radius))
	{
		const Int count = countVisiblePowerEnemies(source, player, location, radius, TRUE);
		if (count > bestCount)
		{
			bestCount = count;
			best = location;
		}
	}
	return best && canCastPolicyAtPosition(source, button, power, *best) ? best : std::nullopt;
}

std::optional<Coord3D> findSummonLocation(const Object& source, const CommandButton& button, const SpecialPowerTemplate& power,
	const AISpecialPowerUpdateModuleData& data, const Coord3D& location)
{
	if (canSummonAt(source, button, power, data, location))
		return location;
	if (!data.m_randomizeTargetLocation)
		return std::nullopt;

	const Real dx = GameLogicRandomValueReal(-1.0f, 1.0f);
	const Real dy = GameLogicRandomValueReal(-1.0f, 1.0f);
	const Real length = WWMath::Sqrt(dx * dx + dy * dy);
	if (length <= 0.0f)
		return std::nullopt;

	constexpr Real retryStep = 15.0f;
	constexpr Real retryDistance = 150.0f;
	for (Real distance = retryStep; distance <= retryDistance; distance += retryStep)
	{
		const Coord3D shifted{location.x + dx / length * distance, location.y + dy / length * distance, location.z};
		if (canSummonAt(source, button, power, data, shifted))
			return shifted;
	}
	return std::nullopt;
}

std::optional<Coord3D> selectSummonLocation(const Object& source, const Player& player, const CommandButton& button,
	const SpecialPowerTemplate& power, const AISpecialPowerUpdateModuleData& data, Real defaultRange)
{
	const Real configuredRange = specialPowerSearchRange(source, power, data.m_specialPowerRange);
	const Real range = configuredRange > 0.0f ? configuredRange : defaultRange;
	if (!std::isfinite(range))
		return std::nullopt;
	constexpr Int minimumEnemies = 2;
	Int bestCount = minimumEnemies - 1;
	std::optional<Coord3D> best;
	for (const Coord3D& location : diagonalPowerLocations(source, range))
	{
		const Int count = countVisiblePowerEnemies(source, player, location, range, FALSE);
		if (count > bestCount)
		{
			if (const auto validLocation = findSummonLocation(source, button, power, data, location))
			{
				best = validLocation;
				bestCount = count;
			}
		}
	}
	return best;
}

std::optional<Coord3D> selectAreaBuffLocation(const Object& source, const Player& player, const CommandButton& button,
	const SpecialPowerTemplate& power, const AISpecialPowerUpdateModuleData& data)
{
	const Real range = specialPowerSearchRange(source, power, data.m_specialPowerRange);
	if (!(range > 0.0f) || !std::isfinite(range))
		return std::nullopt;

	SimpleObjectIterator* nearby = ThePartitionManager->iterateObjectsInRange(source.getPosition(), range, FROM_CENTER_2D);
	MemoryPoolObjectHolder hold(nearby);
	Int allies = 0;
	const Object* nearest = nullptr;
	Real nearestDistance = 0.0f;
	for (const Object* candidate = nearby->first(); candidate; candidate = nearby->next())
	{
		if (candidate->isEffectivelyDead() || candidate->isKindOf(KINDOF_STRUCTURE) ||
			candidate->isKindOf(KINDOF_IGNORES_SELECT_ALL) ||
			(!candidate->isKindOf(KINDOF_CAN_ATTACK) && !candidate->isKindOf(KINDOF_HERO)))
			continue;
		const auto relationship = source.getRelationship(candidate);
		if (relationship == ALLIES)
		{
			++allies;
			continue;
		}
		if (relationship != ENEMIES || (candidate->testStatus(OBJECT_STATUS_STEALTHED) &&
			!candidate->testStatus(OBJECT_STATUS_DETECTED)))
			continue;
		const auto shroud = candidate->getShroudedStatus(player.getPlayerIndex());
		if (shroud != OBJECTSHROUD_CLEAR && shroud != OBJECTSHROUD_PARTIAL_CLEAR)
			continue;
		const Real distance = ThePartitionManager->getDistanceSquared(&source, candidate, FROM_CENTER_2D);
		if (!nearest || distance < nearestDistance || (distance == nearestDistance && candidate->getID() < nearest->getID()))
		{
			nearest = candidate;
			nearestDistance = distance;
		}
	}

	constexpr Int minimumAllies = 2;
	constexpr Real minimumRangeFraction = 0.8f;
	const Real minimumRange = range * minimumRangeFraction;
	if (allies < minimumAllies || !nearest ||
		ThePartitionManager->getDistanceSquared(&source, nearest, FROM_CENTER_3D) < minimumRange * minimumRange)
		return std::nullopt;
	return findSummonLocation(source, button, power, data, *nearest->getPosition());
}

void scoutWithPower(Object& source, const CommandButton& button, const SpecialPowerTemplate& power, Real range)
{
	constexpr Int samplesPerAxis = 16;
	Player* player = source.getControllingPlayer();
	const Coord3D& origin = *source.getPosition();
	const Real castRange = power.getMaxCastRange();
	if (power.hasRetailFlag(SpecialPowerTemplate::RETAIL_LIMIT_DISTANCE) && castRange > 0.0f)
		range = range > 0.0f ? std::min(range, castRange) : castRange;

	Region3D search;
	TheTerrainLogic->getExtent(&search);
	if (range > 0.0f)
	{
		search.lo.x = std::max(search.lo.x, origin.x - range);
		search.lo.y = std::max(search.lo.y, origin.y - range);
		search.hi.x = std::min(search.hi.x, origin.x + range);
		search.hi.y = std::min(search.hi.y, origin.y + range);
	}
	if (search.width() <= 0.0f || search.height() <= 0.0f)
		return;

	const auto startFor = [](Player& slot) -> const Waypoint* {
		const Int index = slot.getMpStartIndex();
		if (index < 0 || index >= MAX_PLAYER_COUNT)
			return nullptr;
		AsciiString name;
		name.format("Player_%d_Start", index + 1);
		return TheTerrainLogic->getWaypointByName(name);
	};
	const Waypoint* ownStart = startFor(*player);
	const Coord3D& from = ownStart && range <= 0.0f ? *ownStart->getLocation() : origin;

	for (const auto desired : {CELLSHROUD_SHROUDED, CELLSHROUD_FOGGED})
	{
		const auto tryPosition = [&](Coord3D position) {
			if (!search.containsWithin(position.asCoord2D()) ||
				(range > 0.0f && ThePartitionManager->getDistanceSquared(&source, &position, FROM_CENTER_2D) > range * range) ||
				ThePartitionManager->getShroudStatusForPlayer(player->getPlayerIndex(), &position) != desired)
				return FALSE;
			position.z = TheTerrainLogic->getGroundHeight(position.x, position.y);
			if (!button.isValidToUseOn(&source, nullptr, &position, CMD_FROM_AI) ||
				!power.isValidRetailTarget(&source, nullptr, &position, CMD_FROM_AI))
				return FALSE;
			if (!AISkirmishPlayer::rollRetailSpecialPowerActivation(*player))
				return TRUE;

			static const Bool trace = std::getenv("GENERALSX_SKIRMISH_AI_TRACE") != nullptr;
			if (trace)
			{
				std::fprintf(stderr, "AI_SCOUT\t%u\t%d\t%s\t%a\t%a\t%d\n", TheGameLogic->getFrame(),
					player->getPlayerIndex(), power.getName().str(), position.x, position.y, desired);
				std::fflush(stderr);
			}
			source.doSpecialPowerAtLocation(&power, &position, INVALID_ANGLE, button.getOptions());
			return TRUE;
		};

		for (Int i = 0; i < ThePlayerList->getPlayerCount(); ++i)
		{
			Player* opponent = ThePlayerList->getNthPlayer(i);
			if (!opponent || !opponent->isPlayableSide() || !opponent->isPlayerActive() ||
				player->getRelationship(opponent->getDefaultTeam()) != ENEMIES)
				continue;
			const Waypoint* start = startFor(*opponent);
			if (!start)
				continue;
			const Coord3D direction = *start->getLocation() - from;
			for (Int step = samplesPerAxis; step > 0; --step)
			{
				const Real fraction = INT_TO_REAL(step) / samplesPerAxis;
				if (tryPosition({from.x + direction.x * fraction, from.y + direction.y * fraction, 0.0f}))
					return;
			}
		}

		for (Int y = 0; y < samplesPerAxis; ++y)
			for (Int x = 0; x < samplesPerAxis; ++x)
				if (tryPosition({search.lo.x + search.width() * (INT_TO_REAL(x) + 0.5f) / samplesPerAxis,
					search.lo.y + search.height() * (INT_TO_REAL(y) + 0.5f) / samplesPerAxis, 0.0f}))
					return;
	}
}
#endif
}

void AISpecialPowerUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpdateModuleData::buildFieldParse(p);

	static const FieldParse fields[] =
	{
		{ "CommandButtonName", INI::parseAsciiString, nullptr, offsetof( AISpecialPowerUpdateModuleData, m_commandButtonName ) },
		{ "SpecialPowerAIType", parseSpecialPowerAIType, nullptr, offsetof( AISpecialPowerUpdateModuleData, m_specialPowerAIType ) },
		{ "SpecialPowerRadius", INI::parseReal, nullptr, offsetof( AISpecialPowerUpdateModuleData, m_specialPowerRadius ) },
		{ "SpecialPowerRange", INI::parseReal, nullptr, offsetof( AISpecialPowerUpdateModuleData, m_specialPowerRange ) },
		{ "RandomizeTargetLocation", INI::parseBool, nullptr, offsetof( AISpecialPowerUpdateModuleData, m_randomizeTargetLocation ) },
		{ "SpellMakesAStructure", INI::parseBool, nullptr, offsetof( AISpecialPowerUpdateModuleData, m_spellMakesAStructure ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

AISpecialPowerUpdate::AISpecialPowerUpdate(Thing* thing, const ModuleData* data)
	: UpdateModule(thing, data)
{
#if RTS_ZEROHOUR
	const Int type = getAISpecialPowerUpdateModuleData()->m_specialPowerAIType;
	const std::string_view policy(type >= 0 ? specialPowerAITypes[type] : "");
	if (stancePolicy(policy) != AttributeModifierRules::STANCE_UNINITIALIZED ||
		combatPowerTargets(policy) != CombatPowerTargets::None || policy == "AI_SPECIAL_POWER_TARGETAOE_SUMMON")
		setWakeFrame(getObject(), retailPolicyCooldown);
#endif
}

AISpecialPowerUpdate::~AISpecialPowerUpdate() = default;

UpdateSleepTime AISpecialPowerUpdate::update()
{
#if RTS_ZEROHOUR
	if (!RetailFileLayers::enabled())
		return UPDATE_SLEEP_FOREVER;
	Object* source = getObject();
	const UpdateSleepTime checkAgain = UPDATE_SLEEP(LOGICFRAMES_PER_SECOND * 2);
	Player* player = source->getControllingPlayer();
	if (!player || !player->isSkirmishAIPlayer() || !player->isPlayerActive() || !TheRetailCommandCatalog ||
		source->isDestroyed() || source->isDisabled() ||
		source->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION) || source->testStatus(OBJECT_STATUS_IS_USING_ABILITY))
		return checkAgain;
	const Bool book = source->isKindOf(KINDOF_SPELL_BOOK) && source == player->getRetailSpellBook();
	if (source->isEffectivelyDead() && !book)
		return checkAgain;

	const auto* data = getAISpecialPowerUpdateModuleData();
	const CommandButton* button = TheRetailCommandCatalog->findCommandButton(data->m_commandButtonName);
	if (!button || data->m_specialPowerAIType < 0 || !isActiveCommand(*source, *button))
		return checkAgain;
	const std::string_view policy(specialPowerAITypes[data->m_specialPowerAIType]);
	const Int stance = stancePolicy(policy);
	if (stance != AttributeModifierRules::STANCE_UNINITIALIZED)
		return updateStancePolicy(*source, *player, *button, stance);

	const Bool siege = policy == "AI_SPECIAL_POWER_TOGGLE_SIEGE";
	const Bool weaponToggle = siege || policy == "AI_SPECIAL_POWER_TOGGLE_MELEE_AND_RANGE";
	const Bool formationButton = button->getCommandType() == GUI_COMMAND_HORDE_TOGGLE_FORMATION;
	if (weaponToggle && (formationButton || button->getCommandType() == GUI_COMMAND_TOGGLE_WEAPONSET))
	{
		const Object* victim = combatVictim(*source);
		if (!victim || !canToggleWeapon(*source, *victim, siege) || !button->canUseRetailToggle(source, -1) ||
			!rollRetailPolicyActivation(*player))
			return retailPolicyPoll;
		if (formationButton)
		{
			auto* horde = const_cast<HordeContain*>(HordeContain::get(source));
			horde->setAlternateFormation(!horde->isAlternateFormation());
		}
		else
			source->doCommandButton(button, CMD_FROM_AI);
		return retailPolicyCooldown;
	}

	const Bool selfBuff = policy == "AI_SPECIAL_POWER_BASIC_SELF_BUFF";
	if (button->getCommandType() == GUI_COMMAND_WAKE_AUTO_PICKUP)
	{
		if (selfBuff && AutoPickUpUpdate::canScanFromButton(source, button))
			source->doCommandButton(button, CMD_FROM_AI);
		return checkAgain;
	}
	const SpecialPowerTemplate* power = button->getSpecialPowerTemplate();
	if (!power || TheRetailCommandCatalog->findPowerButton(source, power) != button ||
		(BitIsSet(button->getOptions(), NEED_UPGRADE) && !HeroRoster::buttonUpgradesMet(button, player, source)))
		return checkAgain;
	const auto* module = source->getSpecialPowerModule(power);
	if (!module || !module->isReady() || !module->isRetailCastAllowed(button->getOptions()) ||
		!TheSpecialPowerStore->canUseSpecialPower(source, power))
		return checkAgain;

	if (weaponToggle || policy == "AI_SPECIAL_POWER_TOGGLE_MOUNTED")
	{
		if (weaponToggle)
		{
			const Object* victim = combatVictim(*source);
			if (!victim || !canToggleWeapon(*source, *victim, siege))
				return retailPolicyPoll;
		}
		else if (!shouldToggleMount(*source, *player))
			return retailPolicyPoll;

		const UnsignedInt options = button->getOptions();
		const Coord3D* location = BitIsSet(options, NEED_TARGET_POS) ? source->getPosition() : nullptr;
		if (!button->isValidToUseOn(source, nullptr, location, CMD_FROM_AI) ||
			!power->isValidRetailTarget(source, nullptr, location, CMD_FROM_AI) || !rollRetailPolicyActivation(*player))
			return retailPolicyPoll;
		if (location)
			source->doSpecialPowerAtLocation(power, location, INVALID_ANGLE, options);
		else
			source->doSpecialPower(power, options);
		return retailPolicyCooldown;
	}

	if (policy == "AI_SPECIAL_POWER_SELFAOEHEALHEROS")
	{
		const UnsignedInt options = button->getOptions();
		if (!button->isValidToUseOn(source, nullptr, nullptr, CMD_FROM_AI) ||
			!power->isValidRetailTarget(source, nullptr, nullptr, CMD_FROM_AI) ||
			GameLogicRandomValueReal(0.0f, 1.0f) <= 0.5f ||
			!shouldHealHeroes(*source, data->m_specialPowerRadius) || !AISkirmishPlayer::rollRetailSpecialPowerActivation(*player))
			return retailPolicyPoll;
		source->doSpecialPower(power, options);
		return retailPolicyCooldown;
	}

	const Bool wizardBlast = policy == "AI_SPECIAL_POWER_GANDALF_WIZARD_BLAST";
	const Bool callOfTheDeep = policy == "AI_SPECIAL_POWER_GOBLINKING_CALLOFTHEDEEP";
	if (wizardBlast || callOfTheDeep || policy == "AI_SPECIAL_POWER_TARGETAOE_SUMMON")
	{
		if (!combatVictim(*source) || GameLogicRandomValueReal(0.0f, 1.0f) <= 0.5f)
			return retailPolicyPoll;
		constexpr Real defaultSummonRange = 149.0f;
		constexpr Real callOfTheDeepRange = 200.0f;
		const auto location = wizardBlast
			? selectWizardBlastLocation(*source, *player, *button, *power, data->m_specialPowerRadius)
			: selectSummonLocation(*source, *player, *button, *power, *data, callOfTheDeep ? callOfTheDeepRange : defaultSummonRange);
		if (!location || !AISkirmishPlayer::rollRetailSpecialPowerActivation(*player))
			return retailPolicyPoll;
		source->doSpecialPowerAtLocation(power, &*location, INVALID_ANGLE, button->getOptions());
		return retailPolicyCooldown;
	}

	if (policy == "AI_SPECIAL_POWER_AOE_AND_BUFF")
	{
		if (GameLogicRandomValueReal(0.0f, 1.0f) <= 0.5f)
			return retailPolicyPoll;
		const auto location = selectAreaBuffLocation(*source, *player, *button, *power, *data);
		if (!location || !AISkirmishPlayer::rollRetailSpecialPowerActivation(*player))
			return retailPolicyPoll;
		source->doSpecialPowerAtLocation(power, &*location, INVALID_ANGLE, button->getOptions());
		return retailPolicyCooldown;
	}

	const auto combatTargets = combatPowerTargets(policy);
	if (combatTargets != CombatPowerTargets::None)
	{
		if (!combatVictim(*source))
			return retailPolicyPoll;
		const Real range = specialPowerSearchRange(*source, *power, data->m_specialPowerRange);
		Object* target = selectCombatPowerTarget(*source, *player, *button, *power, *module, combatTargets, range);
		if (!target || !rollRetailPolicyActivation(*player))
			return retailPolicyPoll;
		tracePowerTarget(*source, *power, *target, target->getShroudedStatus(player->getPlayerIndex()));
		source->doSpecialPowerAtObject(power, target, button->getOptions());
		return retailPolicyCooldown;
	}

	const Bool always = policy == "AI_SPELLBOOK_ALWAYS_FIRE";
	const Bool allyBattle = policy == "AI_SPELLBOOK_ASSIST_BATTLE_BUFF" ||
		policy == "AI_SPELLBOOK_BUFFTERRAIN" || policy == "AI_SPECIAL_POWER_GIVEXP_AOE";
	const Bool repair = policy == "AI_SPELLBOOK_REBUILD";
	const Bool heal = policy == "AI_SPELLBOOK_HEAL" || policy == "AI_SPECIAL_POWER_HEAL_AOE";
	const Bool allyEconomy = policy == "AI_SPELLBOOK_BUFFECONOMYBUILDING";
	const Bool enemyStructure = policy == "AI_SPELLBOOK_STRUCTURE_BREAKER" ||
		policy == "AI_SPELLBOOK_STRUCTURE_BREAKER_PREF_WALLS" || policy == "AI_SPELLBOOK_STRUCTURE_BASEKILL" ||
		policy == "AI_SPELLBOOK_DEBUFFECONOMYBUILDING" || policy == "AI_SPELLBOOK_DEBUFFPRODUCTIONBUILDING" ||
		policy == "AI_SPECIAL_POWER_ENEMY_TYPE_KILLER_STRUCTURES";
	const Bool enemyArmy = policy == "AI_SPELLBOOK_ASSIST_BATTLE_DEBUFF" || policy == "AI_SPELLBOOK_ARMY_BREAKER" ||
		policy == "AI_SPECIAL_POWER_RANGED_AOE_ATTACK" || policy == "AI_SPECIAL_POWER_RANGED_AOE_ATTACK_UNITS" ||
		policy == "AI_SPECIAL_POWER_ENEMY_TYPE_KILLER_RANGED";
	const Bool enemyTypeKiller = policy == "AI_SPECIAL_POWER_ENEMY_TYPE_KILLER";
	const Bool goblinPoison = policy == "AI_SPECIAL_POWER_GOBLIN_POISON";
	const Bool reveal = policy == "AI_SPELLBOOK_SHROUD_REVEAL";
	const Bool captureCreep = policy == "AI_SPELLBOOK_CAPTURE_CREEP";
	const Bool selfDebuff = policy == "AI_SPECIAL_POWER_BASIC_SELF_DEBUFF";
	if (!(always || allyBattle || repair || heal || allyEconomy || enemyStructure || enemyArmy ||
		reveal || captureCreep || selfBuff || selfDebuff || goblinPoison || enemyTypeKiller))
		return checkAgain;

	const UnsignedInt options = button->getOptions();
	const Bool objectTarget = (options & COMMAND_OPTION_NEED_OBJECT_TARGET) != 0;
	const Bool positionTarget = (options & NEED_TARGET_POS) != 0;
	const Object* victim = nullptr;
	if (selfBuff || goblinPoison || enemyTypeKiller)
	{
		victim = combatVictim(*source);
		if (!victim)
			return checkAgain;
	}
	if (selfBuff || goblinPoison)
	{
		if (!objectTarget && !positionTarget &&
			(goblinPoison ? canPoisonVictim(*source, *victim) : canUseSelfBuff(*source)) &&
			button->isValidToUseOn(source, nullptr, nullptr, CMD_FROM_AI) &&
			AISkirmishPlayer::rollRetailSpecialPowerActivation(*player))
		{
			tracePowerTarget(*source, *power, *victim, victim->getShroudedStatus(player->getPlayerIndex()));
			source->doSpecialPower(power, options);
		}
		return checkAgain;
	}
	if (!objectTarget && !positionTarget && (always ||
		(selfDebuff && source->getAI() && source->getAI()->isAttacking())))
	{
		if (button->isValidToUseOn(source, nullptr, nullptr, CMD_FROM_AI) &&
			AISkirmishPlayer::rollRetailSpecialPowerActivation(*player))
			source->doSpecialPower(power, options);
		return checkAgain;
	}

	Real range = data->m_specialPowerRange > 0.0f || (book && captureCreep)
		? data->m_specialPowerRange : data->m_specialPowerRadius;
	if (enemyTypeKiller)
	{
		range = specialPowerSearchRange(*source, *power, data->m_specialPowerRange);
		if (range <= 0.0f)
			range = power->getMaxCastRange();
		if (range <= 0.0f)
			return checkAgain;
	}
	if (reveal && positionTarget && !objectTarget)
	{
		scoutWithPower(*source, *button, *power, book ? 0.0f : range);
		return checkAgain;
	}

	Object* best = nullptr;
	ObjectShroudStatus bestShroud = OBJECTSHROUD_INVALID;
	Real bestScore = 0.0f;
	for (Object* target = TheGameLogic->getFirstObject(); target; target = target->getNextObject())
	{
		if (target->isEffectivelyDead() || target->isKindOf(KINDOF_INERT) ||
			(target->getContainedBy() && !(heal && HordeContain::get(target->getContainedBy()))))
			continue;
		const Relationship relationship = source->getRelationship(target);
		const auto shroud = target->getShroudedStatus(player->getPlayerIndex());
		if (shroud != OBJECTSHROUD_CLEAR && shroud != OBJECTSHROUD_PARTIAL_CLEAR)
			continue;
		if (relationship == ENEMIES && target->testStatus(OBJECT_STATUS_STEALTHED) && !target->testStatus(OBJECT_STATUS_DETECTED))
			continue;
		const Bool building = target->isKindOf(KINDOF_STRUCTURE);
		AIUpdateInterface* ai = target->getAI();
		Real score = 0.0f;
		if (enemyTypeKiller)
		{
			if (relationship == ENEMIES && (target->isKindOf(KINDOF_HERO) || target->isKindOf(KINDOF_MONSTER) ||
				target->isKindOf(KINDOF_BIG_MONSTER) || target->isKindOf(KINDOF_CREATE_A_HERO)))
				score = 2.0f - bodyHealthFraction(*target);
		}
		else if (captureCreep)
		{
			if (building && relationship == ENEMIES && !target->getControllingPlayer()->isPlayableSide() &&
				module->isValidPowerTarget(target))
				score = 1.0f;
		}
		else if (relationship == ALLIES)
		{
			if ((heal && !building) || (repair && building))
			{
				const auto* body = target->getBodyModule();
				if (body->getMaxHealth() > 0.0f)
					score = 1.0f - body->getHealth() / body->getMaxHealth() - 0.25f;
			}
			else if (allyBattle && !building && ai)
			{
				const auto* horde = static_cast<const HordeAIUpdate*>(target->findUpdateModule(NAMEKEY("HordeAIUpdate")));
				if (ai->isAttacking() || (horde && horde->isEngaging()))
					score = 1.0f + target->getTemplate()->getThreatValue();
			}
			else if (allyEconomy && target->isKindOf(KINDOF_ECONOMY_STRUCTURE))
				score = 1.0f;
		}
		else if (relationship == ENEMIES &&
			((building && (enemyStructure || reveal)) || (!building && enemyArmy)))
		{
			score = 1.0f + target->getTemplate()->getThreatValue();
		}
		if (score <= 0.0f || ((!book || captureCreep) && range > 0.0f && target != source &&
			ThePartitionManager->getDistanceSquared(source, target, FROM_CENTER_2D) > range * range))
			continue;
		if (best && (score < bestScore || (score == bestScore && target->getID() > best->getID())))
			continue;
		const Coord3D* location = positionTarget ? target->getPosition() : nullptr;
		if (!button->isValidToUseOn(source, objectTarget ? target : nullptr, location, CMD_FROM_AI) ||
			!power->isValidRetailTarget(source, objectTarget ? target : nullptr, location, CMD_FROM_AI))
			continue;
		if (data->m_spellMakesAStructure && positionTarget && target->isKindOf(KINDOF_IMMOBILE))
			continue;
		best = target;
		bestShroud = shroud;
		bestScore = score;
	}
	if (best && AISkirmishPlayer::rollRetailSpecialPowerActivation(*player))
	{
		if (objectTarget || positionTarget)
			tracePowerTarget(*source, *power, *best, bestShroud);
		if (objectTarget)
			source->doSpecialPowerAtObject(power, best, options);
		else if (positionTarget)
			source->doSpecialPowerAtLocation(power, best->getPosition(), INVALID_ANGLE, options);
		else
			source->doSpecialPower(power, options);
	}
	return checkAgain;
#else
	return UPDATE_SLEEP_FOREVER;
#endif
}

void AISpecialPowerUpdate::crc(Xfer* xfer)
{
	UpdateModule::crc(xfer);
}

void AISpecialPowerUpdate::xfer(Xfer* xfer)
{
	UpdateModule::xfer(xfer);
}

void AISpecialPowerUpdate::loadPostProcess()
{
	UpdateModule::loadPostProcess();
}
