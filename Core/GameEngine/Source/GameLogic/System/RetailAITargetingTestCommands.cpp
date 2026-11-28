#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include "Common/Player.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/AI.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object.h"

namespace
{
void observeTarget(const RetailTestCommands::Context& ctx)
{
	Object* object = ctx.requireObject();
	if (!object)
		return;
	const Object* target = object->getAI() ? object->getAI()->getGoalObject() : nullptr;
	const Player* owner = target ? target->getControllingPlayer() : nullptr;
	std::fprintf(stderr, "TEST ai-target frame=%u source=%s skirmishAI=%d target=%s targetOwner=%s\n", ctx.frame,
		object->getTemplate()->getName().str(), object->getControllingPlayer()->isSkirmishAIPlayer() ? 1 : 0,
		target ? target->getTemplate()->getName().str() : "none", owner ? owner->getPlayerName().str() : "none");
}

void observeRetaliateFriendsRadius(const RetailTestCommands::Context& ctx)
{
	std::fprintf(stderr, "TEST ai-retaliate-friends frame=%u radius=%.2f\n", ctx.frame,
		TheAI->getAiData()->m_retaliateFriendsRadius);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailAITargetingTestCommands()
{
#if !RTS_GENERALS
	registerAction("ai-target", observeTarget);
	registerAction("ai-retaliate-friends", observeRetaliateFriendsRadius);
#endif
}
}
