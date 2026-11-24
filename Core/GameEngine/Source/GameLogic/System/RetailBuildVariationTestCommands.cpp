#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if RTS_ZEROHOUR
#include <cstdio>

#include "Common/RandomValue.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "GameClient/Drawable.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/FoundationAIUpdate.h"
#include "GameLogic/Object.h"
#include "../../../../GameEngineDevice/Include/W3DDevice/GameClient/Module/W3DModelDraw.h"

namespace
{
const char* getRenderedModel(Drawable& drawable)
{
	for (auto modules = drawable.getDrawModules(); *modules; ++modules)
	{
		auto* draw = dynamic_cast<W3DModelDraw*>(*modules);
		if (draw && draw->getRenderObject())
			return draw->getRenderObject()->Get_Name();
	}
	return "none";
}

void logObject(const RetailTestCommands::Context& ctx, const Object* object, unsigned count)
{
	Drawable* drawable = object ? object->getDrawable() : nullptr;
	const Coord3D position = object ? *object->getPosition() : Coord3D{};
	const char* model = drawable ? getRenderedModel(*drawable) : "missing";
	std::fprintf(stderr, "TEST build-variations %u %s count=%u template=%s model=%s scale=%.3f x=%.2f y=%.2f under=%d dead=%d\n",
		ctx.frame, ctx.action.data(), count, object ? object->getTemplate()->getName().str() : "-",
		model, drawable ? drawable->getScale() : 0.0f, position.x, position.y,
		object && object->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION), object && object->isEffectivelyDead());
}

void logFoundation(const RetailTestCommands::Context& ctx)
{
	const auto* source = ctx.requireObject();
	const auto* foundation = source ? FoundationAIUpdate::find(*source) : nullptr;
	const Object* expansion = foundation ? foundation->getStructure() : nullptr;
	logObject(ctx, expansion, expansion ? 1 : 0);
}

void logProduction(const RetailTestCommands::Context& ctx)
{
	const Object* producer = ctx.requireObject();
	const Object* product = nullptr;
	unsigned count = 0;
	for (const Object* object = TheGameLogic->getFirstObject(); producer && object; object = object->getNextObject())
	{
		if (object->getProducerID() != producer->getID() || object->isDestroyed())
			continue;
		product = object;
		++count;
	}
	logObject(ctx, count == 1 ? product : nullptr, count);
}

void logHatch(const RetailTestCommands::Context& ctx)
{
	const Object* source = ctx.requireObject();
	const Object* child = nullptr;
	unsigned count = 0;
	for (const Object* object = TheGameLogic->getFirstObject(); source && object; object = object->getNextObject())
	{
		const Coord3D& position = *object->getPosition();
		const Coord3D& origin = *source->getPosition();
		if (object->getTeam() != source->getTeam() || object->isEffectivelyDead() ||
			object->getTemplate()->getName().compareNoCase(ctx.argument) != 0 ||
			position.x < origin.x - 180.0f || position.x > origin.x + 180.0f ||
			position.y < origin.y - 180.0f || position.y > origin.y + 180.0f)
			continue;
		child = object;
		++count;
	}
	logObject(ctx, count == 1 ? child : nullptr, count);
}

void checkSingleton(const RetailTestCommands::Context& ctx)
{
	Object* source = ctx.requireObject();
	const ThingTemplate* wrapper = TheThingFactory->findTemplate(ctx.argument, FALSE);
	if (!source || !wrapper)
		return;

	const GameLogicRandomState before = getGameLogicRandomState();
	GameLogicRandomValue(0, 1);
	const UnsignedInt expected = GetGameLogicRandomSeedCRC();
	setGameLogicRandomState(before);

	Object* object = TheThingFactory->newObject(wrapper, source->getTeam());
	std::fprintf(stderr, "TEST build-variations %u singleton template=%s logic-sample=%d\n",
		ctx.frame, object->getTemplate()->getName().str(), GetGameLogicRandomSeedCRC() == expected);
	TheGameLogic->destroyObject(object);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailBuildVariationTestCommands()
{
#if RTS_ZEROHOUR
	registerAction("build-variations-plot", logFoundation);
	registerAction("build-variations-product", logProduction);
	registerAction("build-variations-hatch", logHatch);
	registerAction("build-variations-singleton", checkSingleton);
#endif
}
}
