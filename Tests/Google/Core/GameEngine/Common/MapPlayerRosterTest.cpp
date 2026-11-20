#include <gtest/gtest.h>

#include "GameLogic/MapPlayerRoster.h"

TEST(MapPlayerRoster, UsesRotwkRulesStartingAtVersionFive)
{
	EXPECT_FALSE(MapPlayerRoster::isRotwk(0));
	EXPECT_FALSE(MapPlayerRoster::isRotwk(3));
	EXPECT_FALSE(MapPlayerRoster::isRotwk(4));
	EXPECT_TRUE(MapPlayerRoster::isRotwk(5));
	EXPECT_TRUE(MapPlayerRoster::isRotwk(6));
}

TEST(MapPlayerRoster, KeepsOnlyNeutralCivilianAndCreepsFromHighPass)
{
	for (const char* name : {"", "PlyrCivilian", "PlyrCreeps"})
		EXPECT_TRUE(MapPlayerRoster::keepsSide(name)) << name;
	for (const char* name : {"PlyrNeutral", "Player_1", "Player_2", "Player_3", "Player_4",
		"SkirmishMen", "SkirmishRohan", "SkirmishDwarves", "SkirmishElves", "SkirmishArnor",
		"SkirmishMirkwood", "SkirmishMordor", "SkirmishIsengard", "SkirmishWild",
		"SkirmishGuldur", "SkirmishAngmar", "SkirmishHuman", "ReplayObserver"})
		EXPECT_FALSE(MapPlayerRoster::keepsSide(name)) << name;
}

TEST(MapPlayerRoster, UsesStartPositionForPlayersAndSlotForObservers)
{
	EXPECT_EQ("Player_1", MapPlayerRoster::slotName(true, 3, 0, false));
	EXPECT_EQ("Player_8", MapPlayerRoster::slotName(true, 0, 7, false));
	EXPECT_EQ("Observer4", MapPlayerRoster::slotName(true, 3, -1, true));
	EXPECT_EQ("player3", MapPlayerRoster::slotName(false, 3, 0, false));
	EXPECT_EQ("player3", MapPlayerRoster::slotName(false, 3, -1, true));
}

TEST(MapPlayerRoster, ExtractsTeamAfterFirstSlash)
{
	EXPECT_EQ("teamPlyrCreeps", MapPlayerRoster::qualifiedTeam("PlyrCreeps/teamPlyrCreeps"));
	EXPECT_EQ("team", MapPlayerRoster::qualifiedTeam("/team"));
	EXPECT_EQ("team/suffix", MapPlayerRoster::qualifiedTeam("side/team/suffix"));
	EXPECT_TRUE(MapPlayerRoster::qualifiedTeam("teamPlayer_1").empty());
	EXPECT_TRUE(MapPlayerRoster::qualifiedTeam("side/").empty());
}
