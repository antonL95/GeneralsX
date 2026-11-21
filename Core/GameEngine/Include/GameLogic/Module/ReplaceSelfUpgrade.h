#pragma once

#include "GameLogic/Module/UpgradeModule.h"

class ReplaceSelfUpgradeModuleData : public UpgradeModuleData
{
public:
	std::vector<AsciiString> m_replacements;

	static void buildFieldParse(MultiIniFieldParse& fields);
};

class ReplaceSelfUpgrade final : public UpgradeModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(ReplaceSelfUpgrade, "ReplaceSelfUpgrade")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(ReplaceSelfUpgrade, ReplaceSelfUpgradeModuleData)

public:
	ReplaceSelfUpgrade(Thing* thing, const ModuleData* data) : UpgradeModule(thing, data) {}
	Bool isSubObjectsUpgrade() override { return FALSE; }
	Bool wouldUpgrade(const UpgradeMaskType& mask) const override;

protected:
	void upgradeImplementation() override;

private:
	struct Site
	{
		const ThingTemplate* type;
		Coord3D position;
	};

	Bool findSites(std::vector<Site>& sites) const;
};
