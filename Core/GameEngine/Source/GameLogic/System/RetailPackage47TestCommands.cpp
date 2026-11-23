#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include "Common/ThingTemplate.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object.h"

namespace
{
void observeTarget(const RetailTestCommands::Context& ctx)
{
	Object* object = ctx.requireObject();
	if (!object)
		return;
	const auto* target = object->getAI() ? object->getAI()->getGoalObject() : nullptr;
	std::fprintf(stderr, "TEST pkg47 target frame=%u source=%s target=%s\n", ctx.frame,
		object->getTemplate()->getName().str(), target ? target->getTemplate()->getName().str() : "none");
}
}
#endif

namespace RetailTestCommands
{
void registerRetailPackage47TestCommands()
{
#if !RTS_GENERALS
	registerAction("pkg47-target", observeTarget);
#endif
}
}
