#include "PreRTS.h"
#include "GameLogic/RetailTerrainFire.h"
#include "GameLogic/RetailTerrainRaster.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "Common/GameCommon.h"
#include "Common/GameState.h"
#include "Common/INI.h"
#include "Common/RetailFileLayers.h"
#include "Common/RetailINIStartup.h"
#include "Common/RotwkDuration.h"
#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"
#include "GameClient/ClientRandomValue.h"
#include "GameLogic/Damage.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/GeometryUpgrade.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"
#include "WWMath/wwmath.h"

namespace
{
constexpr Int SLICE_COUNT = 10;
constexpr UnsignedInt UPDATE_INTERVAL = RotwkDuration::engineFrames(1, LOGICFRAMES_PER_SECOND);
constexpr Int MAX_FUEL = 65535;
constexpr Int MAX_DECAY = 1023;
constexpr Int MAX_BURN_RATE = 4095;
constexpr Int MAX_RESISTANCE = 255;

struct CellType
{
	RGBColor color{};
	AsciiString name;
	Int fuel = 0;
	Int maxRate = 0;
	Int decay = 0;
	Int resistance = 0;
};

struct LogicSettings
{
	UnsignedInt maxBurning = 0x7fffffff;
	std::array<CellType, 4> types;
} logicSettings, defaultLogicSettings;

struct VisualSettings
{
	AsciiString fire;
	AsciiString smoke;
	Bool enableScorches = FALSE;
	GameClientRandomVariable scorchFrequency;
	GameClientRandomVariable scorchSize;
	Real scorchIntensity = 0.0f;
} visualSettings, defaultVisualSettings;

Int gridCoordinate(Real position, Real offset)
{
	return static_cast<Int>(WWMath::Floor(position * (1.0f / RetailTerrainFire::CELL_SIZE) + offset));
}

Int limitedAmount(Real amount, Int lower, Int upper)
{
	return std::isfinite(amount) ? static_cast<Int>(std::clamp(amount, static_cast<Real>(lower), static_cast<Real>(upper))) : 0;
}

RetailTerrainFire::Cell defaultCell(UnsignedByte type)
{
	if (type == 0 || type >= logicSettings.types.size())
		return {};
	const CellType& data = logicSettings.types[type];
	return {std::clamp(data.fuel, 0, MAX_FUEL), 0, std::clamp(data.maxRate, 0, MAX_BURN_RATE),
		std::clamp(data.decay, 0, MAX_DECAY), std::clamp(data.resistance, 0, MAX_RESISTANCE), FALSE};
}
}

void RetailTerrainFire::loadSettings(Xfer* xfer)
{
	if (!RetailFileLayers::enabled())
		return;

	loadVisualSettings(xfer);
	loadLogicSettings(xfer);
}

void RetailTerrainFire::loadVisualSettings(Xfer* xfer)
{
	logicSettings = {};
	visualSettings = {};
	RetailINIStartup::loadFile("Data\\INI\\Default\\Fire.ini", xfer);
	RetailINIStartup::loadFile("Data\\INI\\Fire.ini", xfer);
}

void RetailTerrainFire::loadLogicSettings(Xfer* xfer)
{
	if (!RetailINIStartup::loadSubsystem("TheFireLogicSystem", xfer))
	{
		RetailINIStartup::loadFile("Data\\INI\\Default\\FireLogicSystem.ini", xfer);
		RetailINIStartup::loadFile("Data\\INI\\FireLogicSystem.ini", xfer);
	}
	defaultLogicSettings = logicSettings;
	defaultVisualSettings = visualSettings;
}

void RetailTerrainFire::restoreSettings()
{
	if (RetailFileLayers::enabled())
	{
		logicSettings = defaultLogicSettings;
		visualSettings = defaultVisualSettings;
	}
}

void RetailTerrainFire::parseLogicSettings(INI* ini)
{
	static const FieldParse fields[] = {
		{ "MaxCellsBurning", INI::parseUnsignedInt, nullptr, offsetof(LogicSettings, maxBurning) },
		{ "TerrainCellType", parseCellType, nullptr, 0 },
		{ nullptr, nullptr, nullptr, 0 }
	};
	ini->initFromINI(&logicSettings, fields);
}

void RetailTerrainFire::parseCellType(INI* ini, void*, void*, const void*)
{
	static const FieldParse fields[] = {
		{ "Color", INI::parseRGBColor, nullptr, offsetof(CellType, color) },
		{ "Name", INI::parseQuotedAsciiString, nullptr, offsetof(CellType, name) },
		{ "Fuel", INI::parseInt, nullptr, offsetof(CellType, fuel) },
		{ "MaxBurnRate", INI::parseInt, nullptr, offsetof(CellType, maxRate) },
		{ "Decay", INI::parseInt, nullptr, offsetof(CellType, decay) },
		{ "Resistance", INI::parseInt, nullptr, offsetof(CellType, resistance) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	const UnsignedInt index = INI::scanUnsignedInt(ini->getNextToken());
	CellType ignored;
	ini->initFromINI(index < logicSettings.types.size() ? &logicSettings.types[index] : &ignored, fields);
}

void RetailTerrainFire::parseVisualSettings(INI* ini)
{
	static const FieldParse fields[] = {
		{ "TerrainFireSystem", INI::parseAsciiString, nullptr, offsetof(VisualSettings, fire) },
		{ "TerrainSmokeSystem", INI::parseAsciiString, nullptr, offsetof(VisualSettings, smoke) },
		{ "EnableScorches", INI::parseBool, nullptr, offsetof(VisualSettings, enableScorches) },
		{ "ScorchFrequency", INI::parseGameClientRandomVariable, nullptr, offsetof(VisualSettings, scorchFrequency) },
		{ "ScorchSize", INI::parseGameClientRandomVariable, nullptr, offsetof(VisualSettings, scorchSize) },
		{ "ScorchIntensity", INI::parseReal, nullptr, offsetof(VisualSettings, scorchIntensity) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	ini->initFromINI(&visualSettings, fields);
}

AsciiString RetailTerrainFire::fireSystem()
{
	return visualSettings.fire;
}

AsciiString RetailTerrainFire::smokeSystem()
{
	return visualSettings.smoke;
}

void RetailTerrainFire::initialize(Int width, Int height, std::vector<UnsignedByte> types)
{
	clearState();
	m_width = width;
	m_height = height;
	m_types = std::move(types);
}

void RetailTerrainFire::reset()
{
	clearState();
	m_types.clear();
	m_width = 0;
	m_height = 0;
}

void RetailTerrainFire::clearState()
{
	m_slice = 0;
	m_legacyLoadPending = FALSE;
	m_modified.clear();
	m_burning.clear();
}

void RetailTerrainFire::prepareLegacyLoad()
{
	clearState();
	m_legacyLoadPending = TRUE;
}

void RetailTerrainFire::restoreLegacySlice(UnsignedInt frame)
{
	if (m_legacyLoadPending && !m_types.empty())
	{
		const UnsignedInt elapsedTicks = frame / UPDATE_INTERVAL + (frame % UPDATE_INTERVAL != 0);
		m_slice = elapsedTicks % SLICE_COUNT;
	}
	m_legacyLoadPending = FALSE;
}

Coord2D RetailTerrainFire::center(Int index) const
{
	return {static_cast<Real>((index / m_height) * CELL_SIZE + CELL_SIZE / 2),
		static_cast<Real>((index % m_height) * CELL_SIZE + CELL_SIZE / 2)};
}

Bool RetailTerrainFire::inBounds(Int x, Int y) const
{
	return x >= 0 && x < m_width && y >= 0 && y < m_height;
}

RetailTerrainFire::Cell RetailTerrainFire::cell(Int index) const
{
	const auto found = m_modified.find(index);
	return found != m_modified.end() ? found->second : defaultCell(m_types[index]);
}

RetailTerrainFire::Cell RetailTerrainFire::cellAt(Real x, Real y) const
{
	const Int ix = gridCoordinate(x, 0.0f);
	const Int iy = gridCoordinate(y, 0.0f);
	return inBounds(ix, iy) ? cell(cellIndex(ix, iy)) : Cell{};
}

void RetailTerrainFire::setCell(Int index, const Cell& value)
{
	const Cell base = defaultCell(m_types[index]);
	if (value.rate == 0 && !value.recovering && value.fuel == base.fuel && value.maxRate == base.maxRate &&
		value.decay == base.decay && value.resistance == base.resistance)
		m_modified.erase(index);
	else
		m_modified[index] = value;
}

void RetailTerrainFire::apply(TerrainLogic& terrain, const Coord3D& impact, const Coord3D* facing, const Edit& edit)
{
	const Int amount = limitedAmount(edit.amount, -MAX_FUEL, MAX_FUEL);
	if (m_types.empty() || !(edit.radius > 0.0f) || !std::isfinite(edit.radius) || amount == 0)
		return;

	const Bool fuelEdit = edit.action == INCREASE_FUEL || edit.action == INCREASE_FUEL_ON_EXISTING_FIRE;
	const Bool existingOnly = edit.action == INCREASE_BURN_RATE_ON_EXISTING_FIRE || edit.action == INCREASE_FUEL_ON_EXISTING_FIRE;
	const Int rateChange = edit.action == DECREASE_BURN_RATE ? -amount : amount;
	const Bool directional = edit.action == INCREASE_BURN_RATE && edit.arc > 0.0f && edit.arc < PI;
	const Coord3D normal = facing ? *facing : Coord3D{1.0f, 0.0f, 0.0f};
	const Real threshold = directional && facing ? WWMath::CosTrig(edit.arc) : 1.0f;
	const Int cx = gridCoordinate(impact.x, 0.5f);
	const Int cy = gridCoordinate(impact.y, 0.5f);

	const Int minMaxRate = limitedAmount(edit.minMaxBurnRate, 0, MAX_BURN_RATE);
	const Int minDecay = limitedAmount(edit.minDecay, 0, MAX_DECAY);
	const Int maxResistance = limitedAmount(edit.maxResistance, 0, MAX_RESISTANCE);
	const auto editCell = [&](Int x, Int y)
	{
		const Int index = cellIndex(x, y);
		Cell value = cell(index);
		if ((existingOnly || (!fuelEdit && rateChange < 0)) && value.rate == 0)
			return;

		const Coord2D point = center(index);
		if (terrain.isUnderwater(point.x, point.y))
		{
			if (!fuelEdit)
			{
				value.rate = 0;
				m_burning.erase(index);
				setCell(index, value);
			}
			return;
		}
		if (directional)
		{
			const Real dx = point.x - impact.x;
			const Real dy = point.y - impact.y;
			const Real dot = normal.z * -impact.z + normal.y * dy + normal.x * dx;
			if (dot < threshold)
				return;
		}

		if (fuelEdit)
		{
			value.fuel = std::clamp(value.fuel + amount, 0, MAX_FUEL);
			value.maxRate = std::max(value.maxRate, minMaxRate);
			value.decay = std::max(value.decay, minDecay);
			value.resistance = std::min(value.resistance, maxResistance);
		}
		else
		{
			value.rate = std::clamp(value.rate + rateChange, 0, MAX_FUEL);
			if (value.rate == 0)
				m_burning.erase(index);
		}
		setCell(index, value);
	};

	if (!fuelEdit && rateChange < 0)
	{
		const Real width = static_cast<Real>(std::max(std::abs(cx), std::abs(m_width - 1 - cx)));
		const Real height = static_cast<Real>(std::max(std::abs(cy), std::abs(m_height - 1 - cy)));
		if (edit.radius >= CELL_SIZE * (width + height))
		{
			for (auto it = m_modified.begin(); it != m_modified.end();)
			{
				const Int index = (it++)->first;
				editCell(index / m_height, index % m_height);
			}
			return;
		}
	}

	const auto row = [&](Int y, Int halfWidth)
	{
		if (y < 0 || y >= m_height)
			return;
		for (Int x = std::max(0, cx - halfWidth); x <= std::min(m_width - 1, cx + halfWidth); ++x)
			editCell(x, y);
	};

	const Int coverRadius = std::max(std::abs(cx), std::abs(m_width - 1 - cx)) +
		std::max(std::abs(cy), std::abs(m_height - 1 - cy)) + 1;
	const Int radius = static_cast<Int>(WWMath::Ceil(std::min(edit.radius * (1.0f / CELL_SIZE), static_cast<Real>(coverRadius))));
	RetailTerrain::visitCircleRows(cy, radius, row);
}

void RetailTerrainFire::update(UnsignedInt frame)
{
	if (m_types.empty() || frame % UPDATE_INTERVAL != 0)
		return;
	const Int begin = m_slice * m_width / SLICE_COUNT;
	const Int end = (m_slice + 1) * m_width / SLICE_COUNT;
	m_slice = (m_slice + 1) % SLICE_COUNT;

	for (Int x = begin; x < end; ++x)
	{
		const auto first = m_modified.lower_bound(cellIndex(x, 0));
		auto it = m_modified.lower_bound(cellIndex(x + 1, 0));
		std::vector<Int> changed;
		while (it != first)
			changed.push_back((--it)->first);
		for (Int index : changed)
		{
			Cell value = cell(index);
			if (value.rate == 0)
				continue;
			value.rate = std::max(0, value.rate - value.decay);
			if (value.rate == 0 || (m_burning.count(index) == 0 && m_burning.size() >= logicSettings.maxBurning))
			{
				value.rate = 0;
				m_burning.erase(index);
			}
			else
			{
				const Int consumed = std::min(value.fuel, value.rate);
				value.fuel -= consumed;
				if (value.fuel > 0)
					m_burning.insert(index);
				else
				{
					value.rate = 0;
					value.recovering = TRUE;
					m_burning.erase(index);
				}
			}
			setCell(index, value);
		}
	}

	applyDamage();
	spread(std::max(0, begin - 1), end + 1 < m_width ? end - 1 : end);
}

void RetailTerrainFire::applyDamage() const
{
	if (m_burning.empty())
		return;

	std::map<ObjectID, Int> damage;
	for (Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
	{
		if (object->isEffectivelyDead() || object->isSignificantlyAboveTerrain() || object->getEnclosingContainedBy() ||
			object->isKindOf(KINDOF_PROJECTILE))
			continue;

		const Coord3D& position = *object->getPosition();
		const GeometryInfo& geometry = object->getGeometryInfo();
		std::set<Int> overlap;
		const auto add = [&](Int x, Int y)
		{
			if (inBounds(x, y) && m_burning.count(cellIndex(x, y)))
				overlap.insert(cellIndex(x, y));
		};
		if (object->isKindOf(KINDOF_STRUCTURE) || (!geometry.getIsSmall() && geometry.getBoundingCircleRadius() >= 30.0f))
		{
			const Real angle = object->getOrientation();
			const Real sine = WWMath::SinTrig(angle);
			const Real cosine = WWMath::CosTrig(angle);
			const auto addPart = [&](const GeometryInfo& shape, const Coord3D& origin)
			{
				if (shape.getGeomType() == GEOMETRY_BOX)
				{
					const Real step = CELL_SIZE * 0.5f;
					const Int nx = static_cast<Int>(WWMath::Ceil(shape.getMajorRadius() * 2.0f / step));
					const Int ny = static_cast<Int>(WWMath::Ceil(shape.getMinorRadius() * 2.0f / step));
					for (Int x = 0; x < nx; ++x)
					{
						const Real forward = x * step - shape.getMajorRadius();
						for (Int y = 0; y < ny; ++y)
						{
							const Real side = y * step - shape.getMinorRadius();
							const Real px = origin.x + forward * cosine - side * sine;
							const Real py = origin.y + forward * sine + side * cosine;
							add(gridCoordinate(px + 0.5f, 0.0f), gridCoordinate(py + 0.5f, 0.0f));
						}
					}
					return;
				}
				const Real radius = shape.getMajorRadius() + 4.0f;
				const Int left = std::max(0, gridCoordinate(origin.x - radius, 0.0f));
				const Int right = std::min(m_width - 1, gridCoordinate(origin.x + radius, 0.0f));
				const Int bottom = std::max(0, gridCoordinate(origin.y - radius, 0.0f));
				const Int top = std::min(m_height - 1, gridCoordinate(origin.y + radius, 0.0f));
				for (Int x = left; x <= right; ++x)
					for (Int y = bottom; y <= top; ++y)
					{
						const Coord2D point = center(cellIndex(x, y));
						const Real dx = point.x - origin.x;
						const Real dy = point.y - origin.y;
						if (dx * dx + dy * dy <= radius * radius)
							add(x, y);
					}
			};
			const auto& data = object->getTemplate()->getRotWKObjectData();
			std::vector<GeometryUpgrade::CollisionPart> parts;
			if (GeometryUpgrade::collectCollisionParts(*object, parts))
			{
				for (const auto& part : parts)
					if (part.position.z - position.z <= CELL_SIZE)
						addPart(*part.shape, part.position);
			}
			else
			{
				const auto addOffsetPart = [&](const GeometryInfo& shape, const ThingTemplate::GeometryDetails& details)
				{
					const auto& offset = details.offset;
					if (details.active && offset.z <= CELL_SIZE)
						addPart(shape, {position.x + offset.x * cosine - offset.y * sine,
							position.y + offset.x * sine + offset.y * cosine, position.z + offset.z});
				};
				addOffsetPart(geometry, data.primaryGeometry);
				for (const auto& part : data.additionalGeometries)
					addOffsetPart(part.shape, part.details);
			}
		}
		else
			add(gridCoordinate(position.x, 0.5f), gridCoordinate(position.y, 0.5f));

		Int amount = 0;
		for (Int index : overlap)
			amount += cell(index).rate;
		if (amount > 0)
			damage.emplace(object->getID(), amount);
	}

	for (const auto& [id, amount] : damage)
	{
		Object* object = TheGameLogic->findObjectByID(id);
		if (!object || object->isEffectivelyDead())
			continue;
		DamageInfo info;
		info.in.m_damageType = DAMAGE_LOGICAL_FIRE;
		info.in.m_deathType = DEATH_BURNED;
		info.in.m_sourceID = INVALID_ID;
		info.in.m_amount = static_cast<Real>(amount);
		object->attemptDamage(&info);
	}
}

void RetailTerrainFire::spread(Int begin, Int end)
{
	std::set<Int> candidates;
	const auto add = [&](Int x, Int y)
	{
		if (x >= begin && x < end && inBounds(x, y))
			candidates.insert(cellIndex(x, y));
	};
	for (const auto& [index, source] : m_modified)
	{
		if (source.rate == 0)
			continue;
		const Int x = index / m_height;
		const Int y = index % m_height;
		add(x - 1, y);
		add(x + 1, y);
		add(x, y - 1);
		add(x, y + 1);
	}
	for (auto it = m_modified.lower_bound(cellIndex(begin, 0)), last = m_modified.lower_bound(cellIndex(end, 0)); it != last; ++it)
	{
		if (it->second.recovering)
			candidates.insert(it->first);
	}

	std::vector<std::pair<Int, Cell>> pending;
	UnsignedInt burningCount = static_cast<UnsignedInt>(m_burning.size());
	for (Int index : candidates)
	{
		Cell value = cell(index);
		if (value.fuel == 0)
		{
			if (value.recovering)
			{
				value.fuel = std::min(defaultCell(m_types[index]).fuel, 1);
				value.recovering = value.fuel != defaultCell(m_types[index]).fuel;
				pending.emplace_back(index, value);
			}
			continue;
		}

		const Int x = index / m_height;
		const Int y = index % m_height;
		const auto heat = [&](Int nx, Int ny) {
			return inBounds(nx, ny) ? std::max(0, cell(cellIndex(nx, ny)).rate - value.resistance) : 0;
		};
		const Int incoming = heat(x - 1, y) + heat(x + 1, y) + heat(x, y - 1) + heat(x, y + 1);
		if (incoming == 0)
			continue;
		if (value.rate == 0)
		{
			if (burningCount >= logicSettings.maxBurning)
				continue;
			++burningCount;
		}
		value.rate = std::min(value.rate + incoming, value.maxRate);
		pending.emplace_back(index, value);
	}

	for (const auto& [index, value] : pending)
	{
		if (value.rate > 0)
			m_burning.insert(index);
		else
			m_burning.erase(index);
		setCell(index, value);
	}
}

void RetailTerrainFire::xfer(Xfer* xfer)
{
	const Bool loading = xfer->getXferMode() == XFER_LOAD;
	if (loading)
		clearState();

	xfer->xferInt(&m_slice);
	UnsignedInt count = static_cast<UnsignedInt>(m_modified.size());
	xfer->xferUnsignedInt(&count);
	if (m_slice < 0 || m_slice >= SLICE_COUNT || count > m_types.size())
		throw SC_INVALID_DATA;

	const auto transferCell = [&](Int& index, Cell& value, Bool& burning)
	{
		xfer->xferInt(&index);
		xfer->xferInt(&value.fuel);
		xfer->xferInt(&value.rate);
		xfer->xferInt(&value.maxRate);
		xfer->xferInt(&value.decay);
		xfer->xferInt(&value.resistance);
		xfer->xferBool(&value.recovering);
		xfer->xferBool(&burning);
	};
	if (loading)
	{
		Int previous = -1;
		for (UnsignedInt i = 0; i < count; ++i)
		{
			Int index = 0;
			Cell value;
			Bool burning = FALSE;
			transferCell(index, value, burning);
			if (index <= previous || static_cast<size_t>(index) >= m_types.size() || value.fuel < 0 || value.fuel > MAX_FUEL ||
				value.rate < 0 || value.rate > MAX_FUEL || value.maxRate < 0 || value.maxRate > MAX_BURN_RATE ||
				value.decay < 0 || value.decay > MAX_DECAY || value.resistance < 0 || value.resistance > MAX_RESISTANCE ||
				(burning && (value.rate == 0 || value.fuel == 0)))
				throw SC_INVALID_DATA;
			m_modified.emplace(index, value);
			if (burning)
				m_burning.insert(index);
			previous = index;
		}
	}
	else
	{
		for (const auto& [index, saved] : m_modified)
		{
			Int key = index;
			Cell value = saved;
			Bool burning = m_burning.count(index) != 0;
			transferCell(key, value, burning);
		}
	}
}
