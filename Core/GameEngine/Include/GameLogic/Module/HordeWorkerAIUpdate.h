#pragma once

#include "GameLogic/Module/HordeAIUpdate.h"
#if RTS_GENERALS
#include "GameLogic/Module/SpecialPowerModule.h"
#endif

class HordeWorkerAIUpdateModuleData final : public HordeAIUpdateModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& p);

	UnsignedInt m_specialContactPoints = 0;
};

class HordeWorkerAIUpdate final : public HordeAIUpdate
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(HordeWorkerAIUpdate, "HordeWorkerAIUpdate")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(HordeWorkerAIUpdate, HordeWorkerAIUpdateModuleData)

public:
	HordeWorkerAIUpdate(Thing* thing, const ModuleData* data);
	void aiDoCommand(const AICommandParms* parms) override;
	UnsignedInt getSpecialContactPoints() const { return getHordeWorkerAIUpdateModuleData()->m_specialContactPoints; }
	static const HordeWorkerAIUpdate* find(const Object* object);
};

#if RTS_GENERALS
class RepairSpecialPower final : public SpecialPowerModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(RepairSpecialPower, "RepairSpecialPower")
	MAKE_STANDARD_MODULE_MACRO(RepairSpecialPower)

public:
	RepairSpecialPower(Thing* thing, const ModuleData* data);
	static ModuleData* friend_newModuleData(INI* ini);
	void doSpecialPowerAtObject(Object* target, UnsignedInt options) override;
};
#endif
