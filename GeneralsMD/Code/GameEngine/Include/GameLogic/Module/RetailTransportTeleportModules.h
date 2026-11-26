#pragma once

#include <vector>

#include "Common/SpecialPowerINI.h"
#include "Common/Team.h"
#include "GameLogic/Module/ObjectHelper.h"
#include "GameLogic/Module/SpecialAbilityUpdate.h"

class FXList;
class WeaponTemplate;

void retailTeleportObjectWithMembers(Object* unit, const Coord3D& destination);

class TemporarilyDefectUpdateModuleData : public ModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& p);

	UnsignedInt m_defectDuration = 0;
};

class TemporarilyDefectUpdate final : public ObjectHelper
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(TemporarilyDefectUpdate, "TemporarilyDefectUpdate")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(TemporarilyDefectUpdate, TemporarilyDefectUpdateModuleData)

public:
	TemporarilyDefectUpdate(Thing* thing, const ModuleData* data) : ObjectHelper(thing, data) {}

	static TemporarilyDefectUpdate* find(Object* object);
	void restoreLegacy(ObjectID caster, TeamID originalTeam, UnsignedInt deadline);
	void invalidateCaster(ObjectID caster);
	Bool convert(ObjectID caster, Team* team, UnsignedInt fallbackDuration);
	void cancel();
	UpdateSleepTime update() override;
	DisabledMaskType getDisabledTypesToProcess() const override { return DISABLEDMASK_ALL; }

private:
	friend class Object;
	void schedule();
	UnsignedInt m_legacyEarliestDeadline = 0;
};

class DominateEnemySpecialPowerModuleData : public SpecialAbilityUpdateModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& p);

	SpecialPowerObjectFilter m_filter;
	Bool m_hasFilter = FALSE;
	Real m_dominateRadius = 0.0f;
	const FXList* m_dominatedFX = nullptr;
	const FXList* m_triggerFX = nullptr;
	Bool m_permanentlyConvert = FALSE;
};

class DominateEnemySpecialPower final : public SpecialAbilityUpdate
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(DominateEnemySpecialPower, "DominateEnemySpecialPower")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(DominateEnemySpecialPower, DominateEnemySpecialPowerModuleData)

public:
	DominateEnemySpecialPower(Thing* thing, const ModuleData* data) : SpecialAbilityUpdate(thing, data) {}
	void onDelete() override;

protected:
	Bool acceptsIntent(const Object* target, const Coord3D* position) const override;
	void triggerAbilityEffect() override;

private:
	struct DominatedEntry
	{
		ObjectID objectID = INVALID_ID;
		TeamID originalTeamID = TEAM_ID_INVALID;
		UnsignedInt revertFrame = 0;
	};

	Bool matchesDominateFilter(const Object* victim) const;

	std::vector<DominatedEntry> m_legacyDominated;
};

class FlingPassengerSpecialAbilityUpdateModuleData : public SpecialAbilityUpdateModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& p);

	Coord3D m_flingVelocity = {};
	const WeaponTemplate* m_landingWarhead = nullptr;
};

class FlingPassengerSpecialAbilityUpdate final : public SpecialAbilityUpdate
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(FlingPassengerSpecialAbilityUpdate, "FlingPassengerSpecialAbilityUpdate")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(FlingPassengerSpecialAbilityUpdate, FlingPassengerSpecialAbilityUpdateModuleData)

public:
	FlingPassengerSpecialAbilityUpdate(Thing* thing, const ModuleData* data) : SpecialAbilityUpdate(thing, data) {}

protected:
	Bool acceptsIntent(const Object* target, const Coord3D* position) const override;
	void triggerAbilityEffect() override;
};

class StoreObjectsSpecialPowerModuleData : public SpecialAbilityUpdateModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& p);

	Real m_radius = 0.0f;
};

class StoreObjectsSpecialPower final : public SpecialAbilityUpdate
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(StoreObjectsSpecialPower, "StoreObjectsSpecialPower")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(StoreObjectsSpecialPower, StoreObjectsSpecialPowerModuleData)

public:
	StoreObjectsSpecialPower(Thing* thing, const ModuleData* data) : SpecialAbilityUpdate(thing, data) {}

	const std::vector<ObjectID>& getStoredObjects() const { return m_storedObjects; }
	void clearStoredObjects() { m_storedObjects.clear(); }
	static StoreObjectsSpecialPower* findForCaster(Object* caster, const SpecialPowerTemplate* teleportPower);

protected:
	Bool acceptsIntent(const Object* target, const Coord3D* position) const override;
	void triggerAbilityEffect() override;

private:
	void xferStoredState(Xfer* xfer);

	std::vector<ObjectID> m_storedObjects;
};

class TeleportSpecialAbilityUpdateModuleData : public SpecialAbilityUpdateModuleData
{
public:
	TeleportSpecialAbilityUpdateModuleData()
	{
		m_startAbilityRange = 1000000.0f;
	}
	static void buildFieldParse(MultiIniFieldParse& p);

	UnsignedInt m_busyForDuration = 0;
	const WeaponTemplate* m_destinationWeapon = nullptr;
	const WeaponTemplate* m_sourceWeapon = nullptr;
	Real m_maxDistance = 0.0f;
};

class TeleportSpecialAbilityUpdate final : public SpecialAbilityUpdate
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(TeleportSpecialAbilityUpdate, "TeleportSpecialAbilityUpdate")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(TeleportSpecialAbilityUpdate, TeleportSpecialAbilityUpdateModuleData)

public:
	TeleportSpecialAbilityUpdate(Thing* thing, const ModuleData* data) : SpecialAbilityUpdate(thing, data) {}

protected:
	Bool acceptsIntent(const Object* target, const Coord3D* position) const override;
	void triggerAbilityEffect() override;
	UnsignedInt holdFramesAfterTrigger() const override;
};

class TeleportToCasterSpecialPowerModuleData : public SpecialAbilityUpdateModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& p);

	Real m_radius = 0.0f;
	const FXList* m_targetFX = nullptr;
	const FXList* m_triggerFX = nullptr;
	Real m_minDestinationRadius = 0.0f;
	Real m_maxDestinationRadius = 0.0f;
};

class TeleportToCasterSpecialPower final : public SpecialAbilityUpdate
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(TeleportToCasterSpecialPower, "TeleportToCasterSpecialPower")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(TeleportToCasterSpecialPower, TeleportToCasterSpecialPowerModuleData)

public:
	TeleportToCasterSpecialPower(Thing* thing, const ModuleData* data) : SpecialAbilityUpdate(thing, data) {}

protected:
	Bool acceptsIntent(const Object* target, const Coord3D* position) const override;
	void triggerAbilityEffect() override;
};
