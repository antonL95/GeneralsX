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

#include "Common/CRCDebug.h"
#include "Common/RetailFileLayers.h"
#include "Common/RetailSaveBlocks.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/Object.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Pathfinder/PathfindCell.h"
#include "GameLogic/Pathfinder/Path.h"
#include "GameLogic/Pathfinder/PathNode.h"
#include "GameLogic/TerrainLogic.h"
#include "WWMath/wwmath.h"
#include <vector>

//-----------------------------------------------------------------------------------
Path::Path():
m_path(nullptr),
m_pathTail(nullptr),
m_isOptimized(FALSE),
m_blockedByAlly(FALSE),
m_cpopRecentStart(nullptr),
m_cpopCountdown(MAX_CPOP),
m_cpopValid(FALSE)
{
	m_cpopIn.zero();
	m_cpopOut.distAlongPath=0;
	m_cpopOut.layer = LAYER_GROUND;
	m_cpopOut.posOnPath.zero();
}

Path::~Path()
{
	PathNode *node, *nextNode;

	// delete all of the path nodes
	for( node = m_path; node; node = nextNode )
	{
		nextNode = node->getNext();
		deleteInstance(node);
	}
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void Path::crc( Xfer *xfer )
{
	if (RetailFileLayers::enabled())
		xferRetailBlocks(xfer, TRUE);
}

void Path::xferRetailBlocks(Xfer* xfer, Bool present)
{
	RetailSaveBlocks::transferSection<Xfer>(xfer, xfer->getXferMode() == XFER_LOAD, present, {
		{"path-generated-arcs", [this](Xfer* transfer) {
			Int count = 0;
			for (PathNode* node = m_path; node; node = node->getNext())
				++count;
			Int savedCount = count;
			transfer->xferInt(&savedCount);
			if (savedCount != count)
				throw XFER_INVALID_PARAMETERS;
			for (PathNode* node = m_path; node; node = node->getNext())
			{
				Bool generated = node->isGeneratedArc();
				transfer->xferBool(&generated);
				if (transfer->getXferMode() == XFER_LOAD)
					node->setGeneratedArc(generated);
			}
		}, [this] {
			for (PathNode* node = m_path; node; node = node->getNext())
				node->setGeneratedArc(FALSE);
		}}
	});
}

// ------------------------------------------------------------------------------------------------
/** Xfer Method */
// ------------------------------------------------------------------------------------------------
void Path::xfer( Xfer *xfer )
{
  // version
  const XferVersion currentVersion = RetailFileLayers::enabled() ? 3 : 1;
  XferVersion version = currentVersion;
  xfer->xferVersion( &version, currentVersion );

	PathNode *node = m_path;
	Int count = 0;
	while (node) {
		count++;
		node = node->getNext();
	}
	xfer->xferInt(&count);

	if (xfer->getXferMode() == XFER_SAVE)	{
		node = m_pathTail;  // Write them out backwards.
		while (node) {
			node->m_id = count;
			xfer->xferInt(&count);
			Coord3D pos = *node->getPosition();
			xfer->xferCoord3D(&pos);
			PathfindLayerEnum layer = node->getLayer();
			xfer->xferUser(&layer, sizeof(layer));
			Bool canOpt = node->getCanOptimize();
			xfer->xferBool(&canOpt);
			Int id = -1;
			if (node->getNextOptimized()) {
				id = node->getNextOptimized()->m_id;
			}
			xfer->xferInt(&id);
			count--;
			node = node->getPrevious();
		}
		DEBUG_ASSERTCRASH(count==0, ("Wrong data count"));
	} else {
		m_cpopValid = FALSE;
		while (count) {
			Int nodeId;
			xfer->xferInt(&nodeId);
			DEBUG_ASSERTCRASH(nodeId==count, ("Bad data"));
			Coord3D pos;
			xfer->xferCoord3D(&pos);
			PathfindLayerEnum layer;
			xfer->xferUser(&layer, sizeof(layer));
			Bool canOpt;
			xfer->xferBool(&canOpt);
			Int optID = -1;
			xfer->xferInt(&optID);
			PathNode *node = newInstance(PathNode);
			node->m_id = nodeId;
			node->setPosition(&pos);
			node->setLayer(layer);
			node->setCanOptimize(canOpt);
			PathNode *optNode = nullptr;
			if (optID > 0) {
				optNode = m_path;
				while (optNode && optNode->m_id != optID) {
					optNode = optNode->getNext();
				}
				DEBUG_ASSERTCRASH (optNode && optNode->m_id == optID, ("Could not find optimized link."));
			}
			m_path = node->prependToList(m_path);
			if (m_pathTail == nullptr)
				m_pathTail = node;
			if (optNode) {
				node->setNextOptimized(optNode);
			}
			count--;
		}
	}

	xfer->xferBool(&m_isOptimized);
	Int obsolete1 = 0;
	xfer->xferInt(&obsolete1);
	UnsignedInt obsolete2 = 0;
	xfer->xferUnsignedInt(&obsolete2);
	xfer->xferBool(&m_blockedByAlly);

	if (version >= 2)
	{
		xfer->xferBool(&m_cpopValid);
		xfer->xferInt(&m_cpopCountdown);
		xfer->xferCoord3D(&m_cpopIn);
		xfer->xferCoord3D(&m_cpopOut.posOnPath);
		xfer->xferReal(&m_cpopOut.distAlongPath);
		xfer->xferUser(&m_cpopOut.layer, sizeof(m_cpopOut.layer));

		Int recentID = m_cpopRecentStart ? m_cpopRecentStart->m_id : 0;
		xfer->xferInt(&recentID);
		if (xfer->getXferMode() == XFER_LOAD)
		{
			m_cpopRecentStart = nullptr;
			if (recentID)
			{
				const PathNode *recent = m_path;
				while (recent && recent->m_id != recentID)
					recent = recent->getNext();
				if (!recent)
					throw XFER_INVALID_PARAMETERS;
				m_cpopRecentStart = recent;
			}
		}
	}

	if (RetailFileLayers::enabled())
		xferRetailBlocks(xfer, version >= 3);

#if defined(RTS_DEBUG)
	if (TheGlobalData->m_debugAI == AI_DEBUG_PATHS)
	{
		extern void addIcon(const Coord3D *pos, Real width, Int numFramesDuration, RGBColor color);
 		RGBColor color;
		color.blue = 0;
		color.red = color.green = 1;
		Coord3D pos;
		addIcon(nullptr, 0, 0, color); // erase feedback.
		for( PathNode *node = getFirstNode(); node; node = node->getNext() )
		{

			// create objects to show path - they decay

			pos = *node->getPosition();
			addIcon(&pos, PATHFIND_CELL_SIZE_F*.25f, 200, color);
		}

		// show optimized path
		for( node = getFirstNode(); node; node = node->getNextOptimized() )
		{
			pos = *node->getPosition();
			addIcon(&pos, PATHFIND_CELL_SIZE_F*.8f, 200, color);
		}
		TheAI->pathfinder()->setDebugPath(this);
	}
#endif
}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void Path::loadPostProcess()
{
}

/**
 * Create a new node at the head of the path
 */
void Path::prependNode( const Coord3D *pos, PathfindLayerEnum layer )
{
	PathNode *node = newInstance(PathNode);

	node->setPosition( pos );
	node->setLayer(layer);

	m_path = node->prependToList( m_path );

	if (m_pathTail == nullptr)
		m_pathTail = node;

	m_isOptimized = false;
}

/**
 * Create a new node at the tail of the path
 */
void Path::appendNode( const Coord3D *pos, PathfindLayerEnum layer )
{
	if (m_isOptimized && m_pathTail)
	{
		/* Check for duplicates. */
		if (pos->x == m_pathTail->getPosition()->x && pos->y == m_pathTail->getPosition()->y) {
			DEBUG_LOG(("Warning - Path Seg length == 0, ignoring. john a."));
			return;
		}
	}
	PathNode *node = newInstance(PathNode);

	node->setPosition( pos );
	node->setLayer(layer);

	if (!m_path)
	{
		m_path = node;
		m_pathTail = node;

		return;
	}

	m_pathTail->append(node);

	if (m_isOptimized)
	{
		m_pathTail->setNextOptimized(node);
	}

	m_pathTail = node;
}
/**
 * Create a new node at the tail of the path
 */
void Path::updateLastNode( const Coord3D *pos )
{
	PathfindLayerEnum layer = TheTerrainLogic->getLayerForDestination(pos);
	if (m_pathTail) {
		m_pathTail->setPosition(pos);
		m_pathTail->setLayer(layer);
	}
	if (m_isOptimized && m_pathTail)
	{
		PathNode *node = m_path;
		while(node && node->getNextOptimized() != m_pathTail) {
			node = node->getNextOptimized();
		}
		if (node && node->getNextOptimized() == m_pathTail) {
			node->setNextOptimized(m_pathTail);
		}
	}
}

/**
 * Optimize the path by checking line of sight
 */
void Path::optimize( const Object *obj, LocomotorSurfaceTypeMask acceptableSurfaces, Bool blocked )
{
	PathNode *node, *anchor;

	// start with first node in the path
	anchor = getFirstNode();

	Bool firstNode = true;
	PathfindLayerEnum firstLayer = anchor->getLayer();

	// backwards.

	//
	// For each node in the path, check LOS from last node in path, working forward.
	// When a clear LOS is found, keep the resulting straight line segment.
	//
	while( anchor != getLastNode() )
	{
		// find the farthest node in the path that has a clear line-of-sight to this anchor
		Bool optimizedSegment = false;
		PathfindLayerEnum layer = anchor->getLayer();
		PathfindLayerEnum curLayer = anchor->getLayer();
		Int count = 0;
		const Int ALLOWED_STEPS = 3; // we can optimize 3 steps to or from a bridge.  Otherwise, we need to insert a point. jba.
		for (node = anchor->getNext(); node->getNext(); node=node->getNext()) {
			count++;
			if (curLayer==LAYER_GROUND) {
				if (node->getLayer() != curLayer) {
					layer = node->getLayer();
					curLayer = layer;
					if (count > ALLOWED_STEPS) break;
				}
			}	else {
				if (node->getNext()->getLayer() != curLayer) {
					if (count > ALLOWED_STEPS) break;
				}
			}
			curLayer = node->getLayer();
			if (node->getCanOptimize()==false) {
				break;
			}
		}
		if (firstNode) {
			layer = firstLayer;
			firstNode = false;
		}
		//PathfindLayerEnum curLayer = LAYER_GROUND;
		for( ; node != anchor; node = node->getPrevious() )
		{
			Bool isPassable = false;
			//CRCDEBUG_LOG(("Path::optimize() calling isLinePassable()"));
			if (TheAI->pathfinder()->isLinePassable( obj, acceptableSurfaces, layer, *anchor->getPosition(),
				*node->getPosition(), blocked, false))
			{
				isPassable = true;
			}
			PathfindCell* cell = TheAI->pathfinder()->getCell( layer, node->getPosition());
			if (cell && cell->getType()==PathfindCell::CELL_CLIFF && !cell->getPinched()) {
				isPassable = true;
			}
			// Horizontal, diagonal, and vertical steps are passable.
			if (!isPassable) {
				Int dx = node->getPosition()->x - anchor->getPosition()->x;
				Int dy = node->getPosition()->y - anchor->getPosition()->y;
				Bool mightBePassable = false;
				if (IABS(dx)==PATHFIND_CELL_SIZE && IABS(dy)==PATHFIND_CELL_SIZE) {
					isPassable = true;
				}
				PathNode *tmpNode;
				if (dx==0) {
					mightBePassable = true;
					for (tmpNode = node->getPrevious(); tmpNode && tmpNode != anchor; tmpNode = tmpNode->getPrevious()) {
						dx = tmpNode->getNext()->getPosition()->x - tmpNode->getPosition()->x;
						if (dx!=0) mightBePassable = false;
					}
				}
				if (dy==0) {
					mightBePassable = true;
					for (tmpNode = node->getPrevious(); tmpNode && tmpNode != anchor; tmpNode = tmpNode->getPrevious()) {
						dy = tmpNode->getNext()->getPosition()->y - tmpNode->getPosition()->y;
						if (dy!=0) mightBePassable = false;
					}
				}
				if (dx == dy) {
					mightBePassable = true;
					for (tmpNode = node->getPrevious(); tmpNode &&   tmpNode != anchor; tmpNode = tmpNode->getPrevious()) {
						dx = tmpNode->getNext()->getPosition()->x - tmpNode->getPosition()->x;
						dy = tmpNode->getNext()->getPosition()->y - tmpNode->getPosition()->y;
						if (dy!=dx) mightBePassable = false;
					}
				}
				if (dx == -dy) {
					mightBePassable = true;
					for (tmpNode = node->getPrevious(); tmpNode &&   tmpNode != anchor; tmpNode = tmpNode->getPrevious()) {
						dx = tmpNode->getNext()->getPosition()->x - tmpNode->getPosition()->x;
						dy = tmpNode->getNext()->getPosition()->y - tmpNode->getPosition()->y;
						if (dy!=-dx) mightBePassable = false;
					}
				}
				if (mightBePassable) {
					isPassable = true;
				}
			}
			if (isPassable)
			{
				// anchor can directly see this node, make it next in the optimized path
				anchor->setNextOptimized( node );
				anchor = node;
				optimizedSegment = true;
				break;
			}
		}

		if (optimizedSegment == false)
		{
			// for some reason, there is no clear LOS between the anchor node and the very next node
			anchor->setNextOptimized( anchor->getNext() );
			anchor = anchor->getNext();
		}
	}

	if (obj && obj->getAI() && obj->getPhysics() && m_path && m_path->getNextOptimized())
	{
		const AIUpdateInterface* ai = obj->getAI();
		const Locomotor* locomotor = ai->getCurLocomotor();
		if (locomotor)
		{
			Region3D pathExtent;
			TheTerrainLogic->getMaximumPathfindExtent(&pathExtent);
			PathNode* firstTarget = m_path->getNextOptimized();
			const Coord3D& velocity = *obj->getPhysics()->getVelocity();
			const Real speed = WWMath::SqrtfOrigin(velocity.x * velocity.x + velocity.y * velocity.y);
			const Bool fast = speed > ai->getCurLocomotorSpeed() * 0.25f;
			Real radius = fast ? locomotor->getFastTurnRadius() : locomotor->getSlowTurnRadius();
			Real maxTurn = locomotor->getMaxTurnWithoutReform();
			if (fast && locomotor->getTurnThresholdHighSpeed() < maxTurn)
				maxTurn = locomotor->getTurnThresholdHighSpeed();
			PathNode* next = m_path->getNextOptimized();
			const Coord3D start = *m_path->getPosition();
			const Coord3D goal = *next->getPosition();
			const Real dx = goal.x - start.x;
			const Real dy = goal.y - start.y;
			const Real length = WWMath::SqrtfOrigin(dx * dx + dy * dy);
			const Real heading = obj->getOrientation();
			const Real turn = WWMath::Atan2Origin(dy, dx) - heading;
			const Real angle = WWMath::Atan2Origin(WWMath::Sin(turn), WWMath::Cos(turn));
			if (radius > 0.0f && length > 0.0f && fabs(angle) >= PI / 16.0f && fabs(angle) <= maxTurn &&
				m_path->getLayer() == next->getLayer())
			{
				if (radius > length * 0.5f)
					radius = length * 0.5f;
				for (Int attempt = 0; attempt < 2; ++attempt)
				{
					if (attempt && (fabs(angle) < PI / 2.0f || locomotor->usesFloatingZ()))
						break;
					const Real direction = (angle > 0.0f ? 1.0f : -1.0f) * (attempt ? -1.0f : 1.0f);
					const Real hx = WWMath::Cos(heading);
					const Real hy = WWMath::Sin(heading);
					const Real cx = start.x - direction * hy * radius;
					const Real cy = start.y + direction * hx * radius;
					const Real gx = goal.x - cx;
					const Real gy = goal.y - cy;
					const Real distance = WWMath::SqrtfOrigin(gx * gx + gy * gy);
					if (distance > radius)
					{
						const Real ratio = radius / distance;
						const Real tangent = WWMath::SqrtfOrigin(1.0f - ratio * ratio);
						const Real tx = gx / distance * ratio + direction * gy / distance * tangent;
						const Real ty = gy / distance * ratio - direction * gx / distance * tangent;
						const Real rx = direction * hy;
						const Real ry = -direction * hx;
						Real sweep = WWMath::Atan2Origin(rx * ty - ry * tx, rx * tx + ry * ty);
						if (sweep * direction < 0.0f)
							sweep += direction * 2.0f * PI;
						Int count = static_cast<Int>(WWMath::Ceil(radius * (1.0f / 30.0f) * fabs(sweep) * 5.092958f));
						if (count > 128)
							count = 128;
						if (count <= 1)
							break;
						if (count > 1)
						{
							std::vector<Coord3D> points;
							Coord3D previous = start;
							Bool passable = TRUE;
							for (Int i = 1; i <= count; ++i)
							{
								const Real step = sweep * static_cast<Real>(i) / static_cast<Real>(count);
								const Real cosine = WWMath::Cos(step);
								const Real sine = WWMath::Sin(step);
								Coord3D point = start;
								point.x = cx + radius * (rx * cosine - ry * sine);
								point.y = cy + radius * (rx * sine + ry * cosine);
								if (!pathExtent.containsWithin(point.asCoord2D()) ||
									!TheAI->pathfinder()->isLinePassable(obj, acceptableSurfaces, m_path->getLayer(),
									previous, point, blocked, false))
								{
									passable = FALSE;
									break;
								}
								points.push_back(point);
								previous = point;
							}
							if (passable && TheAI->pathfinder()->isLinePassable(obj, acceptableSurfaces, m_path->getLayer(),
								previous, goal, blocked, false))
							{
								PathNode* tail = m_path;
								for (const Coord3D& point : points)
								{
									PathNode* inserted = newInstance(PathNode);
									inserted->setPosition(&point);
									inserted->setLayer(m_path->getLayer());
									inserted->setGeneratedArc(TRUE);
									tail->append(inserted);
									tail->setNextOptimized(inserted);
									tail = inserted;
								}
								tail->setNextOptimized(next);
								break;
							}
						}
					}
				}
			}
			PathNode* previous = m_path;
			while (previous->getNextOptimized() && previous->getNextOptimized() != firstTarget)
				previous = previous->getNextOptimized();
			PathNode* corner = firstTarget;
			while (corner && corner->getNextOptimized())
			{
				PathNode* following = corner->getNextOptimized();
				const Coord3D before = *previous->getPosition();
				const Coord3D middle = *corner->getPosition();
				const Coord3D after = *following->getPosition();
				const Real inX = middle.x - before.x;
				const Real inY = middle.y - before.y;
				const Real outX = after.x - middle.x;
				const Real outY = after.y - middle.y;
				const Real inLength = WWMath::SqrtfOrigin(inX * inX + inY * inY);
				const Real outLength = WWMath::SqrtfOrigin(outX * outX + outY * outY);
				Real cornerRadius = locomotor->getFastTurnRadius();
				if (inLength > 0.0f && outLength > 0.0f && cornerRadius > 0.0f && corner->getCanOptimize() &&
					previous->getLayer() == corner->getLayer() && corner->getLayer() == following->getLayer())
				{
					const Real ux = inX / inLength;
					const Real uy = inY / inLength;
					const Real vx = outX / outLength;
					const Real vy = outY / outLength;
					const Real turnAngle = WWMath::Atan2Origin(ux * vy - uy * vx, ux * vx + uy * vy);
					const Real absTurn = fabs(turnAngle);
					const Real limit = 0.75f * (inLength < outLength ? inLength : outLength);
					if (cornerRadius > limit)
						cornerRadius = limit;
					if (absTurn < 0.72256633f)
						cornerRadius *= 1.75f;
					const Real halfCosine = WWMath::Cos(absTurn * 0.5f);
					if (absTurn > 0.0f && absTurn <= 1.7278761f && halfCosine > 0.0f)
					{
						const Real offset = cornerRadius * WWMath::Sin(absTurn * 0.5f) / halfCosine;
						Int count = static_cast<Int>(WWMath::Ceil(cornerRadius * (1.0f / 30.0f) * absTurn * 5.092958f));
						if (count > 128)
							count = 128;
						if (offset < inLength && offset < outLength && count > 1)
						{
							const Real side = turnAngle > 0.0f ? 1.0f : -1.0f;
							Coord3D entry = middle;
							entry.x -= ux * offset;
							entry.y -= uy * offset;
							const Real centerX = entry.x - side * uy * cornerRadius;
							const Real centerY = entry.y + side * ux * cornerRadius;
							const Real radialX = side * uy * cornerRadius;
							const Real radialY = -side * ux * cornerRadius;
							std::vector<Coord3D> points;
							points.push_back(entry);
							Coord3D last = before;
							Bool passable = pathExtent.containsWithin(entry.asCoord2D()) &&
								TheAI->pathfinder()->isLinePassable(obj, acceptableSurfaces,
								corner->getLayer(), last, entry, blocked, false);
							last = entry;
							for (Int i = 1; passable && i <= count; ++i)
							{
								const Real step = turnAngle * static_cast<Real>(i) / static_cast<Real>(count);
								const Real cosine = WWMath::Cos(step);
								const Real sine = WWMath::Sin(step);
								Coord3D point = middle;
								point.x = centerX + radialX * cosine - radialY * sine;
								point.y = centerY + radialX * sine + radialY * cosine;
								passable = pathExtent.containsWithin(point.asCoord2D()) &&
									TheAI->pathfinder()->isLinePassable(obj, acceptableSurfaces,
									corner->getLayer(), last, point, blocked, false);
								points.push_back(point);
								last = point;
							}
							if (passable && TheAI->pathfinder()->isLinePassable(obj, acceptableSurfaces,
								corner->getLayer(), last, after, blocked, false))
							{
								for (const Coord3D& point : points)
								{
									PathNode* inserted = newInstance(PathNode);
									inserted->setPosition(&point);
									inserted->setLayer(corner->getLayer());
									inserted->setGeneratedArc(TRUE);
									previous->append(inserted);
									previous->setNextOptimized(inserted);
									previous = inserted;
								}
								previous->setNextOptimized(following);
								corner = following;
								continue;
							}
						}
					}
				}
				previous = corner;
				corner = following;
			}
		}
	}

	// the path has been optimized
	m_isOptimized = true;
}

/**
 * Optimize the path by checking line of sight
 */
void Path::optimizeGroundPath( Bool crusher, Int pathDiameter )
{
	PathNode *node, *anchor;

	// start with first node in the path
	anchor = getFirstNode();

	//
	// For each node in the path, check LOS from last node in path, working forward.
	// When a clear LOS is found, keep the resulting straight line segment.
	//
	while( anchor != getLastNode() )
	{
		// find the farthest node in the path that has a clear line-of-sight to this anchor
		Bool optimizedSegment = false;
		PathfindLayerEnum layer = anchor->getLayer();
		PathfindLayerEnum curLayer = anchor->getLayer();
		Int count = 0;
		const Int ALLOWED_STEPS = 3; // we can optimize 3 steps to or from a bridge.  Otherwise, we need to insert a point. jba.
		for (node = anchor->getNext(); node->getNext(); node=node->getNext()) {
			count++;
			if (curLayer==LAYER_GROUND) {
				if (node->getLayer() != curLayer) {
					layer = node->getLayer();
					curLayer = layer;
					if (count > ALLOWED_STEPS) break;
				}
			}	else {
				if (node->getNext()->getLayer() != curLayer) {
					if (count > ALLOWED_STEPS) break;
				}
			}
			curLayer = node->getLayer();
		}

		// find the farthest node in the path that has a clear line-of-sight to this anchor
		for( ; node != anchor; node = node->getPrevious() )
		{
			Bool isPassable = false;
			//CRCDEBUG_LOG(("Path::optimize() calling isLinePassable()"));
			if (TheAI->pathfinder()->isGroundPathPassable( crusher, *anchor->getPosition(), layer,
				*node->getPosition(), pathDiameter))
			{
				isPassable = true;
			}
			// Horizontal, diagonal, and vertical steps are passable.
			if (!isPassable) {
				Int dx = node->getPosition()->x - anchor->getPosition()->x;
				Int dy = node->getPosition()->y - anchor->getPosition()->y;
				Bool mightBePassable = false;
				PathNode *tmpNode;
				if (dx==0) {
					mightBePassable = true;
					for (tmpNode = node->getPrevious(); tmpNode && tmpNode != anchor; tmpNode = tmpNode->getPrevious()) {
						dx = tmpNode->getNext()->getPosition()->x - tmpNode->getPosition()->x;
						if (dx!=0) mightBePassable = false;
					}
				}
				if (dy==0) {
					mightBePassable = true;
					for (tmpNode = node->getPrevious(); tmpNode && tmpNode != anchor; tmpNode = tmpNode->getPrevious()) {
						dy = tmpNode->getNext()->getPosition()->y - tmpNode->getPosition()->y;
						if (dy!=0) mightBePassable = false;
					}
				}
				if (dx == dy) {
					mightBePassable = true;
					for (tmpNode = node->getPrevious(); tmpNode &&   tmpNode != anchor; tmpNode = tmpNode->getPrevious()) {
						dx = tmpNode->getNext()->getPosition()->x - tmpNode->getPosition()->x;
						dy = tmpNode->getNext()->getPosition()->y - tmpNode->getPosition()->y;
						if (dy!=dx) mightBePassable = false;
					}
				}
				if (dx == -dy) {
					mightBePassable = true;
					for (tmpNode = node->getPrevious(); tmpNode &&   tmpNode != anchor; tmpNode = tmpNode->getPrevious()) {
						dx = tmpNode->getNext()->getPosition()->x - tmpNode->getPosition()->x;
						dy = tmpNode->getNext()->getPosition()->y - tmpNode->getPosition()->y;
						if (dy!=-dx) mightBePassable = false;
					}
				}
				if (mightBePassable) {
					isPassable = true;
				}
			}
			if (isPassable)
			{
				// anchor can directly see this node, make it next in the optimized path
				anchor->setNextOptimized( node );
				anchor = node;
				optimizedSegment = true;
				break;
			}
		}

		if (optimizedSegment == false)
		{
			// for some reason, there is no clear LOS between the anchor node and the very next node
			anchor->setNextOptimized( anchor->getNext() );
			anchor = anchor->getNext();
		}
	}

	// Remove jig/jogs :) jba.
	for (anchor=getFirstNode(); anchor!=nullptr; anchor=anchor->getNextOptimized()) {
		node = anchor->getNextOptimized();
		if (node && node->getNextOptimized()) {
			Real dx = node->getPosition()->x - anchor->getPosition()->x;
			Real dy = node->getPosition()->y - anchor->getPosition()->y;
			// If the x & y offsets are less than 2 pathfind cells, kill it.
			if (dx*dx+dy*dy < sqr(PATHFIND_CELL_SIZE_F)*3.9f) {
				anchor->setNextOptimized(node->getNextOptimized());
			}
		}
	}

	// the path has been optimized
	m_isOptimized = true;
}

inline Bool isReallyClose(const Coord3D& a, const Coord3D& b)
{
	const Real CLOSE_ENOUGH = 0.1f;
	return
		fabs(a.x-b.x) <= CLOSE_ENOUGH &&
		fabs(a.y-b.y) <= CLOSE_ENOUGH &&
		fabs(a.z-b.z) <= CLOSE_ENOUGH;
}

/**
 * Given a location, return the closest position on the path.
 * If 'allowBacktrack' is true, the entire path is considered.
 * If it is false, the point computed cannot be prior to previously returned non-backtracking points on this path.
 * Because the path "knows" the direction of travel, it will "lead" the given position a bit
 * to ensure the path is followed in the intended direction.
 *
 * Note: The path cleanup does not take into account rolling terrain, so we can end up with
 * these situations:
 *
 *           B
 *        ######
 *      ##########
 *  A-##----------##---C
 * #######################
 *
 *
 * When an agent gets to B, he seems far off of the path, but it really not.
 * There are similar problems with valleys.
 *
 * Since agents track the closest path, if a high hill gets close to the underside of
 * a bridge, an agent may 'jump' to the higher path.  This must be avoided in maps.
 *
 * return along-path distance to the end will be returned as function result
 */
void Path::computePointOnPath(
	const Object* obj,
	const LocomotorSet& locomotorSet,
	const Coord3D& pos,
	ClosestPointOnPathInfo& out,
	Bool updateCache
)
{
	CRCDEBUG_LOG(("Path::computePointOnPath() for %s", DebugDescribeObject(obj).str()));

	out.layer = LAYER_GROUND;
	out.posOnPath.zero();
	out.distAlongPath = 0;

	if (m_path == nullptr)
	{
		if (updateCache)
			m_cpopValid = false;
		return;
	}
	out.layer = m_path->getLayer();

	if (m_cpopValid && m_cpopCountdown>0 && isReallyClose(pos, m_cpopIn))
	{
		out = m_cpopOut;
		if (updateCache)
			m_cpopCountdown--;
		CRCDEBUG_LOG(("Path::computePointOnPath() end because we're really close"));
		return;
	}
	if (updateCache)
		m_cpopCountdown = MAX_CPOP;

	// default pathPos to end of the path
	out.posOnPath = *getLastNode()->getPosition();

	const PathNode* closeNode = nullptr;
	Coord2D toPos;
	Real closeDistSqr = 99999999.9f;
	Real totalPathLength = 0.0f;
	Real lengthAlongPathToPos = 0.0f;

	//
	// Find the closest segment of the path
	//
	const PathNode* prevNode = m_path;
	Coord2D segmentDirNorm;
	Real segmentLength;

	// note that the seg dir and len returned by this is the dist & vec from 'prevNode' to 'node'
	for ( const PathNode* node = prevNode->getNextOptimized(&segmentDirNorm, &segmentLength);
				node != nullptr;
				node = node->getNextOptimized(&segmentDirNorm, &segmentLength) )
	{
		const Coord3D* prevNodePos = prevNode->getPosition();
		const Coord3D* nodePos = node->getPosition();

		// compute vector from start of segment to pos
		toPos.x = pos.x - prevNodePos->x;
		toPos.y = pos.y - prevNodePos->y;

		// compute distance projection of 'toPos' onto segment
		Real alongPathDist = segmentDirNorm.x * toPos.x + segmentDirNorm.y * toPos.y;

		Coord3D pointOnPath;
		if (alongPathDist < 0.0f)
		{
			// projected point is before start of segment, use starting point
			alongPathDist = 0.0f;
			pointOnPath = *prevNodePos;
		}
		else if (alongPathDist > segmentLength)
		{
			// projected point is beyond end of segment, use end point
			if (node->getNextOptimized() == nullptr)
			{
				alongPathDist = segmentLength;
				pointOnPath = *nodePos;
			}
			else
			{
				// beyond the end of this segment, skip this segment
				// if bend is sharp, start of next segment will grab this point
				// if bend is gradual, the point will project into the next segment
				totalPathLength += segmentLength;
				prevNode = node;
				continue;
			}
		}
		else
		{
			// projected point is on this segment, compute it
			pointOnPath.x = prevNodePos->x + alongPathDist * segmentDirNorm.x;
			pointOnPath.y = prevNodePos->y + alongPathDist * segmentDirNorm.y;
			pointOnPath.z = 0;
		}

		// compute distance to point on path, and track the closest we've found so far
		Coord2D offset;
		offset.x = pos.x - pointOnPath.x;
		offset.y = pos.y - pointOnPath.y;

		Real offsetDistSqr = offset.x*offset.x + offset.y*offset.y;
		if (offsetDistSqr < closeDistSqr)
		{
			closeDistSqr = offsetDistSqr;
			closeNode = prevNode;
			out.posOnPath = pointOnPath;

			lengthAlongPathToPos = totalPathLength + alongPathDist;
		}

		// add this segment's length to find total path length
		/// @todo Precompute this and store in path
		totalPathLength += segmentLength;
		prevNode = node;
		DUMPCOORD3D(&pointOnPath);
	}

	//
	// Compute the goal movement position for this agent
	//
	if (closeNode && closeNode->getNextOptimized())
	{
		// note that the seg dir and len returned by this is the dist & vec from 'closeNode' to 'closeNext'
		const PathNode* closeNext = closeNode->getNextOptimized(&segmentDirNorm, &segmentLength);
		const Coord3D* nextNodePos = closeNext->getPosition();
		const Coord3D* closeNodePos = closeNode->getPosition();

		const PathNode* closePrev = closeNode->getPrevious();
		if (closePrev && closePrev->getLayer() > LAYER_GROUND)
		{
			out.layer = closeNode->getLayer();
		}
		if (closeNode->getLayer() > LAYER_GROUND)
		{
			out.layer = closeNode->getLayer();
		}

		if (closeNext->getLayer() > LAYER_GROUND)
		{
			out.layer = closeNext->getLayer();
		}

		// compute vector from start of segment to pos
		toPos.x = pos.x - closeNodePos->x;
		toPos.y = pos.y - closeNodePos->y;

		// compute distance projection of 'toPos' onto segment
		Real alongPathDist = segmentDirNorm.x * toPos.x + segmentDirNorm.y * toPos.y;

		// we know this is the closest segment, so don't allow farther back than the start node
		if (alongPathDist < 0.0f)
			alongPathDist = 0.0f;

		// compute distance of point from this path segment
		Real toDistSqr = sqr(toPos.x) + sqr(toPos.y);
		Real offsetDistSq = toDistSqr - sqr(alongPathDist);
		Real offsetDist = (offsetDistSq <= 0.0) ? 0.0f : (Real)WWMath::SqrtfOrigin(offsetDistSq);

		// If we are basically on the path, return the next path node as the movement goal.
		// However, the farther off the path we get, the movement goal becomes closer to our
		// projected position on the path.  If we are very far off the path, we will move
		// directly towards the nearest point on the path, and not the next path node.
		const Real maxPathError = 3.0f * PATHFIND_CELL_SIZE_F;
		const Real maxPathErrorInv = 1.0 / maxPathError;
		Real k = offsetDist * maxPathErrorInv;
		if (k > 1.0f)
			k = 1.0f;

		Bool gotPos = false;
		CRCDEBUG_LOG(("Path::computePointOnPath() calling isLinePassable() 1"));
		if (TheAI->pathfinder()->isLinePassable( obj, locomotorSet.getValidSurfaces(), out.layer, pos, *nextNodePos,
			false, true ))
		{
			out.posOnPath = *nextNodePos;
			gotPos = true;

			Bool tryAhead = alongPathDist > segmentLength * 0.5;
			if (closeNext->getCanOptimize() == false)
			{
				tryAhead = false; // don't go past no-opt nodes.
			}
			if (closeNode->getLayer() != closeNext->getLayer())
			{
				tryAhead = false; // don't go past layers.
			}
			if (obj->getLayer()!=LAYER_GROUND) {
				tryAhead = false;
			}
			Bool veryClose = false;
			if (segmentLength-alongPathDist<1.0f) {
				tryAhead = true;
				veryClose = true;
			}
			if (tryAhead)
			{
				// try next segment middle.
				const PathNode *next = closeNext->getNextOptimized();
				if (next)
				{
					Coord3D tryPos;
					tryPos.x = (nextNodePos->x + next->getPosition()->x) * 0.5;
					tryPos.y = (nextNodePos->y + next->getPosition()->y) * 0.5;
					tryPos.z = nextNodePos->z;
					CRCDEBUG_LOG(("Path::computePointOnPath() calling isLinePassable() 2"));
					if (veryClose || TheAI->pathfinder()->isLinePassable( obj, locomotorSet.getValidSurfaces(), closeNext->getLayer(), pos, tryPos, false, true ))
					{
						gotPos = true;
						out.posOnPath = tryPos;
					}
				}
			}
		}
		else if (k > 0.5f)
		{
			Real tryDist = alongPathDist + (0.5) * (segmentLength - alongPathDist);

			// projected point is on this segment, compute it
			out.posOnPath.x = closeNodePos->x + tryDist * segmentDirNorm.x;
			out.posOnPath.y = closeNodePos->y + tryDist * segmentDirNorm.y;
			out.posOnPath.z = closeNodePos->z;

			CRCDEBUG_LOG(("Path::computePointOnPath() calling isLinePassable() 3"));
			if (TheAI->pathfinder()->isLinePassable( obj, locomotorSet.getValidSurfaces(), out.layer, pos, out.posOnPath, false, true ))
			{
				k = 0.5f;
				gotPos = true;
			}
		}

		// if we are on the path (k == 0), then alongPathDist == segmentLength
		// if we are way off the path (k == 1), then alongPathDist is unchanged, and it projection of actual pos
		alongPathDist += (1.0f - k) * (segmentLength - alongPathDist);

		if (!gotPos)
		{
			if (alongPathDist > segmentLength)
			{
				alongPathDist = segmentLength;
				out.posOnPath = *nextNodePos;
			}
			else
			{
				// projected point is on this segment, compute it
				out.posOnPath.x = closeNodePos->x + alongPathDist * segmentDirNorm.x;
				out.posOnPath.y = closeNodePos->y + alongPathDist * segmentDirNorm.y;
				out.posOnPath.z = closeNodePos->z;
				Real dx = fabs(pos.x - out.posOnPath.x);
				Real dy = fabs(pos.y - out.posOnPath.y);
				if (dx<1 && dy<1 && closeNode->getNextOptimized() && closeNode->getNextOptimized()->getNextOptimized()) {
					out.posOnPath = *closeNode->getNextOptimized()->getNextOptimized()->getPosition();
				}
			}
		}
	}

	if (updateCache)
		TheAI->pathfinder()->setDebugPathPosition( &out.posOnPath );

	out.distAlongPath = totalPathLength - lengthAlongPathToPos;

	Coord3D delta;
	delta.x = out.posOnPath.x - pos.x;
	delta.y = out.posOnPath.y - pos.y;
	delta.z = 0;
	Real lenDelta = delta.length();
	if (lenDelta > out.distAlongPath && out.distAlongPath > PATHFIND_CLOSE_ENOUGH)
	{
		out.distAlongPath = lenDelta;
	}

	if (updateCache)
	{
		m_cpopIn = pos;
		m_cpopOut = out;
		m_cpopValid = true;
	}
	CRCDEBUG_LOG(("Path::computePointOnPath() end"));

}


/**
	Given a position, computes the distance to the goal.  Returns 0 if we are past the goal.
	Returns the goal position in goalPos.  This is intended for use with flying paths, that go
	directly to the goal and don't consider obstacles.  jba.
 */
Real Path::computeFlightDistToGoal( const Coord3D *pos, Coord3D& goalPos, Bool updateCache )
{
	if (m_path == nullptr)
	{
		goalPos.x = 0.0f;
		goalPos.y = 0.0f;
		goalPos.z = 0.0f;
		return 0.0f;
	}
	const PathNode *curNode = getFirstNode();
	if (m_cpopRecentStart) {
		curNode = m_cpopRecentStart;
	} else if (updateCache) {
		m_cpopRecentStart = curNode;
	}
	const PathNode *nextNode = curNode->getNextOptimized();
	goalPos = *curNode->getPosition();
	Real distance = 0;
	Bool useNext = true;
	while (nextNode) {

		if (useNext) {
			goalPos = *nextNode->getPosition();
		}

		Coord3D startPos = *curNode->getPosition();
		Coord3D endPos = *nextNode->getPosition();

		Coord2D posToGoalVector;
		// posToGoalVector is pos to goalPos vector.
		posToGoalVector.x = endPos.x - pos->x;
		posToGoalVector.y = endPos.y - pos->y;

		// pathVector is the startPos to goal pos vector.
		Coord2D pathVector;
		pathVector.x = endPos.x - startPos.x;
		pathVector.y = endPos.y - startPos.y;

		// Normalize pathVector
		pathVector.normalize();

		// Dot product is the posToGoal vector projected onto the path vector.
		Real dotProduct = posToGoalVector.x*pathVector.x	+ posToGoalVector.y*pathVector.y;
		if (dotProduct>=0) {
			distance += dotProduct;
			useNext = false;
		}	else if (useNext && updateCache) {
			m_cpopRecentStart = nextNode;
		}
		curNode = nextNode;
		nextNode = curNode->getNextOptimized();
	}
	return distance;

}
