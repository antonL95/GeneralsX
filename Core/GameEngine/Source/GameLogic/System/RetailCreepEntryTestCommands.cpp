#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>
#include <string_view>
#include <utility>

#include "Common/ActionManager.h"
#include "Common/ThingTemplate.h"
#include "GameClient/Drawable.h"
#include "GameClient/GameClient.h"
#include "GameClient/Keyboard.h"
#include "GameClient/Mouse.h"
#include "GameClient/View.h"
#include "GameLogic/AIStateMachine.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Object.h"

namespace
{
class IdleKeyboard final : public Keyboard
{
	Bool getCapsState() override { return FALSE; }
	void getKey(KeyboardIO* key) override { key->key = KEY_NONE; }
};

void handleCreepEntry(const RetailTestCommands::Context& ctx)
{
	const std::string_view action(ctx.action);
	Object* source = ctx.requireObject(FALSE);
	if (!source)
		return;

	if (action == "creep-entry-state")
	{
		const Object* container = source->getContainedBy();
		auto* ai = source->getAI();
		const bool attacking = ctx.target && ai && ai->isAttacking() &&
			ai->getGoalObject() == ctx.target && ai->getLastCommandSource() == CMD_FROM_PLAYER;
		const bool entering = ai && ai->getCurrentStateID() == AI_ENTER;
		std::fprintf(stderr, "TEST creep-entry state frame=%u dead=%d contained=%d target-container=%d player-attacking=%d entering=%d health=%.1f\n",
			ctx.frame, source->isEffectivelyDead(), container != nullptr, container && container == ctx.target,
			attacking, entering, source->getBodyModule()->getHealth());
		return;
	}

	Object* target = ctx.target;
	if (!target)
		return;

	if (action == "creep-entry-check")
	{
		const bool allowed = TheActionManager->canEnterObject(source, target, CMD_FROM_PLAYER, CHECK_CAPACITY);
		const bool aiAllowed = TheActionManager->canEnterObject(source, target, CMD_FROM_AI, CHECK_CAPACITY);
		std::fprintf(stderr, "TEST creep-entry eligibility frame=%u target=%s player=%d ai=%d\n",
			ctx.frame, target->getTemplate()->getName().str(), allowed, aiAllowed);
		return;
	}

	if (action == "creep-entry-cursor")
	{
		const auto cursor = TheMouse->getMouseCursor();
		std::fprintf(stderr, "TEST creep-entry cursor frame=%u attack=%d enter=%d\n",
			ctx.frame, cursor == Mouse::ATTACK_OBJECT, cursor == Mouse::ENTER_FRIENDLY);
		return;
	}

	Drawable* drawable = target->getDrawable();
	if (!drawable)
		return;

	if (action == "creep-entry-click")
	{
		ICoord2D pixel;
		if (!TheTacticalView->worldToScreen(target->getPosition(), &pixel))
			return;
		TheMouse->setPosition(pixel.x, pixel.y);
		for (const auto type : {GameMessage::MSG_RAW_MOUSE_RIGHT_BUTTON_DOWN, GameMessage::MSG_RAW_MOUSE_RIGHT_BUTTON_UP})
		{
			GameMessage* message = TheMessageStream->appendMessage(type);
			message->appendPixelArgument(pixel);
			message->appendIntegerArgument(0);
			message->appendIntegerArgument(TheMouse->getMouseStatus()->time);
		}
		return;
	}

	if (action != "creep-entry-hover" && action != "creep-entry-order")
		return;

	const bool hover = action == "creep-entry-hover";
	IdleKeyboard idleKeyboard;
	Keyboard* keyboard = std::exchange(TheKeyboard, TheKeyboard ? TheKeyboard : &idleKeyboard);
	const auto order = TheGameClient->evaluateContextCommand(drawable, target->getPosition(),
		hover ? CommandTranslator::DO_HINT : CommandTranslator::DO_COMMAND);
	TheKeyboard = keyboard;
	const bool attack = order == GameMessage::MSG_DO_ATTACK_OBJECT ||
		order == GameMessage::MSG_DO_ATTACK_OBJECT_HINT || order == GameMessage::MSG_DO_ATTACK_OBJECT_AFTER_MOVING_HINT;
	const bool enter = order == GameMessage::MSG_ENTER || order == GameMessage::MSG_ENTER_HINT;
	std::fprintf(stderr, "TEST creep-entry context frame=%u hover=%d attack=%d enter=%d\n", ctx.frame, hover, attack, enter);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailCreepEntryTestCommands()
{
#if !RTS_GENERALS
	registerPrefix("creep-entry-", handleCreepEntry);
#endif
}
}
