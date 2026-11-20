#include "PreRTS.h"

#include "GameLogic/Module/TerrainResourceBehavior.h"

#include "Common/BuildAssistant.h"
#include "Common/GlobalData.h"
#include "Common/INI.h"
#include "Common/Money.h"
#include "Common/Player.h"
#include "Common/PlayerTemplate.h"
#include "Common/RetailFileLayers.h"
#include "Common/ScoreKeeper.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "Common/Xfer.h"
#include "GameClient/InGameUI.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/TerrainResourceRules.h"
#include "WWMath/wwmath.h"

#include <cmath>
#include <limits>

namespace
{
struct Claim
{
	Int x;
	Int y;
	Int radius;
	TerrainResourceRules::ClaimOrder order;
	std::vector<Int> rowWidths;
};

Claim makeClaim(const Coord3D& pos, Real radius, TerrainResourceRules::ClaimOrder order,
	const Region3D& bounds, Real cellSize)
{
	const Int cells = static_cast<Int>(WWMath::Ceil(std::clamp(radius / cellSize, 0.0f, 32767.0f)));
	return {
		static_cast<Int>(WWMath::Floor((pos.x - bounds.lo.x) / cellSize + 0.5f)),
		static_cast<Int>(WWMath::Floor((pos.y - bounds.lo.y) / cellSize + 0.5f)),
		cells,
		order,
		TerrainResourceRules::rowHalfWidths(cells)
	};
}

Bool isComplete(const Object& object)
{
	return !object.isDestroyed() && !object.isEffectivelyDead() &&
		object.getConstructionPercent() == CONSTRUCTION_COMPLETE;
}

Bool isClaimableTerrain(const Coord3D& point)
{
	const PathfindCell* cell = TheAI->pathfinder()->getCell(LAYER_GROUND, &point);
	if (!cell)
		return FALSE;
	const auto type = cell->getType();
	return type != PathfindCell::CELL_WATER && type != PathfindCell::CELL_CLIFF &&
		type != PathfindCell::CELL_BRIDGE_IMPASSABLE &&
		(type != PathfindCell::CELL_IMPASSABLE || cell->getObstacleID() != INVALID_ID);
}
}

void TerrainResourceBehaviorModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpdateModuleData::buildFieldParse(p);
	static const FieldParse fields[] = {
		{ "Radius", INI::parseReal, nullptr, offsetof(TerrainResourceBehaviorModuleData, m_radius) },
		{ "MaxIncome", INI::parseInt, nullptr, offsetof(TerrainResourceBehaviorModuleData, m_maxIncome) },
		{ "IncomeInterval", INI::parseDurationUnsignedInt, nullptr, offsetof(TerrainResourceBehaviorModuleData, m_interval) },
		{ "HighPriority", INI::parseBool, nullptr, offsetof(TerrainResourceBehaviorModuleData, m_highPriority) },
		{ "Visible", INI::parseBool, nullptr, offsetof(TerrainResourceBehaviorModuleData, m_visible) },
		{ "Upgrade", INI::parseAsciiString, nullptr, offsetof(TerrainResourceBehaviorModuleData, m_upgrade) },
		{ "UpgradeBonusPercent", INI::parsePercentToReal, nullptr, offsetof(TerrainResourceBehaviorModuleData, m_upgradeScale) },
		{ "UpgradeMustBePresent", SpecialPowerObjectFilter::parse, nullptr, offsetof(TerrainResourceBehaviorModuleData, m_upgradeFilter) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields, 0);
}

TerrainResourceBehavior::TerrainResourceBehavior(Thing* thing, const ModuleData* data) : UpdateModule(thing, data)
{
	setWakeFrame(getObject(), UPDATE_SLEEP_NONE);
}

TerrainResourceBehavior::~TerrainResourceBehavior() = default;

UpdateSleepTime TerrainResourceBehavior::update()
{
	if (!RetailFileLayers::enabled())
		return UPDATE_SLEEP_FOREVER;
	Object* object = getObject();
	if (!isComplete(*object))
		return object->isEffectivelyDead() || object->isDestroyed() ? UPDATE_SLEEP_FOREVER : UPDATE_SLEEP_NONE;
	if (!m_claiming)
	{
		m_claimFrame = TheGameLogic->getFrame();
		m_claiming = TRUE;
	}

	const auto* data = getTerrainResourceBehaviorModuleData();
	if (data->m_maxIncome <= 0)
		return UPDATE_SLEEP_FOREVER;
	Player* player = object->getControllingPlayer();
	if (player && player->getPlayerTemplate() && !object->isNeutralControlled())
	{
		const auto claim = calculateClaim(*data, *object->getTemplate(), *object->getPosition(), *player,
			object, m_claimFrame, static_cast<UnsignedInt>(object->getID()));
		const UnsignedInt amount = TerrainResourceRules::income(data->m_maxIncome, claim.available,
			claim.total, claim.upgradeScale, 1.0f, claim.playerScale);
		if (amount)
		{
			player->getMoney()->deposit(amount);
			player->getScoreKeeper()->addMoneyEarned(amount);
			if (TheInGameUI && object->isLogicallyVisible())
			{
				Coord3D pos = *object->getPosition();
				pos.z += object->getGeometryInfo().getMaxHeightAbovePosition();
				UnicodeString text;
				text.format(L"+%u", amount);
				TheInGameUI->addFloatingText(text, &pos, player->getPlayerColor() | GameMakeColor(0, 0, 0, 230));
			}
		}
	}
	return static_cast<UpdateSleepTime>(std::clamp<UnsignedInt>(data->m_interval, 1, UPDATE_SLEEP_FOREVER));
}

Bool TerrainResourceBehavior::getDisplayInfo(const ThingTemplate& type, const Coord3D& position,
	const Player& player, const Object* object, Real& radius, Real& efficiency)
{
	if (!player.getPlayerTemplate() || (object && (object->isDestroyed() || object->isEffectivelyDead())))
		return FALSE;

	const TerrainResourceBehaviorModuleData* data = nullptr;
	UnsignedInt frame = std::numeric_limits<UnsignedInt>::max();
	UnsignedInt id = std::numeric_limits<UnsignedInt>::max();
	const NameKeyType moduleKey = NAMEKEY("TerrainResourceBehavior");
	if (object)
	{
		for (BehaviorModule** module = object->getBehaviorModules(); module && *module; ++module)
		{
			if ((*module)->getModuleNameKey() != moduleKey)
				continue;
			const auto* resource = static_cast<const TerrainResourceBehavior*>(*module);
			data = resource->getTerrainResourceBehaviorModuleData();
			if (resource->m_claiming)
			{
				frame = resource->m_claimFrame;
				id = static_cast<UnsignedInt>(object->getID());
			}
			break;
		}
	}
	else
	{
		const ModuleInfo& modules = type.getBehaviorModuleInfo();
		for (Int i = 0; i < modules.getCount(); ++i)
		{
			if (modules.getNthName(i).compareNoCase("TerrainResourceBehavior") == 0)
			{
				data = static_cast<const TerrainResourceBehaviorModuleData*>(modules.getNthData(i));
				break;
			}
		}
	}
	if (!data || !data->m_visible || data->m_maxIncome <= 0 || !(data->m_radius > 0.0f))
		return FALSE;

	const auto claim = calculateClaim(*data, type, position, player, object, frame, id);
	if (claim.total == 0)
		return FALSE;
	radius = data->m_radius;
	efficiency = static_cast<Real>(claim.available) / static_cast<Real>(claim.total) *
		claim.upgradeScale * claim.playerScale;
	return std::isfinite(efficiency);
}

TerrainResourceBehavior::ClaimYield TerrainResourceBehavior::calculateClaim(
	const TerrainResourceBehaviorModuleData& data, const ThingTemplate& type, const Coord3D& position,
	const Player& player, const Object* object, UnsignedInt frame, UnsignedInt id)
{
	const Real cellSize = TheGlobalData->m_terrainResourceCellSize;
	if (!(cellSize >= 0.001f && std::isfinite(cellSize)) || !std::isfinite(data.m_radius))
		return {};

	Region3D bounds;
	TheTerrainLogic->getExtent(&bounds);
	const Claim claim = makeClaim(position, data.m_radius,
		{data.m_highPriority != FALSE, frame, id, player.getPlayerIndex()}, bounds, cellSize);
	const PlayerTemplate* playerTemplate = player.getPlayerTemplate();
	const auto& modifiers = playerTemplate->getResourceModifierValues();
	const auto& modifierFilter = playerTemplate->getResourceModifierObjectFilter();
	const Bool useModifier = !modifiers.empty() && modifierFilter.matchesTemplate(type);
	const UpgradeTemplate* upgrade = data.m_upgrade.isEmpty() ? nullptr : TheUpgradeCenter->findUpgrade(data.m_upgrade);
	const Bool hasUpgrade = upgrade && player.hasUpgradeComplete(upgrade);
	Bool hasUpgradeBuilding = FALSE;
	std::size_t buildingCount = useModifier && (!object || !isComplete(*object)) ? 1 : 0;
	std::vector<Claim> blockers;
	const NameKeyType moduleKey = NAMEKEY("TerrainResourceBehavior");
	for (Object* other = TheGameLogic->getFirstObject(); other; other = other->getNextObject())
	{
		if (!isComplete(*other) || !other->getControllingPlayer())
			continue;
		if (other->getControllingPlayer() == &player)
		{
			if (useModifier && modifierFilter.matchesTemplate(*other->getTemplate()))
				++buildingCount;
			if (hasUpgrade && data.m_upgradeFilter.matchesTemplate(*other->getTemplate()))
				hasUpgradeBuilding = TRUE;
		}
		if (other == object)
			continue;
		for (BehaviorModule** module = other->getBehaviorModules(); module && *module; ++module)
		{
			if ((*module)->getModuleNameKey() != moduleKey)
				continue;
			const auto* resource = static_cast<const TerrainResourceBehavior*>(*module);
			const auto* otherData = resource->getTerrainResourceBehaviorModuleData();
			if (!resource->m_claiming || !std::isfinite(otherData->m_radius))
				continue;
			const TerrainResourceRules::ClaimOrder order{otherData->m_highPriority != FALSE, resource->m_claimFrame,
				static_cast<UnsignedInt>(other->getID()), other->getControllingPlayer()->getPlayerIndex()};
			if (!TerrainResourceRules::blocks(claim.order, order))
				continue;
			const Coord3D& pos = *other->getPosition();
			const Real reach = std::max(0.0f, data.m_radius) + std::max(0.0f, otherData->m_radius) + 3.0f * cellSize;
			if (std::abs(pos.x - position.x) <= reach && std::abs(pos.y - position.y) <= reach)
				blockers.push_back(makeClaim(pos, otherData->m_radius, order, bounds, cellSize));
		}
	}

	std::uint64_t total = 0;
	std::uint64_t available = 0;
	const Int columns = static_cast<Int>(WWMath::Ceil((bounds.hi.x - bounds.lo.x) / cellSize));
	const Int rows = static_cast<Int>(WWMath::Ceil((bounds.hi.y - bounds.lo.y) / cellSize));
	for (Int offset = -claim.radius; offset <= claim.radius; ++offset)
	{
		const Int width = claim.rowWidths[std::abs(offset)];
		total += 2 * width + 1;
		const Int y = claim.y + offset;
		if (y < 0 || y >= rows)
			continue;
		for (Int x = std::max(0, claim.x - width); x <= std::min(columns - 1, claim.x + width); ++x)
		{
			const auto blocked = [x, y](const Claim& other)
			{
				const Int dy = std::abs(y - other.y);
				return dy <= other.radius && std::abs(x - other.x) <= other.rowWidths[dy];
			};
			if (std::any_of(blockers.begin(), blockers.end(), blocked))
				continue;
			const Coord3D point = {bounds.lo.x + (x + 0.5f) * cellSize, bounds.lo.y + (y + 0.5f) * cellSize, 0.0f};
			if (isClaimableTerrain(point))
				++available;
		}
	}
	return {available, total, hasUpgradeBuilding ? data.m_upgradeScale : 1.0f,
		useModifier ? TerrainResourceRules::incomeScale(modifiers, buildingCount) : 1.0f};
}

void TerrainResourceBehavior::crc(Xfer* xfer)
{
	UpdateModule::crc(xfer);
	xfer->xferUnsignedInt(&m_claimFrame);
	xfer->xferBool(&m_claiming);
}

void TerrainResourceBehavior::xfer(Xfer* xfer)
{
	XferVersion version = 1;
	xfer->xferVersion(&version, 1);
	UpdateModule::xfer(xfer);
	xfer->xferUnsignedInt(&m_claimFrame);
	xfer->xferBool(&m_claiming);
}

void TerrainResourceBehavior::loadPostProcess()
{
	UpdateModule::loadPostProcess();
}
