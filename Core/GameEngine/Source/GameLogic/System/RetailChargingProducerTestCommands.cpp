#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/ThingTemplate.h"
#include "GameLogic/Object.h"

namespace
{
void sampleCharging(const RetailTestCommands::Context& ctx)
{
	const Object* object = ctx.requireObject();
	if (!object)
		return;

	const ModelConditionFlags& conditions = object->getLogicModelConditions();
	std::fprintf(stderr, "TEST charging-producer %u %s active=%d rider8=%d\n", ctx.frame,
		object->getTemplate()->getName().str(), conditions.test(MODELCONDITION_CHARGING),
		conditions.test(MODELCONDITION_RIDER8));
}
}
#endif

namespace RetailTestCommands
{
void registerRetailChargingProducerTestCommands()
{
#if !RTS_GENERALS
	registerAction("charging-producer-sample", sampleCharging);
#endif
}
}
