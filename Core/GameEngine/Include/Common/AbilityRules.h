#pragma once

#include <string>

namespace AbilityRules
{
enum AbilityCondition : unsigned int
{
	ABILITY_CONDITION_MOUNTED = 1u << 0,
	ABILITY_CONDITION_WEAPON_TOGGLE = 1u << 1,
};

inline bool conditionsAllow(unsigned int required, unsigned int rejected, unsigned int current)
{
	return (current & required) == required && (current & rejected) == 0;
}

enum class QueryMode
{
	All,
	Any,
	None,
};

struct QueryCandidate
{
	bool excluded = false;
	bool nameIncluded = false;
	int includeKinds = 0;
	int includeKindsHit = 0;
	bool hasIncludeNames = false;
};

inline bool queryIncludes(QueryMode mode, const QueryCandidate& candidate)
{
	if (candidate.excluded)
		return false;
	if (candidate.nameIncluded)
		return true;
	if (candidate.includeKinds == 0)
		return mode != QueryMode::None && !candidate.hasIncludeNames;
	if (mode == QueryMode::All)
		return candidate.includeKindsHit == candidate.includeKinds;
	return candidate.includeKindsHit > 0;
}

struct RelationFilter
{
	bool self = false;
	bool allies = false;
	bool enemies = false;
	bool neutrals = false;
	bool samePlayer = false;
};

enum class Relation
{
	Self,
	SamePlayer,
	Ally,
	Enemy,
	Neutral,
};

inline bool relationAllows(const RelationFilter& filter, Relation relation)
{
	if (!filter.self && !filter.allies && !filter.enemies && !filter.neutrals && !filter.samePlayer)
		return true;
	switch (relation)
	{
		case Relation::Self:
			return filter.self || filter.samePlayer || filter.allies;
		case Relation::SamePlayer:
			return filter.samePlayer || filter.allies;
		case Relation::Ally:
			return filter.allies;
		case Relation::Enemy:
			return filter.enemies;
		case Relation::Neutral:
			return filter.neutrals;
	}
	return false;
}

inline std::string specialWeaponCondition(int which)
{
	static const char* const names[] = { "SPECIAL_WEAPON_ONE", "SPECIAL_WEAPON_TWO", "SPECIAL_WEAPON_THREE",
		"SPECIAL_WEAPON_FOUR", "SPECIAL_WEAPON_FIVE", "SPECIAL_WEAPON_SIX" };
	if (which < 1 || which > 6)
		return std::string();
	return names[which - 1];
}

inline std::string packingTypeCondition(int variation)
{
	if (variation < 1 || variation > 6)
		return std::string();
	return "PACKING_TYPE_" + std::to_string(variation);
}

inline bool repeatsTrigger(bool persistentPrep, int triggersDone, int persistentCount)
{
	return persistentPrep && triggersDone < persistentCount;
}

inline float healAmount(float maxHealth, float amount, bool asPercent)
{
	if (amount <= 0.0f)
		return 0.0f;
	return asPercent ? maxHealth * amount : amount;
}

inline bool autoAbilityActive(bool startsActive, bool computerPlayer, int toggledState)
{
	if (toggledState > 0)
		return true;
	if (toggledState < 0)
		return false;
	return startsActive || computerPlayer;
}

enum class CommandSetChoice
{
	Keep,
	UseToggle,
	Restore,
};

inline CommandSetChoice monitorCommandSet(bool conditionMet, bool usingToggleSet)
{
	if (conditionMet && !usingToggleSet)
		return CommandSetChoice::UseToggle;
	if (!conditionMet && usingToggleSet)
		return CommandSetChoice::Restore;
	return CommandSetChoice::Keep;
}

inline bool mountButtonAllowed(bool mountedOnly, bool unmountedOnly, bool mounted)
{
	if (mountedOnly && !mounted)
		return false;
	if (unmountedOnly && mounted)
		return false;
	return true;
}
}
