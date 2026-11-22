#pragma once

#include "Common/Geometry.h"
#include "Common/KindOf.h"
#include "GameLogic/Module/SlowDeathBehavior.h"

class Object;

class ClearanceTestingSlowDeathBehaviorModuleData : public SlowDeathBehaviorModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& p);

	GeometryInfo m_clearance{GEOMETRY_BOX, false, 1.0f, 1.0f, 1.0f};
	Coord3D m_offset{};
	Real m_minDamage = -1.0f;
	Real m_minKillerAngle = 0.0f;
	Real m_maxKillerAngle = 0.0f;
	Real m_maxHeight = 0.0f;
	Real m_maxHeightFraction = 1.0f;
	Real m_minHeight = -20.0f;
	Real m_minHeightFraction = 1.0f;
};

class ClearanceTestingSlowDeathBehavior : public SlowDeathBehavior
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(ClearanceTestingSlowDeathBehavior, "ClearanceTestingSlowDeathBehavior")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(ClearanceTestingSlowDeathBehavior, ClearanceTestingSlowDeathBehaviorModuleData)

public:
	ClearanceTestingSlowDeathBehavior(Thing* thing, const ModuleData* data) : SlowDeathBehavior(thing, data) {}
	Bool isDieApplicable(const DamageInfo* damageInfo) const override;
};

class GiantBirdSlowDeathBehaviorModuleData : public SlowDeathBehaviorModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& p);

	KindOfMaskType m_crashAvoidKinds;
	Real m_crashAvoidRadius = 0.0f;
	Real m_crashAvoidStrength = 0.0f;
	Bool m_holdFlailingHeight = false;
	Bool m_ejectRider = false;
};

class GiantBirdSlowDeathBehavior : public SlowDeathBehavior
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(GiantBirdSlowDeathBehavior, "GiantBirdSlowDeathBehavior")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(GiantBirdSlowDeathBehavior, GiantBirdSlowDeathBehaviorModuleData)

public:
	GiantBirdSlowDeathBehavior(Thing* thing, const ModuleData* data) : SlowDeathBehavior(thing, data) {}
	static Bool controlsMotion(const Object& bird);
	void beginSlowDeath(const DamageInfo* damageInfo) override;
	UpdateSleepTime update() override;

private:
	void xferCrashState(Xfer* xfer);

	Coord3D m_crashSite{};
	Bool m_crashing = false;
};

class ShipSlowDeathBehavior : public SlowDeathBehavior
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(ShipSlowDeathBehavior, "ShipSlowDeathBehavior")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(ShipSlowDeathBehavior, SlowDeathBehaviorModuleData)

public:
	ShipSlowDeathBehavior(Thing* thing, const ModuleData* data) : SlowDeathBehavior(thing, data) {}
	void beginSlowDeath(const DamageInfo* damageInfo) override;
	UpdateSleepTime update() override;

private:
	void xferTiltState(Xfer* xfer);

	Real m_pitchPerFrame = 0.0f;
	Real m_rollPerFrame = 0.0f;
	UnsignedInt m_startFrame = 0;
	Real m_heading = 0.0f;
};
