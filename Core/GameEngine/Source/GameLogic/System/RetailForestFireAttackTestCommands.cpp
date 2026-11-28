#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "Common/ActionManager.h"
#include "Common/MessageStream.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/AI.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Object.h"

namespace
{
Bool s_hasSavedAllowForestFires = FALSE;
Bool s_savedAllowForestFires = FALSE;

void handleForestFireAttack(const RetailTestCommands::Context& ctx)
{
	if (ctx.action == "forest-fire-flag")
	{
		if (!TheAI)
			return;
		auto* aiData = const_cast<TAiData*>(TheAI->getAiData());
		if (ctx.argument && std::strcmp(ctx.argument, "restore") == 0)
		{
			if (s_hasSavedAllowForestFires)
			{
				aiData->m_allowForestFires = s_savedAllowForestFires;
				s_hasSavedAllowForestFires = FALSE;
			}
			std::fprintf(stderr, "TEST forest-fire flag frame=%u allow=%d\n",
				ctx.frame, aiData->m_allowForestFires ? 1 : 0);
			std::fflush(stderr);
			return;
		}
		if (!s_hasSavedAllowForestFires)
		{
			s_savedAllowForestFires = aiData->m_allowForestFires;
			s_hasSavedAllowForestFires = TRUE;
		}
		const Bool allow = (ctx.argument && std::atoi(ctx.argument) != 0) ? TRUE : FALSE;
		aiData->m_allowForestFires = allow;
		std::fprintf(stderr, "TEST forest-fire flag frame=%u allow=%d\n", ctx.frame, allow ? 1 : 0);
		std::fflush(stderr);
	}
	else if (ctx.action == "forest-fire-attack")
	{
		Object* source = ctx.requireObject(TRUE);
		Object* target = ctx.findTarget(TRUE);
		if (!source || !target)
			return;
		const Bool canAttack = TheActionManager &&
			TheActionManager->getCanAttackObject(source, target, CMD_FROM_PLAYER, ATTACK_NEW_TARGET) != ATTACKRESULT_NOT_POSSIBLE;
		ctx.selectObject(source);
		GameMessage* message = TheMessageStream->appendMessage(GameMessage::MSG_DO_ATTACK_OBJECT);
		message->appendObjectIDArgument(target->getID());
		std::fprintf(stderr, "TEST forest-fire attack frame=%u can=%d\n", ctx.frame, canAttack ? 1 : 0);
		std::fflush(stderr);
	}
	else if (ctx.action == "forest-fire-health")
	{
		Object* source = ctx.requireObject(FALSE);
		if (!source || !source->getBodyModule())
			return;
		std::fprintf(stderr, "TEST forest-fire health frame=%u name=%s hp=%.0f\n",
			ctx.frame, source->getTemplate()->getName().str(), source->getBodyModule()->getHealth());
		std::fflush(stderr);
	}
	else if (ctx.action == "forest-fire-victim")
	{
		Object* source = ctx.requireObject(FALSE);
		if (!source)
			return;
		const AIUpdateInterface* ai = source->getAIUpdateInterface();
		const Object* victim = ai ? ai->getCurrentVictim() : nullptr;
		const Bool isTree = (victim && victim->isKindOf(KINDOF_TREE)) ? TRUE : FALSE;
		std::fprintf(stderr, "TEST forest-fire victim frame=%u name=%s tree=%d\n",
			ctx.frame, source->getTemplate()->getName().str(), isTree ? 1 : 0);
		std::fflush(stderr);
	}
}
}
#endif

namespace RetailTestCommands
{
void registerRetailForestFireAttackTestCommands()
{
#if !RTS_GENERALS
	registerPrefix("forest-fire-", handleForestFireAttack);
#endif
}
}
