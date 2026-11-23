#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if RTS_ZEROHOUR
#include <cstdio>

#include "Common/MessageStream.h"
#include "GameClient/ControlBar.h"
#include "GameClient/InGameUI.h"
#include "GameClient/Module/RadarMarkerClientUpdate.h"
#include "GameClient/Mouse.h"
#include "GameClient/View.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"

namespace
{
void handleCursorFeedback(const RetailTestCommands::Context& ctx)
{
	if (ctx.action == "cursor-arm")
	{
		const AsciiString name(ctx.argument);
		const CommandButton* button = TheRetailCommandCatalog->findCommandButton(name);
		if (button && ctx.source)
			ctx.selectObject(ctx.source);
		TheInGameUI->setGUICommand(button);
		std::fprintf(stderr, "TEST cursor arm frame=%u button=%s found=%d\n", ctx.frame, name.str(), button != nullptr);
	}
	else if (ctx.action == "cursor-point" || ctx.action == "cursor-screen")
	{
		Coord3D position;
		ICoord2D pixel;
		if (ctx.action == "cursor-screen")
		{
			pixel = {static_cast<Int>(ctx.position.x), static_cast<Int>(ctx.position.y)};
			if (!TheTacticalView->screenToTerrain(&pixel, &position))
				return;
		}
		else
		{
			position = ctx.source ? *ctx.source->getPosition() : ctx.groundLocation();
			if (!TheTacticalView->worldToScreen(&position, &pixel))
				return;
		}
		TheMouse->setPosition(pixel.x, pixel.y);
		GameMessage* message = TheMessageStream->appendMessage(GameMessage::MSG_RAW_MOUSE_POSITION);
		message->appendPixelArgument(pixel);
		message->appendIntegerArgument(0);
		std::fprintf(stderr, "TEST cursor point frame=%u pixel=%d,%d world=%.1f,%.1f water=%d cliff=%d\n", ctx.frame,
			pixel.x, pixel.y, position.x, position.y, TheTerrainLogic->isUnderwater(position.x, position.y),
			TheTerrainLogic->isCliffCell(position.x, position.y));
	}
	else if (ctx.action == "cursor-cancel")
	{
		const MouseIO* mouse = TheMouse->getMouseStatus();
		for (const auto type : {GameMessage::MSG_RAW_MOUSE_RIGHT_BUTTON_DOWN, GameMessage::MSG_RAW_MOUSE_RIGHT_BUTTON_UP})
		{
			GameMessage* message = TheMessageStream->appendMessage(type);
			message->appendPixelArgument(mouse->pos);
			message->appendIntegerArgument(0);
			message->appendIntegerArgument(mouse->time);
		}
	}
	else if (ctx.action == "cursor-template")
	{
		const AsciiString name(ctx.argument);
		const CommandButton* button = TheRetailCommandCatalog->findCommandButton(name);
		const Bool defined = button && RadiusDecalTemplate::findNamedCursor(button->getRadiusCursorType());
		const Bool targeted = button && BitIsSet(button->getOptions(), COMMAND_OPTION_NEED_TARGET) &&
			!button->isRetailPassive();
		std::fprintf(stderr, "TEST cursor template frame=%u button=%s found=%d template=%d targeted=%d\n",
			ctx.frame, name.str(), button != nullptr, defined, targeted);
	}
	else if (ctx.action == "cursor-radar")
	{
		Int count = 0;
		for (const auto* marker : RadarMarkerClientUpdate::activeMarkers())
		{
			if (marker->getMarkerPosition())
			{
				++count;
				std::fprintf(stderr, "TEST cursor marker frame=%u export=%s\n", ctx.frame, marker->getMarkerType().str());
			}
		}
		std::fprintf(stderr, "TEST cursor radar frame=%u markers=%d\n", ctx.frame, count);
	}
	else if (ctx.action == "cursor-name")
	{
		if (Object* object = ctx.requireObject())
			std::fprintf(stderr, "TEST cursor name frame=%u label=%ls\n", ctx.frame, object->getDisplayName().str());
	}
	else if (ctx.action == "cursor-selected")
	{
		const CommandButton* button = TheInGameUI->getGUICommand();
		const Int actual = TheMouse->getMouseCursor();
		const Int expected = TheMouse->getCursorIndex(AsciiString(ctx.argument));
		std::fprintf(stderr, "TEST cursor selected frame=%u button=%s expected=%s actual=%d matched=%d\n",
			ctx.frame, button ? button->getName().str() : "-", ctx.argument, actual,
			expected != Mouse::INVALID_MOUSE_CURSOR && actual == expected);
	}
	else if (ctx.action == "cursor-status")
	{
		const CommandButton* button = TheInGameUI->getGUICommand();
		Coord3D position;
		Int valid = -1;
		if (button && TheTacticalView->screenToTerrain(&TheMouse->getMouseStatus()->pos, &position))
			valid = button->getCommandType() == GUI_COMMAND_SPELL_BOOK
				? TheControlBar->targetRetailSpell(button, nullptr, &position, FALSE)
				: TheInGameUI->canSelectedObjectsDoSpecialPower(button, nullptr, &position,
					InGameUI::SELECTION_ANY, button->getOptions(), nullptr);
		std::fprintf(stderr, "TEST cursor status frame=%u pending=%d mouse=%d control=%d valid=%d\n",
			ctx.frame, button != nullptr, TheMouse->getMouseCursor(), TheInGameUI->areSelectedObjectsControllable(), valid);
	}
}
}
#endif

namespace RetailTestCommands
{
void registerRetailCursorFeedbackTestCommands()
{
#if RTS_ZEROHOUR
	registerPrefix("cursor-", handleCursorFeedback);
#endif
}
}
