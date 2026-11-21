#include "PreRTS.h"

#include "Common/Player.h"
#include "Common/SpecialPower.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/TerrainLogic.h"

#include <cmath>

Bool SpecialPowerTemplate::isValidRetailTarget(const Object* source, const Object* target, const Coord3D* position,
	CommandSourceType commandSource) const
{
	constexpr UnsignedInt needsTarget = 1u << 0;
	constexpr UnsignedInt waterOk = 1u << 1;
	constexpr UnsignedInt needsObjectFilter = 1u << 2;
	constexpr UnsignedInt limitDistance = 1u << 3;
	constexpr UnsignedInt noForbiddenObjects = 1u << 4;
	constexpr UnsignedInt pathableOnly = 1u << 6;
	const SpecialPowerTemplate* power = getFO();
	const UnsignedInt flags = power->m_flags;

	if (source->getStatusBits().anyIntersectionWith(power->m_preventActivationConditions))
		return FALSE;
	if (target)
		position = target->getPosition();
	if (!position)
		return !(flags & (needsTarget | needsObjectFilter));
	if (!std::isfinite(position->x) || !std::isfinite(position->y) || !std::isfinite(position->z))
		return FALSE;
	Region3D map;
	TheTerrainLogic->getExtent(&map);
	if (!map.containsWithin(position->asCoord2D()))
		return FALSE;

	if (!target)
	{
		if (!(flags & waterOk) && TheTerrainLogic->isUnderwater(position->x, position->y))
			return FALSE;
		const Bool revealsShroud = power->m_type == SPECIAL_REVEAL_MAP_AREA || power->m_type == SPECIAL_SPELL_BOOK_PALANTIR_VISION ||
			power->m_type == SPECIAL_SPELL_BOOK_EYE_OF_SAURON;
		if (!revealsShroud && commandSource != CMD_FROM_SCRIPT &&
			ThePartitionManager->getShroudStatusForPlayer(source->getControllingPlayer()->getPlayerIndex(), position) == CELLSHROUD_SHROUDED)
			return FALSE;
	}
	if (flags & pathableOnly)
	{
		Pathfinder* pathfinder = TheAI->pathfinder();
		if (!pathfinder->validMovementPosition(FALSE, LOCOMOTORSURFACE_GROUND, pathfinder->getCell(LAYER_GROUND, position)))
			return FALSE;
	}
	if ((flags & limitDistance) && power->m_maxCastRange > 0.0f)
	{
		const Coord3D* origin = source->getPosition();
		const Real dx = origin->x - position->x;
		const Real dy = origin->y - position->y;
		if (dx * dx + dy * dy > power->m_maxCastRange * power->m_maxCastRange)
			return FALSE;
	}
	if ((flags & noForbiddenObjects) && power->m_forbiddenObjectRange > 0.0f)
	{
		SimpleObjectIterator* nearby = ThePartitionManager->iterateObjectsInRange(position, power->m_forbiddenObjectRange, FROM_CENTER_2D);
		MemoryPoolObjectHolder hold(nearby);
		for (Object* object = nearby->first(); object; object = nearby->next())
			if (object != source && !object->isEffectivelyDead() && power->m_forbiddenObjectFilter.matchesObject(source, object))
				return FALSE;
	}
	if (!(flags & needsObjectFilter))
		return TRUE;
	if (target)
		return power->m_objectFilter.matchesObject(source, target);
	if (power->m_radiusCursorRadius <= 0.0f)
		return FALSE;

	SimpleObjectIterator* nearby = ThePartitionManager->iterateObjectsInRange(position, power->m_radiusCursorRadius, FROM_CENTER_2D);
	MemoryPoolObjectHolder hold(nearby);
	for (Object* object = nearby->first(); object; object = nearby->next())
		if (!object->isEffectivelyDead() && power->m_objectFilter.matchesObject(source, object))
			return TRUE;
	return FALSE;
}
