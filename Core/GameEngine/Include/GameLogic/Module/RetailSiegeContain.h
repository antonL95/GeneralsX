#pragma once

#include "GameLogic/Module/RetailHordeTransport.h"

class SiegeEngineContainModuleData : public HordeTransportContainModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& fields);

	SpecialPowerObjectFilter m_crewFilter;
	ObjectStatusMaskType m_crewStatus;
	InitialPayload m_initialCrew{};
	Int m_crewMax = 0;
	Real m_speedPercentPerCrew = 0.0f;
	Bool m_crewAllowedToFire = FALSE;

private:
	static void parseInitialCrew(INI* ini, void*, void* store, const void*);
};

class SiegeEngineContain : public HordeTransportContain
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(SiegeEngineContain, "SiegeEngineContain")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(SiegeEngineContain, SiegeEngineContainModuleData)

public:
	SiegeEngineContain(Thing* thing, const ModuleData* data);
	static SiegeEngineContain* find(const Object* object);
	Int getContainMax() const override;
	Bool isValidContainerFor(const Object* rider, Bool checkCapacity) const override;
	Bool isCrew(const Object* rider) const;
	Real crewSpeedScale() const;
	void releasePassengersOnWall();
	void exitObjectViaDoor(Object* rider, ExitDoorType door) override;
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

protected:
	void createPayload() override;
	Bool isSpecificRiderFreeToExit(Object* rider) override;

private:
	Int countCrew() const;
	void releaseOnWall(Object* rider, const Coord3D& landing);
	void positionSingleOccupants();
};

class HordeSiegeEngineContain final : public SiegeEngineContain
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(HordeSiegeEngineContain, "HordeSiegeEngineContain")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(HordeSiegeEngineContain, SiegeEngineContainModuleData)

public:
	HordeSiegeEngineContain(Thing* thing, const ModuleData* data);
};
