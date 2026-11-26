#include "PreRTS.h"

#include <algorithm>

#include "Common/Player.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/Object.h"
#include "GameLogic/Module/GeometryUpgrade.h"
#include "GameLogic/PartitionManager.h"
#include "WWMath/wwmath.h"

namespace
{
Bool containsGeometry(const std::vector<AsciiString>& list, const AsciiString& name)
{
	return name.isNotEmpty() && std::any_of(list.begin(), list.end(), [&](const AsciiString& entry)
	{
		return entry.compareNoCase(name) == 0;
	});
}

Bool affectsGeometry(const GeometryUpgradeModuleData& upgrade, const AsciiString& name)
{
	return containsGeometry(upgrade.m_showGeometry, name) || containsGeometry(upgrade.m_hideGeometry, name);
}

Bool isActive(const ThingTemplate::GeometryDetails& details, const std::vector<const GeometryUpgradeModuleData*>& upgrades)
{
	Bool active = details.active;
	for (const auto* upgrade : upgrades)
	{
		if (containsGeometry(upgrade->m_showGeometry, details.name))
			active = TRUE;
		if (containsGeometry(upgrade->m_hideGeometry, details.name))
			active = FALSE;
	}
	return active;
}

struct GeometryBounds
{
	Real major = 0.0f;
	Real minor = 0.0f;
	Real height = 0.0f;
	Bool isSmall = TRUE;
	Int partCount = 0;

	void include(const GeometryInfo& shape, const Coord3D& offset)
	{
		++partCount;
		isSmall = partCount == 1 && shape.getIsSmall() && offset.x == 0.0f && offset.y == 0.0f;
		const Real halfDepth = shape.getGeomType() == GEOMETRY_BOX ? shape.getMinorRadius() : shape.getMajorRadius();
		major = std::max(major, std::max(offset.x, -offset.x) + shape.getMajorRadius());
		minor = std::max(minor, std::max(offset.y, -offset.y) + halfDepth);
		height = std::max(height, offset.z + shape.getMaxHeightAbovePosition());
	}
};

Bool canOccupyConstructionFootprint(const ThingTemplate& definition, const ThingTemplate::GeometryDetails& details,
	const std::vector<const GeometryUpgradeModuleData*>& activeUpgrades, const UpgradeMaskType* completed)
{
	if (isActive(details, activeUpgrades))
		return TRUE;
	if (details.name.isEmpty())
		return FALSE;

	const ModuleInfo& modules = definition.getBehaviorModuleInfo();
	for (Int i = 0; i < modules.getCount(); ++i)
	{
		if (modules.getNthName(i).compareNoCase("GeometryUpgrade") != 0)
			continue;
		const auto* data = static_cast<const GeometryUpgradeModuleData*>(modules.getNthData(i));
		if (!data || !containsGeometry(data->m_showGeometry, details.name) ||
			std::find(activeUpgrades.begin(), activeUpgrades.end(), data) != activeUpgrades.end())
			continue;
		if (!completed || !completed->any())
			return TRUE;
		UpgradeMaskType activation, conflicting;
		data->m_upgradeMuxData.getUpgradeActivationMasks(activation, conflicting);
		if (!completed->testForAny(conflicting))
			return TRUE;
	}
	return FALSE;
}

template<typename Predicate>
Bool anyConstructionGeometry(const ThingTemplate& definition, const Coord3D& position, Real angle, const Predicate& overlaps,
	const std::vector<const GeometryUpgradeModuleData*>& activeUpgrades = {}, const UpgradeMaskType* completed = nullptr)
{
	const auto& data = definition.getRotWKObjectData();
	const Real cosAngle = WWMath::CosTrig(angle);
	const Real sinAngle = WWMath::SinTrig(angle);
	const auto test = [&](const GeometryInfo& geometry, const ThingTemplate::GeometryDetails& details) -> Bool
	{
		if (!canOccupyConstructionFootprint(definition, details, activeUpgrades, completed))
			return FALSE;
		const Coord3D center = {
			position.x + details.offset.x * cosAngle - details.offset.y * sinAngle,
			position.y + details.offset.x * sinAngle + details.offset.y * cosAngle,
			position.z + details.offset.z
		};
		return overlaps(geometry, center);
	};

	if (test(definition.getTemplateGeometryInfo(), data.primaryGeometry))
		return TRUE;
	return std::any_of(data.additionalGeometries.begin(), data.additionalGeometries.end(),
		[&](const auto& geometry) { return test(geometry.shape, geometry.details); });
}
}

Bool GeometryUpgrade::constructionFootprintsOverlap(const ThingTemplate& build, const Coord3D& position, Real angle,
	const Object& obstacle)
{
	const ThingTemplate& existing = *obstacle.getTemplate();
	const auto& buildData = build.getRotWKObjectData();
	const auto& existingData = existing.getRotWKObjectData();
	const auto hasExtendedFootprint = [](const auto& data)
	{
		return !data.additionalGeometries.empty() || data.primaryGeometry.offset.x != 0.0f ||
			data.primaryGeometry.offset.y != 0.0f || data.primaryGeometry.offset.z != 0.0f;
	};
	if (!hasExtendedFootprint(buildData) && !hasExtendedFootprint(existingData))
		return FALSE;

	auto completed = obstacle.getObjectCompletedUpgradeMask();
	if (const Player* owner = obstacle.getControllingPlayer())
		completed.set(owner->getCompletedUpgradeMask());
	const auto upgrades = activeUpgrades(obstacle, nullptr);
	const Real obstacleAngle = obstacle.getOrientation();
	return anyConstructionGeometry(build, position, angle, [&](const GeometryInfo& shape, const Coord3D& center)
	{
		return anyConstructionGeometry(existing, *obstacle.getPosition(), obstacleAngle,
			[&](const GeometryInfo& obstacleShape, const Coord3D& obstacleCenter)
			{
				return ThePartitionManager->geomCollidesWithGeom(&center, shape, angle, &obstacleCenter, obstacleShape, obstacleAngle);
			}, upgrades, &completed);
	});
}

void GeometryUpgradeModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpgradeModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "ShowGeometry", INI::parseAsciiStringVector, nullptr, offsetof(GeometryUpgradeModuleData, m_showGeometry) },
		{ "HideGeometry", INI::parseAsciiStringVector, nullptr, offsetof(GeometryUpgradeModuleData, m_hideGeometry) },
		{ "CustomAnimAndDuration", SpecialPowerAnimation::parse, nullptr, offsetof(GeometryUpgradeModuleData, m_animation) },
		{ "WallBoundsMesh", INI::parseAsciiString, nullptr, offsetof(GeometryUpgradeModuleData, m_wallBoundsMesh) },
		{ "RampMesh1", INI::parseAsciiString, nullptr, offsetof(GeometryUpgradeModuleData, m_rampMesh1) },
		{ "RampMesh2", INI::parseAsciiString, nullptr, offsetof(GeometryUpgradeModuleData, m_rampMesh2) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

GeometryUpgrade::~GeometryUpgrade() = default;

std::vector<const GeometryUpgradeModuleData*> GeometryUpgrade::activeUpgrades(const Object& object, const GeometryUpgrade* applying)
{
	static const NameKeyType key = NAMEKEY("GeometryUpgrade");
	std::vector<const GeometryUpgradeModuleData*> upgrades;
	for (BehaviorModule** module = object.getBehaviorModules(); *module; ++module)
	{
		if ((*module)->getModuleNameKey() != key)
			continue;
		const auto* upgrade = static_cast<const GeometryUpgrade*>(*module);
		if (upgrade == applying || upgrade->isAlreadyUpgraded())
			upgrades.push_back(upgrade->getGeometryUpgradeModuleData());
	}
	return upgrades;
}

Bool GeometryUpgrade::collectCollisionParts(const Object& object, std::vector<CollisionPart>& parts, Bool includeAuthored)
{
	const ThingTemplate* definition = object.getTemplate();
	const auto& data = definition->getRotWKObjectData();
	const auto& primary = data.primaryGeometry;
	const Bool hasAdditional = !data.additionalGeometries.empty();
	const Bool hasAuthoredParts = includeAuthored && (hasAdditional || !primary.active ||
		primary.offset.x != 0.0f || primary.offset.y != 0.0f || primary.offset.z != 0.0f);
	if (!hasAuthoredParts && !hasAdditional && primary.name.isEmpty())
		return FALSE;

	const auto upgrades = activeUpgrades(object, nullptr);
	const auto hasUpgrade = [&](const ThingTemplate::GeometryDetails& details)
	{
		return std::any_of(upgrades.begin(), upgrades.end(),
			[&](const auto* upgrade) { return affectsGeometry(*upgrade, details.name); });
	};
	const Bool upgraded = !upgrades.empty() && (hasUpgrade(primary) ||
		std::any_of(data.additionalGeometries.begin(), data.additionalGeometries.end(),
			[&](const auto& geometry) { return hasUpgrade(geometry.details); }));
	const GeometryInfo& baseShape = definition->getTemplateGeometryInfo();
	const GeometryInfo& currentShape = object.getGeometryInfo();
	if (!upgraded && (!hasAuthoredParts || currentShape.getGeomType() != baseShape.getGeomType() ||
		currentShape.getMajorRadius() != baseShape.getMajorRadius() ||
		currentShape.getMinorRadius() != baseShape.getMinorRadius()))
		return FALSE;

	const Coord3D& position = *object.getPosition();
	const Real angle = object.getOrientation();
	const Real cosAngle = WWMath::CosTrig(angle);
	const Real sinAngle = WWMath::SinTrig(angle);
	const auto addPart = [&](const GeometryInfo& shape, const ThingTemplate::GeometryDetails& details)
	{
		if (!isActive(details, upgrades))
			return;
		const Coord3D center = {
			position.x + details.offset.x * cosAngle - details.offset.y * sinAngle,
			position.y + details.offset.x * sinAngle + details.offset.y * cosAngle,
			position.z + details.offset.z
		};
		parts.push_back({&shape, center});
	};
	addPart(upgraded ? baseShape : currentShape, primary);
	for (const auto& geometry : data.additionalGeometries)
		addPart(geometry.shape, geometry.details);
	return TRUE;
}

Bool GeometryUpgrade::collectLocalShapes(const Object& object, std::vector<LocalShape>& shapes)
{
	const ThingTemplate* definition = object.getTemplate();
	const auto& data = definition->getRotWKObjectData();
	const auto& primary = data.primaryGeometry;
	const Bool hasAdditional = !data.additionalGeometries.empty();
	const Bool hasOffset = primary.offset.x != 0.0f || primary.offset.y != 0.0f || primary.offset.z != 0.0f;
	const auto upgrades = activeUpgrades(object, nullptr);
	if (!hasAdditional && !hasOffset && primary.active && upgrades.empty())
	{
		shapes.push_back({&object.getGeometryInfo(), {0.0f, 0.0f, 0.0f}});
		return TRUE;
	}

	const auto hasUpgrade = [&](const ThingTemplate::GeometryDetails& details)
	{
		return std::any_of(upgrades.begin(), upgrades.end(),
			[&](const auto* upgrade) { return affectsGeometry(*upgrade, details.name); });
	};
	const Bool upgraded = !upgrades.empty() && (hasUpgrade(primary) ||
		std::any_of(data.additionalGeometries.begin(), data.additionalGeometries.end(),
			[&](const auto& geometry) { return hasUpgrade(geometry.details); }));
	const GeometryInfo& baseShape = definition->getTemplateGeometryInfo();
	const GeometryInfo& currentShape = object.getGeometryInfo();
	if (isActive(primary, upgrades))
		shapes.push_back({upgraded ? &baseShape : &currentShape, primary.offset});
	for (const auto& geometry : data.additionalGeometries)
	{
		if (isActive(geometry.details, upgrades))
			shapes.push_back({&geometry.shape, geometry.details.offset});
	}
	if (shapes.empty())
		shapes.push_back({&currentShape, {0.0f, 0.0f, 0.0f}});
	return TRUE;
}

thread_local const GeometryUpgrade::ScopedPathfindDeferral* GeometryUpgrade::ScopedPathfindDeferral::s_current = nullptr;

GeometryUpgrade::ScopedPathfindDeferral::ScopedPathfindDeferral(const Object* object) :
	m_object(object), m_previous(s_current)
{
	s_current = this;
}

GeometryUpgrade::ScopedPathfindDeferral::~ScopedPathfindDeferral()
{
	s_current = m_previous;
}

Bool GeometryUpgrade::ScopedPathfindDeferral::contains(const Object* object)
{
	for (const auto* scope = s_current; scope; scope = scope->m_previous)
		if (scope->m_object == object)
			return TRUE;
	return FALSE;
}

void GeometryUpgrade::upgradeImplementation()
{
	Object* object = getObject();
	const ThingTemplate* definition = object->getTemplate();
	const auto& retail = definition->getRotWKObjectData();
	const auto* data = getGeometryUpgradeModuleData();
#if !RTS_GENERALS
	object->applyUpgradeAnimation(data->m_animation);
#endif
	const auto upgrades = activeUpgrades(*object, this);

	Bool affectsBounds = FALSE;
	GeometryBounds bounds;
	const auto addGeometry = [&](const GeometryInfo& shape, const ThingTemplate::GeometryDetails& details)
	{
		if (affectsGeometry(*data, details.name))
			affectsBounds = TRUE;
		if (isActive(details, upgrades))
			bounds.include(shape, details.offset);
	};
	const GeometryInfo& primary = definition->getTemplateGeometryInfo();
	addGeometry(primary, retail.primaryGeometry);
	for (const auto& geometry : retail.additionalGeometries)
		addGeometry(geometry.shape, geometry.details);
	const Bool affectsMeshes = data->m_wallBoundsMesh.isNotEmpty() || data->m_rampMesh1.isNotEmpty() || data->m_rampMesh2.isNotEmpty();
	if (!affectsBounds && !affectsMeshes)
		return;

	Pathfinder* pathfinder = TheAI && !ScopedPathfindDeferral::contains(object) ? TheAI->pathfinder() : nullptr;
	if (pathfinder)
		pathfinder->removeObjectFromPathfindMap(object);

	if (affectsBounds)
	{
		const GeometryInfo& current = object->getGeometryInfo();
		if (bounds.major == current.getMajorRadius() && bounds.minor == current.getMinorRadius() &&
			bounds.isSmall == current.getIsSmall() && current.getGeomType() == GEOMETRY_BOX)
			object->setGeometryInfoZ(bounds.height);
		else
			object->setGeometryInfo(GeometryInfo(GEOMETRY_BOX, bounds.isSmall, bounds.height, bounds.major, bounds.minor));
	}

	setUpgradeExecuted(TRUE);
	if (pathfinder)
		pathfinder->addObjectToPathfindMap(object);
}

Bool GeometryUpgrade::resetUpgrade(const UpgradeMaskType& mask)
{
	const auto* data = getGeometryUpgradeModuleData();
	UpgradeMaskType activation, conflicting;
	getUpgradeActivationMasks(activation, conflicting);
	const Bool changesMesh = data->m_wallBoundsMesh.isNotEmpty() || data->m_rampMesh1.isNotEmpty() || data->m_rampMesh2.isNotEmpty();
	Pathfinder* pathfinder = changesMesh && !data->m_upgradeMuxData.m_permanent && isAlreadyUpgraded() &&
		mask.testForAny(activation) && TheAI && !ScopedPathfindDeferral::contains(getObject()) ? TheAI->pathfinder() : nullptr;
	if (pathfinder)
		pathfinder->removeObjectFromPathfindMap(getObject());
	const Bool reset = UpgradeModule::resetUpgrade(mask);
	if (pathfinder)
		pathfinder->addObjectToPathfindMap(getObject());
	return reset;
}

void GeometryUpgrade::crc(Xfer* xfer)
{
	UpgradeModule::crc(xfer);
}

void GeometryUpgrade::xfer(Xfer* xfer)
{
	UpgradeModule::xfer(xfer);
}

void GeometryUpgrade::loadPostProcess()
{
	UpgradeModule::loadPostProcess();
}
