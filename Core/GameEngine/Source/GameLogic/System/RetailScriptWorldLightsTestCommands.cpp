#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if RTS_ZEROHOUR
#include <cstdio>

#include "GameClient/GameClient.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Scripts.h"

namespace
{
constexpr RGBColor dimColor = {0.25f, 0.5f, 0.75f};
constexpr Real fadeSeconds = 1.0f;

void dim(const RetailTestCommands::Context &)
{
	const Int type = ScriptEngine::findRotwkActionByRetailIndex(437);
	if (type < 0)
		return;
	auto *action = newInstance(ScriptAction)(static_cast<ScriptAction::ScriptActionType>(type));
	action->getParameter(0)->friend_setReal(dimColor.red);
	action->getParameter(1)->friend_setReal(dimColor.green);
	action->getParameter(2)->friend_setReal(dimColor.blue);
	action->getParameter(3)->friend_setReal(fadeSeconds);
	TheScriptEngine->friend_executeAction(action);
	deleteInstance(action);
}

void check(const RetailTestCommands::Context &ctx)
{
	const RGBColor color = TheGameClient->getScriptWorldLight(TheGameClient->getFrame());
	const Bool normal = color.red == 1.0f && color.green == 1.0f && color.blue == 1.0f;
	const Bool settled = color.red == dimColor.red && color.green == dimColor.green && color.blue == dimColor.blue;
	const Bool fading = color.red > dimColor.red && color.red < color.green && color.green < color.blue && color.blue < 1.0f;
	std::fprintf(stderr, "TEST script-world-lights frame=%u normal=%d fading=%d dimmed=%d\n",
		ctx.frame, normal, fading, settled);
}

void restore(const RetailTestCommands::Context &)
{
	const Int type = ScriptEngine::findRotwkActionByRetailIndex(438);
	if (type < 0)
		return;
	auto *action = newInstance(ScriptAction)(static_cast<ScriptAction::ScriptActionType>(type));
	action->getParameter(0)->friend_setReal(fadeSeconds);
	TheScriptEngine->friend_executeAction(action);
	deleteInstance(action);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailScriptWorldLightsTestCommands()
{
#if RTS_ZEROHOUR
	registerAction("script-dim-world", dim);
	registerAction("script-restore-world", restore);
	registerAction("script-world-lights-check", check);
#endif
}
}
