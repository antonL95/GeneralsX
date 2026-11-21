#pragma once

#include "GameLogic/Module/AIUpdate.h"

class HordeAIUpdateModuleData final : public AIUpdateModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& p);

	AsciiString m_comboLocomotorSet;
	Real m_comboLocoAttackDistance = 0.0f;
};

class HordeAIUpdate final : public AIUpdateInterface
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(HordeAIUpdate, "HordeAIUpdate")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(HordeAIUpdate, HordeAIUpdateModuleData)

public:
	HordeAIUpdate(Thing* thing, const ModuleData* data);
	UpdateSleepTime update() override;
	void aiDoCommand(const AICommandParms* parms) override;
	static void retaliate(Object* victim, Object* attacker);
	Bool hasAttackOrder() const { return m_attackTarget != INVALID_ID || m_order == AICMD_ATTACKMOVE_TO_POSITION; }
	Bool isEngaging() const { return m_engaging; }

private:
	static constexpr AICommandType NO_ORDER = (AICommandType)-1;
	void respondToAttack(Object* attacker, Int maxShots, CommandSourceType source);
	Object* updateOrder(Object* anchor, Object* target, Bool refresh);
	void approach(const Coord3D& position, Real range);
	void xferOrder(Xfer* xfer);

	ObjectID m_attackTarget = INVALID_ID;
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
};
