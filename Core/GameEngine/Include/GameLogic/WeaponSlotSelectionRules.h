#pragma once

#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace WeaponSlotSelectionRules
{
enum class Criteria : int
{
	PreferMostDamage = 0,
	PreferLongestRange = 1,
	PreferGrabOverDamage = 2,
	PreferLeastMovement = 3,
	SelectAtRandom = 4,
	UseWeaponSetDefault = 5
};

inline Criteria resolveCriteria(Criteria requested, Criteria setDefault)
{
	if (requested == Criteria::UseWeaponSetDefault)
		return setDefault;
	return requested;
}

inline float computeMovementNeeded(float distance, float minRange, float attackRange, bool isMeleeOrContact)
{
	if (isMeleeOrContact)
		return distance;
	if (distance < minRange)
		return minRange - distance;
	if (distance > attackRange)
		return distance - attackRange;
	return 0.0f;
}

inline bool shouldHoldWeaponSetForSharedReadyStatus(bool sharedReadyStatus, bool anySlotNotReady)
{
	return sharedReadyStatus && anySlotNotReady;
}

struct SlotEvaluation
{
	int slot = 0;
	float damage = 0.0f;
	float attackRange = 0.0f;
	float movement = 0.0f;
	float randomScore = 0.0f;
	bool ready = false;
	bool disqualified = false;
	bool canGrab = false;
};

struct SlotCandidateInput
{
	int slot = 0;
	float damage = 0.0f;
	float attackRange = 0.0f;
	float minRange = 0.0f;
	float distance = 0.0f;
	float randomRoll = 0.0f;
	bool isMeleeOrContact = false;
	bool baseReady = false;
	bool outOfAmmo = false;
	bool preferredMatch = false;
	float preferredScore = 1e10f;
	bool hasOnlyInCondition = false;
	bool onlyInConditionMatched = false;
};

inline SlotEvaluation evaluateSlotCandidate(const SlotCandidateInput& in)
{
	SlotEvaluation out;
	out.slot = in.slot;
	out.damage = in.damage;
	out.attackRange = in.attackRange;
	out.movement = computeMovementNeeded(in.distance, in.minRange, in.attackRange, in.isMeleeOrContact);
	out.randomScore = in.randomRoll;
	out.ready = in.baseReady && in.distance >= in.minRange;

	if (in.preferredMatch)
	{
		out.damage = in.preferredScore;
		out.attackRange = in.preferredScore;
		out.movement = 0.0f;
		out.randomScore += 1.0f;
		out.ready = !in.outOfAmmo && in.distance >= in.minRange;
	}

	if (in.hasOnlyInCondition)
	{
		if (in.onlyInConditionMatched)
		{
			out.damage = std::numeric_limits<float>::max();
			out.attackRange = std::numeric_limits<float>::max();
			out.movement = 0.0f;
			out.randomScore += 1.0f;
			out.ready = !in.outOfAmmo && in.distance >= in.minRange;
		}
		else
		{
			out.damage = -1.0f;
			out.attackRange = -1.0f;
			out.movement = std::numeric_limits<float>::max();
			out.randomScore -= 1.0f;
			out.ready = false;
			out.disqualified = true;
		}
	}

	return out;
}

inline int selectBestSlot(const std::vector<SlotEvaluation>& candidates, Criteria criteria)
{
	bool found = false;
	bool foundBackup = false;
	int bestSlot = -1;
	int bestBackupSlot = -1;

	float bestDamage = 0.0f;
	float bestDamageBackup = 0.0f;
	float longestRange = 0.0f;
	float longestRangeBackup = 0.0f;
	float leastMovement = std::numeric_limits<float>::max();
	float leastMovementBackup = std::numeric_limits<float>::max();
	float bestRandom = 0.0f;
	float bestRandomBackup = 0.0f;

	for (const SlotEvaluation& c : candidates)
	{
		if (c.disqualified)
			continue;

		switch (criteria)
		{
			case Criteria::PreferGrabOverDamage:
				if (c.canGrab)
					return c.slot;
				[[fallthrough]];
			case Criteria::PreferMostDamage:
				if (c.ready)
				{
					if (c.damage >= bestDamage)
					{
						bestDamage = c.damage;
						bestSlot = c.slot;
						found = true;
					}
				}
				else if (c.damage >= bestDamageBackup)
				{
					bestDamageBackup = c.damage;
					bestBackupSlot = c.slot;
					foundBackup = true;
				}
				break;
			case Criteria::PreferLongestRange:
				if (c.ready)
				{
					if (c.attackRange > longestRange)
					{
						longestRange = c.attackRange;
						bestSlot = c.slot;
						found = true;
					}
				}
				else if (c.attackRange > longestRangeBackup)
				{
					longestRangeBackup = c.attackRange;
					bestBackupSlot = c.slot;
					foundBackup = true;
				}
				break;
			case Criteria::PreferLeastMovement:
				if (c.ready)
				{
					if (c.movement <= leastMovement)
					{
						leastMovement = c.movement;
						bestSlot = c.slot;
						found = true;
					}
				}
				else if (c.movement <= leastMovementBackup)
				{
					leastMovementBackup = c.movement;
					bestBackupSlot = c.slot;
					foundBackup = true;
				}
				break;
			case Criteria::SelectAtRandom:
				if (c.ready)
				{
					if (c.randomScore > bestRandom)
					{
						bestRandom = c.randomScore;
						bestSlot = c.slot;
						found = true;
					}
				}
				else if (c.randomScore > bestRandomBackup)
				{
					bestRandomBackup = c.randomScore;
					bestBackupSlot = c.slot;
					foundBackup = true;
				}
				break;
			case Criteria::UseWeaponSetDefault:
				break;
		}
	}

	if (found)
		return bestSlot;
	if (foundBackup)
		return bestBackupSlot;
	return -1;
}

inline int selectBestSlotWithSharedReady(
	const std::vector<SlotEvaluation>& candidates,
	Criteria criteria,
	int currentSlot,
	bool sharedReadyBusy)
{
	if (sharedReadyBusy)
	{
		for (const SlotEvaluation& c : candidates)
		{
			if (c.slot == currentSlot && !c.disqualified)
				return currentSlot;
		}
	}
	return selectBestSlot(candidates, criteria);
}

inline bool isEnemyWithinDualWeaponRange(float dx, float dy, float dz, float switchDistance)
{
	if (switchDistance <= 0.0f)
		return false;
	const float distSq = dx * dx + dy * dy + dz * dz;
	if (distSq > switchDistance * switchDistance)
		return false;
	if (std::fabs(dz) > switchDistance * 0.5f)
		return false;
	return true;
}

struct DualWeaponDecision
{
	bool shouldUseCloseRange = false;
	bool switched = false;
	std::uint32_t nextLastSwitchFrame = 0;
	std::uint32_t sleepFrames = 0;
};

inline DualWeaponDecision evaluateDualWeaponDecision(
	bool currentlyCloseRange,
	bool forceCloseRange,
	bool useHordeRangeWeapon,
	bool inHorde,
	bool useCloseRangeWhileMounted,
	bool isMounted,
	bool hasCloseEnemy,
	std::uint32_t currentFrame,
	std::uint32_t lastSwitchFrame,
	std::uint32_t minimumSwitchFrames)
{
	DualWeaponDecision out;
	out.shouldUseCloseRange = currentlyCloseRange;
	out.nextLastSwitchFrame = lastSwitchFrame;

	if (useHordeRangeWeapon && inHorde)
	{
		out.shouldUseCloseRange = false;
		out.switched = currentlyCloseRange;
		return out;
	}

	if (forceCloseRange)
	{
		out.shouldUseCloseRange = true;
		out.switched = !currentlyCloseRange;
		return out;
	}

	if (useCloseRangeWhileMounted && isMounted)
	{
		out.shouldUseCloseRange = true;
		out.switched = !currentlyCloseRange;
		return out;
	}

	if (minimumSwitchFrames > 0 && currentFrame < lastSwitchFrame + minimumSwitchFrames)
	{
		out.sleepFrames = (lastSwitchFrame + minimumSwitchFrames) - currentFrame;
		return out;
	}

	out.shouldUseCloseRange = hasCloseEnemy;
	out.switched = (hasCloseEnemy != currentlyCloseRange);
	out.nextLastSwitchFrame = currentFrame;
	return out;
}
}
