#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>
#include <string>
#include <vector>

#include "Common/ActionManager.h"
#include "Common/Player.h"
#include "Common/SpecialPower.h"
#include "GameClient/ControlBar.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Object.h"

namespace
{
void probeRequirementsFilter(const RetailTestCommands::Context& ctx)
{
	Object* book = ctx.requireObject();
	const SpecialPowerTemplate* power = TheSpecialPowerStore->findSpecialPowerTemplate(ctx.argument);
	const auto* module = book && power ? static_cast<const SpecialPowerModule*>(book->getSpecialPowerModule(power)) : nullptr;
	if (!module)
		return;
	const Bool storeOk = TheSpecialPowerStore->canUseSpecialPower(book, power);
	std::fprintf(stderr, "TEST req-filter frame=%u ready=%d paused=%d met=%d store=%d pct=%.3f\n",
		ctx.frame, module->isReady() ? 1 : 0, module->isPaused() ? 1 : 0,
		module->meetsRequirementsFilter() ? 1 : 0, storeOk ? 1 : 0, module->getPercentReady());
}

void probeRequirementsFilterBooks(const RetailTestCommands::Context& ctx)
{
	Object* anchor = ctx.requireObject();
	const SpecialPowerTemplate* power = TheSpecialPowerStore->findSpecialPowerTemplate(ctx.argument);
	Player* player = anchor ? anchor->getControllingPlayer() : nullptr;
	if (!player || !power)
		return;
	std::vector<Object*> books;
	player->collectRetailSpellBooks(books);
	std::string paused;
	for (const Object* book : books)
	{
		const auto* module = static_cast<const SpecialPowerModule*>(book->getSpecialPowerModule(power));
		if (!paused.empty())
			paused += ',';
		paused += module ? (module->isPaused() ? '1' : '0') : '-';
	}
	std::fprintf(stderr, "TEST req-filter-books frame=%u count=%u paused=%s\n",
		ctx.frame, static_cast<unsigned>(books.size()), paused.c_str());
}

void castRequirementsFilter(const RetailTestCommands::Context& ctx)
{
	Object* book = ctx.requireObject();
	const CommandButton* button = TheRetailCommandCatalog ? TheRetailCommandCatalog->findCommandButton(ctx.argument) : nullptr;
	const SpecialPowerTemplate* power = button ? button->getSpecialPowerTemplate() : nullptr;
	if (!book || !power || !ctx.hasPosition)
		return;
	const Coord3D point = ctx.groundLocation();
	const UnsignedInt options = button->getOptions();
	const Bool allowed = button->canCastRetailSpell(book, nullptr, &point) &&
		TheActionManager->canDoSpecialPowerAtLocation(book, &point, CMD_FROM_PLAYER, power, nullptr, options);
	auto* module = static_cast<SpecialPowerModule*>(book->getSpecialPowerModule(power));
	if (module)
		module->resetLastCastAccepted();
	if (allowed)
		book->doSpecialPowerAtLocation(power, &point, INVALID_ANGLE, options, FALSE);
	else if (module)
		module->doSpecialPowerAtLocation(&point, INVALID_ANGLE, options);
	if (!allowed && ctx.frame == 388)
		std::fprintf(stderr, "TEST req-filter-diag frame=%u spell=%d act=%d ready=%d paused=%d met=%d\n",
			ctx.frame, button->canCastRetailSpell(book, nullptr, &point) ? 1 : 0,
			TheActionManager->canDoSpecialPowerAtLocation(book, &point, CMD_FROM_PLAYER, power, nullptr, options) ? 1 : 0,
			module && module->isReady() ? 1 : 0, module && module->isPaused() ? 1 : 0,
			module && module->meetsRequirementsFilter() ? 1 : 0);
	std::fprintf(stderr, "TEST req-filter-cast frame=%u allowed=%d accepted=%d\n",
		ctx.frame, allowed ? 1 : 0, module && module->wasLastCastAccepted() ? 1 : 0);
}

void setUnderConstruction(const RetailTestCommands::Context& ctx)
{
	Object* obj = ctx.requireObject();
	if (!obj)
		return;
	if (ctx.argument && std::atoi(ctx.argument) != 0)
		obj->setStatus(MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_UNDER_CONSTRUCTION));
	else
		obj->clearStatus(MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_UNDER_CONSTRUCTION));
	std::fprintf(stderr, "TEST req-filter-build frame=%u uc=%d\n",
		ctx.frame, obj->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION) ? 1 : 0);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailRequirementsFilterTestCommands()
{
#if !RTS_GENERALS
	registerAction("req-filter-probe", probeRequirementsFilter);
	registerAction("req-filter-books", probeRequirementsFilterBooks);
	registerAction("req-filter-cast", castRequirementsFilter);
	registerAction("req-filter-uc", setUnderConstruction);
#endif
}
}
