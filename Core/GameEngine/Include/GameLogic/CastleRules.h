#pragma once

#include <cmath>

namespace CastleRules
{
struct Offset
{
	float x;
	float y;
};

inline constexpr float DEFAULT_START_UNIT_RADIUS = 50.0f;
inline constexpr float START_UNIT_RADIUS_SPREAD = 1.4f;
inline constexpr float START_UNIT_AXIS = 0.70710678f;

inline bool isZeroOffset(float x, float y, float z)
{
	return x == 0.0f && y == 0.0f && z == 0.0f;
}

inline Offset startingUnitOffset(float x, float y, float z)
{
	const float side = std::sqrt(x * x + z * z);
	const float signedSide = x < 0.0f ? -side : side;
	return {(y + signedSide) * START_UNIT_AXIS, (signedSide - y) * START_UNIT_AXIS};
}

inline float startingUnitSearchRadius(bool hasBuilding, float boundingCircleRadius, bool hasCastle, float maxCastleRadius)
{
	if (!hasBuilding)
		return DEFAULT_START_UNIT_RADIUS;
	return hasCastle ? maxCastleRadius : boundingCircleRadius;
}

enum class KeepAction
{
	None,
	Lost,
	KillEverything,
};

inline KeepAction keepAction(bool keepExists, bool keepDead, bool killsEverything)
{
	if (keepExists && !keepDead)
		return KeepAction::None;
	return killsEverything ? KeepAction::KillEverything : KeepAction::Lost;
}

struct FilterFacts
{
	bool includedByName;
	bool excludedByName;
	bool excludedByKind;
	bool hasIncludedKinds;
	bool includedByKind;
	bool ruleNone;
};

inline bool filterMatches(const FilterFacts& facts)
{
	if (facts.includedByName)
		return true;
	if (facts.excludedByName || facts.excludedByKind)
		return false;
	if (facts.hasIncludedKinds)
		return facts.includedByKind;
	return !facts.ruleNone;
}

struct VictoryFacts
{
	bool dead;
	bool structure;
	bool mine;
	bool projectile;
	bool ignoreForVictory;
	bool inert;
};

inline bool countsAsStructure(const VictoryFacts& facts, bool matchesStructureFilter)
{
	return !facts.dead && !facts.mine && matchesStructureFilter;
}

inline bool countsAsUnit(const VictoryFacts& facts, bool matchesUnitFilter)
{
	return !facts.dead && !facts.structure && !facts.projectile && !facts.ignoreForVictory && !facts.inert &&
		matchesUnitFilter;
}

inline float constructionStep(int framesToBuild)
{
	return framesToBuild > 0 ? 100.0f / static_cast<float>(framesToBuild) : 100.0f;
}
}
