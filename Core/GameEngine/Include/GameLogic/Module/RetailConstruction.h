#pragma once

#include <vector>

#include "GameLogic/Module/UpdateModule.h"
#include "GameLogic/Module/UpgradeModule.h"

struct BuildingBehaviorModuleData : public UpdateModuleData
{
	std::vector<AsciiString> m_nightWindows;
	AsciiString m_fireWindow;
	AsciiString m_glowWindow;
	AsciiString m_fire;
	static void buildFieldParse(MultiIniFieldParse& p);
};

class BuildingBehavior final : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(BuildingBehavior, "BuildingBehavior")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(BuildingBehavior, BuildingBehaviorModuleData)

public:
	BuildingBehavior(Thing* thing, const ModuleData* data) : UpdateModule(thing, data) {}
	UpdateSleepTime update() override;
	Int getWindowState() const { return m_windowState; }
	std::size_t getNightWindowCount() const { return getBuildingBehaviorModuleData()->m_nightWindows.size(); }

private:
	Int m_windowState = -1;
};

struct BaseUpgradeModuleData : public UpgradeModuleData
{
	AsciiString m_building;
	AsciiString m_prefix;
	Int m_index = 0;
	static void buildFieldParse(MultiIniFieldParse& p);
};

class BaseUpgrade final : public UpgradeModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(BaseUpgrade, "BaseUpgrade")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(BaseUpgrade, BaseUpgradeModuleData)

public:
	BaseUpgrade(Thing* thing, const ModuleData* data) : UpgradeModule(thing, data) {}
	Bool isSubObjectsUpgrade() override { return FALSE; }

private:
	void upgradeImplementation() override;
};
