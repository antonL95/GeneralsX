#pragma once

#include "GameLogic/Module/AIUpdate.h"
#include <optional>

class HordeAIUpdateModuleData : public AIUpdateModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& p);

	AsciiString m_comboLocomotorSet;
	Real m_comboLocoAttackDistance = 0.0f;
};

class HordeAIUpdate : public AIUpdateInterface
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(HordeAIUpdate, "HordeAIUpdate")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(HordeAIUpdate, HordeAIUpdateModuleData)

public:
	HordeAIUpdate(Thing* thing, const ModuleData* data);
	UpdateSleepTime update() override;
	void aiDoCommand(const AICommandParms* parms) override;
	void startSiegeDeployment(ObjectID carrier, ObjectID wall);
	Bool issuePlannerOrder(Object* target, const Coord3D& position);
	static void retaliate(Object* victim, Object* attacker);
	Bool hasAttackOrder() const { return m_attackTarget != INVALID_ID || m_order == AICMD_ATTACKMOVE_TO_POSITION; }
	Bool hasActiveAttack() const { return m_order == AICMD_ATTACK_POSITION || getAttackTarget() != nullptr; }
	Bool hasGuardOrder() const;
	ObjectID getGuardObject() const override { return m_order == AICMD_GUARD_OBJECT ? m_guardObject : INVALID_ID; }
	Object* getAttackTarget() const;
	static void memberCollided(Object* member, Object* target);
	Bool hasRecentContact() const;
	Bool isOnPlannerAdvance() const { return m_plannerOrder && m_order == AICMD_ATTACKMOVE_TO_POSITION; }
	Bool isOnPlannerAdvance(UnsignedInt frame) const { return isOnPlannerAdvance() && m_plannerOrderFrame == frame; }
	Bool isExecutingPlannerOrder(ObjectID target, UnsignedInt issuedFrame) const
	{
		return m_plannerOrder && m_plannerOrderFrame == issuedFrame && m_attackTarget == target && hasAttackOrder();
	}
	UnsignedInt blockedPlannerRevision(UnsignedInt issuedFrame) const
	{
		return issuedFrame == m_plannerOrderFrame ? m_blockedPlannerRevision : 0;
	}
	Bool isEngaging() const { return m_engaging; }
	Bool isApproachingAttackTarget() const { return m_attackTarget != INVALID_ID && !m_engaging && isMoving(); }
	Real getCavalryTurn() const { return m_cavalryTurn; }
	Bool isCavalryReforming() const { return m_cavalryReforming; }
	Bool isPacingCavalryReform() const { return m_cavalryReforming || m_cavalryLaunchFrames > 0; }
	std::optional<Real> getCavalryStartupHeading() const { return m_cavalryStartupHeading; }
	void setCavalryStartupHeading(Real heading);
	static Bool isReforming(const AIUpdateInterface* ai);
	void crcReformState(Xfer* xfer);
	void xferRetailBlocks(Xfer* xfer, Bool present);

private:
	enum class SiegeDeploymentPhase { Follow, Wait, Board };
	struct SiegeDeployment
	{
		ObjectID carrier = INVALID_ID;
		ObjectID wall = INVALID_ID;
		UnsignedInt nextFrame = 0;
		SiegeDeploymentPhase phase = SiegeDeploymentPhase::Follow;
	};

	static constexpr AICommandType NO_ORDER = (AICommandType)-1;
	Bool advanceSiegeDeployment();
	void respondToAttack(Object* attacker, Int maxShots, CommandSourceType source);
	Object* updateOrder(Object* anchor, Object* target, Bool refresh);
	void approach(const Coord3D& position, Bool inRange);
	void advance(const Coord3D& position);
	void xferOrder(Xfer* xfer);

	ObjectID m_attackTarget = INVALID_ID;
	ObjectID m_contactTarget = INVALID_ID;
	UnsignedInt m_contactUntil = 0;
	Real m_cavalryTurn = 0.0f;
	Bool m_cavalryReforming = FALSE;
	UnsignedInt m_cavalryLaunchFrames = 0;
	std::optional<Real> m_cavalryStartupHeading;
	UnsignedInt m_reformUntil = 0;
	CommandSourceType m_attackSource = CMD_FROM_AI;
	Int m_maxShots = 0;
	Bool m_forceAttack = FALSE;
	Bool m_engaging = FALSE;
	AICommandType m_order = NO_ORDER;
	Coord3D m_orderPosition{};
	Real m_retaliationStartDistanceSquared = 0.0f;
	ObjectID m_guardObject = INVALID_ID;
	Int m_guardArea = -1;
	GuardMode m_guardMode = GUARDMODE_NORMAL;
	Bool m_updatingAnchor = FALSE;
	Bool m_plannerOrder = FALSE;
	Int m_advanceAttempts = 0;
	Coord3D m_advanceOrigin{};
	UnsignedInt m_advanceRouteRevision = 0;
	UnsignedInt m_plannerOrderFrame = 0;
	UnsignedInt m_blockedPlannerRevision = 0;
	SiegeDeployment m_siegeDeployment;
};
