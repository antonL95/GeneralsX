#include "PreRTS.h"

#include <algorithm>

#include "Common/ThingTemplate.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/Object.h"
#include "GameLogic/Module/GeometryUpgrade.h"

namespace
{
Bool containsGeometry(const std::vector<AsciiString>& list, const AsciiString& name)
{
	return name.isNotEmpty() && std::any_of(list.begin(), list.end(), [&](const AsciiString& entry)
	{
		return entry.compareNoCase(name) == 0;
	});
}

struct GeometryBounds
{
	Real major = 0.0f;
	Real minor = 0.0f;
	Real height = 0.0f;

	void include(const GeometryInfo& shape, const Coord3D& offset)
	{
		major = std::max(major, std::max(offset.x, -offset.x) + shape.getMajorRadius());
		minor = std::max(minor, std::max(offset.y, -offset.y) + shape.getMinorRadius());
		height = std::max(height, offset.z + shape.getMaxHeightAbovePosition());
	}
};
}

void GeometryUpgradeModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpgradeModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "ShowGeometry", INI::parseAsciiStringVector, nullptr, offsetof(GeometryUpgradeModuleData, m_showGeometry) },
		{ "HideGeometry", INI::parseAsciiStringVector, nullptr, offsetof(GeometryUpgradeModuleData, m_hideGeometry) },
		{ "WallBoundsMesh", INI::parseAsciiString, nullptr, offsetof(GeometryUpgradeModuleData, m_wallBoundsMesh) },
		{ "RampMesh1", INI::parseAsciiString, nullptr, offsetof(GeometryUpgradeModuleData, m_rampMesh1) },
		{ "RampMesh2", INI::parseAsciiString, nullptr, offsetof(GeometryUpgradeModuleData, m_rampMesh2) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

GeometryUpgrade::~GeometryUpgrade() = default;

void GeometryUpgrade::upgradeImplementation()
{
	Object* object = getObject();
	const ThingTemplate* definition = object->getTemplate();
	const auto& retail = definition->getRotWKObjectData();
	const auto* data = getGeometryUpgradeModuleData();
	const NameKeyType key = getModuleNameKey();
	std::vector<const GeometryUpgradeModuleData*> activeUpgrades;
	for (BehaviorModule** module = object->getBehaviorModules(); *module; ++module)
	{
		if ((*module)->getModuleNameKey() != key)
			continue;
		const auto* upgrade = static_cast<const GeometryUpgrade*>(*module);
		if (upgrade == this || upgrade->isAlreadyUpgraded())
			activeUpgrades.push_back(upgrade->getGeometryUpgradeModuleData());
	}

	Bool affectsGeometry = FALSE;
	GeometryBounds bounds;
	const auto addGeometry = [&](const GeometryInfo& shape, const ThingTemplate::GeometryDetails& details)
	{
		Bool visible = details.active;
		for (const auto* upgrade : activeUpgrades)
		{
			if (containsGeometry(upgrade->m_showGeometry, details.name))
				visible = TRUE;
			if (containsGeometry(upgrade->m_hideGeometry, details.name))
				visible = FALSE;
		}
		if (containsGeometry(data->m_showGeometry, details.name) || containsGeometry(data->m_hideGeometry, details.name))
			affectsGeometry = TRUE;
		if (visible)
			bounds.include(shape, details.offset);
	};
	const GeometryInfo& primary = definition->getTemplateGeometryInfo();
	addGeometry(primary, retail.primaryGeometry);
	for (const auto& geometry : retail.additionalGeometries)
		addGeometry(geometry.shape, geometry.details);
	if (!affectsGeometry)
		return;

	const GeometryInfo& current = object->getGeometryInfo();
	if (bounds.major == current.getMajorRadius() && bounds.minor == current.getMinorRadius() &&
		current.getGeomType() != GEOMETRY_SPHERE)
	{
		object->setGeometryInfoZ(bounds.height);
		return;
	}

	Pathfinder* pathfinder = TheAI ? TheAI->pathfinder() : nullptr;
	if (pathfinder)
		pathfinder->removeObjectFromPathfindMap(object);
	object->setGeometryInfo(GeometryInfo(GEOMETRY_BOX, current.getIsSmall(), bounds.height, bounds.major, bounds.minor));
	if (pathfinder)
		pathfinder->addObjectToPathfindMap(object);
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
