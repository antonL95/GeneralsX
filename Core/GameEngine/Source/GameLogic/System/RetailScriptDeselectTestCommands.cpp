#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if RTS_ZEROHOUR
#include <cstdio>

#include "GameClient/InGameUI.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Scripts.h"

namespace
{
void deselect(const RetailTestCommands::Context &ctx)
{
	Object *unit = ctx.requireObject();
	if (!unit)
		return;
	ctx.selectObject(unit);
	const Int before = TheInGameUI->getSelectCount();
	const Int type = ScriptEngine::findRotwkActionByRetailIndex(506);
	if (type >= 0)
	{
		auto *action = newInstance(ScriptAction)(static_cast<ScriptAction::ScriptActionType>(type));
		TheScriptEngine->friend_executeAction(action);
		deleteInstance(action);
	}
	std::fprintf(stderr, "TEST script-deselect frame=%u before=%d after=%d\n",
		ctx.frame, before, TheInGameUI->getSelectCount());
}
}
#endif

namespace RetailTestCommands
{
void registerRetailScriptDeselectTestCommands()
{
#if RTS_ZEROHOUR
	registerAction("script-deselect", deselect);
#endif
}
}
