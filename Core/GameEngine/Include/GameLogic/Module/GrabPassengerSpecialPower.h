#pragma once

#include "GameLogic/Module/SpecialPowerModule.h"

class GrabPassengerSpecialPowerModuleData : public SpecialPowerModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& fields);

	Real m_grabRadius = 0.0f;
	Bool m_allowTree = TRUE;
};

class GrabPassengerSpecialPower : public SpecialPowerModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(GrabPassengerSpecialPower, "GrabPassengerSpecialPower")
	MAKE_STANDARD_MODULE_MACRO(GrabPassengerSpecialPower)

public:
	using SpecialPowerModule::SpecialPowerModule;

	static ModuleData* friend_newModuleData(INI* ini);
	ObjectID getPendingTarget() const { return m_target; }

	void doSpecialPower(UnsignedInt commandOptions) override;
	Bool isValidPowerTarget(const Object* target) const override;
	void doSpecialPowerAtObject(Object* target, UnsignedInt commandOptions) override;
	void doSpecialPowerAtLocation(const Coord3D* position, Real angle, UnsignedInt commandOptions) override;

protected:
	void applyPowerEffect(const Coord3D*) override;

private:
	Bool canGrab(const Object* target) const;
	const Object* findGrabTarget(const Object* target) const;
	ObjectID m_target = INVALID_ID;
};
