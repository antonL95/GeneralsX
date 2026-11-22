#pragma once

#include "GameClient/ControlBar.h"
#include "GameLogic/Module/UpdateModule.h"
#include "GameLogic/Module/SpecialAbilityUpdate.h"

class ToggleDeploySpecialAbilityUpdateModuleData : public SpecialAbilityUpdateModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& fields);

	AsciiString m_soundDeploy;
	AsciiString m_soundUndeploy;
};

class ToggleDeploySpecialAbilityUpdate final : public SpecialAbilityUpdate
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(ToggleDeploySpecialAbilityUpdate, "ToggleDeploySpecialAbilityUpdate")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(ToggleDeploySpecialAbilityUpdate, ToggleDeploySpecialAbilityUpdateModuleData)

public:
	ToggleDeploySpecialAbilityUpdate(Thing* thing, const ModuleData* data);
	Bool initiateIntentToDoSpecialPower(const SpecialPowerTemplate* power, const Object* target,
		const Coord3D* location, const Waypoint* way, UnsignedInt options) override;
};
