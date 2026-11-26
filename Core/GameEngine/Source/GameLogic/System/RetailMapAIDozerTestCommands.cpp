#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if RTS_ZEROHOUR
#include "Common/Player.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/AI.h"
#include "GameLogic/Module/ProductionUpdate.h"
#include "GameLogic/Object.h"

#include <cstdio>

namespace
{
void probe(const RetailTestCommands::Context& ctx)
{
	Object* factory = ctx.requireObject();
	if (!factory || !ctx.player)
		return;
	const TAiData* active = TheAI->getAiData();
	const TAiData* base = active;
	while (base->m_next)
		base = base->m_next;
	const AsciiString side = ctx.player->getSide();
	const AsciiString assignment = active->getRetailDozerAssignment(side);
	const AsciiString defaultUnit = base->getRetailDozerAssignment(side);
	const ProductionUpdateInterface* production = factory->getProductionUpdateInterface();
	const ProductionEntry* entry = production ? production->firstProduction() : nullptr;
	const ThingTemplate* queued = entry && entry->getProductionType() == PRODUCTION_UNIT ? entry->getProductionObject() : nullptr;
	const Object* trained = ctx.findTarget("Player_2:MapScriptDozerReplacement:0", true);
	std::fprintf(stderr, "TEST map-ai-dozer frame=%u assigned=%s base=%s queued=%s trained=%d\n",
		ctx.frame, assignment.str(), defaultUnit.str(), queued ? queued->getName().str() : "none", trained != nullptr);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailMapAIDozerTestCommands()
{
#if RTS_ZEROHOUR
	registerAction("map-ai-dozer-probe", probe);
#endif
}
}
