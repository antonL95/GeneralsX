#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include "Common/ThingTemplate.h"
#include "GameClient/Drawable.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/GeometryUpgrade.h"
#include "GameLogic/Object.h"
#include "../../../../GameEngineDevice/Include/W3DDevice/GameClient/Module/W3DModelDraw.h"

namespace
{
void logAppearance(const RetailTestCommands::Context& ctx)
{
	const Object* object = ctx.requireObject(true);
	const Drawable* drawable = object ? object->getDrawable() : nullptr;
	if (!drawable)
		return;
	for (auto module = drawable->getDrawModules(); *module; ++module)
	{
		const auto* model = dynamic_cast<const W3DModelDraw*>(*module);
		if (!model)
			continue;
		Int known = 0;
		Int heads = 0;
		Int shields = 0;
		for (Int index = 1; index <= 3; ++index)
		{
			AsciiString name;
			Bool hidden = FALSE;
			name.format("HEAD%02d", index);
			if (model->getSubObjectHidden(name, hidden))
			{
				++known;
				if (!hidden)
					heads |= 1 << index;
			}
			name.format("SHIELD%02d", index);
			if (model->getSubObjectHidden(name, hidden))
			{
				++known;
				if (!hidden)
					shields |= 1 << index;
			}
		}
		if (known)
		{
			std::fprintf(stderr, "TEST lua appearance frame=%u index=%d known=%d head=%d shield=%d\n",
				ctx.frame, ctx.ordinal, known, heads, shields);
			return;
		}
	}
}

void logGeometry(const RetailTestCommands::Context& ctx)
{
	const Object* object = ctx.requireObject(true);
	if (!object)
		return;
	const auto& data = object->getTemplate()->getRotWKObjectData();
	std::vector<GeometryUpgrade::CollisionPart> parts;
	GeometryUpgrade::collectCollisionParts(*object, parts, TRUE);
	for (const auto& entry : data.additionalGeometries)
		if (entry.details.name == ctx.argument)
			std::fprintf(stderr, "TEST lua geometry frame=%u authored=%d active=%d parts=%zu\n",
				ctx.frame, entry.details.active, object->getLuaGeometryActive(entry.details.name, entry.details.active), parts.size());
}

void logFear(const RetailTestCommands::Context& ctx)
{
	if (const Object* object = ctx.requireObject(true))
		std::fprintf(stderr, "TEST lua fear frame=%u terror=%d\n",
			ctx.frame, object->getLogicModelConditions().test(MODELCONDITION_EMOTION_TERROR));
}

void logBlastHealth(const RetailTestCommands::Context& ctx)
{
	if (const Object* object = ctx.requireObject(true))
		std::fprintf(stderr, "TEST lua blast frame=%u health=%.2f\n", ctx.frame, object->getBodyModule()->getHealth());
}
}
#endif

namespace RetailTestCommands
{
void registerRetailLuaTestCommands()
{
#if !RTS_GENERALS
	registerAction("lua-appearance", logAppearance);
	registerAction("lua-geometry", logGeometry);
	registerAction("lua-fear", logFear);
	registerAction("lua-blast-health", logBlastHealth);
#endif
}
}
