#pragma once

#include "Common/RetailSaveBlocks.h"
#include "Common/Xfer.h"
#include "GameLogic/AIStateMachine.h"

namespace RetailGuardSave
{
template<typename Conditions>
void transferAttack(Xfer* xfer, Bool present, StateMachine* machine, AIAttackState*& attack,
	Conditions& conditions, Bool follow, Bool* reenterOnLoad = nullptr, AIEnterState** enter = nullptr)
{
	const Bool loading = xfer->getXferMode() == XFER_LOAD;
	RetailSaveBlocks::transferSection<Xfer>(xfer, loading, present, {
		{ "guard-attack", [&](Xfer* transfer)
		{
			if (loading && reenterOnLoad)
				*reenterOnLoad = FALSE;
			Bool hasAttack = attack != nullptr;
			transfer->xferBool(&hasAttack);
			if (hasAttack)
			{
				transfer->xferUnsignedInt(&conditions.m_attackGiveUpFrame);
				if constexpr (requires { conditions.m_center; })
				{
					transfer->xferCoord3D(&conditions.m_center);
					transfer->xferReal(&conditions.m_radiusSqr);
					transfer->xferInt(&conditions.m_conditionsToConsider);
				}
				if (loading && !attack)
					attack = newInstance(AIAttackState)(machine, follow, TRUE, FALSE, &conditions);
				transfer->xferSnapshot(attack);
			}
			if (enter)
			{
				Bool hasEnter = *enter != nullptr;
				transfer->xferBool(&hasEnter);
				if (hasEnter)
				{
					if (loading && !*enter)
						*enter = newInstance(AIEnterState)(machine);
					transfer->xferSnapshot(*enter);
				}
			}
		}, [&] { if (reenterOnLoad) *reenterOnLoad = TRUE; } }
	});
}

inline void transferMode(Xfer* xfer, Bool present, GuardMode& mode)
{
	const Bool loading = xfer->getXferMode() == XFER_LOAD;
	RetailSaveBlocks::transferSection<Xfer>(xfer, loading, present, {
		{ "guard-mode", [&](Xfer* transfer)
		{
			Int value = mode;
			transfer->xferInt(&value);
			if (loading)
				mode = static_cast<GuardMode>(value);
		}, [&] { mode = GUARDMODE_NORMAL; } }
	});
}
}
