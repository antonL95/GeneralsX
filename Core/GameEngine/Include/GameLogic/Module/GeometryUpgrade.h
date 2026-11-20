#pragma once

#include "GameLogic/Module/UpgradeModule.h"

class GeometryUpgradeModuleData : public UpgradeModuleData
{
public:
	std::vector<AsciiString> m_showGeometry;
	std::vector<AsciiString> m_hideGeometry;
	AsciiString m_wallBoundsMesh;
	AsciiString m_rampMesh1;
	AsciiString m_rampMesh2;

	static void buildFieldParse(MultiIniFieldParse& p);
};

class GeometryUpgrade final : public UpgradeModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(GeometryUpgrade, "GeometryUpgrade")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(GeometryUpgrade, GeometryUpgradeModuleData)

public:
	GeometryUpgrade(Thing* thing, const ModuleData* data) : UpgradeModule(thing, data) {}

	Bool isSubObjectsUpgrade() override { return FALSE; }

protected:
	void upgradeImplementation() override;
};
