#include "PreRTS.h"

#include "GameLogic/Module/AnimalAIUpdate.h"

#include <algorithm>

#include "Common/GlobalData.h"
#include "Common/INI.h"
#include "Common/RandomValue.h"
#include "Common/Xfer.h"
#include "GameClient/FXList.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Object.h"
#include "GameLogic/ObjectCreationList.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/TerrainLogic.h"
#include "WWMath/wwmath.h"

void AnimalAIUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	AIUpdateModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "FleeRange", INI::parseInt, nullptr, offsetof(AnimalAIUpdateModuleData, m_fleeRange) },
		{ "FleeDistance", INI::parseInt, nullptr, offsetof(AnimalAIUpdateModuleData, m_fleeDistance) },
		{ "WanderPercentage", INI::parseInt, nullptr, offsetof(AnimalAIUpdateModuleData, m_wanderPercentage) },
		{ "MaxWanderDistance", INI::parseInt, nullptr, offsetof(AnimalAIUpdateModuleData, m_maxWanderDistance) },
		{ "MaxWanderRadius", INI::parseInt, nullptr, offsetof(AnimalAIUpdateModuleData, m_maxWanderRadius) },
		{ "UpdateTimer", INI::parseInt, nullptr, offsetof(AnimalAIUpdateModuleData, m_updateTimer) },
		{ "AfraidOfCastles", INI::parseBool, nullptr, offsetof(AnimalAIUpdateModuleData, m_afraidOfCastles) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields, 0);
}

AnimalAIUpdate::AnimalAIUpdate(Thing* thing, const ModuleData* data) : AIUpdateInterface(thing, data)
{
}

AnimalAIUpdate::~AnimalAIUpdate() = default;

Object* AnimalAIUpdate::findThreat(Real range) const
{
	const Object* animal = getObject();
	SimpleObjectIterator* nearby = ThePartitionManager->iterateObjectsInRange(animal, range, FROM_CENTER_2D);
	Object* closest = nullptr;
	Real closestDistance = range * range;
	for (Object* object = nearby->first(); object; object = nearby->next())
	{
		if (object == animal || object->isEffectivelyDead() || object->isKindOf(KINDOF_INERT))
			continue;
		const Bool unit = object->isMobile() && (object->isKindOf(KINDOF_INFANTRY) || object->isKindOf(KINDOF_VEHICLE) ||
			object->isKindOf(KINDOF_CAVALRY) || object->isKindOf(KINDOF_MONSTER) || object->isKindOf(KINDOF_HORDE));
		const Bool castle = getAnimalAIUpdateModuleData()->m_afraidOfCastles &&
			(object->isKindOf(KINDOF_CASTLE_CENTER) || object->isKindOf(KINDOF_CASTLE_KEEP));
		if (!castle && (animal->getRelationship(object) == ALLIES || !unit))
			continue;
		const Real dx = animal->getPosition()->x - object->getPosition()->x;
		const Real dy = animal->getPosition()->y - object->getPosition()->y;
		const Real distance = dx * dx + dy * dy;
		if (distance < closestDistance || (distance == closestDistance && (!closest || object->getID() < closest->getID())))
		{
			closest = object;
			closestDistance = distance;
		}
	}
	deleteInstance(nearby);
	return closest;
}

void AnimalAIUpdate::fleeFrom(const Object& threat)
{
	Object* animal = getObject();
	const Coord3D& at = *animal->getPosition();
	Coord3D direction = { at.x - threat.getPosition()->x, at.y - threat.getPosition()->y, 0.0f };
	const Real length = WWMath::Sqrt(direction.x * direction.x + direction.y * direction.y);
	if (length > 0.0f)
	{
		direction.x /= length;
		direction.y /= length;
	}
	else
		direction = *animal->getUnitDirectionVector2D();

	const Real distance = static_cast<Real>(getAnimalAIUpdateModuleData()->m_fleeDistance);
	Coord3D goal = { at.x + direction.x * distance, at.y + direction.y * distance, at.z };
	chooseLocomotorSet(LOCOMOTORSET_PANIC);
	animal->setModelConditionState(MODELCONDITION_PANICKING);
	m_fleeFrom = threat.getID();
	privateMoveToPosition(&goal, CMD_FROM_SCRIPT);
}

void AnimalAIUpdate::wander()
{
	const auto* data = getAnimalAIUpdateModuleData();
	const Coord3D at = *getObject()->getPosition();
	const Real radius = static_cast<Real>(data->m_maxWanderRadius);
	const Real dx = at.x - m_home.x;
	const Real dy = at.y - m_home.y;
	if (dx * dx + dy * dy > radius * radius)
	{
		privateMoveToPosition(&m_home, CMD_FROM_SCRIPT);
		return;
	}
	if (data->m_maxWanderDistance <= 0 || radius <= 0.0f ||
		GameLogicRandomValue(0, 100) >= data->m_wanderPercentage)
		return;

	const Real angle = GameLogicRandomValueReal(-PI, PI);
	const Real distance = static_cast<Real>(GameLogicRandomValue(1, data->m_maxWanderDistance));
	Coord3D goal = { at.x + distance * WWMath::CosTrig(angle), at.y + distance * WWMath::SinTrig(angle), at.z };
	const Real goalX = goal.x - m_home.x;
	const Real goalY = goal.y - m_home.y;
	const Real goalDistance = WWMath::Sqrt(goalX * goalX + goalY * goalY);
	if (goalDistance > radius)
	{
		goal.x = m_home.x + goalX * (radius / goalDistance);
		goal.y = m_home.y + goalY * (radius / goalDistance);
	}
	privateMoveToPosition(&goal, CMD_FROM_SCRIPT);
}

UpdateSleepTime AnimalAIUpdate::update()
{
	const UpdateSleepTime sleep = AIUpdateInterface::update();
	Object* animal = getObject();
	if (animal->isEffectivelyDead() || !getCurLocomotor())
		return sleep;
	if (!m_hasHome)
	{
		m_home = *animal->getPosition();
		m_hasHome = TRUE;
	}

	const auto* data = getAnimalAIUpdateModuleData();
	const UnsignedInt frame = TheGameLogic->getFrame();
	const Int timer = data->m_updateTimer;
	if (timer > 0 && frame % static_cast<UnsignedInt>(timer) == 0 && data->m_fleeRange > 0 && data->m_fleeDistance > 0)
	{
		if (const Object* threat = findThreat(static_cast<Real>(data->m_fleeRange)))
		{
			if (m_fleeFrom != threat->getID() || isIdle())
				fleeFrom(*threat);
			return UPDATE_SLEEP_NONE;
		}
	}
	if (isIdle())
	{
		if (m_fleeFrom != INVALID_ID)
		{
			chooseLocomotorSet(LOCOMOTORSET_NORMAL);
			animal->clearModelConditionState(MODELCONDITION_PANICKING);
			m_fleeFrom = INVALID_ID;
		}
		wander();
	}
	return UPDATE_SLEEP_NONE;
}

void AnimalAIUpdate::xferAnimalState(Xfer* xfer)
{
	xfer->xferCoord3D(&m_home);
	xfer->xferObjectID(&m_fleeFrom);
	xfer->xferBool(&m_hasHome);
}

void AnimalAIUpdate::crc(Xfer* xfer)
{
	AIUpdateInterface::crc(xfer);
	xferAnimalState(xfer);
}

void AnimalAIUpdate::xfer(Xfer* xfer)
{
	XferVersion version = 1;
	xfer->xferVersion(&version, 1);
	AIUpdateInterface::xfer(xfer);
	xferAnimalState(xfer);
}

void AnimalAIUpdate::loadPostProcess()
{
	AIUpdateInterface::loadPostProcess();
}

namespace
{
template<class Asset, INIFieldParseProc parse>
void parseEmitterAsset(INI* ini, void* instance, void* store, const void* userData)
{
	const Asset* asset = nullptr;
	parse(ini, instance, &asset, userData);
	if (asset)
		static_cast<std::vector<const Asset*>*>(store)->push_back(asset);
}
}

void CritterEmitterUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	CollideModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "FX", parseEmitterAsset<FXList, INI::parseFXList>, nullptr, offsetof(CritterEmitterUpdateModuleData, m_effects) },
		{ "SpawnObject", parseEmitterAsset<ObjectCreationList, INI::parseObjectCreationList>, nullptr, offsetof(CritterEmitterUpdateModuleData, m_spawns) },
		{ "ReloadTime", INI::parseDurationUnsignedInt, nullptr, offsetof(CritterEmitterUpdateModuleData, m_reloadTime) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields, 0);
}

CritterEmitterUpdate::CritterEmitterUpdate(Thing* thing, const ModuleData* data) : CollideModule(thing, data)
{
}

CritterEmitterUpdate::~CritterEmitterUpdate() = default;

Bool CritterEmitterUpdate::wouldLikeToCollideWith(const Object* other) const
{
	return other && other->isMobile() && !other->isEffectivelyDead() &&
		!other->isKindOf(KINDOF_INERT) && !other->isSignificantlyAboveTerrain() &&
		TheGameLogic->getFrame() >= m_nextEmission;
}

void CritterEmitterUpdate::onCollide(Object* other, const Coord3D*, const Coord3D*)
{
	if (!wouldLikeToCollideWith(other))
		return;
	const auto* data = getCritterEmitterUpdateModuleData();
	m_nextEmission = TheGameLogic->getFrame() + std::max(1u, data->m_reloadTime);
	if (!data->m_effects.empty())
	{
		const Int choice = GameLogicRandomValue(0, static_cast<Int>(data->m_effects.size()) - 1);
		FXList::doFXObj(data->m_effects[choice], getObject());
	}
	if (!data->m_spawns.empty())
	{
		const Int choice = GameLogicRandomValue(0, static_cast<Int>(data->m_spawns.size()) - 1);
		ObjectCreationList::create(data->m_spawns[choice], getObject(), static_cast<const Object*>(nullptr));
	}
}

void CritterEmitterUpdate::crc(Xfer* xfer)
{
	CollideModule::crc(xfer);
	xfer->xferUnsignedInt(&m_nextEmission);
}

void CritterEmitterUpdate::xfer(Xfer* xfer)
{
	XferVersion version = 1;
	xfer->xferVersion(&version, 1);
	CollideModule::xfer(xfer);
	xfer->xferUnsignedInt(&m_nextEmission);
}

void CritterEmitterUpdate::loadPostProcess()
{
	CollideModule::loadPostProcess();
}
