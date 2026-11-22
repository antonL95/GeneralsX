#include "GameLogic/RetailEconomyRules.h"

#include <gtest/gtest.h>

TEST(RetailEconomyRules, RetainsCashBeyondSinglePrecision)
{
	using namespace RetailEconomy;
	EXPECT_EQ(scaleMoney(16'777'217, fullRate, Rounding::Down), 16'777'217);
	EXPECT_EQ(scaleMoney(16'777'217, fullRate, Rounding::Up), 16'777'217);
	EXPECT_EQ(scaleMoney(16'777'217, fixedRate(0.5), Rounding::Up), 8'388'609);
}

TEST(RetailEconomyRules, UsesThePayoutRoundingRule)
{
	using namespace RetailEconomy;
	EXPECT_EQ(scaleMoney(101, fixedRate(0.5), Rounding::Down), 50);
	EXPECT_EQ(scaleMoney(101, fixedRate(0.5), Rounding::Up), 51);
	EXPECT_EQ(scaleMoney(5, fixedRate(25), Rounding::Down), 125);
	EXPECT_EQ(scaleMoney(3, fixedRate(0.333333), Rounding::Down), 0);
	EXPECT_EQ(scaleMoney(std::numeric_limits<int>::max(), fixedRate(25), Rounding::Down),
		std::numeric_limits<int>::max());
}
