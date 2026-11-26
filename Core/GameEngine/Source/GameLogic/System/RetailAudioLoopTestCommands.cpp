#include "PreRTS.h"

#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include "Common/AudioEventInfo.h"
#include "Common/AudioEventRTS.h"
#include "Common/FileSystem.h"
#include "Common/GameAudio.h"
#include "Common/MessageStream.h"
#include "GameClient/CommandXlat.h"
#include "GameClient/Drawable.h"
#include "GameClient/GameClient.h"
#include "GameClient/InGameUI.h"
#include "GameClient/RetailLargeGroupAudio.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string_view>
#include <vector>

namespace
{
void traceAudioRequestFiles(const char* name)
{
	AudioEventRTS event{AsciiString(name)};
	TheAudio->getInfoForAudioEvent(&event);
	const AudioEventInfo* info = event.getAudioEventInfo();
	if (!info)
		return;
	for (Int i = 0; i < static_cast<Int>(info->m_sounds.size()); ++i)
	{
		event.setPlayingAudioIndex(i - 1);
		event.generateFilename(FALSE);
		const AsciiString filename = event.getFilename();
		std::fprintf(stderr, "AUDIO REQUEST %s %s %d\n", name, filename.str(),
			TheFileSystem->doesFileExist(filename.str()) ? 1 : 0);
	}
}

void traceAudioPicks(const char* name)
{
	AudioEventRTS event{AsciiString(name)};
	TheAudio->getInfoForAudioEvent(&event);
	const AudioEventInfo* info = event.getAudioEventInfo();
	for (Int i = 0; i < 64; ++i)
	{
		if (info)
		{
			event.setPlayingAudioIndex(-1);
			event.generateFilename();
			std::fprintf(stderr, "AUDIO PICK %s file %s\n", name, event.getFilename().str());
		}
		else
		{
			std::vector<AsciiString> children;
			Bool loop = FALSE;
			if (RetailLargeGroupAudio::expandMultiSound(name, children, loop) && children.size() == 1)
				std::fprintf(stderr, "AUDIO PICK %s expand %s\n", name, children.front().str());
		}
	}
	if (!info)
	{
		for (Int i = 0; i < 64; ++i)
		{
			const AsciiString track = RetailLargeGroupAudio::chooseMultiSoundTrack(name);
			std::fprintf(stderr, "AUDIO PICK %s track %s\n", name, track.str());
		}
	}
}

void handleAudioLoop(const RetailTestCommands::Context& ctx)
{
	const std::string_view cmd = ctx.action.substr(sizeof("audio-loop-") - 1);
	if (cmd == "request-file")
	{
		if (TheAudio && TheFileSystem)
			traceAudioRequestFiles(ctx.argument);
		return;
	}
	if (cmd == "picks")
	{
		if (TheAudio)
			traceAudioPicks(ctx.argument);
		return;
	}
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
	if (cmd == "emitters")
	{
		if (!TheGameClient)
			return;
		for (Drawable* draw = TheGameClient->firstDrawable(); draw; draw = draw->getNextDrawable())
		{
			const AudioEventRTS* ambient = draw->getAmbientSound();
			if (!ambient || !std::strstr(ambient->getEventName().str(), ctx.argument))
				continue;
			const Coord3D* pos = draw->getPosition();
			std::fprintf(stderr, "AUDIO EMITTER event=%s x=%.0f y=%.0f playing=%d\n", ambient->getEventName().str(),
				pos->x, pos->y, ambient->isCurrentlyPlaying() ? 1 : 0);
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
