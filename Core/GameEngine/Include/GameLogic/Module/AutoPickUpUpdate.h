#pragma once

#include <vector>

#include "Common/SpecialPowerINI.h"
#include "GameLogic/Module/UpdateModule.h"

class CommandButton;
class GrabPassengerSpecialPower;

class AutoPickUpUpdateModuleData : public UpdateModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& fields);
	static void parseEatEntry(INI* ini, void* instance, void* store, const void* userData);

	struct EatEntry
	{
		Real myHealth;
		Real targetHealth;
		SpecialPowerObjectFilter filter;
	};

	std::vector<EatEntry> m_eatEntries;
	SpecialPowerObjectFilter m_pickUpFilter;
	Real m_range = 300.0f;
	UnsignedInt m_delay = 0;
	Int m_buttonNumber = 1;
	Bool m_runFromButton = FALSE;
	Bool m_scanWhileBusy = FALSE;
};

class AutoPickUpUpdate final : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(AutoPickUpUpdate, "AutoPickUpUpdate")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(AutoPickUpUpdate, AutoPickUpUpdateModuleData)

public:
	AutoPickUpUpdate(Thing* thing, const ModuleData* data);

	static Bool canUseButton(const Object* object, const CommandButton* button);
	static Bool canScanFromButton(const Object* object, const CommandButton* button);
	static Bool hasEatingButton(const Object* object);
	static void issueButton(Object* object, const CommandButton* button);
	UpdateSleepTime update() override;

private:
	static AutoPickUpUpdate* findButtonModule(const Object* object, const CommandButton* button);
	static AutoPickUpUpdate* findButtonNumber(const Object* object, Int number);
	GrabPassengerSpecialPower* findEligibleGrab();
	void scan();
};
