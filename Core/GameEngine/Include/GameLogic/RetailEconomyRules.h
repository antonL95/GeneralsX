#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>

class INI;

namespace RetailEconomy
{
inline constexpr std::uint32_t fullRate = 1'000'000;

void parsePercent(INI* ini, void* instance, void* store, const void* data);

enum class Rounding
{
	Down,
	Up
};

constexpr std::uint32_t fixedRate(double factor)
{
	constexpr auto maxRate = std::numeric_limits<std::uint32_t>::max();
	if (!(factor > 0.0))
		return 0;
	const double rounded = factor * fullRate + 0.5;
	return rounded >= maxRate ? maxRate : static_cast<std::uint32_t>(rounded);
}

constexpr int scaleMoney(int amount, std::uint32_t rate, Rounding rounding)
{
	if (amount <= 0)
		return 0;

	const std::uint64_t product = static_cast<std::uint64_t>(amount) * rate;
	const std::uint64_t scaled = (product + (rounding == Rounding::Up ? fullRate - 1 : 0)) / fullRate;
	return static_cast<int>(std::min<std::uint64_t>(scaled, std::numeric_limits<int>::max()));
}
}
