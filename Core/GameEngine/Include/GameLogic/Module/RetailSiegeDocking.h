#pragma once

#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/SpecialPowerModule.h"

class ModuleInfo;

class SiegeDockingBehavior final : public BehaviorModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(SiegeDockingBehavior, "SiegeDockingBehavior")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(SiegeDockingBehavior, BehaviorModuleData)

public:
	struct Socket
	{
		Coord3D position;
		Real facing;
	};

	SiegeDockingBehavior(Thing* thing, const ModuleData* data);
	static SiegeDockingBehavior* find(const Object* wall);
	ObjectID getWallID() const;
	Int getSockets(const Object* carrier, Socket (&sockets)[10]) const;
	Bool available(const Object* carrier, Int slot) const;
	void reserve(const Object* carrier, Int slot);
	void release(const Object* carrier);

private:
	void transferSockets(Xfer* xfer);
	ObjectID m_owners[2][10]{};
};

class SiegeDeploySpecialPowerModuleData : public SpecialPowerModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& fields);
	UnsignedInt m_lowerDelay = 0;
	UnsignedInt m_raiseDelay = 0;
	Bool m_evacuate = FALSE;
	Bool m_skipAdjustPosition = FALSE;
	Real m_wallSearchDistance = 0;
	Real m_extraWallDistance = 0;
};

class SiegeAIUpdate final : public AIUpdateInterface
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(SiegeAIUpdate, "SiegeAIUpdate")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(SiegeAIUpdate, AIUpdateModuleData)

public:
	SiegeAIUpdate(Thing* thing, const ModuleData* data);
	static SiegeAIUpdate* find(const Object* carrier);
	static Bool hasSpecializedAI(const ModuleInfo& modules);
	const AsciiString& getRetailLuaEventList() const override;
	Bool beginDock(Object* wall, const SiegeDeploySpecialPowerModuleData& data, CommandSourceType source);
	Bool stopDock();
	Bool canBeginDock() const { return m_phase == Idle; }
	Bool isUsingDock() const { return m_phase != Idle && m_phase != Raising; }
	Bool isDocked() const { return m_phase == Docked; }
	Bool evacuatesOnDock() const { return m_evacuate; }
	Bool isDockedAt(const Object* wall) const;
	Bool isApproaching() const { return m_phase == Approaching; }
	const Coord3D& getDockPosition() const { return m_dockPosition; }
	Bool getReleasePosition(Coord3D& position) const;
	UpdateSleepTime update() override;
	void aiDoCommand(const AICommandParms* parms) override;
	void onDelete() override;

private:
	enum Phase { Idle, Approaching, Lowering, Docked, Raising };
	void transferDock(Xfer* xfer);
	void resetDock();
	Real dockingRadiusSquared() const;
	Bool chooseSocket(SiegeDockingBehavior& wall, CommandSourceType source);
	Bool chooseNearbyWall(const Object& wall, CommandSourceType source);
	void setPhase(Phase phase);

	Phase m_phase = Idle;
	ObjectID m_wall = INVALID_ID;
	Int m_rejected = 0;
	Coord3D m_dockPosition{};
	Real m_dockFacing = 0;
	UnsignedInt m_deadline = 0;
	UnsignedInt m_lowerDelay = 0;
	UnsignedInt m_raiseDelay = 0;
	Real m_retreatDistance = 0;
	Real m_wallSearchDistance = 0;
	Bool m_evacuate = FALSE;
	Bool m_skipAdjustPosition = FALSE;
};

class SiegeDeploySpecialPower final : public SpecialPowerModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(SiegeDeploySpecialPower, "SiegeDeploySpecialPower")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(SiegeDeploySpecialPower, SiegeDeploySpecialPowerModuleData)

public:
	SiegeDeploySpecialPower(Thing* thing, const ModuleData* data);
	Bool isValidPowerTarget(const Object* target) const override;
	Bool isRetailCastAllowed(UnsignedInt options = 0) const override;
	Real getPercentReady() const override;
	void doSpecialPowerAtObject(Object* target, UnsignedInt options) override;
};

class StopSpecialPowerModuleData : public SpecialPowerModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& fields);
	AsciiString m_stoppedPower;
};

class StopSpecialPower final : public SpecialPowerModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(StopSpecialPower, "StopSpecialPower")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(StopSpecialPower, StopSpecialPowerModuleData)

public:
	StopSpecialPower(Thing* thing, const ModuleData* data);
	Bool isRetailCastAllowed(UnsignedInt options = 0) const override;
	Real getPercentReady() const override;
	void doSpecialPower(UnsignedInt options) override;
};

class SiegeDeployHordeSpecialPowerModuleData : public SpecialPowerModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& fields);
	Bool m_hordeDeploy = FALSE;
};

class SiegeDeployHordeSpecialPower final : public SpecialPowerModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(SiegeDeployHordeSpecialPower, "SiegeDeployHordeSpecialPower")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(SiegeDeployHordeSpecialPower, SiegeDeployHordeSpecialPowerModuleData)

public:
	SiegeDeployHordeSpecialPower(Thing* thing, const ModuleData* data);
	Bool isValidPowerTarget(const Object* target) const override;
	void doSpecialPowerAtObject(Object* target, UnsignedInt options) override;

private:
	Object* findCarrier(const Object* target) const;
	Object* findMemberCarrier(const Object* target) const;
};

class ManTheWallsSpecialPower final : public SpecialPowerModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(ManTheWallsSpecialPower, "ManTheWallsSpecialPower")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(ManTheWallsSpecialPower, SpecialPowerModuleData)

public:
	ManTheWallsSpecialPower(Thing* thing, const ModuleData* data);
	Bool isValidPowerTarget(const Object* target) const override;
	void doSpecialPowerAtObject(Object* target, UnsignedInt options) override;
};
