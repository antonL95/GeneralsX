#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>

namespace CommandPointRules
{

struct Pair
{
	int starting = 0;
	int maximum = 0;
};

struct Config
{
	Pair good;
	Pair evil;
	Pair goodAI;
	Pair evilAI;
	Pair goodMP[7];
	Pair evilMP[7];
	int goodBonus = 0;
	int evilBonus = 0;
	int goodLimit = 0;
	int evilLimit = 0;
};

inline Pair resolveBaseAndCap(const Config& config, bool isSkirmishOrMP, int activePlayerCount,
	bool isEvil, bool isAI, int factorPercent = 100)
{
	Pair result;
	if (isSkirmishOrMP)
	{
		int index = activePlayerCount - 2;
		if (index < 0)
			index = 0;
		if (index > 6)
			index = 6;
		result = isEvil ? config.evilMP[index] : config.goodMP[index];
		if (factorPercent > 0 && factorPercent != 100)
		{
			const auto scaled = (static_cast<std::int64_t>(result.maximum) * factorPercent) / 100;
			result.maximum = static_cast<int>(std::clamp<std::int64_t>(scaled,
				std::numeric_limits<int>::min(), std::numeric_limits<int>::max()));
		}
	}
	else if (isAI)
	{
		result = isEvil ? config.evilAI : config.goodAI;
	}
	else
	{
		result = isEvil ? config.evil : config.good;
	}
	return result;
}

inline int computeTotalLimit(int startingBase, int buildingBonus, int upgradeBonus, int hardCap)
{
	int total = startingBase + buildingBonus + upgradeBonus;
	if (hardCap > 0 && total > hardCap)
		total = hardCap;
	if (total < 0)
		total = 0;
	return total;
}

inline int objectUsedCommandPoints(int templateCommandPoints, bool isSelectable, bool isStructure, bool isHorde)
{
	if (templateCommandPoints > 0 && isSelectable && !isStructure && !isHorde)
		return templateCommandPoints;
	return 0;
}

inline bool canAffordCommandPoints(int usedPoints, int totalLimit, int cost, bool isArmyOfDead)
{
	if (cost <= 0 || isArmyOfDead)
		return true;
	return usedPoints + cost <= totalLimit;
}

}
