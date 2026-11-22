#include "PreRTS.h"

#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include "Common/GameUtility.h"
#include "Common/Player.h"
#include "GameClient/CommandXlat.h"
#include "GameClient/Drawable.h"
#include "GameClient/FXList.h"
#include "GameLogic/Object.h"

#include <cstdio>

namespace
{
void dispatchFX(const RetailTestCommands::Context& ctx)
{
	Object* source = ctx.requireObject();
	if (!source)
		return;
	const std::string_view name = ctx.action.substr(sizeof("pkg49-fx:") - 1);
	const std::string fxName(name);
	const FXList* fx = TheFXListStore->findFXList(fxName.c_str());
	if (!fx)
		return;

	const Object* target = ctx.findTarget();
	std::fprintf(stderr, "PKG49 FX frame=%u %s\n", ctx.frame, fxName.c_str());
	if (target)
		FXList::doFXPos(fx, source->getPosition(), source->getTransformMatrix(), 0.0f,
			target->getPosition(), 0.0f, source, target);
	else
		FXList::doFXObj(fx, source);
	std::fprintf(stderr, "PKG49 FX end\n");
}

void heroCondition(const RetailTestCommands::Context& ctx)
{
	if (Object* hero = ctx.requireObject())
		hero->setModelConditionState(MODELCONDITION_CREATE_A_HERO_02);
}

void raisingFlag(const RetailTestCommands::Context& ctx)
{
	if (Object* unit = ctx.requireObject(true))
	{
		const std::string_view change(ctx.argument);
		const ModelConditionFlagType condition = change.starts_with("point") ? MODELCONDITION_EMOTION_POINTING :
			change.starts_with("melee") ? MODELCONDITION_WEAPONSET_TOGGLE_1 : MODELCONDITION_RAISING_FLAG;
		if (change.ends_with("on"))
			unit->setModelConditionState(condition);
		else
			unit->clearModelConditionState(condition);
	}
}

void voice(const RetailTestCommands::Context& ctx)
{
	Object* source = ctx.requireObject();
	if (!source || !source->getDrawable())
		return;
	DrawableList units{source->getDrawable()};
	if (const auto* partner = ctx.findTarget(); partner && partner->getDrawable())
		units.push_back(partner->getDrawable());
	const auto message = ctx.action == "pkg49-attack-voice" ?
		GameMessage::MSG_DO_ATTACK_OBJECT : GameMessage::MSG_CREATE_SELECTED_GROUP;
	pickAndPlayUnitVoiceResponse(&units, message);
}

void alliance(const RetailTestCommands::Context& ctx)
{
	Player* viewer = rts::getObservedOrLocalPlayer_Safe();
	Player* ally = ctx.findPlayer("PlyrCivilian");
	if (!viewer || !ally)
		return;
	if (std::string_view(ctx.argument) == "on")
		viewer->setPlayerRelationship(ally, ALLIES);
	else
		viewer->removePlayerRelationship(ally);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailPackage49TestCommands()
{
#if !RTS_GENERALS
	registerPrefix("pkg49-fx:", dispatchFX);
	registerAction("pkg49-hero-condition", heroCondition);
	registerAction("pkg49-model-condition", raisingFlag);
	registerAction("pkg49-attack-voice", voice);
	registerAction("pkg49-select-voice", voice);
	registerAction("pkg49-alliance", alliance);
#endif
}
}
