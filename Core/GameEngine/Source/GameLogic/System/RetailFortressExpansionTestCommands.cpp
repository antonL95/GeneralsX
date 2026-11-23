#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#include <cstdio>

#include "GameLogic/Module/FoundationAIUpdate.h"
#include "GameLogic/Object.h"

namespace
{
void traceExpansion(const RetailTestCommands::Context& ctx)
{
	const Object* pad = ctx.findObject(FALSE);
	const auto* foundation = pad ? FoundationAIUpdate::find(*pad) : nullptr;
	const Object* expansion = foundation ? foundation->getStructure() : nullptr;
	std::fprintf(stderr, "TEST fortress-expansion %u pad=%d id=%u percent=%.2f under=%d dead=%d\n",
		ctx.frame, foundation != nullptr, expansion ? static_cast<unsigned>(expansion->getID()) : 0,
		expansion ? expansion->getConstructionPercent() : 0.0f,
		expansion && expansion->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION),
		expansion && expansion->isEffectivelyDead());
}
}

namespace RetailTestCommands
{
void registerRetailFortressExpansionTestCommands()
{
	registerAction("fortress-expansion-state", traceExpansion);
}
}
