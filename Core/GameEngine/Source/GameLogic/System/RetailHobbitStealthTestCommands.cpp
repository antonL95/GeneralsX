#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/ThingTemplate.h"
#include "GameClient/Drawable.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/RetailDetectionModules.h"
#include "GameLogic/Object.h"

namespace
{
void probeHobbitStealth(const RetailTestCommands::Context& ctx)
{
	const Object* object = ctx.requireObject();
	if (!object)
		return;

	const auto& conditions = object->getLogicModelConditions();
	const auto* draw = object->getDrawable();
	const auto* ai = object->getAI();
	const auto* position = object->getPosition();
	const char* name = object->getTemplate()->getName().str();
	std::fprintf(stderr,
		"TEST hobbit-stealth %u %s moving=%d trees=%d legacy=%d stealth=%d detected=%d hidden=%d can=%d stealth-model=%d camo-model=%d hidden-model=%d\n",
		ctx.frame, name, ai && ai->isMoving(), RetailDetection::nearTrees(object), object->getStealth() != nullptr,
		object->testStatus(OBJECT_STATUS_STEALTHED), object->testStatus(OBJECT_STATUS_DETECTED),
		object->testStatus(OBJECT_STATUS_HIDDEN), object->testStatus(OBJECT_STATUS_CAN_STEALTH),
		conditions.test(MODELCONDITION_INVISIBLE_STEALTH), conditions.test(MODELCONDITION_INVISIBLE_CAMOUFLAGE),
		conditions.test(MODELCONDITION_HIDDEN));
	if (draw)
	{
		const auto& displayed = draw->getModelConditionFlags();
		std::fprintf(stderr,
			"TEST hobbit-stealth-draw %u %s x=%.2f y=%.2f look=%d opacity=%.3f second-pass=%.3f stealth-model=%d camo-model=%d hidden-model=%d\n",
			ctx.frame, name, position->x, position->y, static_cast<Int>(draw->getStealthLook()),
			draw->getEffectiveOpacity(), draw->getSecondMaterialPassOpacity(), displayed.test(MODELCONDITION_INVISIBLE_STEALTH),
			displayed.test(MODELCONDITION_INVISIBLE_CAMOUFLAGE), displayed.test(MODELCONDITION_HIDDEN));
	}
	for (BehaviorModule** module = object->getBehaviorModules(); *module; ++module)
	{
		const NameKeyType type = (*module)->getModuleNameKey();
		const NameKeyType tag = (*module)->getModuleTagNameKey();
		if (type != NAMEKEY("InvisibilityUpdate"))
			continue;
		const auto& invisibility = *static_cast<InvisibilityUpdate*>(*module);
		const auto& nugget = invisibility.getNugget();
		std::fprintf(stderr, "TEST hobbit-stealth-source %u %s %s type=%d enabled=%d eligible=%d\n",
			ctx.frame, name, TheNameKeyGenerator->keyToName(tag).str(), nugget.m_type,
			invisibility.isEnabled(), nugget.canConceal(object));
	}
}
}
#endif

namespace RetailTestCommands
{
void registerRetailHobbitStealthTestCommands()
{
#if !RTS_GENERALS
	registerAction("hobbit-stealth", probeHobbitStealth);
#endif
}
}
