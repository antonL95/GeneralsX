#include "PreRTS.h"

#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include "GameClient/RetailLargeGroupAudio.h"
#include "GameLogic/Object.h"

namespace
{
void trace(const RetailTestCommands::Context& ctx)
{
	RetailLargeGroupAudio::traceGroup(ctx.argument);
}

void celebrate(const RetailTestCommands::Context& ctx)
{
	if (Object* object = ctx.requireObject(true))
	{
		if (std::string_view(ctx.argument) == "on")
			object->setModelConditionState(MODELCONDITION_EMOTION_CELEBRATING);
		else
			object->clearModelConditionState(MODELCONDITION_EMOTION_CELEBRATING);
	}
}
}
#endif

namespace RetailTestCommands
{
void registerRetailLargeGroupAudioTestCommands()
{
#if !RTS_GENERALS
	registerAction("largegroup-audio-trace", trace);
	registerAction("largegroup-audio-celebrate", celebrate);
#endif
}
}
