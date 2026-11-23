#pragma once

#include <array>
#include <map>
#include <set>
#include <vector>

#include "Lib/BaseType.h"

class AsciiString;
class INI;
class TerrainLogic;
class Xfer;

class RetailTerrainFire
{
public:
	enum Action
	{
		INCREASE_BURN_RATE,
		DECREASE_BURN_RATE,
		INCREASE_FUEL,
		INCREASE_BURN_RATE_ON_EXISTING_FIRE,
		INCREASE_FUEL_ON_EXISTING_FIRE
	};

	struct Edit
	{
		Action action = INCREASE_BURN_RATE;
		Real amount = 0.0f;
		Real radius = 0.0f;
		Real arc = 0.0f;
		Real minMaxBurnRate = 0.0f;
		Real minDecay = 0.0f;
		Real maxResistance = 0.0f;
	};

	struct Cell
	{
		Int fuel = 0;
		Int rate = 0;
		Int maxRate = 0;
		Int decay = 0;
		Int resistance = 0;
		Bool recovering = FALSE;
	};

	static constexpr Int CELL_SIZE = 10;

	static void loadSettings(Xfer* xfer);
	static void restoreSettings();
	static void parseLogicSettings(INI* ini);
	static void parseVisualSettings(INI* ini);
	static AsciiString fireSystem();
	static AsciiString smokeSystem();

	void initialize(Int width, Int height, std::vector<UnsignedByte> types);
	void reset();
	void clearState();
	void apply(TerrainLogic& terrain, const Coord3D& impact, const Coord3D* facing, const Edit& edit);
	void update();
	void xfer(Xfer* xfer);

	Int cellIndex(Int x, Int y) const { return x * m_height + y; }
	Coord2D center(Int index) const;
	const std::set<Int>& burning() const { return m_burning; }
	Cell cellAt(Real x, Real y) const;

private:
	static void parseCellType(INI* ini, void*, void*, const void*);
	Bool inBounds(Int x, Int y) const;
	Cell cell(Int index) const;
	void setCell(Int index, const Cell& value);
	void applyDamage() const;
	void spread(Int begin, Int end);

	Int m_width = 0;
	Int m_height = 0;
	Int m_slice = 0;
	std::vector<UnsignedByte> m_types;
	std::map<Int, Cell> m_modified;
	std::set<Int> m_burning;
};
