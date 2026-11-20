#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <tuple>
#include <vector>

namespace TerrainResourceRules
{
inline std::vector<int> rowHalfWidths(int radius)
{
	std::vector<int> widths(radius + 1);
	int halfWidth = 0;
	const auto edge = [radius](int x, int y)
	{
		return std::int64_t(x) * (x + 2) + std::int64_t(y) * (y - 2) -
			std::int64_t(radius) * radius + 2;
	};
	for (int y = radius; y >= 0; --y)
	{
		if (y < radius && edge(halfWidth, y) < halfWidth)
			++halfWidth;
		while (edge(halfWidth, y) + y <= 0)
			++halfWidth;
		widths[y] = radius == 1 && y == 0 ? 1 : halfWidth;
	}
	return widths;
}

struct ClaimOrder
{
	bool priority;
	unsigned int frame;
	unsigned int objectId;
	int player;
};

inline bool blocks(const ClaimOrder& claim, const ClaimOrder& other)
{
	if (claim.priority != other.priority)
		return other.priority;
	const auto order = std::tie(claim.frame, claim.objectId);
	const auto otherOrder = std::tie(other.frame, other.objectId);
	if (claim.priority)
		return otherOrder > order;
	return other.player == claim.player && otherOrder < order;
}

inline float incomeScale(const std::vector<int>& values, std::size_t buildingCount)
{
	if (values.empty())
		return 1.0f;
	if (buildingCount < values.size())
		return static_cast<float>(values[buildingCount]) * 0.01f;
	return std::max(0.0f, static_cast<float>(values.back()) * 0.01f -
		static_cast<float>(buildingCount - values.size()) * 0.02f);
}

inline unsigned int income(int maximum, std::uint64_t claimed, std::uint64_t total,
	float upgradeScale, float productionScale, float playerScale)
{
	if (maximum <= 0 || claimed == 0 || total == 0)
		return 0;
	constexpr unsigned int maximumDeposit = std::numeric_limits<int>::max();
	const float fraction = static_cast<float>(claimed) / static_cast<float>(total);
	const float base = static_cast<float>(maximum) * fraction * productionScale * upgradeScale;
	if (!(base > 0.0f))
		return 0;
	const auto roundUp = [maximumDeposit](float value)
	{
		if (value >= static_cast<float>(maximumDeposit))
			return maximumDeposit;
		const auto whole = static_cast<unsigned int>(value);
		return whole + (value > static_cast<float>(whole) ? 1u : 0u);
	};
	const auto roundedBase = roundUp(base);
	const float adjusted = static_cast<float>(roundedBase) * playerScale;
	return adjusted > 0.0f ? roundUp(adjusted) : 1;
}
}
