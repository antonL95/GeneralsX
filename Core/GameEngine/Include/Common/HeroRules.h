#pragma once

#include <algorithm>
#include <vector>

namespace HeroRules
{
enum class State
{
	Recruitable,
	Revivable,
	Lost,
};

enum class Slot
{
	Hidden,
	Disabled,
	Busy,
	Recruit,
	Revive,
};

struct Price
{
	int cost = 0;
	unsigned int frames = 0;
};

struct ReviveEntry
{
	int level = 0;
	int cost = 0;
	unsigned int frames = 0;
};

template <class T>
std::vector<T> rosterOrder(const std::vector<T>& ringHeroes, const std::vector<T>& buildableHeroes)
{
	std::vector<T> roster(ringHeroes);
	roster.insert(roster.end(), buildableHeroes.begin(), buildableHeroes.end());
	return roster;
}

inline std::vector<int> reviveSlots(const std::vector<bool>& countedRevive)
{
	std::vector<int> slots(countedRevive.size(), -1);
	int next = 0;
	for (std::size_t i = 0; i < countedRevive.size(); ++i)
	{
		if (countedRevive[i])
			slots[i] = next++;
	}
	return slots;
}

inline Price revivePrice(const Price& base, const std::vector<ReviveEntry>& entries, int level)
{
	Price price = base;
	int matched = 0;
	for (const ReviveEntry& entry : entries)
	{
		if (entry.level <= level && entry.level > matched)
		{
			matched = entry.level;
			price.cost = entry.cost;
			price.frames = entry.frames;
		}
	}
	return price;
}

inline int levelFromVeterancy(int veterancy)
{
	return std::max(0, veterancy) + 1;
}

inline bool neededUpgradesMet(const std::vector<bool>& owned, bool any)
{
	if (owned.empty())
		return true;
	if (any)
		return std::any_of(owned.begin(), owned.end(), [](bool has) { return has; });
	return std::all_of(owned.begin(), owned.end(), [](bool has) { return has; });
}

inline State deathOutcome(bool respawnBody, bool canRespawn, bool killedByFilter)
{
	if (!respawnBody)
		return State::Recruitable;
	if (!canRespawn || killedByFilter)
		return State::Lost;
	return State::Revivable;
}

inline Slot slotFor(bool hasHero, bool upgradesMet, State state, bool alive, bool queued)
{
	if (!hasHero || state == State::Lost)
		return Slot::Hidden;
	if (queued)
		return Slot::Busy;
	if (alive || !upgradesMet)
		return Slot::Disabled;
	return state == State::Revivable ? Slot::Revive : Slot::Recruit;
}
}
