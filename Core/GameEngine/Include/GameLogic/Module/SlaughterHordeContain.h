#pragma once

#include "Common/SpecialPowerINI.h"
#include "GameLogic/Module/OpenContain.h"

class SlaughterHordeContainModuleData : public OpenContainModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& p);

	SpecialPowerObjectFilter m_passengers;
	ObjectStatusMaskType m_passengerStatus;
	Real m_cashBack = 0.0f;
	Int m_maxHordeCapacity = 0;
};

class SlaughterHordeContain : public OpenContain
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(SlaughterHordeContain, "SlaughterHordeContain")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(SlaughterHordeContain, SlaughterHordeContainModuleData)

public:
	SlaughterHordeContain(Thing* thing, const ModuleData* data);

	Bool isValidContainerFor(const Object* passenger, Bool checkCapacity) const override;
	void addToContain(Object* passenger) override;

private:
	Bool canSlaughter(const Object* passenger) const;
	void slaughter(Object* passenger);
};
