#include "PreRTS.h"

#if !RTS_GENERALS
#include <cstdio>
#include <string_view>

#include "Common/SpecialPower.h"
#include "Common/ThingTemplate.h"
#include "GameClient/ControlBar.h"
#include "GameLogic/Damage.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/ActiveBody.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/AutoHealBehavior.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Object.h"
#include "GameLogic/Weapon.h"

void runRetailPackage11TestCommand(std::string_view action, Object* source, Object* target, const char* name,
	const Coord3D& inputs, UnsignedInt frame)
{
	if (action == "pkg11-damage" && target)
	{
		DamageInfo damage;
		damage.in.m_sourceID = source->getID();
		damage.in.m_amount = inputs.x;
		damage.in.m_damageType = inputs.y == 1 ? DAMAGE_SIEGE : inputs.y == 2 ? DAMAGE_FLAME : DAMAGE_UNRESISTABLE;
		damage.in.m_deathType = inputs.y == 2 ? DEATH_BURNED : DEATH_NORMAL;
		target->attemptDamage(&damage);
	}
	else if (action == "pkg11-hurt-members")
	{
		const ContainModuleInterface* contain = source->getContain();
		if (contain && contain->getContainedItemsList())
			for (Object* member : *contain->getContainedItemsList())
				member->getBodyModule()->internalChangeHealth(-inputs.x);
	}
	else if (action == "pkg11-heal")
		source->attemptHealing(inputs.x, source);
	else if (action == "pkg11-pulse" && source->getAI())
	{
		const Object* previous = source->getAI()->getCurrentVictim();
		source->getAI()->setCurrentVictim(target);
		for (BehaviorModule** modules = source->getBehaviorModules(); *modules; ++modules)
			if (auto* heal = dynamic_cast<AutoHealBehavior*>(*modules))
			{
				const Bool inCombat = source->getAI()->getCurrentVictim() != nullptr;
				const UpdateSleepTime delay = heal->update();
				std::fprintf(stderr, "TEST pkg11 pulse frame=%u name=%s combat=%d delay=%u\n", frame,
					source->getTemplate()->getName().str(), inCombat, static_cast<UnsignedInt>(delay));
			}
		source->getAI()->setCurrentVictim(previous);
	}
	else if (action == "pkg11-shoot" && target)
	{
		const WeaponTemplate* weapon = TheWeaponStore->findWeaponTemplate(inputs.x == 1 ? "Pkg11Undodgeable" :
			inputs.x == 2 ? "Pkg11SiegeLaunch" : "Pkg11Dodgeable");
		if (weapon)
		{
			if (inputs.x == 2)
				const_cast<WeaponTemplate*>(weapon)->postProcessLoad();
			for (Int shot = 0; shot < std::max(1, static_cast<Int>(inputs.y)); ++shot)
				TheWeaponStore->createAndFireTempWeapon(weapon, source, target, TRUE);
		}
	}
	else if (action == "pkg11-power")
	{
		const SpecialPowerTemplate* power = TheSpecialPowerStore->findSpecialPowerTemplate(
			target ? "SpecialAbilityGrabPassenger2" : name);
		if (power && target)
			source->doSpecialPowerAtObject(power, target, NEED_TARGET_ENEMY_OBJECT | NEED_TARGET_NEUTRAL_OBJECT);
		else if (power)
			source->doSpecialPower(power, 0);
	}
	else if (action == "pkg11-state")
	{
		const auto* body = dynamic_cast<const ActiveBody*>(source->getBodyModule());
		if (!body)
			return;
		const auto* data = body->getRetailBodyData();
		const auto& flags = source->getLogicModelConditions();
		const ContainModuleInterface* contain = source->getContain();
		const ContainedItemsList* items = contain ? contain->getContainedItemsList() : nullptr;
		Real memberHealth = 0.0f;
		if (items)
			for (const Object* item : *items)
				memberHealth += item->getBodyModule()->getHealth();
		std::fprintf(stderr, "TEST pkg11 state frame=%u name=%s hp=%.1f/%.1f state=%d front=%d cheer=%d burning=%u firefx=%d healfx=%d recovery=%u dodge=%.2f vendor=%s grab=%.0f,%.0f,%.0f,%d items=%u first=%s memberhp=%.1f\n",
			frame, source->getTemplate()->getName().str(), body->getHealth(), body->getMaxHealth(),
			body->getDamageState(), flags.test(MODELCONDITION_DESTROYED_FRONT),
			flags.test(MODELCONDITION_EMOTION_CELEBRATING), source->getAI() ? source->getAI()->getRetailBurningDeathDeadline() : 0,
			data->m_burningDeathFX != nullptr, data->m_healingBuffFX != nullptr, data->m_recoveryTime, data->m_dodgePercent,
			data->m_grabObject.str(), data->m_grabDamage, data->m_grabOffset.x, data->m_grabOffset.y, data->m_grabFX != nullptr,
			contain ? contain->getContainCount() : 0, items && !items->empty() ? items->front()->getTemplate()->getName().str() : "-", memberHealth);
	}
	else if (action == "pkg11-debris")
	{
		Int debris = 0;
		for (const Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
			if (object->getTemplate()->getName().startsWith(name))
				++debris;
		std::fprintf(stderr, "TEST pkg11 debris frame=%u prefix=%s count=%d\n", frame, name, debris);
	}
}
#endif
