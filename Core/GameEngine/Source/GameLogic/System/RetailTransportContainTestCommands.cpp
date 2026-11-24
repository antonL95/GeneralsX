#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>
#include <cstring>

#include "Common/ActionManager.h"
#include "Common/SpecialPower.h"
#include "GameClient/ControlBar.h"
#include "GameClient/Drawable.h"
#include "GameLogic/Module/GrabPassengerSpecialPower.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Object.h"
#include "GameLogic/WeaponSet.h"

namespace
{
void parseTargetAndLabel(const RetailTestCommands::Context& ctx, Object*& target, AsciiString& label)
{
	const char* raw = ctx.argument && ctx.argument[0] ? ctx.argument : "";
	const char* comma = std::strchr(raw, ',');
	if (comma)
	{
		AsciiString spec;
		spec.set(raw);
		spec.truncateTo(static_cast<Int>(comma - raw));
		target = ctx.findTarget(spec.str());
		label.set(comma + 1);
	}
	else
	{
		target = ctx.findTarget();
		label.set(raw[0] ? raw : "-");
	}
}

void handleTransportContain(const RetailTestCommands::Context& ctx)
{
	if (ctx.action == "tc-alive")
	{
		std::fprintf(stderr, "TEST tc frame=%u action=alive label=%s present=%d alive=%d\n",
			ctx.frame, ctx.argument, ctx.findObject(FALSE) != nullptr, ctx.findObject(TRUE) != nullptr);
		std::fflush(stderr);
		return;
	}

	Object* source = ctx.requireObject();
	if (!source)
		return;

	const char* label = ctx.argument && ctx.argument[0] ? ctx.argument : "-";

	if (ctx.action == "tc-set-mounted")
	{
		source->setModelConditionState(MODELCONDITION_MOUNTED);
		return;
	}

	if (ctx.action == "tc-clear-mounted")
	{
		source->clearModelConditionState(MODELCONDITION_MOUNTED);
		return;
	}

	if (ctx.action == "tc-board")
	{
		Object* rider = nullptr;
		AsciiString tag;
		parseTargetAndLabel(ctx, rider, tag);
		ContainModuleInterface* contain = source->getContain();
		if (!rider || !contain)
			return;
		const Bool valid = contain->isValidContainerFor(rider, TRUE);
		if (valid)
			contain->addToContain(rider);
		std::fprintf(stderr, "TEST tc frame=%u action=board label=%s valid=%d contained=%d\n",
			ctx.frame, tag.str(), valid ? 1 : 0, rider->getContainedBy() == source ? 1 : 0);
		std::fflush(stderr);
		return;
	}

	if (ctx.action == "tc-eject")
	{
		Object* rider = nullptr;
		AsciiString tag;
		parseTargetAndLabel(ctx, rider, tag);
		ContainModuleInterface* contain = source->getContain();
		if (!rider || !contain)
			return;
		contain->removeFromContain(rider);
		std::fprintf(stderr, "TEST tc frame=%u action=eject label=%s contained=%d\n",
			ctx.frame, tag.str(), rider->getContainedBy() == source ? 1 : 0);
		std::fflush(stderr);
		return;
	}

	if (ctx.action == "tc-can-contain")
	{
		Object* rider = nullptr;
		AsciiString tag;
		parseTargetAndLabel(ctx, rider, tag);
		ContainModuleInterface* contain = source->getContain();
		const Bool valid = rider && contain && contain->isValidContainerFor(rider, TRUE);
		std::fprintf(stderr, "TEST tc frame=%u action=can-contain label=%s valid=%d\n",
			ctx.frame, tag.str(), valid ? 1 : 0);
		std::fflush(stderr);
		return;
	}

	if (ctx.action == "tc-manual-grab")
	{
		Object* target = nullptr;
		AsciiString tag;
		parseTargetAndLabel(ctx, target, tag);
		const SpecialPowerTemplate* power = TheSpecialPowerStore ? TheSpecialPowerStore->findSpecialPowerTemplate("SpecialAbilityGrabPassenger") : nullptr;
		const UnsignedInt manualOpts = NEED_TARGET_ENEMY_OBJECT | NEED_TARGET_NEUTRAL_OBJECT | NEED_TARGET_ALLY_OBJECT | OPTION_ONE;
		const Bool manual = target && power && TheActionManager &&
			TheActionManager->canDoSpecialPowerAtObject(source, target, CMD_FROM_PLAYER, power, manualOpts, FALSE);
		const Bool autoOk = target && power && TheActionManager &&
			TheActionManager->canDoSpecialPowerAtObject(source, target, CMD_FROM_AI, power, COMMAND_OPTION_NONE, FALSE);
		std::fprintf(stderr, "TEST tc frame=%u action=manual-grab label=%s manual=%d auto=%d\n",
			ctx.frame, tag.str(), manual ? 1 : 0, autoOk ? 1 : 0);
		std::fflush(stderr);
		return;
	}

	if (ctx.action == "tc-cast-grab")
	{
		Object* target = nullptr;
		AsciiString tag;
		parseTargetAndLabel(ctx, target, tag);
		const auto* power = TheSpecialPowerStore->findSpecialPowerTemplate("SpecialAbilityGrabPassenger");
		auto* grab = power ? dynamic_cast<GrabPassengerSpecialPower*>(source->getSpecialPowerModule(power)) : nullptr;
		if (!grab || !target)
			return;
		const Bool accepted = grab->requestGrab(target, OPTION_ONE);
		std::fprintf(stderr, "TEST tc frame=%u action=cast-grab label=%s accepted=%d\n", ctx.frame, tag.str(), accepted);
		std::fflush(stderr);
		return;
	}

	if (ctx.action == "tc-probe")
	{
		const auto& mc = source->getLogicModelConditions();
		const Int hp = source->getBodyModule() ? static_cast<Int>(source->getBodyModule()->getHealth()) : 0;
		const Bool unatk = source->testStatus(OBJECT_STATUS_UNATTACKABLE);
		const Bool ws1 = source->testWeaponSetFlag(WEAPONSET_PASSENGER_TYPE_ONE);
		const Bool ws2 = source->testWeaponSetFlag(WEAPONSET_PASSENGER_TYPE_TWO);
		const Bool st1 = mc.test(MODELCONDITION_WEAPONSTATE_ONE);
		const Bool st2 = mc.test(MODELCONDITION_WEAPONSTATE_TWO);
		const Bool pv1 = mc.test(MODELCONDITION_PASSENGER_VARIATION_1);
		const Bool pv2 = mc.test(MODELCONDITION_PASSENGER_VARIATION_2);
		Int pipsTotal = 0;
		Int pipsFull = 0;
		Bool pipsShow = FALSE;
		if (ContainModuleInterface* contain = source->getContain())
			pipsShow = contain->getContainerPipsToShow(pipsTotal, pipsFull);
		const Drawable* draw = source->getDrawable();
		const Int opacityPct = draw ? static_cast<Int>(draw->getEffectiveOpacity() * 100.0f) : 100;
		std::fprintf(stderr,
			"TEST tc frame=%u action=probe label=%s hp=%d unatk=%d ws1=%d ws2=%d st1=%d st2=%d pv1=%d pv2=%d pips=%d/%d/%d opacity=%d\n",
			ctx.frame, label, hp, unatk ? 1 : 0, ws1 ? 1 : 0, ws2 ? 1 : 0, st1 ? 1 : 0, st2 ? 1 : 0,
			pv1 ? 1 : 0, pv2 ? 1 : 0, pipsShow ? 1 : 0, pipsTotal, pipsFull, opacityPct);
		std::fflush(stderr);
		return;
	}
}
}
#endif

namespace RetailTestCommands
{
void registerRetailTransportContainTestCommands()
{
#if !RTS_GENERALS
	registerPrefix("tc-", handleTransportContain);
#endif
}
}
