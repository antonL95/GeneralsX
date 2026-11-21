#include "Common/HordeFormation.h"

#include <algorithm>

#include <cctype>
#include <cerrno>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <map>
#include <tuple>
#include <utility>

namespace
{
bool equalsNoCase(const std::string& text, const char* expected)
{
	std::size_t index = 0;
	for (; index < text.size() && expected[index]; ++index)
	{
		if (std::tolower(static_cast<unsigned char>(text[index])) != std::tolower(static_cast<unsigned char>(expected[index])))
			return false;
	}
	return index == text.size() && !expected[index];
}

bool sameName(const std::string& left, const std::string& right)
{
	return left.size() == right.size() && equalsNoCase(left, right.c_str());
}

class Cursor
{
public:
	explicit Cursor(const std::vector<std::string>& tokens) : m_tokens(tokens) {}

	bool done() const { return m_next >= m_tokens.size(); }
	const std::string& peek() const { return m_tokens[m_next]; }

	bool take(std::string& value)
	{
		if (done())
			return false;
		value = m_tokens[m_next++];
		return true;
	}

	bool expect(const char* key)
	{
		if (done() || !equalsNoCase(m_tokens[m_next], key))
			return false;
		++m_next;
		return true;
	}

	bool takeInt(int& value)
	{
		std::string text;
		if (!take(text) || text.empty())
			return false;
		char* end = nullptr;
		errno = 0;
		const long parsed = std::strtol(text.c_str(), &end, 10);
		if (*end || errno == ERANGE || parsed < INT_MIN || parsed > INT_MAX)
			return false;
		value = static_cast<int>(parsed);
		return true;
	}

	bool takeFloat(float& value)
	{
		std::string text;
		if (!take(text) || text.empty())
			return false;
		char* end = nullptr;
		errno = 0;
		const float parsed = std::strtof(text.c_str(), &end);
		if (*end || errno == ERANGE)
			return false;
		value = parsed;
		return true;
	}

	bool takePoint(HordePoint& point)
	{
		return expect("X") && takeFloat(point.x) && expect("Y") && takeFloat(point.y);
	}

private:
	const std::vector<std::string>& m_tokens;
	std::size_t m_next = 0;
};

bool isRankKey(const std::string& token)
{
	static const char* const keys[] =
	{
		"RankNumber", "UnitType", "Position", "Leader", "RevokedWeaponCondition", "GrantedWeaponCondition"
	};
	for (const char* key : keys)
	{
		if (equalsNoCase(token, key))
			return true;
	}
	return false;
}
}

bool HordeAttributes::parseRankInfo(const std::vector<std::string>& tokens, HordeRankInfo& result)
{
	Cursor cursor(tokens);
	HordeRankInfo rank;
	bool hasNumber = false;
	bool hasType = false;
	std::string key;
	while (cursor.take(key))
	{
		if (equalsNoCase(key, "RankNumber"))
		{
			if (!cursor.takeInt(rank.rankNumber))
				return false;
			hasNumber = true;
		}
		else if (equalsNoCase(key, "UnitType"))
		{
			if (!cursor.take(rank.unitType))
				return false;
			hasType = true;
		}
		else if (equalsNoCase(key, "Position"))
		{
			HordeRankPosition position;
			if (!cursor.takePoint(position.point))
				return false;
			rank.positions.push_back(position);
		}
		else if (equalsNoCase(key, "Leader"))
		{
			if (rank.positions.empty())
				return false;
			HordeRankPosition& position = rank.positions.back();
			if (!cursor.takeInt(position.leaderRank) || !cursor.takeInt(position.leaderIndex))
				return false;
			position.hasLeader = true;
		}
		else if (equalsNoCase(key, "RevokedWeaponCondition") || equalsNoCase(key, "GrantedWeaponCondition"))
		{
			std::string& condition = equalsNoCase(key, "RevokedWeaponCondition") ? rank.revokedWeaponCondition : rank.grantedWeaponCondition;
			condition.clear();
			if (!cursor.done() && !isRankKey(cursor.peek()))
				cursor.take(condition);
		}
		else
		{
			return false;
		}
	}
	if (!hasNumber || !hasType)
		return false;
	result = std::move(rank);
	return true;
}

bool HordeAttributes::parseBannerCarrierPosition(const std::vector<std::string>& tokens, HordeBannerCarrierPosition& result)
{
	Cursor cursor(tokens);
	HordeBannerCarrierPosition banner;
	bool hasType = false;
	bool hasPosition = false;
	std::string key;
	while (cursor.take(key))
	{
		bool ok = false;
		if (equalsNoCase(key, "UnitType"))
			ok = hasType = cursor.take(banner.unitType);
		else if (equalsNoCase(key, "Pos"))
			ok = hasPosition = cursor.takePoint(banner.position);
		if (!ok)
			return false;
	}
	if (!hasType || !hasPosition)
		return false;
	result = std::move(banner);
	return true;
}

bool HordeAttributes::parseSplit(const std::vector<std::string>& tokens, HordeSplit& result)
{
	Cursor cursor(tokens);
	HordeSplit split;
	bool hasResult = false;
	bool hasType = false;
	std::string key;
	while (cursor.take(key))
	{
		bool ok = false;
		if (equalsNoCase(key, "SplitResult"))
			ok = hasResult = cursor.take(split.result);
		else if (equalsNoCase(key, "UnitType"))
			ok = hasType = cursor.take(split.unitType);
		else if (equalsNoCase(key, "RankNumber"))
			ok = cursor.takeInt(split.rankNumber);
		if (!ok)
			return false;
	}
	if (!hasResult || !hasType)
		return false;
	result = std::move(split);
	return true;
}

bool HordeAttributes::parseCombo(const std::vector<std::string>& tokens, HordeCombo& result)
{
	Cursor cursor(tokens);
	HordeCombo combo;
	bool hasTarget = false;
	bool hasResult = false;
	std::string key;
	while (cursor.take(key))
	{
		bool ok = false;
		if (equalsNoCase(key, "Target"))
			ok = hasTarget = cursor.take(combo.target);
		else if (equalsNoCase(key, "Result"))
			ok = hasResult = cursor.take(combo.result);
		else if (equalsNoCase(key, "InitiateVoice"))
			ok = cursor.take(combo.initiateVoice);
		if (!ok)
			return false;
	}
	if (!hasTarget || !hasResult)
		return false;
	result = std::move(combo);
	return true;
}

void HordeLayout::addRank(const HordeRankInfo& rank)
{
	auto insert = m_slots.begin();
	while (insert != m_slots.end() && insert->rank <= rank.rankNumber)
		++insert;
	std::vector<HordeSlot> added;
	added.reserve(rank.positions.size());
	for (std::size_t index = 0; index < rank.positions.size(); ++index)
	{
		const auto& position = rank.positions[index];
		added.push_back({ rank.rankNumber, static_cast<int>(index), rank.unitType, position.point,
			position.hasLeader, position.leaderRank, position.leaderIndex });
	}
	m_slots.insert(insert, added.begin(), added.end());
}

void HordeLayout::addPayload(const std::string& name, int count)
{
	m_payloads.push_back({ name, count });
}

int HordeLayout::findSlot(const std::string& unitType, const std::vector<bool>& occupied) const
{
	const auto isFree = [&occupied](std::size_t index)
	{
		return index >= occupied.size() || !occupied[index];
	};
	for (std::size_t index = 0; index < m_slots.size(); ++index)
	{
		if (isFree(index) && sameName(m_slots[index].unitType, unitType))
			return static_cast<int>(index);
	}
	for (std::size_t index = 0; index < m_slots.size(); ++index)
	{
		if (isFree(index))
			return static_cast<int>(index);
	}
	return -1;
}

std::vector<HordeMemberSlot> HordeLayout::initialMembers(int capacity) const
{
	std::vector<HordeMemberSlot> members;
	if (capacity <= 0)
		return members;

	std::vector<bool> occupied(m_slots.size(), false);
	for (std::size_t payload = 0; payload < m_payloads.size(); ++payload)
	{
		for (int count = 0; count < m_payloads[payload].count; ++count)
		{
			if (static_cast<int>(members.size()) >= capacity)
				return members;
			const int slot = findSlot(m_payloads[payload].name, occupied);
			if (slot >= 0)
				occupied[slot] = true;
			members.push_back({ payload, slot });
		}
	}
	return members;
}

HordePoint HordeLayout::rotate(HordePoint offset, float cosAngle, float sinAngle)
{
	const float forwardX = offset.x * cosAngle;
	const float sideX = offset.y * sinAngle;
	const float forwardY = offset.x * sinAngle;
	const float sideY = offset.y * cosAngle;
	return { forwardX - sideX, forwardY + sideY };
}

int HordeLayout::leaderSlot(int slot) const
{
	if (slot < 0 || static_cast<std::size_t>(slot) >= m_slots.size())
		return -1;
	const HordeSlot& follower = m_slots[slot];
	if (!follower.hasLeader || follower.leaderRank >= follower.rank)
		return -1;
	for (std::size_t index = 0; index < m_slots.size(); ++index)
	{
		const HordeSlot& leader = m_slots[index];
		if (leader.rank == follower.leaderRank && leader.index == follower.leaderIndex)
			return static_cast<int>(index);
	}
	return -1;
}

bool HordeLayout::rankCanAttack(int rank, const std::vector<int>& released, const std::vector<int>& free)
{
	return (released.empty() && free.empty()) ||
		std::find(released.begin(), released.end(), rank) != released.end() ||
		std::find(free.begin(), free.end(), rank) != free.end();
}

std::vector<int> HordeLayout::nearestSlots(const std::vector<int>& currentSlots, const std::vector<HordePoint>& positions,
	const std::vector<bool>& reserved) const
{
	std::vector<int> result(currentSlots.size(), -1);
	struct Rank
	{
		double x = 0.0;
		int number = 0;
		std::vector<std::size_t> slots;
	};
	struct Group
	{
		std::vector<std::size_t> members;
		std::map<int, Rank> ranks;
		std::size_t capacity = 0;
	};
	const auto typeKey = [](std::string type) {
		for (char& letter : type)
		{
			if ('A' <= letter && letter <= 'Z')
				letter += 'a' - 'A';
		}
		return type;
	};
	std::map<std::string, Group> groups;
	for (std::size_t member = 0; member < currentSlots.size() && member < positions.size(); ++member)
	{
		const int slot = currentSlots[member];
		if (slot >= 0 && static_cast<std::size_t>(slot) < m_slots.size() &&
			std::isfinite(positions[member].x) && std::isfinite(positions[member].y))
			groups[typeKey(m_slots[slot].unitType)].members.push_back(member);
	}
	for (std::size_t slot = 0; slot < m_slots.size(); ++slot)
	{
		if (slot < reserved.size() && reserved[slot])
			continue;
		const HordeSlot& point = m_slots[slot];
		if (!std::isfinite(point.position.x) || !std::isfinite(point.position.y))
			continue;
		Group& group = groups[typeKey(point.unitType)];
		Rank& rank = group.ranks[point.rank];
		rank.number = point.rank;
		rank.x += point.position.x;
		rank.slots.push_back(slot);
		++group.capacity;
	}
	for (auto& entry : groups)
	{
		Group& group = entry.second;
		auto& members = group.members;
		if (members.empty() || members.size() > group.capacity)
			continue;
		std::sort(members.begin(), members.end(), [&](std::size_t a, std::size_t b) {
			return std::tie(positions[a].x, positions[a].y, currentSlots[a], a) <
				std::tie(positions[b].x, positions[b].y, currentSlots[b], b);
		});
		std::vector<Rank*> ranks;
		for (auto& item : group.ranks)
		{
			Rank& rank = item.second;
			rank.x /= rank.slots.size();
			ranks.push_back(&rank);
		}
		std::sort(ranks.begin(), ranks.end(), [](const Rank* a, const Rank* b) {
			return std::tie(a->x, a->number) < std::tie(b->x, b->number);
		});
		auto first = members.begin();
		std::size_t laterCapacity = group.capacity;
		for (std::size_t row = 0; row < ranks.size() && first != members.end(); ++row)
		{
			auto& slots = ranks[row]->slots;
			laterCapacity -= slots.size();
			const std::size_t left = members.end() - first;
			const std::size_t minimum = left > laterCapacity ? left - laterCapacity : 0;
			const std::size_t maximum = std::min(left, slots.size());
			auto last = first + maximum;
			if (row + 1 < ranks.size() && minimum < maximum)
			{
				const float boundary = static_cast<float>(0.5 * (ranks[row]->x + ranks[row + 1]->x));
				last = std::upper_bound(first + minimum, last, boundary, [&](float x, std::size_t member) {
					return x < positions[member].x;
				});
			}
			std::sort(first, last, [&](std::size_t a, std::size_t b) {
				return std::tie(positions[a].y, positions[a].x, currentSlots[a], a) <
					std::tie(positions[b].y, positions[b].x, currentSlots[b], b);
			});
			std::sort(slots.begin(), slots.end(), [&](std::size_t a, std::size_t b) {
				return std::tie(m_slots[a].position.y, m_slots[a].position.x, a) <
					std::tie(m_slots[b].position.y, m_slots[b].position.x, b);
			});
			auto slot = slots.begin();
			for (; first != last; ++first)
			{
				const auto latest = slots.end() - (last - first);
				const float y = positions[*first].y;
				auto closest = std::lower_bound(slot, latest, y, [&](std::size_t index, float value) {
					return m_slots[index].position.y < value;
				});
				if (closest != slot && y - m_slots[*(closest - 1)].position.y <= m_slots[*closest].position.y - y)
					--closest;
				result[*first] = static_cast<int>(*closest);
				slot = closest + 1;
			}
		}
	}
	return result;
}
