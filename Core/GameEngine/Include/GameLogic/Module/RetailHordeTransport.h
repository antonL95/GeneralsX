#pragma once

#include <vector>

#include "Common/RetailSaveBlocks.h"
#include "GameLogic/Damage.h"
#include "GameLogic/Module/DamageModule.h"
#include "GameLogic/Module/GarrisonContain.h"
#include "GameLogic/Module/TransportContain.h"

Bool damageRetailHordePassengers(Object* passenger, ObjectID source, Real fraction, DeathType deathType);

class HordeTransportContainModuleData : public TransportContainModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& p);
};

class HordeTransportContain : public TransportContain
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(HordeTransportContain, "HordeTransportContain")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(HordeTransportContain, HordeTransportContainModuleData)

public:
	HordeTransportContain(Thing* thing, const ModuleData* data);

	Bool isValidContainerFor(const Object* rider, Bool checkCapacity) const override;
	Bool isEnclosingContainerFor(const Object* obj) const override;
	void containReactToTransformChange() override;
	UpdateSleepTime update() override;
#if RTS_GENERALS
	void onContaining(Object* rider) override;
	Bool isPassengerAllowedToFire() const override;
#else
	void onContaining(Object* rider, Bool wasSelected) override;
	Bool isPassengerAllowedToFire(ObjectID id = INVALID_ID) const override;
#endif
	void onRemoving(Object* rider) override;
	void exitObjectViaDoor(Object* exitObj, ExitDoorType exitDoor) override;

private:
	void positionContainedHordes();
	void releaseHordeMembers(Object* rider);
};

class HordeGarrisonContainModuleData : public GarrisonContainModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& p);

	UnsignedInt m_exitDelay = 0;
	Int m_maxHordeCapacity = 0;
	Coord3D m_entryOffset{};
	Coord3D m_entryPosition{};
	Coord3D m_exitOffset{};
};

class HordeGarrisonContain : public GarrisonContain
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(HordeGarrisonContain, "HordeGarrisonContain")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(HordeGarrisonContain, HordeGarrisonContainModuleData)

public:
	HordeGarrisonContain(Thing* thing, const ModuleData* data);

	Bool isValidContainerFor(const Object* obj, Bool checkCapacity) const override;
	Bool isEnclosingContainerFor(const Object* obj) const override;
	Bool isExitBusy() const override;
	void exitObjectViaDoor(Object* exitObj, ExitDoorType exitDoor) override;
#if RTS_GENERALS
	void onContaining(Object* obj) override;
	Bool isPassengerAllowedToFire() const override;
#else
	void onContaining(Object* obj, Bool wasSelected) override;
	Bool isPassengerAllowedToFire(ObjectID id = INVALID_ID) const override;
#endif
	void onRemoving(Object* obj) override;

	void xferRetailBlocks(Xfer* xfer, Bool present);

private:
	void positionHordeAtContainer(Object* obj);
	void releaseHordeMembers(Object* obj);
	UnsignedInt m_frameExitNotBusy = 0;
};

class EvacuateDamageModuleData : public DamageModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& p);

	AsciiString m_weaponThatCausesEvacuation;
	DamageType m_damageTypeToTrack = DAMAGE_NUM_TYPES;
	Real m_damageToPanicThreshold = 0.0f;
	UnsignedInt m_trackingTimeSpan = 0;
};

class EvacuateDamage final : public DamageModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(EvacuateDamage, "EvacuateDamage")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(EvacuateDamage, EvacuateDamageModuleData)

public:
	EvacuateDamage(Thing* thing, const ModuleData* data);

	void onDamage(DamageInfo* damageInfo) override;
	void onHealing(DamageInfo*) override {}
	void onBodyDamageStateChange(const DamageInfo*, BodyDamageType, BodyDamageType) override {}

	void xferRetailBlocks(Xfer* xfer, Bool present);

private:
	struct TrackedDamage
	{
		UnsignedInt frame = 0;
		Real amount = 0.0f;
	};

	void evacuateContainer();
	std::vector<TrackedDamage> m_trackedDamage;
};

class HordeTransportContainDamageModuleData : public DamageModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& p);
};

class HordeTransportContainDamage final : public DamageModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(HordeTransportContainDamage, "HordeTransportContainDamage")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(HordeTransportContainDamage, HordeTransportContainDamageModuleData)

public:
	HordeTransportContainDamage(Thing* thing, const ModuleData* data);

	void onDamage(DamageInfo* damageInfo) override;
	void onHealing(DamageInfo*) override {}
	void onBodyDamageStateChange(const DamageInfo*, BodyDamageType, BodyDamageType) override {}
};
