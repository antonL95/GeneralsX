#pragma once

#include "GameLogic/Module/DieModule.h"
#include "GameLogic/Module/StructureCollapseUpdate.h"

#include <array>
#include <vector>

class FXList;

class RubbleRiseUpdateModuleData final : public UpdateModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& p);

	UnsignedInt m_minRubbleRiseDelay = 0;
	UnsignedInt m_maxRubbleRiseDelay = 0;
	Real m_rubbleRiseDamping = 0.0f;
	Real m_rubbleHeight = 0.0f;
	Real m_maxShudder = 0.0f;
	UnsignedInt m_minBurstDelay = 0;
	UnsignedInt m_maxBurstDelay = 0;
	Int m_bigBurstFrequency = 0;
	std::array<std::vector<const FXList*>, SC_PHASE_COUNT> m_fx;
	DieMuxData m_dieMuxData;
};

class RubbleRiseUpdate final : public UpdateModule, public DieModuleInterface
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(RubbleRiseUpdate, "RubbleRiseUpdate")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(RubbleRiseUpdate, RubbleRiseUpdateModuleData)

public:
	RubbleRiseUpdate(Thing* thing, const ModuleData* data);
	static Int getInterfaceMask() { return UpdateModule::getInterfaceMask() | MODULEINTERFACE_DIE; }
	DieModuleInterface* getDie() override { return this; }
	void onDie(const DamageInfo* damageInfo) override;
	UpdateSleepTime update() override;
	Real getRiseOffset() const { return m_height; }

private:
	void xferRetailBlocks(Xfer* xfer, Bool present);
	void playPhase(StructureCollapsePhaseType phase) const;
	void setVisualOffset(Real x, Real y) const;
	UnsignedInt m_riseFrame = 0;
	UnsignedInt m_burstFrame = 0;
	Real m_height = 0.0f;
	Real m_speed = 0.0f;
	Bool m_waiting = FALSE;
	Bool m_rising = FALSE;
};
