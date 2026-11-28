#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if RTS_ZEROHOUR
#include <cstdio>

#include "Common/GlobalData.h"
#include "Common/NameKeyGenerator.h"
#include "Common/Player.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/InGameUI.h"
#include "GameClient/Mouse.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Scripts.h"

namespace
{
void showVictory(const RetailTestCommands::Context &ctx)
{
	const Bool hadInput = TheInGameUI && TheInGameUI->getInputEnabled();
	Object *selected = nullptr;
	if (ctx.player)
	{
		ctx.player->iterateObjects([](Object *obj, void *userData) {
			auto **out = static_cast<Object **>(userData);
			if (!*out && obj && obj->getDrawable() && obj->isSelectable())
				*out = obj;
		}, &selected);
	}
	if (selected)
		ctx.selectObject(selected);
	const Int beforeSelect = TheInGameUI ? TheInGameUI->getSelectCount() : 0;

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
	const Bool disabled = TheInGameUI && !TheInGameUI->getInputEnabled();
	const Bool deselected = beforeSelect > 0 && TheInGameUI && TheInGameUI->getSelectCount() == 0;
	if (hadInput && TheInGameUI)
	{
		TheInGameUI->setInputEnabled(TRUE);
		if (TheMouse)
			TheMouse->setVisibility(TRUE);
	}
	std::fprintf(stderr, "TEST script-victory frame=%u handled=%d shown=%d ending=%d disabled=%d deselected=%d\n",
		ctx.frame, type >= 0, shown, TheScriptEngine->isGameEnding(), disabled, deselected);
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
