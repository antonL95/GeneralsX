#pragma once

#include "GameLogic/Module/AIUpdate.h"

class FXList;

class GiantBirdAIUpdateModuleData final : public AIUpdateModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& p);

	Real m_followThroughDistance = 0.0f;
	Real m_followThroughCheckStep = 0.0f;
	Real m_followThroughGradient = 0.0f;
	Real m_grabTossTimeTrigger = 0.0f;
	Real m_grabTossHeightTrigger = 0.0f;
	const FXList* m_tossFX = nullptr;
	AsciiString m_specialContactPoints;
	AsciiString m_attackLocomotorType;
	AsciiString m_returnForAmmoLocomotorType;
};

class GiantBirdAIUpdate final : public AIUpdateInterface
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(GiantBirdAIUpdate, "GiantBirdAIUpdate")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(GiantBirdAIUpdate, GiantBirdAIUpdateModuleData)

public:
	GiantBirdAIUpdate(Thing* thing, const ModuleData* data);
	UpdateSleepTime update() override;
	void aiDoCommand(const AICommandParms* parms) override;
	static Bool canStrike(const Object& bird, const Coord3D& target);

protected:
	UpdateSleepTime doLocomotor() override;

private:
	void startFollowThrough();
	void setSwooping(Bool swooping);
	void xferFlightState(Xfer* xfer);

	Coord3D m_followThroughGoal = {};
	Bool m_followingThrough = FALSE;
	Bool m_swooping = FALSE;
};
