#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#define DEFINE_DEATH_NAMES
#include <cstdio>
#include <string_view>

#include "Common/ActionManager.h"
#include "Common/PlayerList.h"
#include "Common/SpecialPower.h"
#include "Common/ThingTemplate.h"
#include "GameClient/ControlBar.h"
#include "GameClient/Drawable.h"
#include "GameClient/FXList.h"
#include "GameLogic/Damage.h"
#include "GameLogic/Module/AutoHealBehavior.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/GrabPassengerSpecialPower.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Object.h"
#include "GameLogic/Weapon.h"

void runRetailPackage20TestCommand(std::string_view action, Object* source, Object* target, const char* name,
	const Coord3D& inputs, UnsignedInt frame)
{
	if (action == "pkg20-pulse")
	{
		for (BehaviorModule** modules = source->getBehaviorModules(); *modules; ++modules)
			if (auto* healing = dynamic_cast<AutoHealBehavior*>(*modules))
				for (Int pulse = 0; pulse < std::max(1, static_cast<Int>(inputs.x)); ++pulse)
					healing->update();
	}
	else if (action == "pkg20-fx-list")
	{
		const auto* fx = TheFXListStore->findFXList(name);
		std::fprintf(stderr, "TEST pkg20 fx-list name=%s address=%p\n", name, static_cast<const void*>(fx));
	}
	else if (action == "pkg20-hit-member" && target)
	{
		const auto* horde = HordeContain::get(target);
		const auto* members = horde ? horde->getContainedItemsList() : nullptr;
		if (members && !members->empty())
		{
			DamageInfo damage;
			damage.in.m_sourceID = source->getID();
			damage.in.m_damageType = DAMAGE_UNRESISTABLE;
			damage.in.m_amount = inputs.x;
			members->front()->attemptDamage(&damage);
		}
	}
	else if (action == "pkg20-grab" && target)
	{
		const auto* power = TheSpecialPowerStore->findSpecialPowerTemplate(
			inputs.x > 0 ? "SpecialAbilityGrabPassenger2" : "SpecialAbilityGrabPassenger");
		if (power)
			source->doSpecialPowerAtObject(power, target, NEED_TARGET_NEUTRAL_OBJECT | NEED_TARGET_ENEMY_OBJECT);
	}
	else if (action == "pkg20-check-grab" && target)
	{
		const auto* power = TheSpecialPowerStore->findSpecialPowerTemplate("SpecialAbilityGrabPassenger");
		auto* grab = power ? dynamic_cast<GrabPassengerSpecialPower*>(source->getSpecialPowerModule(power)) : nullptr;
		if (!grab)
			return;
		constexpr UnsignedInt options = NEED_TARGET_NEUTRAL_OBJECT | NEED_TARGET_ENEMY_OBJECT;
		const Bool valid = grab->isValidPowerTarget(target);
		const Bool allowed = TheActionManager->canDoSpecialPowerAtObject(source, target, CMD_FROM_PLAYER, power, options);
		const Bool accepted = grab->requestGrab(target, options);
		std::fprintf(stderr, "TEST pkg20 targeting frame=%u caster=%s valid=%d allowed=%d accepted=%d pending=%d\n",
			frame, source->getTemplate()->getName().str(), valid, allowed, accepted, grab->getPendingTarget() != INVALID_ID);
	}
	else if (action == "pkg20-ownership" && target)
	{
		const auto* contain = source->getContain();
		const auto* passengers = contain ? contain->getContainedItemsList() : nullptr;
		const Object* held = passengers && !passengers->empty() ? passengers->front() : nullptr;
		const Player* neutral = ThePlayerList->getNeutralPlayer();
		std::fprintf(stderr, "TEST pkg20 ownership frame=%u caster=%s vendor-hostile=%d stock=%s neutral=%d producer=%d\n",
			frame, source->getTemplate()->getName().str(), source->getRelationship(target) == ENEMIES,
			held ? held->getTemplate()->getName().str() : "-",
			held && held->getControllingPlayer() == neutral, held && held->getProducerID() == source->getID());
	}
	else if (action == "pkg20-held")
	{
		const auto* contain = source->getContain();
		const auto* passengers = contain ? contain->getContainedItemsList() : nullptr;
		const auto* held = passengers && !passengers->empty() ? passengers->front() : nullptr;
		const Coord3D offset = held ? Coord3D{held->getPosition()->x - source->getPosition()->x,
			held->getPosition()->y - source->getPosition()->y, held->getPosition()->z - source->getPosition()->z} : Coord3D{};
		std::fprintf(stderr, "TEST pkg20 held frame=%u caster=%s id=%u name=%s offset=%.1f,%.1f,%.1f\n",
			frame, source->getTemplate()->getName().str(), static_cast<UnsignedInt>(held ? held->getID() : INVALID_ID),
			held ? held->getTemplate()->getName().str() : "-", offset.x, offset.y, offset.z);
	}
	else if (action == "pkg20-shoot" && target)
	{
		const auto* weapon = TheWeaponStore->findWeaponTemplate(inputs.x > 0 ? "Pkg20Catapult" : "Pkg20PlainShot");
		if (weapon)
			TheWeaponStore->createAndFireTempWeapon(weapon, source, target, TRUE);
	}
	else if (action == "pkg20-state")
	{
		const auto* body = source->getBodyModule();
		const auto* horde = HordeContain::get(source);
		const auto* drawable = source->getDrawable();
		const auto* damage = body->getLastDamageInfo();
		std::fprintf(stderr, "TEST pkg20 state frame=%u name=%s hp=%.1f/%.1f state=%d damaged=%d really=%d cheer=%d last-member-hit=%u death=%s\n",
			frame, source->getTemplate()->getName().str(), body->getHealth(), body->getMaxHealth(), body->getDamageState(),
			drawable && drawable->getModelConditionFlags().test(MODELCONDITION_DAMAGED),
			drawable && drawable->getModelConditionFlags().test(MODELCONDITION_REALLY_DAMAGED),
			source->getLogicModelConditions().test(MODELCONDITION_EMOTION_CELEBRATING),
			horde ? horde->getLastMemberDamageFrame() : 0,
			source->isEffectivelyDead() && damage ? TheDeathNames[damage->in.m_deathType] : "-");
	}
}
#endif

namespace RetailTestCommands
{
void registerRetailPackage20TestCommands()
{
#if !RTS_GENERALS
	registerPrefix("pkg20-", [](const Context& ctx) {
		Object* source = ctx.requireObject();
		if (!source)
			return;
		runRetailPackage20TestCommand(ctx.action, source, ctx.target, ctx.argument, ctx.position, ctx.frame);
	});
#endif
}
}
