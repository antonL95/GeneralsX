#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace AttributeModifierRules
{
enum Kind : int
{
	KIND_NONE,
	ARMOR,
	DAMAGE_ADD,
	DAMAGE_MULT,
	RESIST_FEAR,
	RESIST_TERROR,
	EXPERIENCE,
	RANGE,
	SPEED,
	CRUSH_DECELERATE,
	RESIST_KNOCKBACK,
	SPELL_DAMAGE,
	RECHARGE_TIME,
	PRODUCTION,
	HEALTH,
	HEALTH_MULT,
	VISION,
	BOUNTY_PERCENTAGE,
	MINIMUM_CRUSH_VELOCITY,
	AUTO_HEAL,
	SHROUD_CLEARING,
	RATE_OF_FIRE,
	DAMAGE_STRUCTURE_BOUNTY_ADD,
	CRUSHER_LEVEL,
	COMMAND_POINT_BONUS,
	CRUSHABLE_LEVEL,
	CRUSHED_DECELERATE,
	INVULNERABLE,
	KIND_COUNT
};

inline const char* const* kindNames()
{
	static const char* const names[] =
	{
		"ATTRIBUTE_NONE", "ARMOR", "DAMAGE_ADD", "DAMAGE_MULT", "RESIST_FEAR", "RESIST_TERROR", "EXPERIENCE", "RANGE",
		"SPEED", "CRUSH_DECELERATE", "RESIST_KNOCKBACK", "SPELL_DAMAGE", "RECHARGE_TIME", "PRODUCTION", "HEALTH",
		"HEALTH_MULT", "VISION", "BOUNTY_PERCENTAGE", "MINIMUM_CRUSH_VELOCITY", "AUTO_HEAL", "SHROUD_CLEARING",
		"RATE_OF_FIRE", "DAMAGE_STRUCTURE_BOUNTY_ADD", "CRUSHER_LEVEL", "COMMAND_POINT_BONUS", "CRUSHABLE_LEVEL",
		"CRUSHED_DECELERATE", "INVULNERABLE", nullptr
	};
	return names;
}

enum Category : int
{
	CATEGORY_NONE,
	LEADERSHIP,
	FORMATION,
	SPELL,
	WEAPON,
	STRUCTURE,
	LEVEL,
	BUFF,
	DEBUFF,
	STUN,
	INNATE_ARMOR,
	INNATE_DAMAGEMULT,
	INNATE_VISION,
	INNATE_AUTOHEAL,
	INNATE_HEALTH,
	CATEGORY_COUNT
};

inline const char* const* categoryNames()
{
	static const char* const names[] =
	{
		"NONE", "LEADERSHIP", "FORMATION", "SPELL", "WEAPON", "STRUCTURE", "LEVEL", "BUFF", "DEBUFF", "STUN",
		"INNATE_ARMOR", "INNATE_DAMAGEMULT", "INNATE_VISION", "INNATE_AUTOHEAL", "INNATE_HEALTH", nullptr
	};
	return names;
}

enum Stance : int
{
	STANCE_UNINITIALIZED,
	STANCE_BATTLE,
	STANCE_AGGRESSIVE,
	STANCE_HOLD_GROUND,
	STANCE_PORCUPINE,
	STANCE_HOLD_GROUND_MOVING,
	STANCE_COUNT
};

inline const char* const* stanceNames()
{
	static const char* const names[] =
	{
		"Uninitialized", "Battle", "Aggressive", "HoldGround", "Porcupine", "HoldGroundMoving", nullptr
	};
	return names;
}

inline int indexOf(const char* const* names, const char* name)
{
	for (int index = 0; names[index]; ++index)
	{
		const char* a = names[index];
		const char* b = name;
		while (*a && *b && (*a | 0x20) == (*b | 0x20))
		{
			++a;
			++b;
		}
		if (!*a && !*b)
			return index;
	}
	return -1;
}

inline bool isMultiplicative(int kind)
{
	switch (kind)
	{
		case DAMAGE_MULT:
		case EXPERIENCE:
		case SPEED:
		case CRUSH_DECELERATE:
		case SPELL_DAMAGE:
		case RECHARGE_TIME:
		case PRODUCTION:
		case HEALTH_MULT:
		case MINIMUM_CRUSH_VELOCITY:
		case RATE_OF_FIRE:
		case CRUSHED_DECELERATE:
			return true;
		default:
			return false;
	}
}

inline float amountFromToken(const char* token, float scanned)
{
	return token && std::strchr(token, '%') ? scanned / 100.0f : scanned;
}

inline unsigned categoryBit(int category)
{
	return category > CATEGORY_NONE && category < CATEGORY_COUNT ? 1u << category : 0u;
}

inline bool isInnateCategory(int category)
{
	return category >= INNATE_ARMOR && category <= INNATE_HEALTH;
}

struct Entry
{
	int kind = KIND_NONE;
	float value = 0.0f;
	std::vector<int> damageTypes;
};

struct ListDef
{
	std::vector<Entry> entries;
	int category = CATEGORY_NONE;
	unsigned durationFrames = 0;
	bool replaceInCategoryIfLongest = false;
	bool ignoreIfAnticategoryActive = false;
};

using Owner = std::uint64_t;

constexpr Owner ANONYMOUS_OWNER = 0;

struct Hold
{
	Owner owner = ANONYMOUS_OWNER;
	unsigned endFrame = 0;
};

struct Active
{
	int list = -1;
	unsigned endFrame = 0;
	unsigned antiMask = 0;
	std::vector<Hold> holds;
};

inline unsigned combinedEnd(const std::vector<Hold>& holds)
{
	unsigned end = 0;
	for (const Hold& hold : holds)
	{
		if (hold.endFrame == 0)
			return 0;
		end = std::max(end, hold.endFrame);
	}
	return end;
}

inline void dropExpiredHolds(std::vector<Hold>& holds, unsigned now)
{
	holds.erase(std::remove_if(holds.begin(), holds.end(),
		[now](const Hold& hold) { return hold.endFrame != 0 && hold.endFrame <= now; }), holds.end());
}

enum ApplyResult
{
	APPLIED,
	REFRESHED,
	IGNORED
};

inline bool outlasts(unsigned candidateEnd, unsigned otherEnd)
{
	if (otherEnd == 0)
		return false;
	return candidateEnd == 0 || candidateEnd > otherEnd;
}

class Stack
{
public:
	template <class Lookup>
	ApplyResult apply(int list, unsigned endFrame, unsigned antiMask, const Lookup& defOf, std::vector<int>* removed,
		Owner owner = ANONYMOUS_OWNER)
	{
		const ListDef& def = defOf(list);
		if (def.ignoreIfAnticategoryActive && isWeatherSuppressed(def))
			return IGNORED;
		for (Active& active : m_active)
		{
			if (active.list != list)
				continue;
			auto hold = std::find_if(active.holds.begin(), active.holds.end(), [owner](const Hold& h) { return h.owner == owner; });
			if (hold == active.holds.end())
				active.holds.push_back(Hold{owner, endFrame});
			else if (hold->endFrame != 0 && outlasts(endFrame, hold->endFrame))
				hold->endFrame = endFrame;
			active.endFrame = combinedEnd(active.holds);
			active.antiMask |= antiMask;
			removeBlocked(antiMask, list, defOf, removed);
			return REFRESHED;
		}
		if (def.ignoreIfAnticategoryActive && (blockedMask() & categoryBit(def.category)))
			return IGNORED;
		if (def.replaceInCategoryIfLongest && def.category != CATEGORY_NONE)
		{
			for (const Active& active : m_active)
			{
				const ListDef& other = defOf(active.list);
				if (other.category == def.category && other.replaceInCategoryIfLongest && !outlasts(endFrame, active.endFrame))
					return IGNORED;
			}
			for (size_t index = 0; index < m_active.size();)
			{
				const ListDef& other = defOf(m_active[index].list);
				if (other.category == def.category && other.replaceInCategoryIfLongest)
					eraseAt(index, removed);
				else
					++index;
			}
		}
		m_active.push_back(Active{list, endFrame, antiMask, {Hold{owner, endFrame}}});
		removeBlocked(antiMask, list, defOf, removed);
		return APPLIED;
	}

	bool remove(int list, Owner owner, unsigned now)
	{
		for (size_t index = 0; index < m_active.size(); ++index)
		{
			Active& active = m_active[index];
			if (active.list != list)
				continue;
			active.holds.erase(std::remove_if(active.holds.begin(), active.holds.end(),
				[owner](const Hold& hold) { return hold.owner == owner; }), active.holds.end());
			dropExpiredHolds(active.holds, now);
			if (active.holds.empty())
			{
				m_active.erase(m_active.begin() + static_cast<std::ptrdiff_t>(index));
				return true;
			}
			active.endFrame = combinedEnd(active.holds);
			return false;
		}
		return false;
	}

	std::vector<int> removeTimed()
	{
		std::vector<int> removed;
		for (size_t index = 0; index < m_active.size();)
		{
			Active& active = m_active[index];
			active.holds.erase(std::remove_if(active.holds.begin(), active.holds.end(),
				[](const Hold& hold) { return hold.endFrame != 0; }), active.holds.end());
			if (active.holds.empty())
				eraseAt(index, &removed);
			else
			{
				active.endFrame = combinedEnd(active.holds);
				++index;
			}
		}
		return removed;
	}

	template <class Lookup>
	void removeCategories(unsigned mask, const Lookup& defOf, std::vector<int>* removed)
	{
		for (size_t index = 0; index < m_active.size();)
		{
			if (mask & categoryBit(defOf(m_active[index].list).category))
				eraseAt(index, removed);
			else
				++index;
		}
	}

	std::vector<int> expire(unsigned now)
	{
		std::vector<int> expired;
		for (size_t index = 0; index < m_active.size();)
		{
			if (m_active[index].endFrame != 0 && m_active[index].endFrame <= now)
				eraseAt(index, &expired);
			else
			{
				dropExpiredHolds(m_active[index].holds, now);
				++index;
			}
		}
		return expired;
	}

	unsigned nextExpiry() const
	{
		unsigned next = 0;
		for (const Active& active : m_active)
		{
			if (active.endFrame != 0 && (next == 0 || active.endFrame < next))
				next = active.endFrame;
		}
		return next;
	}

	bool contains(int list) const
	{
		return std::any_of(m_active.begin(), m_active.end(), [list](const Active& active) { return active.list == list; });
	}

	unsigned blockedMask() const
	{
		unsigned mask = m_weatherAntiMask;
		for (const Active& active : m_active)
			mask |= active.antiMask;
		return mask;
	}

	unsigned weatherAntiMask() const { return m_weatherAntiMask; }
	void setWeatherAntiMask(unsigned mask) { m_weatherAntiMask = mask; }
	bool isWeatherSuppressed(const ListDef& def) const { return (m_weatherAntiMask & categoryBit(def.category)) != 0; }

	const std::vector<Active>& active() const { return m_active; }
	std::vector<Active>& mutableActive() { return m_active; }
	void clear() { m_active.clear(); }

private:
	void eraseAt(size_t index, std::vector<int>* removed)
	{
		if (removed)
			removed->push_back(m_active[index].list);
		m_active.erase(m_active.begin() + static_cast<std::ptrdiff_t>(index));
	}

	template <class Lookup>
	void removeBlocked(unsigned antiMask, int keep, const Lookup& defOf, std::vector<int>* removed)
	{
		if (antiMask == 0)
			return;
		for (size_t index = 0; index < m_active.size();)
		{
			const ListDef& other = defOf(m_active[index].list);
			if (m_active[index].list != keep && other.ignoreIfAnticategoryActive && (antiMask & categoryBit(other.category)))
				eraseAt(index, removed);
			else
				++index;
		}
	}

	std::vector<Active> m_active;
	unsigned m_weatherAntiMask = 0;
};

struct TypedArmor
{
	int damageType = -1;
	float sum = 0.0f;
};

struct Totals
{
	float value[KIND_COUNT];
	float damageMultWithoutInnate = 1.0f;
	std::vector<TypedArmor> typedArmor;
	std::vector<int> invulnerableTypes;
	int lists = 0;

	Totals()
	{
		for (int kind = 0; kind < KIND_COUNT; ++kind)
			value[kind] = isMultiplicative(kind) ? 1.0f : 0.0f;
	}

	void add(const ListDef& def)
	{
		++lists;
		for (const Entry& entry : def.entries)
		{
			if (entry.kind <= KIND_NONE || entry.kind >= KIND_COUNT)
				continue;
			if (entry.kind == INVULNERABLE)
			{
				for (int type : entry.damageTypes)
				{
					if (std::find(invulnerableTypes.begin(), invulnerableTypes.end(), type) == invulnerableTypes.end())
						invulnerableTypes.push_back(type);
				}
				continue;
			}
			if (entry.kind == ARMOR && !entry.damageTypes.empty())
			{
				for (int type : entry.damageTypes)
				{
					auto found = std::find_if(typedArmor.begin(), typedArmor.end(),
						[type](const TypedArmor& armor) { return armor.damageType == type; });
					if (found == typedArmor.end())
						typedArmor.push_back(TypedArmor{type, entry.value});
					else
						found->sum += entry.value;
				}
				continue;
			}
			if (isMultiplicative(entry.kind))
				value[entry.kind] *= entry.value;
			else
				value[entry.kind] += entry.value;
			if (entry.kind == DAMAGE_MULT && !isInnateCategory(def.category))
				damageMultWithoutInnate *= entry.value;
		}
	}

	float armorFor(int damageType) const
	{
		float sum = value[ARMOR];
		for (const TypedArmor& armor : typedArmor)
		{
			if (armor.damageType == damageType)
				sum += armor.sum;
		}
		return sum;
	}

	float damageTakenFactor(int damageType, float maxBonus) const
	{
		if (std::find(invulnerableTypes.begin(), invulnerableTypes.end(), damageType) != invulnerableTypes.end())
			return 0.0f;
		const float armor = std::min(armorFor(damageType), maxBonus);
		return std::max(0.0f, 1.0f - armor);
	}

	float damageDealtFactor(bool useInnate = true) const { return useInnate ? value[DAMAGE_MULT] : damageMultWithoutInnate; }

	float rangeFactor() const { return std::max(0.0f, 1.0f + value[RANGE]); }
	float visionFactor() const { return std::max(0.0f, 1.0f + value[VISION]); }
	float shroudFactor() const { return std::max(0.0f, 1.0f + value[SHROUD_CLEARING]); }
};

template <class Lookup>
Totals totalsOf(const std::vector<const Stack*>& stacks, const Lookup& defOf)
{
	Totals totals;
	std::vector<int> seen;
	for (const Stack* stack : stacks)
	{
		if (!stack)
			continue;
		for (const Active& active : stack->active())
		{
			const ListDef& def = defOf(active.list);
			if (stack->isWeatherSuppressed(def) || std::find(seen.begin(), seen.end(), active.list) != seen.end())
				continue;
			seen.push_back(active.list);
			totals.add(def);
		}
	}
	return totals;
}

inline float baseDamageWithAdd(float scaledBase, bool acceptDamageAdd, float damageAdd)
{
	return acceptDamageAdd && scaledBase != 0.0f ? scaledBase + damageAdd : scaledBase;
}

inline bool rankAllowed(int maxActiveRank, int rank)
{
	return maxActiveRank == 0 || rank <= maxActiveRank;
}

inline bool alignmentAllowed(bool affectGood, bool affectEvil, bool targetEvil)
{
	if (affectGood && targetEvil)
		return false;
	return !affectEvil || targetEvil;
}

inline bool healedWithin(unsigned now, unsigned lastHealFrame, unsigned window)
{
	return lastHealFrame <= now && now - lastHealFrame < window;
}

inline unsigned endFrameFor(unsigned now, unsigned durationFrames)
{
	return durationFrames == 0 ? 0u : now + durationFrames;
}

inline int nextStance(const std::vector<int>& cycle, int current)
{
	if (cycle.empty())
		return current;
	for (size_t index = 0; index < cycle.size(); ++index)
	{
		if (cycle[index] == current)
			return cycle[(index + 1) % cycle.size()];
	}
	return cycle.front();
}

inline int stanceImageIndex(const std::vector<int>& cycle, int current)
{
	for (size_t index = 0; index < cycle.size(); ++index)
	{
		if (cycle[index] == current)
			return static_cast<int>(index);
	}
	return -1;
}

inline float maxHealthAfter(float currentMax, float appliedAdd, float appliedMult, float add, float mult)
{
	const float base = (appliedMult > 0.0f ? currentMax / appliedMult : currentMax) - appliedAdd;
	return std::max(1.0f, (base + add) * mult);
}
}
