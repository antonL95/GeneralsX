#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/Player.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/RetailVictorySystem.h"

namespace
{
void sample(const RetailTestCommands::Context& ctx)
{
	const Object* object = ctx.requireObject(true);
	if (!ctx.player || !object)
		return;

	const auto momentum = TheGameLogic->getRetailVictorySystem().sample(*ctx.player, *object->getPosition());
	const auto& conditions = object->getLogicModelConditions();
	std::fprintf(stderr, "TEST victory frame=%u player=%.*s unit=%s map=%.3f cell=%.3f shifted=%.3f rank=%d promoted=%d cheer=%d\n",
		ctx.frame, static_cast<Int>(ctx.playerName.size()), ctx.playerName.data(), ctx.targetName,
		momentum.map, momentum.cells[0], momentum.cells[1], object->getExperienceTracker()->getRank(),
		conditions.test(MODELCONDITION_LEVELED), conditions.test(MODELCONDITION_EMOTION_CELEBRATING));
}
}
#endif

namespace RetailTestCommands
{
void registerRetailVictoryTestCommands()
{
#if !RTS_GENERALS
	registerAction("victory-probe", sample);
#endif
}
}
