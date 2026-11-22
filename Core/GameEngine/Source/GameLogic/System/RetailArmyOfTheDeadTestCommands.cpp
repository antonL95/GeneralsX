#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <algorithm>
#include <cstdio>
#include <vector>

#include "Common/ThingTemplate.h"
#include "GameClient/Drawable.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/DeletionUpdate.h"
#include "GameLogic/Object.h"

namespace
{
void traceArmy(const RetailTestCommands::Context& ctx)
{
	const auto* owner = ctx.findPlayer();
	std::vector<Object*> army;
	for (Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
	{
		if (object->getControllingPlayer() != owner)
			continue;
		const AsciiString& name = object->getTemplate()->getName();
		if (name == "ElessarOathbreakerFlood" || name == "FloodKotD" ||
			name == "FloodOathbreaker" || name == "FloodOathbreakerCOM")
			army.push_back(object);
	}
	std::sort(army.begin(), army.end(), [](const Object* a, const Object* b) {
		return a->getID() < b->getID();
	});
	std::fprintf(stderr, "TEST army-of-the-dead %u count %zu\n", ctx.frame, army.size());
	for (Object* object : army)
	{
		const auto* drawable = object->getDrawable();
		auto* deletion = static_cast<DeletionUpdate*>(object->findUpdateModule(NAMEKEY("DeletionUpdate")));
		const Coord3D& pos = *object->getPosition();
		const UnsignedInt remaining = deletion && deletion->getDieFrame() > ctx.frame
			? deletion->getDieFrame() - ctx.frame : 0;
		std::fprintf(stderr, "TEST army-of-the-dead %u %s %u %.3f %.3f %.3f %.6f %.6f %d %d %u\n",
			ctx.frame, object->getTemplate()->getName().str(), static_cast<UnsignedInt>(object->getID()),
			pos.x - ctx.position.x, pos.y - ctx.position.y, pos.z, object->getOrientation(),
			drawable ? drawable->getEffectiveOpacity() : -1.0f, object->testStatus(OBJECT_STATUS_STEALTHED),
			object->isEffectivelyDead(), remaining);
	}
}
}
#endif

namespace RetailTestCommands
{
void registerRetailArmyOfTheDeadTestCommands()
{
#if !RTS_GENERALS
	registerAction("army-of-the-dead-trace", traceArmy);
#endif
}
}
