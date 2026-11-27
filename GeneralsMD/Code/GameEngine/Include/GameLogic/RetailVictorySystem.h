#pragma once

#include "Common/AsciiString.h"
#include "Common/GameCommon.h"

#include <array>
#include <map>
#include <optional>
#include <vector>

class INI;
class Object;
class Player;
class Xfer;

class RetailVictorySystem
{
public:
	struct Momentum
	{
		Real map = 0.0f;
		std::array<Real, 2> cells{};
	};

	static void parseSettings(INI* ini);
	static void parseFaction(INI* ini);

	void loadSettings(Xfer* xfer);
	void restoreSettings();
	void startMap();
	void endMap();
	void recordDeath(const Object& victim, const Object& killer);
	void update(UnsignedInt frame);
	Momentum sample(const Player& player, const Coord3D& position) const;
	void xfer(Xfer* xfer);
	void clearMomentum();

private:
	struct Faction
	{
		AsciiString name;
		Real deathScale = 0.0f;
		Real killScale = 0.0f;
		Real threshold = 0.0f;
		Real cellRatio = 0.0f;
		Real majorUnitValue = 0.0f;
	};

	struct Settings
	{
		Real cellSize = 0.0f;
		Real scale = 1.0f;
		Real subtract = 0.0f;
		Real bonusRadius = 1.0f;
		std::vector<Faction> factions;

		Faction* findFaction(const AsciiString& name);
		const Faction* findFaction(const AsciiString& name) const;
	};

	struct Score
	{
		Real kills = 0.0f;
		Real deaths = 0.0f;

		Real momentum(const Faction& faction) const;
	};

	using Scores = std::array<Score, MAX_PLAYER_COUNT>;
	using PlayerFactions = std::array<const Faction*, MAX_PLAYER_COUNT>;

	struct Win
	{
		UnsignedInt cell;
		Int player;
	};

	struct Grid
	{
		UnsignedInt columns = 0;
		UnsignedInt rows = 0;
		Real offset = 0.0f;
		std::map<UnsignedInt, Scores> cells;

		std::optional<UnsignedInt> cellAt(const Coord3D& position, Real cellSize) const;
		std::vector<Win> collectWins(const PlayerFactions& factions);
		void erode(const Settings& settings);
		void xfer(Xfer* xfer);
	};

	const Faction* factionFor(const Player& player) const;
	void reward(const Grid& grid, const Win& win) const;
	static void addDeath(Scores& scores, Int victim, Int killer, Real value);
	static Bool erodeScores(Scores& scores, const Settings& settings);
	static void xferScores(Xfer* xfer, Scores& scores);

	Settings m_settings;
	Settings m_defaultSettings;
	Real m_cellSize = 0.0f;
	Real m_rewardRadius = 0.0f;
	Scores m_mapScores{};
	std::array<Grid, 2> m_grids;
};
