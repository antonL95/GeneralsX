#pragma once

#include <algorithm>
#include <cstddef>
#include <optional>
#include <vector>

namespace ExperienceLevelRules
{
inline float progress(float experience, int current, int next)
{
	return next > current ? std::clamp((experience - current) / (next - current), 0.0f, 1.0f) : 1.0f;
}

struct Candidate
{
	int requiredExperience;
	bool singlePlayerOnly;
	bool multiPlayerOnly;
};

inline bool isAvailable(const Candidate& level, bool multiplayer)
{
	return multiplayer ? !level.singlePlayerOnly : !level.multiPlayerOnly;
}

inline std::optional<std::size_t> nextLevel(const std::vector<Candidate>& levels, int currentRequiredExperience, bool multiplayer)
{
	std::optional<std::size_t> best;
	for (std::size_t i = 0; i < levels.size(); ++i)
	{
		const Candidate& level = levels[i];
		if (!isAvailable(level, multiplayer) || level.requiredExperience <= currentRequiredExperience)
			continue;
		if (!best || level.requiredExperience < levels[*best].requiredExperience)
			best = i;
	}
	return best;
}

inline std::optional<std::size_t> firstLevel(const std::vector<Candidate>& levels, bool multiplayer)
{
	return nextLevel(levels, 0, multiplayer);
}
}
