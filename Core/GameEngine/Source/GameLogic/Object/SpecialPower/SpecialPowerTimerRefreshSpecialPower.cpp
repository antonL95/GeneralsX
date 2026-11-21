#include "PreRTS.h"

#include "GameLogic/Module/SpecialPowerTimerRefreshSpecialPower.h"

#include "Common/Player.h"
#include "Common/ThingTemplate.h"
#include "Common/SpecialPower.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"

SpecialPowerTimerRefreshSpecialPower::~SpecialPowerTimerRefreshSpecialPower() = default;

void SpecialPowerTimerRefreshSpecialPower::applyPowerEffect(const Coord3D* position)
{
	Object* caster = getObject();
	const SpecialPowerModuleData* data = getPowerModuleData();
	const Coord3D center = position ? *position : *caster->getPosition();
	const Player* owner = caster->getControllingPlayer();
	const Real radius = data->m_attributeModifierRange;
	if (data->m_setModelConditionTime > 0.0f)
		caster->setPowerModelCondition(data->m_setModelCondition, static_cast<UnsignedInt>(data->m_setModelConditionTime * LOGICFRAMES_PER_SECOND));

	if (radius >= 0.0f)
	{
		SimpleObjectIterator* objects = ThePartitionManager->iterateObjectsInRange(&center, radius, FROM_CENTER_2D,
			nullptr, ITER_FASTEST);
		for (Object* target = objects->first(); target; target = objects->next())
		{
			if (target == caster || target->isEffectivelyDead() ||
				!data->m_attributeModifierAffects.matchesTemplate(*target->getTemplate()))
				continue;

			const Bool sameOwner = target->getControllingPlayer() == owner;
			const Relationship relation = caster->getRelationship(target);
			if (!sameOwner && !data->m_targetAllSides &&
				!(data->m_affectAllies && relation == ALLIES) &&
				!(data->m_targetEnemy && relation == ENEMIES))
				continue;
			if (!sameOwner && (data->m_attributeModifierAffects.m_rules & (1u << 13)))
				continue;

			for (BehaviorModule** mod = target->getBehaviorModules(); *mod; ++mod)
			{
				SpecialPowerModuleInterface* power = (*mod)->getSpecialPower();
				if (power && power->getSpecialPowerTemplate() != getSpecialPowerTemplate())
					power->refreshPowerTimer();
			}
		}
		deleteInstance(objects);
	}

	if (data->m_attributeModifierAffectsSelf)
	{
		for (BehaviorModule** mod = caster->getBehaviorModules(); *mod; ++mod)
		{
			SpecialPowerModuleInterface* power = (*mod)->getSpecialPower();
			if (power && power->getSpecialPowerTemplate() != getSpecialPowerTemplate())
				power->refreshPowerTimer();
		}
	}
}
