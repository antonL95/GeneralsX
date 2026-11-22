#include "PreRTS.h"

#if !RTS_GENERALS
#include <cstdio>
#include <string_view>

#include "Common/ThingTemplate.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/DumbProjectileBehavior.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Weapon.h"

void runRetailPackage22TestCommand(std::string_view action, Object* source, Object* target, const Coord3D& inputs, UnsignedInt frame)
{
	if (action == "pkg22-place")
	{
		Coord3D position = *source->getPosition();
		position.z = (target ? target->getPosition()->z : TheTerrainLogic->getGroundHeight(position.x, position.y)) + inputs.x;
		source->setPosition(&position);
		if (inputs.y > 0.0f)
			source->setLayer(LAYER_WALL);
		return;
	}
	if (action == "pkg22-swoop")
	{
		const Weapon* weapon = source->getCurrentWeapon();
		std::fprintf(stderr, "TEST pkg22 swoop frame=%u source=%s weapon=%s active=%d\n", frame,
			source->getTemplate()->getName().str(), weapon ? weapon->getName().str() : "-",
			source->getLogicModelConditions().test(MODELCONDITION_SWOOPING));
		return;
	}
	if (action == "pkg22-flight")
	{
		std::fprintf(stderr, "TEST pkg22 flight frame=%u source=%s id=%u alive=%d thrown=%d\n",
			frame, source->getTemplate()->getName().str(), static_cast<UnsignedInt>(source->getID()), !source->isEffectivelyDead(),
			source->getLogicModelConditions().test(MODELCONDITION_THROWN_PROJECTILE));
		return;
	}
	if (action == "pkg22-bombardment")
	{
		if (inputs.x > 0.0f)
			source->setWeaponBonusCondition(WEAPONBONUSCONDITION_BATTLEPLAN_BOMBARDMENT);
		else
			source->clearWeaponBonusCondition(WEAPONBONUSCONDITION_BATTLEPLAN_BOMBARDMENT);
		return;
	}
	if (action == "pkg22-choose-near" && target)
	{
		const Weapon* primary = source->getWeaponInWeaponSlot(PRIMARY_WEAPON);
		const Weapon* secondary = source->getWeaponInWeaponSlot(SECONDARY_WEAPON);
		if (!primary || !secondary)
			return;
		std::fprintf(stderr, "TEST pkg22 footprint frame=%u primary-ready=%d primary-range=%d secondary-ready=%d secondary-range=%d\n",
			frame, primary->getStatus() == READY_TO_FIRE, primary->isWithinAttackRange(source, target),
			secondary->getStatus() == READY_TO_FIRE, secondary->isWithinAttackRange(source, target));
		source->chooseBestWeaponForTarget(target, PREFER_LEAST_MOVEMENT, CMD_FROM_PLAYER);
	}
	if (action == "pkg22-choose" && target)
		source->chooseBestWeaponForTarget(target, PREFER_LONGEST_RANGE, CMD_FROM_PLAYER);
	Weapon* weapon = source->getCurrentWeapon();
	if (!weapon)
		return;
	if (action == "pkg22-choose" || action == "pkg22-choose-near" || action == "pkg22-range")
	{
		if (!target)
			return;
		std::fprintf(stderr, "TEST pkg22 range frame=%u source=%s weapon=%s target=%s dz=%.1f range=%.1f eligible=%d\n",
			frame, source->getTemplate()->getName().str(), weapon->getName().str(), target->getTemplate()->getName().str(),
			source->getPosition()->z - target->getPosition()->z, weapon->getAttackRange(source, target->getPosition()),
			weapon->isWithinAttackRange(source, target));
	}
	else if (action == "pkg22-fire")
	{
		const Real health = target ? target->getBodyModule()->getHealth() : 0.0f;
		const Bool ready = weapon->getStatus() == READY_TO_FIRE;
		const auto* contain = source->getContain();
		const Int held = contain ? contain->getContainCount() : 0;
		ObjectID projectileID = INVALID_ID;
		if (ready)
		{
			WeaponTemplate* tmpl = const_cast<WeaponTemplate*>(weapon->getTemplate());
			tmpl->postProcessLoad();
			if (target)
				weapon->fireWeapon(source, target, &projectileID);
			else
				weapon->fireWeapon(source, source->getPosition(), &projectileID);
		}
		std::fprintf(stderr, "TEST pkg22 fire frame=%u source=%s weapon=%s target=%s layer=%d ready=%d held=%d remaining=%d projectile=%u damage=%.1f\n",
			frame, source->getTemplate()->getName().str(), weapon->getName().str(), target ? target->getTemplate()->getName().str() : "-",
			target ? static_cast<Int>(target->getLayer()) : -1, ready, held, contain ? contain->getContainCount() : 0,
			static_cast<UnsignedInt>(projectileID), target ? health - target->getBodyModule()->getHealth() : 0.0f);
		const Object* projectile = TheGameLogic->findObjectByID(projectileID);
		if (projectile && target)
		{
			for (BehaviorModule** module = projectile->getBehaviorModules(); *module; ++module)
			{
				const auto* flight = dynamic_cast<const DumbProjectileBehavior*>(*module);
				if (!flight)
					continue;
				Coord3D bone;
				const Bool named = weapon->getTemplate()->getPreferredTargetPosition(target, bone);
				const auto& aim = flight->landingPosition();
				std::fprintf(stderr, "TEST pkg22 aim frame=%u weapon=%s named=%d aim=%.2f,%.2f,%.2f bone=%.2f,%.2f,%.2f\n",
					frame, weapon->getName().str(), named, aim.x, aim.y, aim.z,
					named ? bone.x : 0.0f, named ? bone.y : 0.0f, named ? bone.z : 0.0f);
				break;
			}
		}
	}
}
#endif
