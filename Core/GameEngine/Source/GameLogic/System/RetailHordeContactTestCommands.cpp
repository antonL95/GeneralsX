#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "GameLogic/Module/HordeAIUpdate.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Module/RotwkParseOnlyModules.h"
#include "GameLogic/Object.h"

namespace
{
void hordeContact(const RetailTestCommands::Context& ctx)
{
	Object* source = ctx.requireObject(true);
	const HordeContain* horde = HordeContain::get(source);
	auto* ai = source ? dynamic_cast<HordeAIUpdate*>(source->getAI()) : nullptr;
	if (!horde || !ai)
		return;
	if (ctx.action != "horde-contact-state")
	{
		Object* target = ctx.findTarget();
		if (!target)
			return;
		const HordeContain* targetHorde = HordeContain::get(target);
		if (ctx.action == "horde-contact-member" && targetHorde)
			target = targetHorde->findNearestLivingMember(*source->getPosition());
		if (!target)
			return;

		Object* member = horde->findNearestLivingMember(*target->getPosition());
		for (auto** module = member ? member->getBehaviorModules() : nullptr; module && *module; ++module)
		{
			if (auto* collide = dynamic_cast<HordeMemberCollide*>(*module))
			{
				collide->onCollide(target, nullptr, nullptr);
				break;
			}
		}
	}
	std::fprintf(stderr, "TEST horde-contact %u active=%d\n", ctx.frame, ai->hasRecentContact());
}
}
#endif

namespace RetailTestCommands
{
void registerRetailHordeContactTestCommands()
{
#if !RTS_GENERALS
	registerAction("horde-contact-member", hordeContact);
	registerAction("horde-contact-anchor", hordeContact);
	registerAction("horde-contact-state", hordeContact);
#endif
}
}
