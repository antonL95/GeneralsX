#pragma once

#include <cmath>
#include <vector>

namespace WeaponNuggetRules
{
struct FilterFacts
{
	bool ruleAll;
	bool ruleNone;
	bool wantsSelf;
	bool wantsAllies;
	bool wantsEnemies;
	bool wantsNeutrals;
	bool wantsSamePlayer;
	bool isSelf;
	bool isAlly;
	bool isEnemy;
	bool isNeutral;
	bool isSamePlayer;
	bool includedByName;
	bool excludedByName;
	bool hasIncludedKinds;
	bool includedByKind;
	bool excludedByKind;
	bool hasIncludedNames;
};

inline bool relationshipMatches(const FilterFacts& facts)
{
	const bool restricted = facts.wantsSelf || facts.wantsAllies || facts.wantsEnemies || facts.wantsNeutrals ||
		facts.wantsSamePlayer;
	if (!restricted)
		return true;
	return (facts.wantsSelf && facts.isSelf) || (facts.wantsAllies && facts.isAlly) ||
		(facts.wantsEnemies && facts.isEnemy) || (facts.wantsNeutrals && facts.isNeutral) ||
		(facts.wantsSamePlayer && facts.isSamePlayer);
}

inline bool filterMatches(const FilterFacts& facts)
{
	if (!relationshipMatches(facts))
		return false;
	if (facts.includedByName)
		return true;
	if (facts.excludedByName || facts.excludedByKind)
		return false;
	if (facts.ruleAll)
		return true;
	if (facts.hasIncludedKinds || facts.hasIncludedNames)
		return facts.includedByKind;
	return !facts.ruleNone;
}

inline bool upgradeGateOpen(bool hasRequiredNames, bool anyRequiredPresent, bool anyForbiddenPresent)
{
	if (anyForbiddenPresent)
		return false;
	return !hasRequiredNames || anyRequiredPresent;
}

inline bool isFullArc(float arcDegrees)
{
	return arcDegrees <= 0.0f || arcDegrees >= 360.0f;
}

inline bool withinArc(float facingX, float facingY, float dx, float dy, float cosHalfArc, bool fullArc, bool inverted)
{
	bool inside = true;
	if (!fullArc)
	{
		const float facingLengthSqr = facingX * facingX + facingY * facingY;
		const float deltaLengthSqr = dx * dx + dy * dy;
		if (facingLengthSqr > 0.0f && deltaLengthSqr > 0.0f)
		{
			const float dot = facingX * dx + facingY * dy;
			inside = dot >= cosHalfArc * std::sqrt(facingLengthSqr * deltaLengthSqr);
		}
	}
	return inverted ? !inside : inside;
}

struct Scalar
{
	float scalar;
	bool matches;
};

inline float scalarProduct(const std::vector<Scalar>& scalars)
{
	float product = 1.0f;
	for (const Scalar& entry : scalars)
	{
		if (entry.matches)
			product *= entry.scalar;
	}
	return product;
}

inline std::vector<unsigned int> dotTickOffsets(unsigned int delayFrames, unsigned int intervalFrames, unsigned int durationFrames)
{
	std::vector<unsigned int> ticks;
	if (intervalFrames == 0 || durationFrames == 0)
	{
		ticks.push_back(delayFrames);
		return ticks;
	}
	for (unsigned int elapsed = 0; elapsed < durationFrames; elapsed += intervalFrames)
		ticks.push_back(delayFrames + elapsed);
	return ticks;
}

inline float clampProjectileSpeed(float speed, float maxSpeed)
{
	return maxSpeed > 0.0f && speed > maxSpeed ? maxSpeed : speed;
}

inline bool needsHitRoll(float hitPercentage)
{
	return hitPercentage < 100.0f;
}

inline bool rollHits(int rollFrom0To99, float hitPercentage)
{
	return static_cast<float>(rollFrom0To99) < hitPercentage;
}
}
