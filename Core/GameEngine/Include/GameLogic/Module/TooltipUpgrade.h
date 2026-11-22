#pragma once

#include "GameLogic/Module/UpgradeModule.h"

class TooltipUpgradeModuleData : public UpgradeModuleData
{
public:
	AsciiString displayName;
	AsciiString description;

	static void buildFieldParse(MultiIniFieldParse& p);
};

class TooltipUpgrade final : public UpgradeModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(TooltipUpgrade, "TooltipUpgrade")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(TooltipUpgrade, TooltipUpgradeModuleData)

public:
	TooltipUpgrade(Thing* thing, const ModuleData* data) : UpgradeModule(thing, data) {}
	Bool isSubObjectsUpgrade() override { return FALSE; }

	static const TooltipUpgradeModuleData* activeLabels(const Object& object);

protected:
	void upgradeImplementation() override { setUpgradeExecuted(TRUE); }
};
