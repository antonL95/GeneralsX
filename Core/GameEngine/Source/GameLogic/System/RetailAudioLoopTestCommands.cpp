#include "PreRTS.h"

#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include "Common/AudioEventRTS.h"
#include "Common/GameAudio.h"
#include "Common/MessageStream.h"
#include "GameClient/CommandXlat.h"
#include "GameClient/Drawable.h"
#include "GameClient/InGameUI.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"

#include <cstdlib>
#include <string_view>

namespace
{
void handleAudioLoop(const RetailTestCommands::Context& ctx)
{
	const std::string_view cmd = ctx.action.substr(sizeof("audio-loop-") - 1);
	if (cmd == "trace")
	{
		if (!TheAudio)
			return;
		const std::string_view arg(ctx.argument);
		if (arg == "on")
		{
			TheAudio->setTraceEnabled(TRUE);
		}
		else if (arg == "virtual")
		{
			TheAudio->setTraceEnabled(TRUE);
			TheAudio->setHeadlessVirtualPlayback(TRUE);
		}
		else if (arg == "off")
		{
			TheAudio->setTraceEnabled(FALSE);
			TheAudio->setHeadlessVirtualPlayback(FALSE);
		}
		return;
	}
	if (cmd == "status")
	{
		if (TheAudio)
			TheAudio->dumpAudioTraceStatus();
		return;
	}
	if (cmd == "music")
	{
		if (!TheAudio)
			return;
		AudioEventRTS event{AsciiString(ctx.argument)};
		TheAudio->addAudioEvent(&event);
		return;
	}
	if (cmd == "advance-music")
	{
		if (TheAudio)
			TheAudio->advanceVirtualMusic(static_cast<Real>(std::atof(ctx.argument)));
		return;
	}
	if (cmd == "select")
	{
		Object* unit = ctx.requireObject(true);
		if (!unit || !unit->getDrawable() || !TheInGameUI || !TheMessageStream)
			return;
		TheInGameUI->deselectAllDrawables();
		TheInGameUI->selectDrawable(unit->getDrawable());
		GameMessage* msg = TheMessageStream->appendMessage(GameMessage::MSG_CREATE_SELECTED_GROUP);
		msg->appendBooleanArgument(TRUE);
		msg->appendObjectIDArgument(unit->getID());
		msg->appendBooleanArgument(TRUE);
		return;
	}
	if (cmd == "move")
	{
		Object* unit = ctx.requireObject(true);
		if (!unit || !unit->getDrawable() || !TheInGameUI || !TheMessageStream)
			return;
		if (!unit->getDrawable()->isSelected())
		{
			TheInGameUI->deselectAllDrawables();
			TheInGameUI->selectDrawable(unit->getDrawable());
		}
		const Coord3D dest = ctx.groundLocation();
		GameMessage* moveMsg = TheMessageStream->appendMessage(GameMessage::MSG_DO_MOVETO);
		moveMsg->appendLocationArgument(dest);
		PickAndPlayInfo info;
		pickAndPlayUnitVoiceResponse(TheInGameUI->getAllSelectedDrawables(), GameMessage::MSG_DO_MOVETO, &info);
		return;
	}
	if (cmd == "attack")
	{
		Object* unit = ctx.requireObject(true);
		if (!unit || !unit->getDrawable())
			return;
		DrawableList units{unit->getDrawable()};
		PickAndPlayInfo info;
		if (Object* target = ctx.findTarget(true); target && target->getDrawable())
			info.m_drawTarget = target->getDrawable();
		pickAndPlayUnitVoiceResponse(&units, GameMessage::MSG_DO_ATTACK_OBJECT, &info);
		return;
	}
	if (cmd == "condition")
	{
		Object* unit = ctx.requireObject(true);
		if (!unit)
			return;
		const std::string_view arg(ctx.argument);
		if (arg == "raising-on")
			unit->setModelConditionState(MODELCONDITION_RAISING_FLAG);
		else if (arg == "raising-off")
			unit->clearModelConditionState(MODELCONDITION_RAISING_FLAG);
		return;
	}
}
}
#endif

namespace RetailTestCommands
{
void registerRetailAudioLoopTestCommands()
{
#if !RTS_GENERALS
	registerPrefix("audio-loop-", handleAudioLoop);
#endif
}
}
