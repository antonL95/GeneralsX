#pragma once

#include "GameLogic/Module/UpdateModule.h"

struct AISpecialPowerUpdateModuleData : public UpdateModuleData
{
	AsciiString m_commandButtonName;
	Int m_specialPowerAIType = -1;
	Real m_specialPowerRadius = 0;
	Real m_specialPowerRange = 0;
	Bool m_randomizeTargetLocation = FALSE;
	Bool m_spellMakesAStructure = FALSE;

	static void buildFieldParse(MultiIniFieldParse& p);
};

class AISpecialPowerUpdate final : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( AISpecialPowerUpdate, "AISpecialPowerUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( AISpecialPowerUpdate, AISpecialPowerUpdateModuleData )

public:
	AISpecialPowerUpdate(Thing* thing, const ModuleData* data);
	UpdateSleepTime update() override;
};
