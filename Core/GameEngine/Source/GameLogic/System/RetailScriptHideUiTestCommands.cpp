#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if RTS_ZEROHOUR
#include <cstdio>

#include "Common/GlobalData.h"
#include "Common/NameKeyGenerator.h"
#include "GameClient/Display.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/InGameUI.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Scripts.h"

namespace
{
void check(const RetailTestCommands::Context &ctx)
{
	GameWindow *bar = TheWindowManager->winGetWindowFromId(nullptr, NAMEKEY("ControlBar.wnd:ControlBarParent"));
	const Bool hidden = TheInGameUI->isScriptUiHidden();
	const Bool matches = TheGlobalData->m_headless || (bar && bar->winIsHidden() == hidden);
	std::fprintf(stderr, "TEST script-hide-ui frame=%u script=%d matches=%d letterbox=%d\n",
		ctx.frame, hidden, matches, TheDisplay->isLetterBoxed());
}

void toggle(const RetailTestCommands::Context &ctx)
{
	const auto type = ctx.action == "script-hide-ui" ? ScriptAction::HIDE_UI : ScriptAction::SHOW_UI;
	auto *action = newInstance(ScriptAction)(type);
	TheScriptEngine->friend_executeAction(action);
	deleteInstance(action);
	check(ctx);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailScriptHideUiTestCommands()
{
#if RTS_ZEROHOUR
	registerAction("script-hide-ui", toggle);
	registerAction("script-show-ui", toggle);
	registerAction("script-hide-ui-check", check);
#endif
}
}
