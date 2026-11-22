#include <gtest/gtest.h>

#include "GameLogic/RetailTestCommandRegistry.h"

namespace
{
using Handler = int (*)(int);

int handlerA(int v) { return v + 1; }
int handlerB(int v) { return v + 2; }
int handlerC(int v) { return v + 3; }

TEST(RetailTestCommandRegistryTest, ExactMatchTakesPrecedenceOverPrefix)
{
	RetailTestCommands::Registry<Handler> registry;
	EXPECT_TRUE(registry.registerPrefix("pkg12-", handlerA));
	EXPECT_TRUE(registry.registerAction("pkg12-attach", handlerB));

	EXPECT_EQ(registry.find("pkg12-attach"), handlerB);
	EXPECT_EQ(registry.find("pkg12-other"), handlerA);
	EXPECT_EQ(registry.find("pkg13-other"), nullptr);
}

TEST(RetailTestCommandRegistryTest, LongestPrefixWinsDeterministically)
{
	RetailTestCommands::Registry<Handler> firstOrder;
	EXPECT_TRUE(firstOrder.registerPrefix("pkg12-", handlerA));
	EXPECT_TRUE(firstOrder.registerPrefix("pkg12-ai-", handlerB));
	EXPECT_TRUE(firstOrder.registerPrefix("pkg12-pickup-", handlerC));

	RetailTestCommands::Registry<Handler> reverseOrder;
	EXPECT_TRUE(reverseOrder.registerPrefix("pkg12-pickup-", handlerC));
	EXPECT_TRUE(reverseOrder.registerPrefix("pkg12-ai-", handlerB));
	EXPECT_TRUE(reverseOrder.registerPrefix("pkg12-", handlerA));

	EXPECT_EQ(firstOrder.find("pkg12-ai-state"), handlerB);
	EXPECT_EQ(reverseOrder.find("pkg12-ai-state"), handlerB);
	EXPECT_EQ(firstOrder.find("pkg12-pickup-test"), handlerC);
	EXPECT_EQ(reverseOrder.find("pkg12-pickup-test"), handlerC);
	EXPECT_EQ(firstOrder.find("pkg12-misc"), handlerA);
	EXPECT_EQ(reverseOrder.find("pkg12-misc"), handlerA);
}

TEST(RetailTestCommandRegistryTest, RejectsDuplicatesAndInvalidRegistrations)
{
	RetailTestCommands::Registry<Handler> registry;
	EXPECT_FALSE(registry.registerAction("", handlerA));
	EXPECT_FALSE(registry.registerPrefix("", handlerA));
	EXPECT_FALSE(registry.registerAction("alpha", nullptr));
	EXPECT_FALSE(registry.registerPrefix("beta-", nullptr));

	EXPECT_TRUE(registry.registerAction("alpha", handlerA));
	EXPECT_FALSE(registry.registerAction("alpha", handlerA));
	EXPECT_FALSE(registry.registerAction("alpha", handlerB));

	EXPECT_TRUE(registry.registerPrefix("beta-", handlerA));
	EXPECT_FALSE(registry.registerPrefix("beta-", handlerA));
	EXPECT_FALSE(registry.registerPrefix("beta-", handlerB));

	EXPECT_EQ(registry.exactCount(), 1u);
	EXPECT_EQ(registry.prefixCount(), 1u);
}
}
