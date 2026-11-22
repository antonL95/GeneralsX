#pragma once

#include "GameLogic/Module/UpdateModule.h"

#include <vector>

class FXList;

class RubbleRiseUpdateModuleData final : public UpdateModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& p);

	Int m_minRubbleRiseDelay = 0;
	Int m_maxRubbleRiseDelay = 0;
	Real m_rubbleRiseDamping = 0.0f;
	Real m_rubbleHeight = 0.0f;
	Real m_maxShudder = 0.0f;
	Int m_minBurstDelay = 0;
	Int m_maxBurstDelay = 0;
	Int m_bigBurstFrequency = 0;
	std::vector<const FXList*> m_fx;
};

class RubbleRiseUpdate final : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(RubbleRiseUpdate, "RubbleRiseUpdate")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(RubbleRiseUpdate, RubbleRiseUpdateModuleData)

public:
	RubbleRiseUpdate(Thing* thing, const ModuleData* data);
	UpdateSleepTime update() override;
};
