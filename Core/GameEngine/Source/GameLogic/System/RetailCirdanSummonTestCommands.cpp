#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/KindOf.h"
#include "Common/Player.h"
#include "Common/ThingTemplate.h"
#include "GameClient/Drawable.h"
#include "GameClient/RetailHudExtras.h"
#include "GameLogic/Object.h"

namespace
{
Object* findPlayerTemplateObject(const Player* player, const char* name)
{
	if (!player || !name || !name[0])
		return nullptr;
	struct Query
	{
		const char* name;
		Object* found;
	};
	Query query{ name, nullptr };
	player->iterateObjects([](Object* obj, void* data)
	{
		Query& q = *static_cast<Query*>(data);
		if (q.found || !obj || obj->isEffectivelyDead())
			return;
		if (obj->getTemplate() && obj->getTemplate()->getName().compareNoCase(q.name) == 0)
			q.found = obj;
	}, &query);
	return query.found;
}

void cirdanStateCommand(const RetailTestCommands::Context& ctx)
{
	Object* obj = ctx.findObject(true);
	const Bool alive = obj != nullptr;
	const Bool inRoster = alive && RetailHeroBar::hasHeroSlot(obj->getID());
	const Bool ignoresSelectAll = alive && obj->isKindOf(KINDOF_IGNORES_SELECT_ALL);
	std::fprintf(stderr, "TEST cirdan-state frame=%u alive=%d roster=%d ignoresSelectAll=%d\n",
		ctx.frame, alive ? 1 : 0, inRoster ? 1 : 0, ignoresSelectAll ? 1 : 0);
}

void cirdanSelectAllHeroesCommand(const RetailTestCommands::Context& ctx)
{
	const Bool ran = RetailHeroBar::selectAllHeroes();
	std::fprintf(stderr, "TEST cirdan-select-all-heroes frame=%u ran=%d\n", ctx.frame, ran ? 1 : 0);
}

void cirdanRosterSelectCommand(const RetailTestCommands::Context& ctx)
{
	Object* obj = ctx.findObject(true);
	const Bool ran = obj && RetailHeroBar::selectHeroSlot(obj->getID());
	std::fprintf(stderr, "TEST cirdan-roster-select frame=%u ran=%d\n", ctx.frame, ran ? 1 : 0);
}

void cirdanSelectionCommand(const RetailTestCommands::Context& ctx)
{
	const Object* cirdan = ctx.findObject(true);
	const Object* control = findPlayerTemplateObject(ctx.findPlayer(), ctx.argument);
	const Drawable* cirdanDraw = cirdan ? cirdan->getDrawable() : nullptr;
	const Drawable* controlDraw = control ? control->getDrawable() : nullptr;
	std::fprintf(stderr, "TEST cirdan-selection frame=%u cirdan=%d control=%d\n",
		ctx.frame,
		cirdanDraw && cirdanDraw->isSelected() ? 1 : 0,
		controlDraw && controlDraw->isSelected() ? 1 : 0);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailCirdanSummonTestCommands()
{
#if !RTS_GENERALS
	registerAction("cirdan-state", cirdanStateCommand);
	registerAction("cirdan-select-all-heroes", cirdanSelectAllHeroesCommand);
	registerAction("cirdan-roster-select", cirdanRosterSelectCommand);
	registerAction("cirdan-selection", cirdanSelectionCommand);
#endif
}
}
