#pragma once

class FXList;

#include "Common/SpecialPowerINI.h"
#include "GameLogic/Module/OpenContain.h"

class SlaughterHordeContainModuleData : public OpenContainModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& p);

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
	Bool canAcceptRingCarrier(const Object* passenger) const;
	Bool hasPlayerRingUpgrade(const Player& player) const;

private:
	Object* findRing(const Object* passenger) const;
};

class AttachUpdateModuleData : public UpdateModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& p);

	SpecialPowerObjectFilter m_filter;
	ObjectStatusMaskType m_parentStatus;
	Real m_scanRange = 10.0f;
	Bool m_anchorToTop = FALSE;
	Bool m_alwaysTeleport = TRUE;
	const FXList* m_attachFX = nullptr;
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
	static void reconcileOwnershipChange(const Object* changed);
	ObjectID getParentID() const { return m_parent; }
	Bool canAttachTo(const Object& parent) const;

private:
	Object* reconcileParent();
	void followParent(const Object* parent);
	ObjectID m_parent = INVALID_ID;
};
