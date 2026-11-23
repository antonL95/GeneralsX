#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/MessageStream.h"
#include "Common/SpecialPower.h"
#include "Common/ThingTemplate.h"
#include "GameClient/CommandXlat.h"
#include "GameClient/ControlBar.h"
#include "GameLogic/Module/DumbProjectileBehavior.h"
#include "GameLogic/Object.h"
#include "GameLogic/Weapon.h"

namespace
{
void handleWindup(const RetailTestCommands::Context& ctx)
{
	if (ctx.action == "pkg62-resolve")
	{
		if (const WeaponTemplate* weapon = TheWeaponStore->findWeaponTemplate(AsciiString(ctx.argument)))
			const_cast<WeaponTemplate*>(weapon)->postProcessLoad();
		return;
	}

	Object* source = ctx.requireObject();
	if (!source)
		return;

	if (ctx.action == "pkg62-stop")
	{
		ctx.selectObject(source);
		TheMessageStream->appendMessage(GameMessage::MSG_DO_STOP);
		return;
	}
	if (ctx.action == "pkg62-switch")
	{
		ctx.selectObject(source);
		auto* message = TheMessageStream->appendMessage(GameMessage::MSG_SWITCH_WEAPONS);
		message->appendIntegerArgument(ctx.argument == "primary" ? PRIMARY_WEAPON : SECONDARY_WEAPON);
		return;
	}
	if (ctx.action == "pkg62-attack")
	{
		const Object* target = ctx.findTarget();
		if (!target)
			return;
		ctx.selectObject(source);
		DrawableList selection{source->getDrawable()};
		PickAndPlayInfo info;
		info.m_drawTarget = target->getDrawable();
		pickAndPlayUnitVoiceResponse(&selection, GameMessage::MSG_DO_ATTACK_OBJECT, &info);
		auto* message = TheMessageStream->appendMessage(GameMessage::MSG_DO_ATTACK_OBJECT);
		message->appendObjectIDArgument(target->getID());
		return;
	}
	if (ctx.action == "pkg62-cast")
	{
		const CommandButton* button = TheControlBar ? TheControlBar->findCommandButton(ctx.argument) : nullptr;
		if (!button || !button->getSpecialPowerTemplate() || !ctx.hasPosition)
			return;
		auto* message = TheMessageStream->appendMessage(GameMessage::MSG_DO_SPECIAL_POWER_AT_LOCATION);
		message->appendIntegerArgument(button->getSpecialPowerTemplate()->getID());
		message->appendLocationArgument(ctx.groundLocation());
		message->appendRealArgument(INVALID_ANGLE);
		message->appendObjectIDArgument(INVALID_ID);
		message->appendIntegerArgument(button->getOptions());
		message->appendObjectIDArgument(source->getID());
		return;
	}
	if (ctx.action != "pkg62-state")
		return;

	const auto& flags = source->getLogicModelConditions();
	const auto* flight = dynamic_cast<const DumbProjectileBehavior*>(source->getProjectileUpdateInterface());
	const Coord3D& position = *source->getPosition();
	std::fprintf(stderr, "TEST pkg62 actor frame=%u name=%s attack=%d firing=%d preattack=%d self=%d air=%d noCollision=%d pos=%.2f,%.2f,%.2f id=%u\n",
		ctx.frame, source->getTemplate()->getName().str(), source->testStatus(OBJECT_STATUS_IS_ATTACKING),
		flags.test(MODELCONDITION_FIRING_A), flags.test(MODELCONDITION_PREATTACK_A), flight && flight->isSelfFlight(),
		source->isAirborneTarget(), source->testStatus(OBJECT_STATUS_NO_COLLISIONS), position.x, position.y, position.z,
		static_cast<UnsignedInt>(source->getID()));
	for (Int slot = PRIMARY_WEAPON; slot < WEAPONSLOT_COUNT; ++slot)
	{
		const Weapon* weapon = source->getWeaponInWeaponSlot(static_cast<WeaponSlotType>(slot));
		if (weapon)
			std::fprintf(stderr, "TEST pkg62 slot frame=%u name=%s slot=%d active=%d weapon=%s pre=%u shot=%u next=%u reload=%u ammo=%u\n",
				ctx.frame, source->getTemplate()->getName().str(), slot, weapon == source->getCurrentWeapon(),
				weapon->getName().str(), weapon->getPreAttackFinishedFrame(), weapon->getLastShotFrame(),
				weapon->getPossibleNextShotFrame(), weapon->getLastReloadStartedFrame(), weapon->getRemainingAmmo());
	}
}
}
#endif

namespace RetailTestCommands
{
void registerRetailPackage62TestCommands()
{
#if !RTS_GENERALS
	registerPrefix("pkg62-", handleWindup);
#endif
}
}
