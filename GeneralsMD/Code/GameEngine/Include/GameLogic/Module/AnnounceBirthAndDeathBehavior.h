#pragma once

#include "GameLogic/Module/CreateModule.h"
#include "GameLogic/Module/DieModule.h"

class AnnounceBirthAndDeathBehavior final : public CreateModule, public DieModuleInterface
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(AnnounceBirthAndDeathBehavior, "AnnounceBirthAndDeathBehavior")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(AnnounceBirthAndDeathBehavior, CreateModuleData)

public:
	AnnounceBirthAndDeathBehavior(Thing* thing, const ModuleData* data) : CreateModule(thing, data) {}
	static Int getInterfaceMask() { return MODULEINTERFACE_CREATE | MODULEINTERFACE_DIE; }
	DieModuleInterface* getDie() override { return this; }
	void onCreate() override;
	Bool shouldDoOnBuildComplete() const override { return FALSE; }
	void onDie(const DamageInfo*) override;
};
