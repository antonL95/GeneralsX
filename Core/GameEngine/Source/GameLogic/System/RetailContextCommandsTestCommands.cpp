#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>
#include <cstring>
#include <utility>

#include "Common/MapObject.h"
#include "Common/MessageStream.h"
#include "Common/SpecialPower.h"
#include "Common/ThingTemplate.h"
#include "GameClient/CommandXlat.h"
#include "GameClient/ControlBar.h"
#include "GameClient/Drawable.h"
#include "GameClient/InGameUI.h"
#include "GameClient/Keyboard.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"

namespace
{
class IdleKeyboard final : public Keyboard
{
	Bool getCapsState() override { return FALSE; }
	void getKey(KeyboardIO* key) override { key->key = KEY_NONE; }
};

Bool nearestGrabbableTree(const Object* source, Coord3D& result)
{
	Real best = sqr(600.0f);
	Bool found = FALSE;
	for (const MapObject* map = MapObject::getFirstMapObject(); map; map = map->getNext())
	{
		const ThingTemplate* type = map->getThingTemplate();
		const Coord3D* pos = map->getLocation();
		if (!type || !type->isKindOf(KINDOF_TREE) || !type->isKindOf(KINDOF_CLUB) ||
			!type->getRotWKObjectData().isGrabbable || !TheTerrainLogic->isNearMapTree(*pos, 0.0f))
			continue;
		const Real distance = sqr(pos->x - source->getPosition()->x) + sqr(pos->y - source->getPosition()->y);
		if (distance < best)
		{
			best = distance;
			result = *pos;
			found = TRUE;
		}
	}
	if (found)
		result.z += TheTerrainLogic->getGroundHeight(result.x, result.y);
	return found;
}

Object* findByTemplate(const char* name)
{
	for (Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
		if (!object->isEffectivelyDead() && object->getTemplate()->getName().compare(name) == 0)
			return object;
	return nullptr;
}

void contextClick(const RetailTestCommands::Context& ctx)
{
	Object* source = ctx.requireObject();
	if (!source || !source->getDrawable())
		return;
	TheInGameUI->deselectAllDrawables();
	ctx.selectObject(source);
	TheInGameUI->setGUICommand(nullptr);
	IdleKeyboard idleKeyboard;
	Keyboard* keyboard = std::exchange(TheKeyboard, TheKeyboard ? TheKeyboard : &idleKeyboard);
	CommandTranslator translator;
	GameMessage::Type result = GameMessage::MSG_INVALID;
	if (std::strcmp(ctx.argument, "tree") == 0)
	{
		Coord3D tree;
		if (nearestGrabbableTree(source, tree))
			result = translator.evaluateContextCommand(nullptr, &tree, CommandTranslator::DO_COMMAND);
	}
	else if (Object* target = findByTemplate(ctx.argument); target && target->getDrawable())
	{
		const Drawable* first = TheInGameUI->getFirstSelectedDrawable();
		const Object* lead = first ? first->getObject() : nullptr;
		std::fprintf(stderr, "TEST context-lead frame=%u unit=%s lead=%s selected=%d can=%d\n", ctx.frame, ctx.targetName,
			lead ? lead->getTemplate()->getName().str() : "-", TheInGameUI->getSelectCount(),
			TheInGameUI->canSelectedObjectsDoAction(InGameUI::ACTIONTYPE_CAPTURE_BUILDING, target, InGameUI::SELECTION_ANY) ? 1 : 0);
		result = translator.evaluateContextCommand(target->getDrawable(), target->getPosition(), CommandTranslator::DO_COMMAND);
	}
	TheKeyboard = keyboard;
	std::fprintf(stderr, "TEST context-click frame=%u unit=%s target=%s sent=%d special=%d\n", ctx.frame, ctx.targetName,
		ctx.argument, result != GameMessage::MSG_INVALID, result == GameMessage::MSG_DO_SPECIAL_POWER_AT_OBJECT);
}

void contextHeld(const RetailTestCommands::Context& ctx)
{
	const Object* source = ctx.requireObject();
	if (!source)
		return;
	const ContainModuleInterface* contain = source->getContain();
	const ContainedItemsList* items = contain ? contain->getContainedItemsList() : nullptr;
	const Object* held = items && !items->empty() ? items->front() : nullptr;
	std::fprintf(stderr, "TEST context-held frame=%u tree=%d club=%d\n", ctx.frame,
		held && held->isKindOf(KINDOF_TREE) ? 1 : 0,
		source->getDrawable() && source->getDrawable()->getModelConditionFlags().test(MODELCONDITION_WEAPONSTATE_ONE) ? 1 : 0);
}

void contextEat(const RetailTestCommands::Context& ctx)
{
	Object* source = ctx.requireObject();
	if (!source || !source->getDrawable())
		return;
	ctx.selectObject(source);
	const CommandButton* button = TheRetailCommandCatalog->findCommandButton(ctx.argument);
	const SpecialPowerTemplate* power = button ? button->getSpecialPowerTemplate() : nullptr;
	const SpecialPowerModuleInterface* module = power ? source->getSpecialPowerModule(power) : nullptr;
	if (!module)
		return;
	const CommandAvailability availability = TheControlBar->queryRetailCommandAvailability(button, source);
	GameMessage* message = TheMessageStream->appendMessage(GameMessage::MSG_DO_SPECIAL_POWER);
	message->appendIntegerArgument(power->getID());
	message->appendIntegerArgument(button->getOptions());
	message->appendObjectIDArgument(source->getID());
	std::fprintf(stderr, "TEST context-eat frame=%u available=%d ready=%d\n", ctx.frame,
		availability == COMMAND_AVAILABLE ? 1 : 0, module->isReady() ? 1 : 0);
}

void contextPower(const RetailTestCommands::Context& ctx)
{
	const Object* source = ctx.requireObject();
	const CommandButton* button = source ? TheRetailCommandCatalog->findCommandButton(ctx.argument) : nullptr;
	const SpecialPowerTemplate* power = button ? button->getSpecialPowerTemplate() : nullptr;
	const SpecialPowerModuleInterface* module = power ? source->getSpecialPowerModule(power) : nullptr;
	std::fprintf(stderr, "TEST context-power frame=%u ready=%d\n", ctx.frame, module && module->isReady() ? 1 : 0);
}

void contextOwner(const RetailTestCommands::Context& ctx)
{
	const Object* target = findByTemplate(ctx.targetName);
	const Player* owner = target ? target->getControllingPlayer() : nullptr;
	std::fprintf(stderr, "TEST context-owner frame=%u template=%s captured=%d\n", ctx.frame, ctx.targetName,
		owner && owner == ctx.player ? 1 : 0);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailContextCommandsTestCommands()
{
#if !RTS_GENERALS
	registerAction("context-click", contextClick);
	registerAction("context-held", contextHeld);
	registerAction("context-eat", contextEat);
	registerAction("context-power", contextPower);
	registerAction("context-owner", contextOwner);
#endif
}
}
