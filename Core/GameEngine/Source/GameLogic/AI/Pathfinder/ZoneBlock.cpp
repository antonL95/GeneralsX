/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "Common/Xfer.h"
#include "GameLogic/Pathfinder/PathfindCell.h"
#include "GameLogic/Pathfinder/PathfindZoneManager.h"
#include "GameLogic/Pathfinder/ZoneBlock.h"

ZoneBlock::ZoneBlock() : m_firstZone(0),
m_numZones(0),
m_groundCliffZones(nullptr),
m_groundWaterZones(nullptr),
m_waterDeepWaterZones(nullptr),
m_groundWaterDeepWaterZones(nullptr),
m_groundRubbleZones(nullptr),
m_crusherZones(nullptr),
m_zonesAllocated(0),
m_interactsWithBridge(FALSE)
{
	m_cellOrigin.x = 0;
	m_cellOrigin.y = 0;
	m_firstZone = 0;
	m_markedPassable = TRUE;
}

ZoneBlock::~ZoneBlock()
{
	freeZones();
}

void ZoneBlock::xferConnectivity(Xfer* xfer, zoneStorageType maxZone)
{
	xfer->xferUnsignedShort(&m_firstZone);
	xfer->xferUnsignedShort(&m_numZones);
	xfer->xferBool(&m_interactsWithBridge);
	xfer->xferBool(&m_markedPassable);
	const Int end = m_firstZone + m_numZones;
	if (m_numZones == 0 || m_firstZone > maxZone || end > maxZone + 1)
		throw XFER_INVALID_PARAMETERS;
	if (m_numZones == 1)
		return;
	if (xfer->getXferMode() == XFER_LOAD)
		allocateZones();
	for (auto* table : {m_groundCliffZones, m_groundWaterZones, m_groundRubbleZones, m_crusherZones})
	{
		for (Int i = 0; i < m_numZones; ++i)
		{
			xfer->xferUnsignedShort(&table[i]);
			if (table[i] < m_firstZone || table[i] >= end)
				throw XFER_INVALID_PARAMETERS;
		}
	}
}

void ZoneBlock::xferDeepWaterConnectivity(Xfer* xfer, zoneStorageType maxZone)
{
	const Int end = m_firstZone + m_numZones;
	if (m_numZones == 0 || m_firstZone > maxZone || end > maxZone + 1)
		throw XFER_INVALID_PARAMETERS;
	if (m_numZones == 1)
		return;
	if (!m_waterDeepWaterZones || !m_groundWaterDeepWaterZones)
		throw XFER_INVALID_PARAMETERS;
	for (auto* table : {m_waterDeepWaterZones, m_groundWaterDeepWaterZones})
	{
		for (Int i = 0; i < m_numZones; ++i)
		{
			xfer->xferUnsignedShort(&table[i]);
			if (table[i] < m_firstZone || table[i] >= end)
				throw XFER_INVALID_PARAMETERS;
		}
	}
}

void ZoneBlock::freeZones()
{
	delete [] m_groundCliffZones;
	m_groundCliffZones = nullptr;

	delete [] m_groundWaterZones;
	m_groundWaterZones = nullptr;

	delete [] m_waterDeepWaterZones;
	m_waterDeepWaterZones = nullptr;

	delete [] m_groundWaterDeepWaterZones;
	m_groundWaterDeepWaterZones = nullptr;

	delete [] m_groundRubbleZones;
	m_groundRubbleZones = nullptr;

	delete [] m_crusherZones;
	m_crusherZones = nullptr;
}

/* Allocate zone equivalency arrays large enough to hold required entries.  If the arrays are already
large enough, reuse.  Then calculate terrain equivalencies. */
void ZoneBlock::blockCalculateZones(PathfindCell **map, PathfindLayer layers[], const IRegion2D &bounds)
{
	Int i, j;
	m_cellOrigin = bounds.lo;
	UnsignedInt minZone = map[bounds.lo.x][bounds.lo.y].getZone();
	UnsignedInt maxZone = minZone;

	for( j=bounds.lo.y; j<=bounds.hi.y; j++ )	{
		for( i=bounds.lo.x; i<=bounds.hi.x; i++ )	{
			PathfindCell *cell = &map[i][j];
			zoneStorageType zone = cell->getZone();
			if (minZone>zone) minZone=zone;
			if (maxZone<zone) maxZone=zone;
		}
	}
	m_firstZone = minZone;
	m_numZones = 1 + maxZone - minZone;

	allocateZones();

	if (m_numZones==1) return; // all zones are equivalent.

	for (i=0; i<m_zonesAllocated; i++) {
		m_groundCliffZones[i] = i+m_firstZone;
		m_groundWaterZones[i] = i+m_firstZone;
		m_waterDeepWaterZones[i] = i+m_firstZone;
		m_groundWaterDeepWaterZones[i] = i+m_firstZone;
		m_groundRubbleZones[i] = i+m_firstZone;
		m_crusherZones[i] = i+m_firstZone;
	}

	for( j=bounds.lo.y; j<=bounds.hi.y; j++ )	{
		for( i=bounds.lo.x; i<=bounds.hi.x; i++ )	{
			if (i>bounds.lo.x && map[i][j].getZone()!=map[i-1][j].getZone()) {

				if (PathfindCell::waterGround(map[i][j], map[i-1][j])) {
					applyBlockZone(map[i][j], map[i-1][j], m_groundWaterZones, m_firstZone, m_numZones);
				}
				if (PathfindCell::waterDeepWater(map[i][j], map[i-1][j])) {
					applyBlockZone(map[i][j], map[i-1][j], m_waterDeepWaterZones, m_firstZone, m_numZones);
				}
				if (PathfindCell::groundWaterDeepWater(map[i][j], map[i-1][j])) {
					applyBlockZone(map[i][j], map[i-1][j], m_groundWaterDeepWaterZones, m_firstZone, m_numZones);
				}
				if (PathfindCell::groundRubble(map[i][j], map[i-1][j])) {
					applyBlockZone(map[i][j], map[i-1][j], m_groundRubbleZones, m_firstZone, m_numZones);
				}
				if (PathfindCell::groundCliff(map[i][j], map[i-1][j])) {
					applyBlockZone(map[i][j], map[i-1][j], m_groundCliffZones, m_firstZone, m_numZones);
				}
				if (PathfindCell::crusherGround(map[i][j], map[i-1][j])) {
					applyBlockZone(map[i][j], map[i-1][j], m_crusherZones, m_firstZone, m_numZones);
				}
			}
			if (j>bounds.lo.y && map[i][j].getZone()!=map[i][j-1].getZone()) {
				if (PathfindCell::waterGround(map[i][j],map[i][j-1])) {
					applyBlockZone(map[i][j], map[i][j-1], m_groundWaterZones, m_firstZone, m_numZones);
				}
				if (PathfindCell::waterDeepWater(map[i][j],map[i][j-1])) {
					applyBlockZone(map[i][j], map[i][j-1], m_waterDeepWaterZones, m_firstZone, m_numZones);
				}
				if (PathfindCell::groundWaterDeepWater(map[i][j],map[i][j-1])) {
					applyBlockZone(map[i][j], map[i][j-1], m_groundWaterDeepWaterZones, m_firstZone, m_numZones);
				}
				if (PathfindCell::groundRubble(map[i][j], map[i][j-1])) {
					applyBlockZone(map[i][j], map[i][j-1], m_groundRubbleZones, m_firstZone, m_numZones);
				}
				if (PathfindCell::groundCliff(map[i][j],map[i][j-1])) {
					applyBlockZone(map[i][j], map[i][j-1], m_groundCliffZones, m_firstZone, m_numZones);
				}
				if (PathfindCell::crusherGround(map[i][j], map[i][j-1])) {
					applyBlockZone(map[i][j], map[i][j-1], m_crusherZones, m_firstZone, m_numZones);
				}
			}
			DEBUG_ASSERTCRASH(map[i][j].getZone() != 0, ("Cleared the zone."));
		}
	}

}

//
// Return the zone at this location.
//
zoneStorageType ZoneBlock::getEffectiveZone( LocomotorSurfaceTypeMask acceptableSurfaces,
																					 Bool crusher, zoneStorageType zone) const
{
#if !(RTS_GENERALS && RETAIL_COMPATIBLE_PATHFINDING)
	if (zone==PathfindZoneManager::UNINITIALIZED_ZONE) {
		return zone;
	}
#endif

	if (acceptableSurfaces&LOCOMOTORSURFACE_AIR) return 1; // air is all zone 1.

	constexpr LocomotorSurfaceTypeMask allTerrain = LOCOMOTORSURFACE_GROUND |
		LOCOMOTORSURFACE_CLIFF | LOCOMOTORSURFACE_WATER | LOCOMOTORSURFACE_DEEP_WATER |
		LOCOMOTORSURFACE_RUBBLE | LOCOMOTORSURFACE_OBSTACLE | LOCOMOTORSURFACE_IMPASSABLE;
	if ((acceptableSurfaces & allTerrain) == allTerrain) {
		return 1;
	}
	if (m_numZones<2) {
		return m_firstZone;
	}
	DEBUG_ASSERTCRASH(zone >=m_firstZone && zone < m_firstZone+m_numZones, ("Invalid range."));
	if (zone<m_firstZone || zone >= m_firstZone+m_numZones) {
		return m_firstZone;
	}
	zone -= m_firstZone;
	if (crusher) {
		zone = m_crusherZones[zone];
		DEBUG_ASSERTCRASH(zone >=m_firstZone && zone < m_firstZone+m_numZones, ("Invalid range."));
		zone -= m_firstZone;
	}

	if ((acceptableSurfaces & LOCOMOTORSURFACE_GROUND) &&
			(acceptableSurfaces & LOCOMOTORSURFACE_WATER) &&
			(acceptableSurfaces & LOCOMOTORSURFACE_DEEP_WATER)) {
		zone = m_groundWaterDeepWaterZones[zone];
		DEBUG_ASSERTCRASH(zone >=m_firstZone && zone < m_firstZone+m_numZones, ("Invalid range."));
		return zone;
	}

	if ((acceptableSurfaces & LOCOMOTORSURFACE_WATER) &&
			(acceptableSurfaces & LOCOMOTORSURFACE_DEEP_WATER)) {
		zone = m_waterDeepWaterZones[zone];
		DEBUG_ASSERTCRASH(zone >=m_firstZone && zone < m_firstZone+m_numZones, ("Invalid range."));
		return zone;
	}

	if ( (acceptableSurfaces&LOCOMOTORSURFACE_GROUND) &&
			(acceptableSurfaces&LOCOMOTORSURFACE_CLIFF)) {
		zone = m_groundCliffZones[zone];
		DEBUG_ASSERTCRASH(zone >=m_firstZone && zone < m_firstZone+m_numZones, ("Invalid range."));
		return zone;
	}

	if ( (acceptableSurfaces&LOCOMOTORSURFACE_GROUND) &&
			(acceptableSurfaces&LOCOMOTORSURFACE_WATER)) {
		zone = m_groundWaterZones[zone];
		DEBUG_ASSERTCRASH(zone >=m_firstZone && zone < m_firstZone+m_numZones, ("Invalid range."));
		return zone;
	}

	if ( (acceptableSurfaces&LOCOMOTORSURFACE_GROUND) &&
			(acceptableSurfaces&LOCOMOTORSURFACE_RUBBLE)) {
		zone = m_groundRubbleZones[zone];
		return zone;
	}

	if ( (acceptableSurfaces&LOCOMOTORSURFACE_CLIFF) &&
			(acceptableSurfaces&LOCOMOTORSURFACE_WATER)) {
		DEBUG_CRASH(("Cliff water only locomotor sets not supported yet."));
	}

	return zone+m_firstZone;
}


/* Allocate zone equivalency arrays large enough to hold m_maxZone entries.  If the arrays are already
large enough, just return. */
void ZoneBlock::allocateZones()
{
	if (m_zonesAllocated>m_numZones && m_groundCliffZones!=nullptr) {
		return;
	}
	freeZones();

	if (m_numZones==1) {
		return;
	}

	if (m_zonesAllocated == 0) {
		m_zonesAllocated = 4;
	}
	while (m_zonesAllocated <= m_numZones) {
		m_zonesAllocated *= 2;
	}
	m_groundCliffZones = MSGNEW("PathfindZoneInfo") zoneStorageType [m_zonesAllocated];
	m_groundWaterZones = MSGNEW("PathfindZoneInfo") zoneStorageType[m_zonesAllocated];
	m_waterDeepWaterZones = MSGNEW("PathfindZoneInfo") zoneStorageType[m_zonesAllocated];
	m_groundWaterDeepWaterZones = MSGNEW("PathfindZoneInfo") zoneStorageType[m_zonesAllocated];
	m_groundRubbleZones = MSGNEW("PathfindZoneInfo") zoneStorageType[m_zonesAllocated];
	m_crusherZones = MSGNEW("PathfindZoneInfo") zoneStorageType[m_zonesAllocated];
}

void __fastcall ZoneBlock::resolveBlockZones(Int srcZone, Int targetZone, zoneStorageType* zoneEquivalency, Int sizeOfZE)
{
	Int i;
	// We have two zones being combined now. Keep the lower zone.
	DEBUG_ASSERTCRASH(srcZone != 0 && targetZone != 0, ("Bad resolve zones	."));
	if (targetZone < srcZone)
	{
		for (i = 0; i < sizeOfZE; i++)
		{
			if (zoneEquivalency[i] == srcZone)
			{
				zoneEquivalency[i] = targetZone;
			}
		}
	}
	else
	{
		for (i = 0; i < sizeOfZE; i++)
		{
			if (zoneEquivalency[i] == targetZone)
			{
				zoneEquivalency[i] = srcZone;
			}
		}
	}
}

void __fastcall ZoneBlock::resolveZones(Int srcZone, Int targetZone, zoneStorageType* zoneEquivalency, Int sizeOfZE)
{
	Int i;
	// We have two zones being combined now. Keep the lower zone.
	DEBUG_ASSERTCRASH(srcZone != 0 && targetZone != 0, ("Bad resolve zones	."));
	DEBUG_ASSERTCRASH(srcZone < sizeOfZE && targetZone < sizeOfZE, ("Bad resolve zones	."));
	srcZone = zoneEquivalency[srcZone];
	targetZone = zoneEquivalency[targetZone];
	DEBUG_ASSERTCRASH(srcZone < sizeOfZE && targetZone < sizeOfZE, ("Bad resolve zones	."));
	zoneStorageType finalZone;
	if (targetZone < srcZone)
	{
		finalZone = zoneEquivalency[targetZone];
	}
	else
	{
		finalZone = zoneEquivalency[srcZone];
	}
	DEBUG_ASSERTCRASH(finalZone < sizeOfZE, ("Bad resolve zones	."));
	for (i = 0; i < sizeOfZE; i++)
	{
		zoneStorageType ze = zoneEquivalency[i];
		if (ze == targetZone || ze == srcZone)
		{
			zoneEquivalency[i] = finalZone;
		}
	}
}

void ZoneBlock::flattenZones(zoneStorageType* zoneArray, zoneStorageType* zoneHierarchical, Int sizeOfZones)
{
	Int i;
	for (i = 0; i < sizeOfZones; i++)
	{
		Int zone1 = zoneArray[i];
		Int zone2 = zoneHierarchical[zone1];
		zone1 = zoneArray[zone2];
		zone2 = zoneHierarchical[zone1];
		zoneArray[i] = zone2;
	}
#if 1

	for (i = 0; i < sizeOfZones; i++)
	{
		Int zone1 = zoneArray[i];
		Int zone2 = zoneHierarchical[i];
		if (zone1 != zone2)
		{
			resolveZones(zone1, zone2, zoneArray, sizeOfZones);
		}
	}
#endif
}

void ZoneBlock::applyZone(PathfindCell& targetCell, const PathfindCell& sourceCell, zoneStorageType* zoneEquivalency, Int sizeOfZE)
{
	DEBUG_ASSERTCRASH(sourceCell.getZone() != 0, ("Unset source zone."));
	Int srcZone = zoneEquivalency[sourceCell.getZone()];
	Int targetZone = zoneEquivalency[targetCell.getZone()];

	if (targetZone == 0)
	{
		targetCell.setZone(srcZone);
		return;
	}
	if (targetZone == srcZone)
	{
		return;    // already match.
	}
	resolveZones(srcZone, targetZone, zoneEquivalency, sizeOfZE);
}

void ZoneBlock::applyBlockZone(PathfindCell& targetCell, const PathfindCell& sourceCell, zoneStorageType* zoneEquivalency, Int firstZone, Int sizeOfZE)
{
	DEBUG_ASSERTCRASH(sourceCell.getZone() >= firstZone && sourceCell.getZone() < firstZone + sizeOfZE, ("Memory overrun - FATAL ERROR."));
	Int srcZone = zoneEquivalency[sourceCell.getZone() - firstZone];
	DEBUG_ASSERTCRASH(targetCell.getZone() >= firstZone && sourceCell.getZone() < firstZone + sizeOfZE, ("Memory overrun - FATAL ERROR."));
	Int targetZone = zoneEquivalency[targetCell.getZone() - firstZone];
	if (targetZone == srcZone)
	{
		return;    // already match.
	}
	resolveBlockZones(srcZone, targetZone, zoneEquivalency, sizeOfZE);
}
