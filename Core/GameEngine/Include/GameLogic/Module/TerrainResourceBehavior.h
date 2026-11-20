#pragma once

#include "Common/SpecialPowerINI.h"
#include "GameLogic/Module/UpdateModule.h"

class TerrainResourceBehaviorModuleData : public UpdateModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& p);

	Real m_radius = 0.0f;
	Int m_maxIncome = 0;
	UnsignedInt m_interval = UPDATE_SLEEP_FOREVER;
	Bool m_highPriority = FALSE;
	Bool m_visible = TRUE;
	AsciiString m_upgrade;
	Real m_upgradeScale = 1.0f;
	SpecialPowerObjectFilter m_upgradeFilter;
};

class TerrainResourceBehavior final : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(TerrainResourceBehavior, "TerrainResourceBehavior")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(TerrainResourceBehavior, TerrainResourceBehaviorModuleData)

public:
	TerrainResourceBehavior(Thing* thing, const ModuleData* data);
	UpdateSleepTime update() override;

private:
	UnsignedInt calculateIncome(const Player& player) const;

	UnsignedInt m_claimFrame = 0;
	Bool m_claiming = FALSE;
};
