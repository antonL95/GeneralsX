#pragma once

#include "GameLogic/Module/UpgradeModule.h"

class RemoveUpgradeUpgradeModuleData : public UpgradeModuleData
{
public:
	std::vector<AsciiString> m_upgrades;
	std::vector<AsciiString> m_groups;
	Bool m_suppressEva = FALSE;
	Bool m_removeFromAllObjects = FALSE;

	static void buildFieldParse(MultiIniFieldParse& p);
};

class RemoveUpgradeUpgrade : public UpgradeModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(RemoveUpgradeUpgrade, "RemoveUpgradeUpgrade")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(RemoveUpgradeUpgrade, RemoveUpgradeUpgradeModuleData);

public:
	RemoveUpgradeUpgrade(Thing* thing, const ModuleData* moduleData);

private:
	void upgradeImplementation() override;
	Bool isSubObjectsUpgrade() override { return FALSE; }
};
