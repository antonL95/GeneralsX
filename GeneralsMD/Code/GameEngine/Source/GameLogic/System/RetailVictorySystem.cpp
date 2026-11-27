#include "PreRTS.h"
#include "GameLogic/RetailVictorySystem.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include "Common/INI.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/RetailINIStartup.h"
#include "Common/RotwkDuration.h"
#include "Common/Xfer.h"
#include "GameLogic/ExperienceLevelStore.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/TerrainLogic.h"
#include "WWMath/wwmath.h"

namespace
{
constexpr Real MINIMUM_CELL_SIZE = 100.0f;
constexpr Real HALF_CELL = 0.5f;
constexpr UnsignedInt UPDATE_INTERVAL = RotwkDuration::engineFrames(1, LOGICFRAMES_PER_SECOND);

Bool participates(const Player& player)
{
	const AsciiString side = player.getSide();
	return side.isNotEmpty() && side.compareNoCase("Observer") != 0 && side.compareNoCase("Civilian") != 0 &&
		&player != ThePlayerList->getNeutralPlayer();
}
}

void RetailVictorySystem::parseSettings(INI* ini)
{
	static const FieldParse fields[] = {
		{ "CellSize", INI::parseReal, nullptr, offsetof(Settings, cellSize) },
		{ "ScalePerLogicFrame", INI::parseReal, nullptr, offsetof(Settings, scale) },
		{ "SubtractPerLogicFrame", INI::parseReal, nullptr, offsetof(Settings, subtract) },
		{ "CellBonusRadius", INI::parseReal, nullptr, offsetof(Settings, bonusRadius) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	ini->getNextAsciiString();
	Settings unused;
	ini->initFromINI(TheGameLogic ? &TheGameLogic->getRetailVictorySystem().m_settings : &unused, fields);
}

void RetailVictorySystem::parseFaction(INI* ini)
{
	static const FieldParse fields[] = {
		{ "AllyDeathScaleFactor", INI::parseReal, nullptr, offsetof(Faction, deathScale) },
		{ "EnemyKillScaleFactor", INI::parseReal, nullptr, offsetof(Faction, killScale) },
		{ "VictoryThreshold", INI::parseReal, nullptr, offsetof(Faction, threshold) },
		{ "MapToCellVictoryRatio", INI::parseReal, nullptr, offsetof(Faction, cellRatio) },
		{ "MajorUnitValue", INI::parseReal, nullptr, offsetof(Faction, majorUnitValue) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	AsciiString name = ini->getNextAsciiString();
	if (name.compareNoCase("neutral") == 0)
		name.clear();
	Faction unused;
	Faction* faction = &unused;
	if (TheGameLogic)
	{
		auto& settings = TheGameLogic->getRetailVictorySystem().m_settings;
		faction = settings.findFaction(name);
		if (!faction)
			faction = &settings.factions.emplace_back(Faction{name});
	}
	ini->initFromINI(faction, fields);
}

const RetailVictorySystem::Faction* RetailVictorySystem::Settings::findFaction(const AsciiString& name) const
{
	const auto it = std::find_if(factions.begin(), factions.end(), [&name](const Faction& faction) {
		return faction.name.compareNoCase(name) == 0;
	});
	return it == factions.end() ? nullptr : &*it;
}

RetailVictorySystem::Faction* RetailVictorySystem::Settings::findFaction(const AsciiString& name)
{
	return const_cast<Faction*>(std::as_const(*this).findFaction(name));
}

void RetailVictorySystem::loadSettings(Xfer* xfer)
{
	m_settings = {};
	m_settings.factions.push_back({
		.name = AsciiString("Default"),
		.deathScale = 1.0f,
		.killScale = 1.0f,
		.threshold = 500.0f,
		.cellRatio = 1.0f,
		.majorUnitValue = 10.0f
	});
	if (!RetailINIStartup::loadSubsystem("TheVictorySystem", xfer))
	{
		RetailINIStartup::loadFile("Data\\INI\\Default\\VictorySystem.ini", xfer);
		RetailINIStartup::loadFile("Data\\INI\\VictorySystem.ini", xfer);
	}
	m_defaultSettings = m_settings;
}

void RetailVictorySystem::restoreSettings()
{
	m_settings = m_defaultSettings;
}

void RetailVictorySystem::startMap()
{
	endMap();
	if (!(m_settings.cellSize > 0.0f))
		return;

	Region3D extent;
	TheTerrainLogic->getMaximumPathfindExtent(&extent);
	const Real width = extent.hi.x - extent.lo.x;
	const Real height = extent.hi.y - extent.lo.y;
	if (!(width > 0.0f && height > 0.0f))
		return;

	const Real cellSize = std::min({std::max(m_settings.cellSize, MINIMUM_CELL_SIZE), width, height});
	const Real columns = WWMath::Ceil(width / cellSize);
	const Real rows = WWMath::Ceil(height / cellSize);
	constexpr auto maxIndex = std::numeric_limits<UnsignedInt>::max();
	if (!(columns >= 1.0f && rows >= 1.0f && columns < Real(maxIndex) && rows < Real(maxIndex)))
		return;

	const auto cols = static_cast<UnsignedInt>(columns);
	const auto lines = static_cast<UnsignedInt>(rows);
	if (cols + 1 > maxIndex / (lines + 1))
		return;

	m_cellSize = cellSize;
	m_rewardRadius = std::max(1.0f, m_settings.bonusRadius) * cellSize * HALF_CELL * WWMath::SqrtfOrigin(2.0f);
	m_grids[0] = {cols, lines, 0.0f, {}};
	m_grids[1] = {cols + 1, lines + 1, -cellSize * HALF_CELL, {}};
}

void RetailVictorySystem::endMap()
{
	m_cellSize = 0.0f;
	m_rewardRadius = 0.0f;
	m_mapScores = {};
	m_grids = {};
}

const RetailVictorySystem::Faction* RetailVictorySystem::factionFor(const Player& player) const
{
	if (const Faction* faction = m_settings.findFaction(player.getSide()))
		return faction;
	return m_settings.findFaction(AsciiString("Default"));
}

std::optional<UnsignedInt> RetailVictorySystem::Grid::cellAt(const Coord3D& position, Real cellSize) const
{
	if (cellSize <= 0.0f)
		return {};
	const double x = (static_cast<double>(position.x) - offset) / cellSize;
	const double y = (static_cast<double>(position.y) - offset) / cellSize;
	if (!(x >= 0.0 && y >= 0.0 && x < columns && y < rows))
		return {};
	return static_cast<UnsignedInt>(y) * columns + static_cast<UnsignedInt>(x);
}

void RetailVictorySystem::addDeath(Scores& scores, Int victim, Int killer, Real value)
{
	scores[victim].deaths += value;
	if (killer != victim)
		scores[killer].kills += value;
}

void RetailVictorySystem::recordDeath(const Object& victim, const Object& killer)
{
	if (m_cellSize <= 0.0f)
		return;

	const Player* loser = victim.getControllingPlayer();
	const Player* winner = killer.getControllingPlayer();
	if (!loser || !winner || !participates(*loser))
		return;
	const Int losingIndex = loser->getPlayerIndex();
	const Int winningIndex = winner->getPlayerIndex();
	if (losingIndex < 0 || losingIndex >= MAX_PLAYER_COUNT || winningIndex < 0 || winningIndex >= MAX_PLAYER_COUNT)
		return;

	const Coord3D& position = *victim.getPosition();
	Region3D extent;
	TheTerrainLogic->getExtent(&extent);
	if (!(position.x > extent.lo.x && position.x < extent.hi.x && position.y > extent.lo.y && position.y < extent.hi.y))
		return;

	const Faction* faction = factionFor(*loser);
	if (!faction)
		return;
	const Real value = victim.isKindOf(KINDOF_HERO) || victim.isKindOf(KINDOF_MONSTER) ? faction->majorUnitValue : 1.0f;
	if (!(value > 0.0f) || !std::isfinite(value))
		return;

	addDeath(m_mapScores, losingIndex, winningIndex, value);
	for (Grid& grid : m_grids)
		if (auto cell = grid.cellAt(position, m_cellSize))
			addDeath(grid.cells[*cell], losingIndex, winningIndex, value);
}

Real RetailVictorySystem::Score::momentum(const Faction& faction) const
{
	return kills * faction.killScale - deaths * faction.deathScale;
}

RetailVictorySystem::Momentum RetailVictorySystem::sample(const Player& player, const Coord3D& position) const
{
	Momentum momentum;
	const Faction* faction = factionFor(player);
	const Int index = player.getPlayerIndex();
	if (!faction || index < 0 || index >= MAX_PLAYER_COUNT)
		return momentum;

	momentum.map = m_mapScores[index].momentum(*faction);
	for (std::size_t i = 0; i < m_grids.size(); ++i)
	{
		const Grid& grid = m_grids[i];
		if (auto cell = grid.cellAt(position, m_cellSize))
			if (auto it = grid.cells.find(*cell); it != grid.cells.end())
				momentum.cells[i] = it->second[index].momentum(*faction);
	}
	return momentum;
}

std::vector<RetailVictorySystem::Win> RetailVictorySystem::Grid::collectWins(const PlayerFactions& factions)
{
	std::vector<Win> wins;
	for (auto cell = cells.begin(); cell != cells.end();)
	{
		Int winner = 0;
		for (; winner < MAX_PLAYER_COUNT; ++winner)
		{
			const Faction* faction = factions[winner];
			const Score& score = cell->second[winner];
			if (faction && score.kills > 0.0f && score.momentum(*faction) > faction->threshold * faction->cellRatio)
				break;
		}

		if (winner < MAX_PLAYER_COUNT)
		{
			wins.push_back({cell->first, winner});
			cell = cells.erase(cell);
		}
		else
			++cell;
	}
	return wins;
}

void RetailVictorySystem::reward(const Grid& grid, const Win& win) const
{
	Player* player = ThePlayerList->getNthPlayer(win.player);
	if (!player || !participates(*player) || !std::isfinite(m_rewardRadius))
		return;

	Coord3D center;
	const Real halfCell = m_cellSize * HALF_CELL;
	center.x = halfCell + Real(win.cell % grid.columns) * m_cellSize;
	center.y = halfCell + Real(win.cell / grid.columns) * m_cellSize;
	center.z = TheTerrainLogic->getGroundHeight(center.x, center.y);
	PartitionFilterSamePlayer samePlayer(player);
	PartitionFilter* filters[] = {&samePlayer, nullptr};
	SimpleObjectIterator* objects = ThePartitionManager->iterateObjectsInRange(&center, m_rewardRadius, FROM_CENTER_2D, filters);
	MemoryPoolObjectHolder holder(objects);

	std::vector<ObjectID> recipients;
	for (Object* object = objects->first(); object; object = objects->next())
		if (!object->isEffectivelyDead() && !object->isDestroyed())
			recipients.push_back(object->getID());
	std::sort(recipients.begin(), recipients.end());

	for (ObjectID id : recipients)
	{
		Object* object = TheGameLogic->findObjectByID(id);
		if (!object || object->isEffectivelyDead() || object->isDestroyed() || object->getControllingPlayer() != player)
			continue;
		ExperienceLevelStore::awardVictoryExperience(object);
	}
}

Bool RetailVictorySystem::erodeScores(Scores& scores, const Settings& settings)
{
	Bool occupied = FALSE;
	for (Score& score : scores)
	{
		if (score.kills > 0.0f)
			score.kills = std::max(0.0f, score.kills * settings.scale - settings.subtract);
		if (score.deaths > 0.0f)
			score.deaths = std::max(0.0f, score.deaths * settings.scale - settings.subtract);
		occupied |= score.kills > 0.0f || score.deaths > 0.0f;
	}
	return occupied;
}

void RetailVictorySystem::Grid::erode(const Settings& settings)
{
	for (auto it = cells.begin(); it != cells.end();)
	{
		if (RetailVictorySystem::erodeScores(it->second, settings))
			++it;
		else
			it = cells.erase(it);
	}
}

void RetailVictorySystem::update(UnsignedInt frame)
{
	if (m_cellSize <= 0.0f || frame % UPDATE_INTERVAL != 0)
		return;

	PlayerFactions factions{};
	Bool overThreshold = FALSE;
	const Int playerCount = std::min(ThePlayerList->getPlayerCount(), Int(MAX_PLAYER_COUNT));
	for (Int index = 0; index < playerCount; ++index)
	{
		const Player* player = ThePlayerList->getNthPlayer(index);
		if (!player)
			continue;
		if (const Faction* faction = factionFor(*player))
		{
			factions[index] = faction;
			overThreshold |= m_mapScores[index].momentum(*faction) > faction->threshold;
		}
	}

	if (overThreshold)
	{
		const auto unshifted = m_grids[0].collectWins(factions);
		const auto shifted = m_grids[1].collectWins(factions);
		const auto grid = shifted.size() > unshifted.size() ? 1 : 0;
		const auto& wins = grid == 0 ? unshifted : shifted;
		for (const Win& win : wins)
			reward(m_grids[grid], win);
	}

	for (Grid& grid : m_grids)
		grid.erode(m_settings);
	erodeScores(m_mapScores, m_settings);
}

void RetailVictorySystem::xferScores(Xfer* xfer, Scores& scores)
{
	for (Score& score : scores)
	{
		xfer->xferReal(&score.kills);
		xfer->xferReal(&score.deaths);
	}
}

void RetailVictorySystem::Grid::xfer(Xfer* xfer)
{
	UnsignedInt count = static_cast<UnsignedInt>(cells.size());
	xfer->xferUnsignedInt(&count);
	if (xfer->getXferMode() == XFER_LOAD)
	{
		cells.clear();
		const UnsignedInt capacity = columns * rows;
		if (count > capacity)
			throw XFER_INVALID_PARAMETERS;
		for (UnsignedInt i = 0; i < count; ++i)
		{
			UnsignedInt cell = 0;
			xfer->xferUnsignedInt(&cell);
			if (cell >= capacity)
				throw XFER_INVALID_PARAMETERS;
			auto [it, inserted] = cells.try_emplace(cell);
			if (!inserted)
				throw XFER_INVALID_PARAMETERS;
			RetailVictorySystem::xferScores(xfer, it->second);
		}
	}
	else
	{
		for (auto& [index, scores] : cells)
		{
			UnsignedInt cell = index;
			xfer->xferUnsignedInt(&cell);
			RetailVictorySystem::xferScores(xfer, scores);
		}
	}
}

void RetailVictorySystem::xfer(Xfer* xfer)
{
	xferScores(xfer, m_mapScores);
	for (Grid& grid : m_grids)
		grid.xfer(xfer);
}

void RetailVictorySystem::clearMomentum()
{
	m_mapScores = {};
	for (Grid& grid : m_grids)
		grid.cells.clear();
}
