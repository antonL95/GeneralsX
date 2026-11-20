#pragma once

#include <cstddef>
#include <string>
#include <vector>

struct HordePoint
{
	float x = 0.0f;
	float y = 0.0f;
};

struct HordeRankPosition
{
	HordePoint point;
	bool hasLeader = false;
	int leaderRank = 0;
	int leaderIndex = 0;
};

struct HordeRankInfo
{
	int rankNumber = 0;
	std::string unitType;
	std::vector<HordeRankPosition> positions;
	std::string revokedWeaponCondition;
	std::string grantedWeaponCondition;
};

struct HordeBannerCarrierPosition
{
	std::string unitType;
	HordePoint position;
};

struct HordeSplit
{
	std::string result;
	std::string unitType;
	int rankNumber = 0;
};

struct HordeCombo
{
	std::string target;
	std::string result;
	std::string initiateVoice;
};

namespace HordeAttributes
{
bool parseRankInfo(const std::vector<std::string>& tokens, HordeRankInfo& result);
bool parseBannerCarrierPosition(const std::vector<std::string>& tokens, HordeBannerCarrierPosition& result);
bool parseSplit(const std::vector<std::string>& tokens, HordeSplit& result);
bool parseCombo(const std::vector<std::string>& tokens, HordeCombo& result);
}

struct HordeSlot
{
	int rank = 0;
	int index = 0;
	std::string unitType;
	HordePoint position;
	bool hasLeader = false;
	int leaderRank = 0;
	int leaderIndex = 0;
};

struct HordePayload
{
	std::string name;
	int count = 1;
};

struct HordeMemberSlot
{
	std::size_t payload = 0;
	int slot = -1;
};

class HordeLayout
{
public:
	void addRank(const HordeRankInfo& rank);
	void addPayload(const std::string& name, int count);
	void setCapacity(int capacity) { m_capacity = capacity; }
	int capacity() const { return m_capacity; }

	const std::vector<HordeSlot>& slots() const { return m_slots; }
	const std::vector<HordePayload>& payloads() const { return m_payloads; }

	int leaderSlot(int slot) const;
	static bool rankCanAttack(int rank, const std::vector<int>& released, const std::vector<int>& free);
	int findSlot(const std::string& unitType, const std::vector<bool>& occupied) const;
	std::vector<HordeMemberSlot> initialMembers(int capacity) const;

	static HordePoint rotate(HordePoint offset, float cosAngle, float sinAngle);

private:
	std::vector<HordeSlot> m_slots;
	std::vector<HordePayload> m_payloads;
	int m_capacity = 0;
};
