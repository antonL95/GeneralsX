#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/AttributeModifierRules.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/RotwkParseOnlyModules.h"
#include "GameLogic/Object.h"

namespace
{
void probeStance(const RetailTestCommands::Context& ctx)
{
	Object* object = ctx.requireObject();
	if (!object)
		return;
	const StancesBehavior* stance = StancesBehavior::find(object);
	if (!stance)
		return;
	const Int index = stance->getStance();
	const auto* melee = stance->getMeleeBehavior(index);
	const char* name = AttributeModifierRules::stanceNames()[index];
	const std::vector<int> cycle = {AttributeModifierRules::STANCE_BATTLE,
		AttributeModifierRules::STANCE_AGGRESSIVE, AttributeModifierRules::STANCE_HOLD_GROUND};
	const Int image = AttributeModifierRules::stanceImageIndex(cycle, index);
	const Int next = AttributeModifierRules::nextStance(cycle, index);
	std::fprintf(stderr, "TEST stance-coverage %u %s %s %d %s\n",
		ctx.frame, name, melee ? melee->name.str() : "-", image, AttributeModifierRules::stanceNames()[next]);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailStancesMeleeTestCommands()
{
#if !RTS_GENERALS
	registerAction("stance-coverage", probeStance);
#endif
}
}
