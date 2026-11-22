#pragma once

#include "Common/RetailFileLayers.h"
#include "GameLogic/Object.h"

inline Bool needsRetailSpawnFootprint(const Object& object)
{
	return RetailFileLayers::enabled() && !object.getEnclosingContainedBy() &&
		(object.isKindOf(KINDOF_STRUCTURE) || object.isKindOf(KINDOF_WALK_ON_TOP_OF_WALL));
}
