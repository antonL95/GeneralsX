#pragma once

namespace ZeroHourUpgradeMaskLayout
{
	inline constexpr int CAPACITY = 1152;

	constexpr int crcBytes(int upgradeCount, int maskBytes)
	{
		return upgradeCount <= 512 ? 64 : maskBytes;
	}
}
