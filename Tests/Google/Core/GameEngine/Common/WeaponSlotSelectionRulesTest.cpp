#include <gtest/gtest.h>

#include "GameLogic/WeaponSlotSelectionRules.h"

namespace
{
using namespace WeaponSlotSelectionRules;

TEST(WeaponSlotSelectionRulesTest, ResolvesWeaponSetDefaultCriteria)
{
	EXPECT_EQ(resolveCriteria(Criteria::UseWeaponSetDefault, Criteria::SelectAtRandom), Criteria::SelectAtRandom);
	EXPECT_EQ(resolveCriteria(Criteria::UseWeaponSetDefault, Criteria::PreferLeastMovement), Criteria::PreferLeastMovement);
	EXPECT_EQ(resolveCriteria(Criteria::PreferLongestRange, Criteria::SelectAtRandom), Criteria::PreferLongestRange);
}

TEST(WeaponSlotSelectionRulesTest, PreferLeastMovementSelectsSlotWithMinimalTravel)
{
	SlotCandidateInput melee;
	melee.slot = 0;
	melee.damage = 1.0f;
	melee.attackRange = 20.0f;
	melee.distance = 80.0f;
	melee.isMeleeOrContact = true;
	melee.baseReady = true;

	SlotCandidateInput ranged;
	ranged.slot = 1;
	ranged.damage = 1.0f;
	ranged.attackRange = 120.0f;
	ranged.minRange = 40.0f;
	ranged.distance = 80.0f;
	ranged.isMeleeOrContact = false;
	ranged.baseReady = true;

	const std::vector<SlotEvaluation> candidates = {
		evaluateSlotCandidate(ranged),
		evaluateSlotCandidate(melee)
	};
	EXPECT_EQ(selectBestSlot(candidates, Criteria::PreferLeastMovement), 1);

	melee.distance = 15.0f;
	ranged.distance = 15.0f;
	const std::vector<SlotEvaluation> closeCandidates = {
		evaluateSlotCandidate(ranged),
		evaluateSlotCandidate(melee)
	};
	EXPECT_EQ(selectBestSlot(closeCandidates, Criteria::PreferLeastMovement), 0);
}

TEST(WeaponSlotSelectionRulesTest, SelectAtRandomChoosesHighestRandomScore)
{
	SlotCandidateInput slot0;
	slot0.slot = 0;
	slot0.damage = 1.0f;
	slot0.attackRange = 100.0f;
	slot0.distance = 50.0f;
	slot0.randomRoll = 0.25f;
	slot0.baseReady = true;

	SlotCandidateInput slot1 = slot0;
	slot1.slot = 1;
	slot1.randomRoll = 0.80f;

	const std::vector<SlotEvaluation> candidates = {
		evaluateSlotCandidate(slot1),
		evaluateSlotCandidate(slot0)
	};
	EXPECT_EQ(selectBestSlot(candidates, Criteria::SelectAtRandom), 1);

	slot0.randomRoll = 0.92f;
	slot1.randomRoll = 0.41f;
	const std::vector<SlotEvaluation> swapped = {
		evaluateSlotCandidate(slot1),
		evaluateSlotCandidate(slot0)
	};
	EXPECT_EQ(selectBestSlot(swapped, Criteria::SelectAtRandom), 0);
}

TEST(WeaponSlotSelectionRulesTest, PreferGrabSelectsFirstEligibleGrabBeforeReadinessAndDamage)
{
	std::vector<SlotEvaluation> candidates = {
		{.slot = 3, .damage = 1.0f, .canGrab = true},
		{.slot = 2, .damage = 10.0f, .ready = true},
		{.slot = 0, .damage = 1.0f, .ready = true, .canGrab = true}
	};
	EXPECT_EQ(selectBestSlot(candidates, Criteria::PreferGrabOverDamage), 3);
	EXPECT_EQ(selectBestSlot(candidates, Criteria::PreferMostDamage), 2);

	candidates[0].disqualified = true;
	EXPECT_EQ(selectBestSlot(candidates, Criteria::PreferGrabOverDamage), 0);

	candidates[2].canGrab = false;
	EXPECT_EQ(selectBestSlot(candidates, Criteria::PreferGrabOverDamage), 2);
}

TEST(WeaponSlotSelectionRulesTest, OnlyInConditionBoostsMatchAndDisqualifiesMismatch)
{
	SlotCandidateInput baseSlot;
	baseSlot.slot = 0;
	baseSlot.damage = 1.0f;
	baseSlot.attackRange = 50.0f;
	baseSlot.distance = 30.0f;
	baseSlot.randomRoll = 0.9f;
	baseSlot.baseReady = true;

	SlotCandidateInput condSlot = baseSlot;
	condSlot.slot = 1;
	condSlot.randomRoll = 0.1f;
	condSlot.hasOnlyInCondition = true;
	condSlot.onlyInConditionMatched = false;

	const std::vector<SlotEvaluation> mismatch = {
		evaluateSlotCandidate(condSlot),
		evaluateSlotCandidate(baseSlot)
	};
	EXPECT_TRUE(mismatch[0].disqualified);
	EXPECT_EQ(selectBestSlot(mismatch, Criteria::PreferMostDamage), 0);
	EXPECT_EQ(selectBestSlot(mismatch, Criteria::SelectAtRandom), 0);

	condSlot.onlyInConditionMatched = true;
	const std::vector<SlotEvaluation> matched = {
		evaluateSlotCandidate(condSlot),
		evaluateSlotCandidate(baseSlot)
	};
	EXPECT_FALSE(matched[0].disqualified);
	EXPECT_EQ(selectBestSlot(matched, Criteria::PreferMostDamage), 1);
	EXPECT_EQ(selectBestSlot(matched, Criteria::SelectAtRandom), 1);
}

TEST(WeaponSlotSelectionRulesTest, SharedReadyStatusHoldsSelectionWhenAnySlotBusy)
{
	EXPECT_FALSE(shouldHoldWeaponSetForSharedReadyStatus(false, true));
	EXPECT_FALSE(shouldHoldWeaponSetForSharedReadyStatus(true, false));
	EXPECT_TRUE(shouldHoldWeaponSetForSharedReadyStatus(true, true));

	SlotCandidateInput gatedPrimary;
	gatedPrimary.slot = 0;
	gatedPrimary.damage = 1.0f;
	gatedPrimary.attackRange = 100.0f;
	gatedPrimary.distance = 50.0f;
	gatedPrimary.baseReady = false;
	gatedPrimary.hasOnlyInCondition = true;
	gatedPrimary.onlyInConditionMatched = true;

	SlotCandidateInput secondary;
	secondary.slot = 1;
	secondary.damage = 1.0f;
	secondary.attackRange = 100.0f;
	secondary.distance = 50.0f;
	secondary.baseReady = true;

	const std::vector<SlotEvaluation> stillEligible = {
		evaluateSlotCandidate(secondary),
		evaluateSlotCandidate(gatedPrimary)
	};
	EXPECT_EQ(selectBestSlotWithSharedReady(stillEligible, Criteria::PreferMostDamage, 0, true), 0);

	gatedPrimary.onlyInConditionMatched = false;
	const std::vector<SlotEvaluation> primaryDisqualified = {
		evaluateSlotCandidate(secondary),
		evaluateSlotCandidate(gatedPrimary)
	};
	EXPECT_EQ(selectBestSlotWithSharedReady(primaryDisqualified, Criteria::PreferMostDamage, 0, true), 1);
}

TEST(WeaponSlotSelectionRulesTest, DualWeaponRangeAndCooldownDecisions)
{
	EXPECT_TRUE(isEnemyWithinDualWeaponRange(30.0f, 40.0f, 10.0f, 60.0f));
	EXPECT_FALSE(isEnemyWithinDualWeaponRange(50.0f, 50.0f, 0.0f, 60.0f));
	EXPECT_FALSE(isEnemyWithinDualWeaponRange(10.0f, 0.0f, 35.0f, 60.0f));

	const DualWeaponDecision enterClose = evaluateDualWeaponDecision(
		false, false, false, false, false, false, true, 100, 0, 15);
	EXPECT_TRUE(enterClose.shouldUseCloseRange);
	EXPECT_TRUE(enterClose.switched);
	EXPECT_EQ(enterClose.nextLastSwitchFrame, 100u);

	const DualWeaponDecision cooldownBlocked = evaluateDualWeaponDecision(
		true, false, false, false, false, false, false, 108, 100, 15);
	EXPECT_TRUE(cooldownBlocked.shouldUseCloseRange);
	EXPECT_FALSE(cooldownBlocked.switched);
	EXPECT_EQ(cooldownBlocked.sleepFrames, 7u);

	const DualWeaponDecision hordeRanged = evaluateDualWeaponDecision(
		true, false, true, true, false, false, true, 108, 100, 15);
	EXPECT_FALSE(hordeRanged.shouldUseCloseRange);
	EXPECT_TRUE(hordeRanged.switched);

	const DualWeaponDecision mountedClose = evaluateDualWeaponDecision(
		false, false, false, false, true, true, false, 108, 100, 15);
	EXPECT_TRUE(mountedClose.shouldUseCloseRange);
	EXPECT_TRUE(mountedClose.switched);
}
}
