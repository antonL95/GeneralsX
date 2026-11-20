#include <gtest/gtest.h>

#include "Common/AttributeModifierRules.h"

using namespace AttributeModifierRules;

namespace
{
struct Defs
{
	std::vector<ListDef> lists;
	const ListDef& operator()(int index) const { return lists[static_cast<size_t>(index)]; }

	int add(int category, std::vector<Entry> entries, bool replace = false, bool ignoreAnti = false)
	{
		ListDef def;
		def.category = category;
		def.entries = std::move(entries);
		def.replaceInCategoryIfLongest = replace;
		def.ignoreIfAnticategoryActive = ignoreAnti;
		lists.push_back(def);
		return static_cast<int>(lists.size()) - 1;
	}
};

Entry entry(int kind, float value, std::vector<int> types = {})
{
	Entry result;
	result.kind = kind;
	result.value = value;
	result.damageTypes = std::move(types);
	return result;
}
}

TEST(AttributeModifierRules, RetailNameTablesKeepTheirOrder)
{
	EXPECT_EQ(indexOf(kindNames(), "ARMOR"), ARMOR);
	EXPECT_EQ(indexOf(kindNames(), "invulnerable"), INVULNERABLE);
	EXPECT_EQ(indexOf(kindNames(), "RATE_OF_FIRE"), RATE_OF_FIRE);
	EXPECT_EQ(indexOf(categoryNames(), "INNATE_HEALTH"), INNATE_HEALTH);
	EXPECT_EQ(indexOf(stanceNames(), "HoldGroundMoving"), STANCE_HOLD_GROUND_MOVING);
	EXPECT_EQ(indexOf(stanceNames(), "Battle"), STANCE_BATTLE);
	EXPECT_EQ(indexOf(kindNames(), "ARMORX"), -1);
}

TEST(AttributeModifierRules, PercentTokensScaleAndPlainNumbersStay)
{
	EXPECT_FLOAT_EQ(amountFromToken("20%", 20.0f), 0.2f);
	EXPECT_FLOAT_EQ(amountFromToken("-33%", -33.0f), -0.33f);
	EXPECT_FLOAT_EQ(amountFromToken("1000", 1000.0f), 1000.0f);
	EXPECT_FLOAT_EQ(amountFromToken("#MULTIPLY(", 3.0f), 3.0f);
}

TEST(AttributeModifierRules, SameListRefreshesInsteadOfStacking)
{
	Defs defs;
	const int leadership = defs.add(LEADERSHIP, {entry(DAMAGE_MULT, 1.33f)});
	Stack stack;
	EXPECT_EQ(stack.apply(leadership, 100, 0, defs, nullptr), APPLIED);
	EXPECT_EQ(stack.apply(leadership, 160, 0, defs, nullptr), REFRESHED);
	ASSERT_EQ(stack.active().size(), 1u);
	EXPECT_EQ(stack.active()[0].endFrame, 160u);
	EXPECT_EQ(stack.apply(leadership, 120, 0, defs, nullptr), REFRESHED);
	EXPECT_EQ(stack.active()[0].endFrame, 160u);
	EXPECT_EQ(stack.apply(leadership, 0, 0, defs, nullptr), REFRESHED);
	EXPECT_EQ(stack.active()[0].endFrame, 0u);
	EXPECT_EQ(stack.apply(leadership, 500, 0, defs, nullptr), REFRESHED);
	EXPECT_EQ(stack.active()[0].endFrame, 0u);
}

TEST(AttributeModifierRules, DifferentListsStackByKind)
{
	Defs defs;
	const int a = defs.add(LEADERSHIP, {entry(ARMOR, 0.2f), entry(DAMAGE_MULT, 1.5f), entry(SPEED, 1.2f)});
	const int b = defs.add(SPELL, {entry(ARMOR, 0.3f), entry(DAMAGE_MULT, 2.0f), entry(DAMAGE_ADD, 0.1f)});
	Stack stack;
	stack.apply(a, 0, 0, defs, nullptr);
	stack.apply(b, 0, 0, defs, nullptr);
	const Totals totals = totalsOf({&stack}, defs);
	EXPECT_FLOAT_EQ(totals.value[ARMOR], 0.5f);
	EXPECT_FLOAT_EQ(totals.value[DAMAGE_MULT], 3.0f);
	EXPECT_FLOAT_EQ(totals.value[SPEED], 1.2f);
	EXPECT_FLOAT_EQ(totals.value[DAMAGE_ADD], 0.1f);
	EXPECT_FLOAT_EQ(totals.damageDealtFactor(), 3.0f);
	EXPECT_FLOAT_EQ(totals.damageTakenFactor(3, 0.75f), 0.5f);
	EXPECT_EQ(totals.lists, 2);
}

TEST(AttributeModifierRules, DamageAddRaisesAcceptingBaseDamageBeforeTheMultiplier)
{
	Defs defs;
	const int draft = defs.add(SPELL, {entry(DAMAGE_ADD, 5.0f)});
	const int banner = defs.add(LEVEL, {entry(DAMAGE_ADD, 20.0f)});
	const int mult = defs.add(LEADERSHIP, {entry(DAMAGE_MULT, 1.5f)});
	Stack stack;
	stack.apply(draft, 0, 0, defs, nullptr);
	stack.apply(mult, 0, 0, defs, nullptr);
	const Totals drafted = totalsOf({&stack}, defs);
	EXPECT_FLOAT_EQ(baseDamageWithAdd(40.0f, true, drafted.value[DAMAGE_ADD]) * drafted.damageDealtFactor(), 67.5f);
	EXPECT_FLOAT_EQ(baseDamageWithAdd(40.0f, false, drafted.value[DAMAGE_ADD]) * drafted.damageDealtFactor(), 60.0f);
	EXPECT_FLOAT_EQ(baseDamageWithAdd(0.0f, true, drafted.value[DAMAGE_ADD]), 0.0f);
	stack.apply(banner, 0, 0, defs, nullptr);
	EXPECT_FLOAT_EQ(baseDamageWithAdd(40.0f, true, totalsOf({&stack}, defs).value[DAMAGE_ADD]), 65.0f);
}

TEST(AttributeModifierRules, RemovalByOwnerKeepsOtherSourcesCopy)
{
	Defs defs;
	const int list = defs.add(FORMATION, {entry(ARMOR, 0.2f)});
	constexpr Owner stance = 7;
	constexpr Owner upgrade = 9;
	Stack stack;
	EXPECT_EQ(stack.apply(list, 0, 0, defs, nullptr, stance), APPLIED);
	EXPECT_EQ(stack.apply(list, 0, 0, defs, nullptr, upgrade), REFRESHED);
	EXPECT_FALSE(stack.remove(list, stance, 10));
	EXPECT_TRUE(stack.contains(list));
	EXPECT_TRUE(stack.remove(list, upgrade, 10));
	EXPECT_FALSE(stack.contains(list));

	stack.apply(list, 0, 0, defs, nullptr, stance);
	stack.apply(list, 50, 0, defs, nullptr);
	EXPECT_EQ(stack.active().front().endFrame, 0u);
	EXPECT_FALSE(stack.remove(list, stance, 20));
	EXPECT_EQ(stack.active().front().endFrame, 50u);
	EXPECT_TRUE(stack.expire(50).size() == 1u);

	stack.apply(list, 0, 0, defs, nullptr, stance);
	stack.apply(list, 30, 0, defs, nullptr);
	EXPECT_TRUE(stack.remove(list, stance, 40));
	EXPECT_TRUE(stack.active().empty());
}

TEST(AttributeModifierRules, RemoveTimedKeepsPermanentHolds)
{
	Defs defs;
	const int list = defs.add(SPELL, {entry(ARMOR, 0.2f)});
	const int timed = defs.add(SPELL, {entry(SPEED, 1.2f)});
	Stack stack;
	stack.apply(list, 0, 0, defs, nullptr, 3);
	stack.apply(list, 90, 0, defs, nullptr);
	stack.apply(timed, 90, 0, defs, nullptr);
	const std::vector<int> removed = stack.removeTimed();
	ASSERT_EQ(removed.size(), 1u);
	EXPECT_EQ(removed.front(), timed);
	ASSERT_EQ(stack.active().size(), 1u);
	EXPECT_EQ(stack.active().front().holds.size(), 1u);
	EXPECT_EQ(stack.active().front().endFrame, 0u);
}

TEST(AttributeModifierRules, AuraTargetRules)
{
	EXPECT_TRUE(rankAllowed(0, 9));
	EXPECT_TRUE(rankAllowed(1, 1));
	EXPECT_FALSE(rankAllowed(1, 2));
	EXPECT_TRUE(alignmentAllowed(false, false, true));
	EXPECT_TRUE(alignmentAllowed(true, false, false));
	EXPECT_FALSE(alignmentAllowed(true, false, true));
	EXPECT_TRUE(alignmentAllowed(false, true, true));
	EXPECT_FALSE(alignmentAllowed(false, true, false));
}

TEST(AttributeModifierRules, NonStackableHealWindow)
{
	EXPECT_TRUE(healedWithin(100, 95, 15));
	EXPECT_FALSE(healedWithin(110, 95, 15));
	EXPECT_FALSE(healedWithin(100, 0xFFFFFFFFu, 15));
}

TEST(AttributeModifierRules, ArmorIsCappedAndNeverHeals)
{
	Defs defs;
	const int a = defs.add(BUFF, {entry(ARMOR, 0.6f)});
	const int b = defs.add(SPELL, {entry(ARMOR, 0.6f)});
	const int c = defs.add(DEBUFF, {entry(ARMOR, -3.0f)});
	Stack stack;
	stack.apply(a, 0, 0, defs, nullptr);
	stack.apply(b, 0, 0, defs, nullptr);
	EXPECT_FLOAT_EQ(totalsOf({&stack}, defs).damageTakenFactor(0, 0.75f), 0.25f);
	Stack weak;
	weak.apply(c, 0, 0, defs, nullptr);
	EXPECT_EQ(weak.active().size(), 1u);
	EXPECT_FLOAT_EQ(totalsOf({&weak}, defs).damageTakenFactor(0, 0.75f), 4.0f);
}

TEST(AttributeModifierRules, TypedArmorAndInvulnerabilityOnlyCoverTheirDamageTypes)
{
	Defs defs;
	const int typed = defs.add(SPELL, {entry(ARMOR, 0.75f, {7, 9})});
	const int shield = defs.add(SPELL, {entry(INVULNERABLE, 0.0f, {2})});
	Stack stack;
	stack.apply(typed, 0, 0, defs, nullptr);
	stack.apply(shield, 0, 0, defs, nullptr);
	const Totals totals = totalsOf({&stack}, defs);
	EXPECT_FLOAT_EQ(totals.damageTakenFactor(7, 0.75f), 0.25f);
	EXPECT_FLOAT_EQ(totals.damageTakenFactor(1, 0.75f), 1.0f);
	EXPECT_FLOAT_EQ(totals.damageTakenFactor(2, 0.75f), 0.0f);
}

TEST(AttributeModifierRules, ExpiryRemovesInApplicationOrder)
{
	Defs defs;
	const int a = defs.add(BUFF, {entry(SPEED, 1.5f)});
	const int b = defs.add(SPELL, {entry(SPEED, 2.0f)});
	const int c = defs.add(LEVEL, {entry(SPEED, 3.0f)});
	Stack stack;
	stack.apply(a, 50, 0, defs, nullptr);
	stack.apply(b, 30, 0, defs, nullptr);
	stack.apply(c, 0, 0, defs, nullptr);
	EXPECT_EQ(stack.nextExpiry(), 30u);
	EXPECT_TRUE(stack.expire(29).empty());
	EXPECT_EQ(stack.expire(30), std::vector<int>{b});
	EXPECT_EQ(stack.nextExpiry(), 50u);
	EXPECT_EQ(stack.expire(1000), std::vector<int>{a});
	EXPECT_EQ(stack.nextExpiry(), 0u);
	EXPECT_FLOAT_EQ(totalsOf({&stack}, defs).value[SPEED], 3.0f);
}

TEST(AttributeModifierRules, ReplaceInCategoryKeepsOnlyTheLongest)
{
	Defs defs;
	const int short20 = defs.add(BUFF, {entry(ARMOR, 0.33f)}, true);
	const int long60 = defs.add(BUFF, {entry(ARMOR, 0.33f)}, true);
	const int other = defs.add(BUFF, {entry(DAMAGE_MULT, 1.5f)});
	Stack stack;
	stack.apply(other, 40, 0, defs, nullptr);
	EXPECT_EQ(stack.apply(short20, 20, 0, defs, nullptr), APPLIED);
	std::vector<int> removed;
	EXPECT_EQ(stack.apply(long60, 60, 0, defs, &removed), APPLIED);
	EXPECT_EQ(removed, std::vector<int>{short20});
	EXPECT_EQ(stack.apply(short20, 30, 0, defs, nullptr), IGNORED);
	EXPECT_TRUE(stack.contains(other));
	EXPECT_EQ(stack.active().size(), 2u);
}

TEST(AttributeModifierRules, AnticategoryDispelsAndBlocksFlaggedLists)
{
	Defs defs;
	const int leadership = defs.add(LEADERSHIP, {entry(DAMAGE_MULT, 1.33f)}, false, true);
	const int levelArmor = defs.add(LEVEL, {entry(ARMOR, 0.13f)});
	const int unflagged = defs.add(LEADERSHIP, {entry(SPEED, 1.1f)});
	const int debuff = defs.add(DEBUFF, {entry(ARMOR, -0.33f)});
	Stack stack;
	stack.apply(leadership, 100, 0, defs, nullptr);
	stack.apply(levelArmor, 0, 0, defs, nullptr);
	stack.apply(unflagged, 100, 0, defs, nullptr);
	std::vector<int> removed;
	EXPECT_EQ(stack.apply(debuff, 50, categoryBit(LEADERSHIP), defs, &removed), APPLIED);
	EXPECT_EQ(removed, std::vector<int>{leadership});
	EXPECT_EQ(stack.apply(leadership, 120, 0, defs, nullptr), IGNORED);
	EXPECT_TRUE(stack.contains(unflagged));
	stack.expire(50);
	EXPECT_EQ(stack.apply(leadership, 120, 0, defs, nullptr), APPLIED);
}

TEST(AttributeModifierRules, HordeAndMemberStacksCountASharedListOnce)
{
	Defs defs;
	const int stance = defs.add(CATEGORY_NONE, {entry(ARMOR, 0.2f)});
	const int aura = defs.add(LEADERSHIP, {entry(DAMAGE_MULT, 1.5f)});
	Stack horde;
	Stack member;
	horde.apply(stance, 0, 0, defs, nullptr);
	horde.apply(aura, 10, 0, defs, nullptr);
	member.apply(aura, 12, 0, defs, nullptr);
	const Totals totals = totalsOf({&member, &horde}, defs);
	EXPECT_EQ(totals.lists, 2);
	EXPECT_FLOAT_EQ(totals.value[DAMAGE_MULT], 1.5f);
	EXPECT_FLOAT_EQ(totals.value[ARMOR], 0.2f);
}

TEST(AttributeModifierRules, StanceToggleCyclesTheButtonList)
{
	const std::vector<int> cycle = {STANCE_HOLD_GROUND, STANCE_BATTLE, STANCE_AGGRESSIVE};
	EXPECT_EQ(nextStance(cycle, STANCE_BATTLE), STANCE_AGGRESSIVE);
	EXPECT_EQ(nextStance(cycle, STANCE_AGGRESSIVE), STANCE_HOLD_GROUND);
	EXPECT_EQ(nextStance(cycle, STANCE_HOLD_GROUND), STANCE_BATTLE);
	EXPECT_EQ(nextStance(cycle, STANCE_PORCUPINE), STANCE_HOLD_GROUND);
	EXPECT_EQ(stanceImageIndex(cycle, STANCE_AGGRESSIVE), 2);
	EXPECT_EQ(stanceImageIndex(cycle, STANCE_UNINITIALIZED), -1);
}

TEST(AttributeModifierRules, MaxHealthIsRebuiltFromTheUnmodifiedBase)
{
	EXPECT_FLOAT_EQ(maxHealthAfter(1000.0f, 0.0f, 1.0f, 200.0f, 1.0f), 1200.0f);
	EXPECT_FLOAT_EQ(maxHealthAfter(1200.0f, 200.0f, 1.0f, 200.0f, 1.5f), 1800.0f);
	EXPECT_FLOAT_EQ(maxHealthAfter(1800.0f, 200.0f, 1.5f, 0.0f, 1.0f), 1000.0f);
	EXPECT_FLOAT_EQ(maxHealthAfter(10.0f, 0.0f, 1.0f, -50.0f, 1.0f), 1.0f);
}

TEST(AttributeModifierRules, EndFramesForPermanentAndTimedLists)
{
	EXPECT_EQ(endFrameFor(100, 0), 0u);
	EXPECT_EQ(endFrameFor(100, 90), 190u);
	EXPECT_TRUE(outlasts(0, 50));
	EXPECT_FALSE(outlasts(50, 0));
	EXPECT_TRUE(outlasts(60, 50));
	EXPECT_FALSE(outlasts(50, 50));
}
