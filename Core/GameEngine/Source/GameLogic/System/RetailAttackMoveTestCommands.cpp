#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>
#include <string>

#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/Object.h"

namespace
{
int healthPercent(const RetailTestCommands::Context& ctx, std::string_view spec)
{
	const Object* target = ctx.findTarget(spec, TRUE);
	const BodyModuleInterface* body = target ? target->getBodyModule() : nullptr;
	if (!body || body->getMaxHealth() <= 0.0f)
		return -1;
	return static_cast<int>(100.0f * body->getHealth() / body->getMaxHealth());
}

void probe(const RetailTestCommands::Context& ctx)
{
	const Object* unit = ctx.requireObject();
	const AIUpdateInterface* ai = unit ? unit->getAI() : nullptr;
	if (!ai)
		return;

	const std::string_view argument = ctx.argument;
	const size_t split = argument.find(',');
	const std::string_view mid = argument.substr(0, split);
	const std::string_view end = split == std::string_view::npos ? std::string_view() : argument.substr(split + 1);
	const Coord3D* position = unit->getPosition();
	std::fprintf(stderr, "TEST attack-move who=%s frame=%u x=%d y=%d attacking=%d state=%d goal=%d mid=%d end=%d\n", unit->getTemplate()->getName().str(), ctx.frame,
		static_cast<int>(position->x), static_cast<int>(position->y), ai->isAttacking(), static_cast<int>(ai->getCurrentStateID()), ai->getGoalPosition() ? static_cast<int>(ai->getGoalPosition()->x) : -1,
		healthPercent(ctx, mid), healthPercent(ctx, end));
}
}
#endif

namespace RetailTestCommands
{
void registerRetailAttackMoveTestCommands()
{
#if !RTS_GENERALS
	registerAction("attack-move-probe", probe);
#endif
}
}
