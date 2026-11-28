#pragma once

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

struct ParsedWeightedAudioToken
{
	std::string name;
	std::string authoredName;
	int weight = 1000;
};

inline ParsedWeightedAudioToken parseWeightedAudioToken(const char* token)
{
	ParsedWeightedAudioToken parsed;
	if (!token)
		return parsed;
	parsed.name = token;
	const char* suffix = std::strrchr(token, ':');
	if (!suffix || suffix == token)
		return parsed;
	const char* end = token + std::strlen(token);
	int value = 0;
	const auto result = std::from_chars(suffix + 1, end, value);
	if (result.ec != std::errc() || result.ptr != end || value < 1)
		return parsed;
	parsed.authoredName = parsed.name;
	parsed.name.assign(token, static_cast<std::size_t>(suffix - token));
	parsed.weight = value;
	return parsed;
}

template <typename WeightFn, typename IntRandomFn, typename RealRandomFn>
inline int pickWeightedAudioIndex(int count, int previous, WeightFn&& weightOf, IntRandomFn&& intRandom, RealRandomFn&& realRandom)
{
	if (count < 2)
		return count - 1;
	if (previous < 0 || previous >= count)
		previous = -1;

	std::uint64_t total = 0;
	for (int i = 0; i < count; ++i)
		if (i != previous)
			total += static_cast<std::uint64_t>(weightOf(i));
	if (total == 0)
		return -1;

	std::uint64_t draw;
	if (total <= static_cast<std::uint64_t>(std::numeric_limits<int>::max()))
		draw = static_cast<std::uint64_t>(intRandom(0, static_cast<int>(total) - 1));
	else
		draw = std::min(total - 1, static_cast<std::uint64_t>(realRandom(0.0f, 1.0f) * static_cast<double>(total)));

	for (int i = 0; i < count; ++i)
	{
		if (i == previous)
			continue;
		const auto weight = static_cast<std::uint64_t>(weightOf(i));
		if (draw < weight)
			return i;
		draw -= weight;
	}
	return -1;
}

template <typename IntRandomFn, typename RealRandomFn>
inline int pickWeightedAudioIndex(const std::vector<int>& weights, int previous, IntRandomFn&& intRandom, RealRandomFn&& realRandom)
{
	return pickWeightedAudioIndex(
		static_cast<int>(weights.size()),
		previous,
		[&](int index) { return weights[static_cast<std::size_t>(index)]; },
		std::forward<IntRandomFn>(intRandom),
		std::forward<RealRandomFn>(realRandom));
}
