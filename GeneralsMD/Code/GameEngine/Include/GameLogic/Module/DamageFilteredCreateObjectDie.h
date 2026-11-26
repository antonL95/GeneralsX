#pragma once

#include "GameLogic/Module/DamageModule.h"
#include "GameLogic/Module/DieModule.h"

class ObjectCreationList;

class DamageFilteredCreateObjectDieModuleData : public DieModuleData
{
public:
	const ObjectCreationList* m_ocl = nullptr;
	AsciiString m_instantSubType;
	AsciiString m_durationSubType;
	UnsignedInt m_triggeredDuration = 0;

	static void buildFieldParse(MultiIniFieldParse& p);
};

class DamageFilteredCreateObjectDie : public DieModule, public DamageModuleInterface
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( DamageFilteredCreateObjectDie, "DamageFilteredCreateObjectDie" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( DamageFilteredCreateObjectDie, DamageFilteredCreateObjectDieModuleData )

public:
	DamageFilteredCreateObjectDie( Thing *thing, const ModuleData* moduleData );

	static Int getInterfaceMask() { return DieModule::getInterfaceMask() | MODULEINTERFACE_DAMAGE; }
	virtual DamageModuleInterface* getDamage() override { return this; }

	virtual void onDamage(DamageInfo* damageInfo) override;
	virtual void onHealing(DamageInfo*) override {}
	virtual void onBodyDamageStateChange(const DamageInfo*, BodyDamageType, BodyDamageType) override {}
	virtual void onDie( const DamageInfo *damageInfo ) override;

private:
	void xferRetailBlocks(Xfer* xfer, Bool present);

	UnsignedInt m_triggeredFrame = 0;
	Int m_sourcePlayerIndex = -1;
};
