#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>
#include <cstdlib>
#include <string_view>

#include "Common/MessageStream.h"
#include "Common/ThingTemplate.h"
#include "GameClient/InGameUI.h"
#include "GameClient/Mouse.h"
#include "GameClient/View.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object.h"

namespace
{
void dragFormation(const RetailTestCommands::Context& ctx)
{
	GameMessage* move = TheMessageStream->appendMessage(GameMessage::MSG_DO_MOVETO_FORMATION);
	move->appendLocationArgument(ctx.groundLocation());
	move->appendRealArgument(ctx.angle);
	move->appendIntegerArgument(std::atoi(ctx.argument));
	move->appendBooleanArgument(FALSE);
}

void dragFormationState(const RetailTestCommands::Context& ctx)
{
	const Object* object = ctx.requireObject();
	if (!object)
		return;
	const AIUpdateInterface* ai = object->getAI();
	const Int idle = ai ? (ai->isIdle() ? 1 : 0) : -1;
	std::fprintf(stderr, "TEST drag-formation-state %u %s %d %.1f %.1f %.2f %d\n", ctx.frame,
		object->getTemplate()->getName().str(), ctx.ordinal, object->getPosition()->x, object->getPosition()->y,
		object->getOrientation(), idle);
}

void dragFormationInput(const RetailTestCommands::Context& ctx)
{
	const Coord3D anchor = ctx.groundLocation();
	ICoord2D point;
	if (!TheTacticalView->worldToScreen(&anchor, &point))
	{
		std::fprintf(stderr, "TEST drag-input %u offscreen\n", ctx.frame);
		return;
	}
	Coord3D ground;
	if (!TheTacticalView->screenToTerrain(&point, &ground))
	{
		std::fprintf(stderr, "TEST drag-input %u no-terrain\n", ctx.frame);
		return;
	}
	std::fprintf(stderr, "TEST drag-input %u %.1f %.1f\n", ctx.frame, ground.x, ground.y);
	TheMouse->setPosition(point.x, point.y);
	const Bool leftFirst = std::string_view(ctx.argument) == "left";
	const auto firstDown = leftFirst ? GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_DOWN : GameMessage::MSG_RAW_MOUSE_RIGHT_BUTTON_DOWN;
	const auto secondDown = leftFirst ? GameMessage::MSG_RAW_MOUSE_RIGHT_BUTTON_DOWN : GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_DOWN;
	const auto firstUp = leftFirst ? GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_UP : GameMessage::MSG_RAW_MOUSE_RIGHT_BUTTON_UP;
	const auto secondUp = leftFirst ? GameMessage::MSG_RAW_MOUSE_RIGHT_BUTTON_UP : GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_UP;
	Bool firstMotion = TRUE;
	for (const auto type : {firstDown, GameMessage::MSG_RAW_MOUSE_POSITION, secondDown,
		GameMessage::MSG_RAW_MOUSE_POSITION, firstUp, secondUp})
	{
		if (type == GameMessage::MSG_RAW_MOUSE_POSITION)
		{
			point.x += firstMotion ? 20 : 3;
			firstMotion = FALSE;
		}
		GameMessage* message = TheMessageStream->appendMessage(type);
		message->appendPixelArgument(point);
		message->appendIntegerArgument(0);
		message->appendIntegerArgument(TheMouse->getMouseStatus()->time);
	}
}

void dragFormationSelection(const RetailTestCommands::Context& ctx)
{
	std::fprintf(stderr, "TEST drag-selection %u %zu\n", ctx.frame, TheInGameUI->getAllSelectedDrawables()->size());
}

void dragFormationHold(const RetailTestCommands::Context& ctx)
{
	const Coord3D anchor = ctx.groundLocation();
	ICoord2D point;
	if (!TheTacticalView->worldToScreen(&anchor, &point))
		return;
	TheMouse->setPosition(point.x, point.y);
	const Bool leftFirst = std::string_view(ctx.argument) == "left";
	const auto firstDown = leftFirst ? GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_DOWN : GameMessage::MSG_RAW_MOUSE_RIGHT_BUTTON_DOWN;
	const auto secondDown = leftFirst ? GameMessage::MSG_RAW_MOUSE_RIGHT_BUTTON_DOWN : GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_DOWN;
	for (const auto type : {firstDown, secondDown, GameMessage::MSG_RAW_MOUSE_POSITION})
	{
		if (type == GameMessage::MSG_RAW_MOUSE_POSITION)
		{
			point.x += 140;
			point.y -= 20;
			TheMouse->setPosition(point.x, point.y);
		}
		GameMessage* message = TheMessageStream->appendMessage(type);
		message->appendPixelArgument(point);
		message->appendIntegerArgument(0);
		message->appendIntegerArgument(TheMouse->getMouseStatus()->time);
	}
}

void dragFormationRelease(const RetailTestCommands::Context& ctx)
{
	const Coord3D anchor = ctx.groundLocation();
	ICoord2D point;
	if (!TheTacticalView->worldToScreen(&anchor, &point))
		return;
	point.x += 140;
	point.y -= 20;
	const Bool leftFirst = std::string_view(ctx.argument) == "left";
	const auto firstUp = leftFirst ? GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_UP : GameMessage::MSG_RAW_MOUSE_RIGHT_BUTTON_UP;
	const auto secondUp = leftFirst ? GameMessage::MSG_RAW_MOUSE_RIGHT_BUTTON_UP : GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_UP;
	for (const auto type : {firstUp, secondUp})
	{
		GameMessage* message = TheMessageStream->appendMessage(type);
		message->appendPixelArgument(point);
		message->appendIntegerArgument(0);
		message->appendIntegerArgument(TheMouse->getMouseStatus()->time);
	}
}

void dragFormationPreview(const RetailTestCommands::Context& ctx)
{
	const InGameUI::FormationDragPreview& preview = TheInGameUI->getFormationDragPreview();
	std::fprintf(stderr, "TEST drag-preview %u %d %d %.1f\n", ctx.frame, preview.active ? 1 : 0,
		preview.templateIndex, preview.dragLength);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailDragFormationTestCommands()
{
#if !RTS_GENERALS
	registerAction("drag-formation", dragFormation);
	registerAction("drag-formation-state", dragFormationState);
	registerAction("drag-formation-input", dragFormationInput);
	registerAction("drag-formation-selection", dragFormationSelection);
	registerAction("drag-formation-hold", dragFormationHold);
	registerAction("drag-formation-release", dragFormationRelease);
	registerAction("drag-formation-preview", dragFormationPreview);
#endif
}
}
