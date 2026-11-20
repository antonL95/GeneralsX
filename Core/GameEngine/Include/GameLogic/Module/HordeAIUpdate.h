#pragma once

#include "GameLogic/Module/AIUpdate.h"

class HordeAIUpdateModuleData final : public AIUpdateModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& p);

	AsciiString m_comboLocomotorSet;
	Real m_comboLocoAttackDistance = 0.0f;
};

class HordeAIUpdate final : public AIUpdateInterface
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(HordeAIUpdate, "HordeAIUpdate")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(HordeAIUpdate, HordeAIUpdateModuleData)

public:
	HordeAIUpdate(Thing* thing, const ModuleData* data);
	UpdateSleepTime update() override;
	void aiDoCommand(const AICommandParms* parms) override;

private:
	ObjectID m_attackTarget = INVALID_ID;
	CommandSourceType m_attackSource = CMD_FROM_AI;
	Int m_maxShots = 0;
	Bool m_forceAttack = FALSE;
	Bool m_engaging = FALSE;
};
