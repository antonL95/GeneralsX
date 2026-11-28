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

#pragma once

#include <vector>

class Bridge;
class LocomotorSet;
class PathfindCell;
class PathfindZoneManager;

/**
 * This class represents a bridge in the map. This is effectively
 * a sub-rectangle of the big pathfind map.
 */
class PathfindLayer
{
public:
	PathfindLayer();
	~PathfindLayer();
public:
	void reset();
	Bool init(Bridge *theBridge, PathfindLayerEnum layer);
	void allocateCells(const IRegion2D *extent);
	void allocateCellsForWallLayer(const IRegion2D *extent, const ObjectID *wallPieces, Int numPieces);
	void rebuildWallCells(const IRegion2D &extent, const std::vector<ObjectID>& wallPieces);
	Real getAuthoredWallHeight(UnsignedByte band) const { return m_wallHeights[band - 1]; }
	void classifyCells();
	void markGroundClearance(const IRegion2D& cells);
	void classifyWallCells(const ObjectID *wallPieces, Int numPieces);
	Bool setDestroyed(Bool destroyed);
	Bool isUnused(); // True if it doesn't contain a bridge.
	Bool isDestroyed() {return m_destroyed;} // True if it has been destroyed.
	PathfindCell *getCell(Int x, Int y);
	Int getZone() {return m_zone;}
	void setZone(Int zone) {m_zone = zone;}
	void applyZone(); // Propagates m_zone to all cells.
	void getStartCellIndex(ICoord2D *start) {*start = m_startCell;}
	void getEndCellIndex(ICoord2D *end) {*end = m_endCell;}

	ObjectID getBridgeID();
	Bool connectsZones(PathfindZoneManager *zm, const LocomotorSet& locomotorSet,Int zone1, Int zone2);
	Bool isPointOnWall(const ObjectID *wallPieces, Int numPieces, const Coord3D *pt);

#if defined(RTS_DEBUG)
	void doDebugIcons() ;
#endif
protected:
	void classifyLayerMapCell( Int i, Int j , PathfindCell *cell, Bridge *theBridge);
	void markGroundClearanceCell(Int i, Int j, const PathfindCell* cell, Bridge* theBridge);
	void classifyWallMapCell( Int i, Int j, PathfindCell *cell, const ObjectID *wallPieces, Int numPieces);

private:
	void classifyWallHeights(const std::vector<ObjectID>& wallPieces);
	std::vector<Real> m_wallHeights;

	PathfindCell *m_blockOfMapCells;		///< Pathfinding map - contains iconic representation of the map
	PathfindCell **m_layerCells;		///< Pathfinding map indexes - contains matrix indexing into the map.
	Int m_width;		// Number of cells in x
	Int m_height;		// Number of cells in y
	Int m_xOrigin;	// Index of first cell in x
	Int m_yOrigin;	// Index of first cell in y
	ICoord2D m_startCell; // pathfind cell indexes for center cell on the from side.
	ICoord2D m_endCell; // pathfind cell indexes for center cell on the to side.

	PathfindLayerEnum m_layer;
	Int m_zone;			// Whole bridge is in same zone.
	Bridge *m_bridge; // Corresponding bridge in TerrainLogic.
	Bool m_destroyed;


};
