#pragma once

#include "GameLogic/Module/WorkerAIUpdate.h"

class HordeWorkerAIUpdateModuleData final : public WorkerAIUpdateModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& p);

	AsciiString m_comboLocomotorSet;
	Int m_comboLocoAttackDistance = 0;
};

class HordeWorkerAIUpdate final : public WorkerAIUpdate
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(HordeWorkerAIUpdate, "HordeWorkerAIUpdate")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(HordeWorkerAIUpdate, HordeWorkerAIUpdateModuleData)

public:
	HordeWorkerAIUpdate(Thing* thing, const ModuleData* data);
};
