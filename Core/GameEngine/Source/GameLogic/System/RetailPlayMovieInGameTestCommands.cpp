#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if RTS_ZEROHOUR
#include <cstdio>

#include "GameClient/Display.h"
#include "GameClient/VideoPlayer.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Scripts.h"

namespace
{
void checkPlayMovieInGame(const RetailTestCommands::Context &ctx)
{
	const Int type = ScriptEngine::findRotwkActionByRetailIndex(368);
	const Bool registered = type >= 0;

	VideoStreamInterface *stream = TheVideoPlayer ? TheVideoPlayer->open("LoadingRing") : nullptr;
	const Bool streamOk = stream != nullptr && stream->width() > 0 && stream->height() > 0;
	if (stream)
		stream->close();

	Bool executed = FALSE;
	if (registered)
	{
		auto *action = newInstance(ScriptAction)(static_cast<ScriptAction::ScriptActionType>(type));
		action->getParameter(0)->friend_setString("LoadingRing");
		action->getParameter(1)->friend_setInt(0);
		TheScriptEngine->friend_executeAction(action);
		action->getParameter(1)->friend_setInt(1);
		TheScriptEngine->friend_executeAction(action);
		deleteInstance(action);
		if (TheDisplay)
			TheDisplay->stopMovie();
		executed = TRUE;
	}

	std::fprintf(stderr, "TEST play-movie-in-game frame=%u registered=%d stream=%d executed=%d\n",
		ctx.frame, registered, streamOk, executed);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailPlayMovieInGameTestCommands()
{
#if RTS_ZEROHOUR
	registerAction("play-movie-in-game-check", checkPlayMovieInGame);
#endif
}
}
