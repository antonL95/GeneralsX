#include "Common/HordeFormation.h"

#include <algorithm>

#include <cctype>
#include <cerrno>
#include <climits>
#include <cstdlib>
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
	std::vector<std::size_t> members;
	for (std::size_t member = 0; member < currentSlots.size() && member < positions.size(); ++member)
	{
		if (currentSlots[member] >= 0 && static_cast<std::size_t>(currentSlots[member]) < m_slots.size())
			members.push_back(member);
	}
	std::vector<std::size_t> available;
	for (std::size_t slot = 0; slot < m_slots.size(); ++slot)
	{
		if (slot >= reserved.size() || !reserved[slot])
			available.push_back(slot);
	}
	const std::size_t rows = members.size();
	const std::size_t columns = available.size();
	if (rows == 0 || rows > columns)
		return result;

	const double incompatible = 1.0e12;
	const auto cost = [&](std::size_t row, std::size_t column)
	{
		const std::size_t member = members[row];
		const auto& slot = m_slots[available[column]];
		if (!sameName(slot.unitType, m_slots[currentSlots[member]].unitType))
			return incompatible;
		const double dx = static_cast<double>(slot.position.x) - positions[member].x;
		const double dy = static_cast<double>(slot.position.y) - positions[member].y;
		return dx * dx + dy * dy;
	};

	const double infinity = 1.0e30;
	std::vector<double> rowPotential(rows + 1, 0.0);
	std::vector<double> columnPotential(columns + 1, 0.0);
	std::vector<std::size_t> columnRow(columns + 1, 0);
	std::vector<std::size_t> way(columns + 1, 0);
	for (std::size_t row = 1; row <= rows; ++row)
	{
		columnRow[0] = row;
		std::size_t column = 0;
		std::vector<double> minimum(columns + 1, infinity);
		std::vector<bool> used(columns + 1, false);
		do
		{
			used[column] = true;
			const std::size_t currentRow = columnRow[column];
			double delta = infinity;
			std::size_t next = 0;
			for (std::size_t candidate = 1; candidate <= columns; ++candidate)
			{
				if (used[candidate])
					continue;
				const double reduced = cost(currentRow - 1, candidate - 1) - rowPotential[currentRow] - columnPotential[candidate];
				if (reduced < minimum[candidate])
				{
					minimum[candidate] = reduced;
					way[candidate] = column;
				}
				if (minimum[candidate] < delta)
				{
					delta = minimum[candidate];
					next = candidate;
				}
			}
			for (std::size_t candidate = 0; candidate <= columns; ++candidate)
			{
				if (used[candidate])
				{
					rowPotential[columnRow[candidate]] += delta;
					columnPotential[candidate] -= delta;
				}
				else
					minimum[candidate] -= delta;
			}
			column = next;
		} while (columnRow[column] != 0);
		do
		{
			const std::size_t previous = way[column];
			columnRow[column] = columnRow[previous];
			column = previous;
		} while (column != 0);
	}

	for (std::size_t column = 1; column <= columns; ++column)
	{
		if (columnRow[column] == 0)
			continue;
		const std::size_t member = members[columnRow[column] - 1];
		if (cost(columnRow[column] - 1, column - 1) < incompatible)
			result[member] = static_cast<int>(available[column - 1]);
		else
			result[member] = currentSlots[member];
	}
	return result;
}
