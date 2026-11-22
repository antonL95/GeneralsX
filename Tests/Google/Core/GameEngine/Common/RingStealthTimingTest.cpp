#include <gtest/gtest.h>

#include "GameLogic/RingStealthTiming.h"

namespace
{

TEST(RingStealthTiming, AnimDurationIsIndependentOfDelay)
{
	RingStealthTiming::Config cfg;
	cfg.delayOn = 30;
	cfg.animOn = 10;
	cfg.delayOff = 20;
	cfg.animOff = 5;

	RingStealthTiming::State state;
	EXPECT_TRUE(RingStealthTiming::canToggle(cfg, state, 100, false));
	RingStealthTiming::startTransition(cfg, state, 100, true);
	EXPECT_EQ(state.toggleFrame, 130u);
	EXPECT_EQ(state.animEndFrame, 110u);
	EXPECT_TRUE(state.toggleOn);
	EXPECT_TRUE(state.animOn);
	EXPECT_FALSE(RingStealthTiming::canToggle(cfg, state, 105, false));

	state.toggleFrame = 0;
	state.animEndFrame = 0;
	RingStealthTiming::startTransition(cfg, state, 200, false);
	EXPECT_EQ(state.toggleFrame, 220u);
	EXPECT_EQ(state.animEndFrame, 205u);
	EXPECT_FALSE(state.toggleOn);
	EXPECT_FALSE(state.animOn);
}

TEST(RingStealthTiming, ZeroAnimLeavesAnimEndUnsetAndZeroDelayTriggersNextFrame)
{
	RingStealthTiming::Config cfg;
	cfg.delayAfterRemoving = 45;

	RingStealthTiming::State state;
	EXPECT_TRUE(RingStealthTiming::canToggle(cfg, state, 50, false));
	RingStealthTiming::startTransition(cfg, state, 50, true);
	EXPECT_EQ(state.toggleFrame, 51u);
	EXPECT_EQ(state.animEndFrame, 0u);
}

TEST(RingStealthTiming, RecoveryBlocksRewearingUntilExpiry)
{
	RingStealthTiming::Config cfg;
	cfg.delayAfterRemoving = 60;

	RingStealthTiming::State state;
	state.blockedUntilFrame = 160;
	EXPECT_FALSE(RingStealthTiming::canToggle(cfg, state, 100, false));
	EXPECT_FALSE(RingStealthTiming::canToggle(cfg, state, 159, false));
	EXPECT_TRUE(RingStealthTiming::canToggle(cfg, state, 160, false));
	EXPECT_TRUE(RingStealthTiming::canToggle(cfg, state, 100, true));
}

TEST(RingStealthTiming, RemovalOnlyConfigIsRejected)
{
	RingStealthTiming::Config cfg;
	cfg.delayOff = 30;
	cfg.animOff = 15;
	cfg.hasOffFX = true;

	RingStealthTiming::State state;
	EXPECT_FALSE(RingStealthTiming::hasRingConfig(cfg));
	EXPECT_FALSE(RingStealthTiming::canToggle(cfg, state, 50, false));
}

}
