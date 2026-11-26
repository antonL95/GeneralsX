#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "GameLogic/Module/AODCrushCollide.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Object.h"

namespace
{
void scan(const RetailTestCommands::Context& ctx)
{
	Object* source = ctx.requireObject();
	if (!source)
		return;
	auto* warning = source->findUpdateModule(NAMEKEY("NotifyTargetsOfImminentProbableCrushingUpdate"));
	auto* physics = source->getPhysics();
	if (!warning || !physics)
		return;

	const Coord3D forward{5.0f, 0.0f, 0.0f};
	const Coord3D backward{-5.0f, 0.0f, 0.0f};
	physics->addVelocityTo(&forward);
	const auto delay = warning->update();
	physics->addVelocityTo(&backward);
	std::fprintf(stderr, "TEST crush-knockback scan %u %d\n", ctx.frame, delay);
}

void brace(const RetailTestCommands::Context& ctx)
{
	if (const Object* victim = ctx.requireObject())
		std::fprintf(stderr, "TEST crush-knockback brace %u %s %d\n", ctx.frame, ctx.argument,
			victim->getLogicModelConditions().test(MODELCONDITION_EMOTION_BRACE_FOR_BEING_CRUSHED));
}

void collide(const RetailTestCommands::Context& ctx)
{
	Object* source = ctx.requireObject();
	Object* victim = ctx.findTarget();
	if (!source || !victim)
		return;
	const NameKeyType key = NAMEKEY("AODCrushCollide");
	for (BehaviorModule** module = source->getBehaviorModules(); *module; ++module)
	{
		if ((*module)->getModuleNameKey() == key)
		{
			static_cast<AODCrushCollide*>(*module)->onCollide(victim, victim->getPosition(), nullptr);
			return;
		}
	}
}

void marker(const RetailTestCommands::Context& ctx)
{
	std::fprintf(stderr, "TEST crush-knockback marker %u %s %d\n", ctx.frame, ctx.targetName, ctx.findObject() != nullptr);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailCrushKnockbackTestCommands()
{
#if !RTS_GENERALS
	registerAction("crush-knockback-scan", scan);
	registerAction("crush-knockback-brace", brace);
	registerAction("crush-knockback-contact", collide);
	registerAction("crush-knockback-marker", marker);
#endif
}
}
