#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <algorithm>
#include <cstdio>

#include "Common/MessageStream.h"
#include "Common/SpecialPower.h"
#include "Common/ThingTemplate.h"
#include "GameClient/ControlBar.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AttributeModifierHelper.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/DeletionUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"

namespace
{
void powerGapCast(const RetailTestCommands::Context& ctx)
{
	Object* book = ctx.requireObject();
	const CommandSet* set = book ? TheControlBar->findCommandSet(book->getCommandSetString()) : nullptr;
	const CommandButton* button = nullptr;
	for (Int slot = 0; set && !button && slot < CommandButtonRules::RETAIL_COMMANDS_PER_SET; ++slot)
	{
		const CommandButton* candidate = set->getCommandButton(slot);
		if (candidate && candidate->getName().compare(ctx.argument) == 0)
			button = candidate;
	}
	const SpecialPowerTemplate* power = button ? button->getSpecialPowerTemplate() : nullptr;
	if (!power || !ctx.hasPosition)
	{
		std::fprintf(stderr, "TEST powergap-cast frame=%u button=%s issued=0\n", ctx.frame, ctx.argument);
		return;
	}
	GameMessage* message = TheMessageStream->appendMessage(GameMessage::MSG_DO_SPECIAL_POWER_AT_LOCATION);
	message->appendIntegerArgument(power->getID());
	message->appendLocationArgument(ctx.groundLocation());
	message->appendRealArgument(INVALID_ANGLE);
	message->appendObjectIDArgument(INVALID_ID);
	message->appendIntegerArgument(button->getOptions());
	message->appendObjectIDArgument(book->getID());
	std::fprintf(stderr, "TEST powergap-cast frame=%u button=%s issued=1\n", ctx.frame, ctx.argument);
}

void powerGapUnit(const RetailTestCommands::Context& ctx)
{
	const Object* unit = ctx.requireObject(false);
	if (!unit)
		return;
	const auto* helper = unit->getAttributeModifierHelper();
	const Int list = AttributeModifiers::findList(AsciiString(ctx.argument));
	const Bool active = helper && list >= 0 && std::any_of(helper->getStack().active().begin(),
		helper->getStack().active().end(), [list](const auto& entry) { return entry.list == list; });
	const Coord3D& position = *unit->getPosition();
	const Object* patch = TheGameLogic->findObjectByID(TheTerrainLogic->retailTaintOwner(position.x, position.y));
	std::fprintf(stderr, "TEST powergap-unit frame=%u unit=%s modifier=%d taint=%s hurt=%d\n", ctx.frame,
		unit->getTemplate()->getName().str(), active, patch ? patch->getTemplate()->getName().str() : "-",
		unit->getBodyModule()->getHealth() < unit->getBodyModule()->getMaxHealth());
}

void powerGapObjects(const RetailTestCommands::Context& ctx)
{
	Int count = 0;
	UnsignedInt lifetime = 0;
	for (Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
	{
		if (object->isDestroyed() || object->getTemplate()->getName().compare(ctx.argument) != 0)
			continue;
		++count;
		auto* deletion = static_cast<DeletionUpdate*>(object->findUpdateModule(NAMEKEY("DeletionUpdate")));
		if (deletion && deletion->getDieFrame() > ctx.frame)
			lifetime = std::max(lifetime, deletion->getDieFrame() - ctx.frame);
	}
	std::fprintf(stderr, "TEST powergap-objects frame=%u template=%s count=%d remaining=%u\n",
		ctx.frame, ctx.argument, count, lifetime);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailPowerGapTestCommands()
{
#if !RTS_GENERALS
	registerAction("powergap-cast", powerGapCast);
	registerAction("powergap-unit", powerGapUnit);
	registerAction("powergap-objects", powerGapObjects);
#endif
}
}
