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
	Bool m_allowOwnPlayer = FALSE;
};

class SlaughterHordeContain : public OpenContain
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(SlaughterHordeContain, "SlaughterHordeContain")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(SlaughterHordeContain, SlaughterHordeContainModuleData)

public:
	SlaughterHordeContain(Thing* thing, const ModuleData* data);

	Bool isValidContainerFor(const Object* passenger, Bool checkCapacity) const override;
	void addToContain(Object* passenger) override;

protected:
	Bool canAcceptEntry(const Object* passenger) const;
	void playEntrySound();

private:
	Bool canSlaughter(const Object* passenger) const;
	void slaughter(Object* passenger);
};

class CitadelSlaughterHordeContainModuleData : public SlaughterHordeContainModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& p);

	ObjectStatusMaskType m_ringStatus;
	std::vector<AsciiString> m_ringUpgrades;
	SpecialPowerObjectFilter m_ringFilter;
	const FXList* m_ringFX = nullptr;
};

class CitadelSlaughterHordeContain final : public SlaughterHordeContain
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(CitadelSlaughterHordeContain, "CitadelSlaughterHordeContain")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(CitadelSlaughterHordeContain, CitadelSlaughterHordeContainModuleData)

public:
	CitadelSlaughterHordeContain(Thing* thing, const ModuleData* data) : SlaughterHordeContain(thing, data) {}
	Bool isValidContainerFor(const Object* passenger, Bool checkCapacity) const override;
	void addToContain(Object* passenger) override;

private:
	Object* findRing(const Object* passenger) const;
};

class AttachUpdateModuleData : public UpdateModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& p);

	SpecialPowerObjectFilter m_filter;
	ObjectStatusMaskType m_parentStatus;
	Real m_scanRange = 0.0f;
	Bool m_anchorToTop = FALSE;
	Bool m_alwaysTeleport = FALSE;
	AsciiString m_parentOwnerAttachmentEva;
	AsciiString m_parentEnemyAttachmentEva;
	AsciiString m_parentOwnerDiedEva;
};

class AttachUpdate final : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(AttachUpdate, "AttachUpdate")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(AttachUpdate, AttachUpdateModuleData)

public:
	AttachUpdate(Thing* thing, const ModuleData* data) : UpdateModule(thing, data) {}
	UpdateSleepTime update() override;
	void onDelete() override;
	ObjectID getParentID() const { return m_parent; }

private:
	ObjectID m_parent = INVALID_ID;
};
