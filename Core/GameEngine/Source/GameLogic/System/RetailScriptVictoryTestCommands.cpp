#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if RTS_ZEROHOUR
#include <cstdio>

#include "Common/GlobalData.h"
#include "Common/NameKeyGenerator.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Scripts.h"

namespace
{
void showVictory(const RetailTestCommands::Context &ctx)
{
	const Int type = ScriptEngine::findRotwkActionByRetailIndex(511);
	if (type >= 0)
	{
		auto *action = newInstance(ScriptAction)(static_cast<ScriptAction::ScriptActionType>(type));
		TheScriptEngine->friend_executeAction(action);
		deleteInstance(action);
	}
	GameWindow *notification = TheWindowManager->winGetWindowFromId(nullptr, NAMEKEY("ControlBar.wnd:Notification"));
	const Bool shown = !TheGlobalData->m_headless && notification &&
		!notification->winIsHidden() && !notification->winGetParent();
	std::fprintf(stderr, "TEST script-victory frame=%u handled=%d shown=%d ending=%d\n",
		ctx.frame, type >= 0, shown, TheScriptEngine->isGameEnding());
}
}
#endif

namespace RetailTestCommands
{
void registerRetailScriptVictoryTestCommands()
{
#if RTS_ZEROHOUR
	registerAction("script-victory-screen", showVictory);
#endif
}
}
