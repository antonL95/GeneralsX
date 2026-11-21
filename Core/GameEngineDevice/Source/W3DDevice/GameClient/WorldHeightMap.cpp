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

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// WorldHeightMap.cpp
// Class to encapsulate height map.
// Author: John Ahlquist, April 2001

#define INSTANTIATE_WELL_KNOWN_KEYS

#include "windows.h"
#include "stdlib.h"
#include "Common/STLTypedefs.h"

#include "Common/DataChunk.h"
//#include "Common/GameFileSystem.h"
#include "Common/FileSystem.h" // for LOAD_TEST_ASSETS
#include "Common/GlobalData.h"
#include "Common/INIAudit.h"
#include "Common/MapReaderWriterInfo.h"
#include "Common/RetailFileLayers.h"
#include "Common/TerrainTypes.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/WellKnownKeys.h"

#include "GameLogic/PolygonTrigger.h"
#include "GameLogic/SidesList.h"
#include "GameLogic/BfmeMapScripts.h"

#include "W3DDevice/GameClient/WorldHeightMap.h"
#include "W3DDevice/GameClient/TileData.h"
#include "W3DDevice/GameClient/HeightMap.h"
#include "W3DDevice/GameClient/TerrainTex.h"
#include "W3DDevice/GameClient/W3DShadow.h"

#include "Common/file.h"
#include "Utility/stringex.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <limits>


#define K_OBSOLETE_HEIGHT_MAP_VERSION 8

#define PATHFIND_CLIFF_SLOPE_LIMIT_F	9.8f

// -----------------------------------------------------------
static AsciiString validateName(AsciiString n, Int flags)
{

	return n;

}

/* ********* GDIFileStream class ****************************/
class GDIFileStream : public InputStream
{
protected:
	File* m_file;
public:
	GDIFileStream(File* pFile):m_file(pFile) {};
	virtual Int read(void *pData, Int numBytes) override {
			return(m_file->read(pData, numBytes));
	};
};


/* ********* MapObject class ****************************/
/*static*/ MapObject *MapObject::TheMapObjectListPtr = nullptr;
/*static*/ Dict MapObject::TheWorldDict;

MapObject::MapObject(Coord3D loc, AsciiString name, Real angle, Int flags, const Dict* props,
										 const ThingTemplate *thingTemplate )
{
	m_objectName = validateName( name, flags );
	m_thingTemplate = thingTemplate;
	m_nextMapObject = nullptr;
	m_location = loc;
	m_angle = normalizeAngle(angle);
	m_color = (0xff)<<8; // Bright green.
	m_flags = flags;
	m_renderObj = nullptr;
	m_shadowObj = nullptr;
	m_runtimeFlags = 0;
	// Note - do NOT set TheKey_objectSelectable on creation - allow it to follow the .ini value unless specified by user action.  jba. [3/20/2003]
	if (props)
	{
		m_properties = *props;
	}
	else
	{
		m_properties.setInt(TheKey_objectInitialHealth, 100);
		m_properties.setBool(TheKey_objectEnabled, true);
		m_properties.setBool(TheKey_objectIndestructible, false);
		m_properties.setBool(TheKey_objectUnsellable, false);
		m_properties.setBool(TheKey_objectPowered, true);
		m_properties.setBool(TheKey_objectRecruitableAI, true);
		m_properties.setBool(TheKey_objectTargetable, false );
	}

	for( Int i = 0; i < BRIDGE_MAX_TOWERS; ++i )
		setBridgeRenderObject( (BridgeTowerType)i, nullptr );

}


MapObject::~MapObject()
{
	setRenderObj(nullptr);
	setShadowObj(nullptr);
	if (m_nextMapObject) {
		MapObject *cur = m_nextMapObject;
		MapObject *next;
		while (cur) {
			next = cur->getNext();
			cur->setNextMap(nullptr); // prevents recursion.
			deleteInstance(cur);
			cur = next;
		}
	}
	for( Int i = 0; i < BRIDGE_MAX_TOWERS; ++i )
		setBridgeRenderObject( (BridgeTowerType)i, nullptr );

}

MapObject *MapObject::duplicate()
{
	MapObject *pObj = newInstance( MapObject)(m_location, m_objectName, m_angle, m_flags, &m_properties, m_thingTemplate);
	pObj->setColor(getColor());
	pObj->m_runtimeFlags = m_runtimeFlags;
	return pObj;
}

void MapObject::setRenderObj(RenderObjClass *pObj)
{
	REF_PTR_SET(m_renderObj, pObj);
}

void MapObject::setBridgeRenderObject( BridgeTowerType type, RenderObjClass* renderObj )
{

	if( type >= 0 && type < BRIDGE_MAX_TOWERS )
		REF_PTR_SET( m_bridgeTowers[ type ], renderObj );

}

RenderObjClass* MapObject::getBridgeRenderObject( BridgeTowerType type )
{

	if( type >= 0 && type < BRIDGE_MAX_TOWERS )
		return m_bridgeTowers[ type ];
	return nullptr;

}

void MapObject::validate()
{
	verifyValidTeam();
	verifyValidUniqueID();
}

void MapObject::verifyValidTeam()
{
	// if this map object has a valid team, then do nothing.
	// if it has an invalid team, the place it on the default neutral team, (by clearing the
	// existing team name.)
	Bool exists;
	AsciiString teamName = getProperties()->getAsciiString(TheKey_originalOwner, &exists);
	if (exists) {
		Bool valid = false;

		int numSides = TheSidesList->getNumTeams();

		for (int i = 0; i < numSides; ++i) {
			TeamsInfo *teamInfo = TheSidesList->getTeamInfo(i);
			if (!teamInfo) {
				continue;
			}

			Bool itBetter;
			AsciiString testAgainstTeamName = teamInfo->getDict()->getAsciiString(TheKey_teamName, &itBetter);
			if (itBetter) {
				if (testAgainstTeamName.compare(teamName) == 0) {
					valid = true;
				}
			}
		}

		if (!valid) {
			getProperties()->remove(TheKey_originalOwner);
		}
	}
}

void MapObject::verifyValidUniqueID()
{
	Bool exists;
	AsciiString uniqueID = getProperties()->getAsciiString(TheKey_uniqueID, &exists);
	MapObject *obj = MapObject::getFirstMapObject();

	// -1 is the sentinel
	int highestIndex = -1;

	while (obj) {
		if (obj == this) {
			// the first object is THIS OBJECT, cause we've already been added.
			obj = obj->getNext();
			continue;
		}

		if (obj->isWaypoint()) {
			// waypoints throw this off. Sad but true. :-(
			obj = obj->getNext();
			continue;
		}

		Bool iterateExists;
		AsciiString tempStr = obj->getProperties()->getAsciiString(TheKey_uniqueID, &iterateExists);
		const char* lastSpace = tempStr.reverseFind(' ');

		int testIndex = -1;
		if (lastSpace) {
			testIndex = atoi(lastSpace);
		}

		if (testIndex > highestIndex) {
			highestIndex = testIndex;
		}
		break;
	}

	int indexOfThisObject = highestIndex + 1;

	const char* thingName;
	if (getThingTemplate()) {
		thingName = getThingTemplate()->getName().str();
	} else if (isWaypoint()) {
		thingName = getWaypointName().str();
	} else {
		thingName = getName().str();
	}
	const char* pName = thingName;

	while (*thingName) {
		if ((*thingName) == '/') {
			pName = thingName + 1;
		}
		++thingName;
	}

	AsciiString newID;
	if (isWaypoint()) {
		newID.format("%s", pName);
	} else {
		newID.format("%s %d", pName, indexOfThisObject);
	}
	getProperties()->setAsciiString(TheKey_uniqueID, newID);
}

void MapObject::fastAssignAllUniqueIDs()
{
	// here's what we do. Take all of them, push them onto a stack. Then, pop each one, setting its id.
	// should be much faster than what we currently do.

	MapObject *pMapObj = getFirstMapObject();

	std::stack<MapObject*> objStack;
	Int actualNumObjects = 0;

	while (pMapObj) {
		++actualNumObjects;
		objStack.push(pMapObj);
		pMapObj = pMapObj->getNext();
	}

	Int indexOfThisObject = 0;
	while (actualNumObjects) {
		MapObject *obj = objStack.top();


		const char* thingName;
		if (obj->getThingTemplate()) {
			thingName = obj->getThingTemplate()->getName().str();
		} else if (obj->isWaypoint()) {
			thingName = obj->getWaypointName().str();
		} else {
			thingName = obj->getName().str();
		}
		const char* pName = thingName;

		while (*thingName) {
			if ((*thingName) == '/') {
				pName = thingName + 1;
			}
			++thingName;
		}

		AsciiString newID;
		if (obj->isWaypoint()) {
			newID.format("%s", pName);
		} else {
			newID.format("%s %d", pName, indexOfThisObject);
		}

		obj->getProperties()->setAsciiString(TheKey_uniqueID, newID);
		objStack.pop();

		++indexOfThisObject;
		--actualNumObjects;
	}
}



void MapObject::setThingTemplate(const ThingTemplate *thing)
{
	m_thingTemplate = thing;
	m_objectName = thing->getName();
}


void MapObject::setName(AsciiString name)
{
	m_objectName = name;
}

WaypointID MapObject::getWaypointID() { return (WaypointID)getProperties()->getInt(TheKey_waypointID); }
AsciiString MapObject::getWaypointName() { return getProperties()->getAsciiString(TheKey_waypointName); }
void MapObject::setWaypointID(Int i) { getProperties()->setInt(TheKey_waypointID, i); }
void MapObject::setWaypointName(AsciiString n) { getProperties()->setAsciiString(TheKey_waypointName, n); }

/*static */ Int MapObject::countMapObjectsWithOwner(const AsciiString& n)
{
	Int count = 0;
	for (MapObject *pMapObj = MapObject::getFirstMapObject(); pMapObj; pMapObj = pMapObj->getNext())
	{
		if (pMapObj->getProperties()->getAsciiString(TheKey_originalOwner) == n)
			++count;
	}
	return count;
}

//-------------------------------------------------------------------------------------------------
const ThingTemplate *MapObject::getThingTemplate() const
{
	if (m_thingTemplate)
		return (const ThingTemplate*) m_thingTemplate->getFinalOverride();

	return nullptr;
}


/* ********* WorldHeightMap class ****************************/

TileData *WorldHeightMap::m_alphaTiles[NUM_ALPHA_TILES]={0};

struct WorldHeightMap::BfmeLighting
{
	GlobalData::TerrainLighting terrain[TIME_OF_DAY_COUNT][MAX_GLOBAL_LIGHTS]{};
	GlobalData::TerrainLighting objects[TIME_OF_DAY_COUNT][MAX_GLOBAL_LIGHTS]{};
	GlobalData::TerrainLighting infantry[TIME_OF_DAY_COUNT][MAX_GLOBAL_LIGHTS]{};
	TimeOfDay timeOfDay;
	Real overbright;
	UnsignedInt filterEnabled;
	RGBColor filterColors[3];
	UnsignedInt shadowColor;
	RGBColor noCloudFactor{1.0f, 1.0f, 1.0f};
};

//
// WorldHeightMap destructor .
//
WorldHeightMap::~WorldHeightMap()
{
	releaseMapData();
	if (m_releaseAlphaTiles)
	{
		for (Int i = 0; i < NUM_ALPHA_TILES; ++i)
			REF_PTR_RELEASE(m_alphaTiles[i]);
	}
}

void WorldHeightMap::releaseMapData()
{
	delete[](m_data);
	m_data = nullptr;

	delete[](m_tileNdxes);
	m_tileNdxes = nullptr;

	delete[](m_blendTileNdxes);
	m_blendTileNdxes = nullptr;

	delete[](m_extraBlendTileNdxes);
	m_extraBlendTileNdxes = nullptr;

	delete[](m_cliffInfoNdxes);
	m_cliffInfoNdxes = nullptr;

	delete[](m_cellFlipState);
	m_cellFlipState = nullptr;

	delete[](m_seismicUpdateFlag);
	m_seismicUpdateFlag = nullptr;

	delete[](m_seismicZVelocities);
	m_seismicZVelocities = nullptr;

	delete[](m_cellCliffState);
	m_cellCliffState = nullptr;

	for (TileData *&tile : m_sourceTiles)
		REF_PTR_RELEASE(tile);
	for (TileData *&tile : m_edgeTiles)
		REF_PTR_RELEASE(tile);
	REF_PTR_RELEASE(m_terrainTex);
	REF_PTR_RELEASE(m_alphaTerrainTex);
	REF_PTR_RELEASE(m_alphaEdgeTex);
}

void WorldHeightMap::freeListOfMapObjects()
{
	if (MapObject::TheMapObjectListPtr)
	{
		deleteInstance(MapObject::TheMapObjectListPtr);
		MapObject::TheMapObjectListPtr = nullptr;
	}
	MapObject::getWorldDict()->clear();
}


/**
 WorldHeightMap - create a new height map for class WorldHeightMap.
 Note that there is 1 m_numBlendedTiles, which is the implied
 transparent tile for non-blended tiles.
*/
WorldHeightMap::WorldHeightMap(Bool initializeEditor):
	m_width(0), m_height(0),  m_dataSize(0), m_data(nullptr), m_cellFlipState(nullptr), m_seismicUpdateFlag(nullptr), m_seismicZVelocities(nullptr),
	m_drawOriginX(0), m_drawOriginY(0),
	m_numTextureClasses(0),
	m_drawWidthX(NORMAL_DRAW_WIDTH), m_drawHeightY(NORMAL_DRAW_HEIGHT),
	m_tileNdxes(nullptr), m_blendTileNdxes(nullptr), m_extraBlendTileNdxes(nullptr), m_cliffInfoNdxes(nullptr),
	m_terrainTexHeight(1), m_alphaTexHeight(1),	m_cellCliffState(nullptr),
#ifdef EVAL_TILING_MODES
	m_tileMode(TILE_4x4),
#endif
	m_numCliffInfo(1),
	m_terrainTex(nullptr), m_alphaTerrainTex(nullptr), m_numBitmapTiles(0), m_numBlendedTiles(1),
	m_releaseAlphaTiles(initializeEditor)
{
	if (initializeEditor)
	{
		m_sourceTiles.resize(NUM_SOURCE_TILES);
		m_blendedTiles.resize(NUM_BLEND_TILES);
		m_cliffInfo.resize(NUM_CLIFF_INFO);
		TheSidesList->validateSides();
		setupAlphaTiles();
	}
}

#ifdef EVAL_TILING_MODES
static Bool ParseFunkyTilingDataChunk(DataChunkInput &file, DataChunkInfo *info, void *userData)
{
	WorldHeightMap *pThis = (WorldHeightMap *)userData;
	*((Int *)&pThis->m_tileMode) = file.readInt();
	return true;
}
#endif

void WorldHeightMap::RegisterLogicalMapParsers(DataChunkInput &file, Bool metadataOnly)
{
	const AsciiString& root = AsciiString::TheEmptyString;
	file.registerVersionedParser("HeightMapData", root, ParseSizeOnlyInChunk, K_HEIGHT_MAP_VERSION_5, true);
	file.registerVersionedParser("BlendTileData", root, ParseLogicalBlendTileDataChunk, 18, true);
	file.registerVersionedParser("SidesList", root, SidesList::ParseSidesDataChunk,
		SidesList::MAX_MAP_SIDES_VERSION, true);
	file.registerVersionedParser("Teams", root, SidesList::ParseMapTeamsDataChunk, 1, true);

	if (!metadataOnly)
	{
		file.registerVersionedParser("WorldInfo", root, ParseWorldDictDataChunk, K_WORLDDICT_VERSION_1);
		file.registerVersionedParser("ObjectsList", root, ParseObjectsDataChunk, K_OBJECTS_VERSION_3);
		file.registerVersionedParser("PolygonTriggers", root, PolygonTrigger::ParsePolygonTriggersDataChunk,
			K_TRIGGERS_VERSION_4);
	}
	else
		file.registerVersionedParser("PolygonTriggers", root, nullptr, K_TRIGGERS_VERSION_4);

	file.registerVersionedParser("GlobalLighting", root, ParseLogicalLightingDataChunk, K_LIGHTING_VERSION_8, true);
	file.registerVersionedParser("MPPositionList", root, nullptr, 0);
	file.registerVersionedParser("LibraryMapLists", root, BfmeMapScripts::parseLibraryLists, 1);
	file.registerVersionedParser("PlayerScriptsList", root, BfmeMapScripts::parsePlayerLists, 6, true);
	file.registerVersionedParser("BuildLists", root, nullptr, 1);
	file.registerVersionedParser("TriggerAreas", root, ParseTriggerAreas, 1);
	file.registerVersionedParser("StandingWaterAreas", root, ParseStandingWaterAreas, 2);
	file.registerVersionedParser("RiverAreas", root, nullptr, 2);
	file.registerVersionedParser("StandingWaveAreas", root, nullptr, 2);
	file.registerVersionedParser("PostEffectsChunk", root, nullptr, 1);
	file.registerVersionedParser("EnvironmentData", root, nullptr, 3);
	file.registerVersionedParser("NamedCameras", root, nullptr, 2);
	file.registerVersionedParser("CameraAnimationList", root, nullptr, 3);
	file.registerVersionedParser("SkyboxSettings", root, nullptr, 1);
}

RefCountPtr<WorldHeightMap> WorldHeightMap::readAuditMap(DataChunkInput &file)
{
	RefCountPtr<WorldHeightMap> map;
	map.Assign_No_Add_Ref(NEW WorldHeightMap(false));
	map->m_readAuditBlendTiles = true;
	RegisterLogicalMapParsers(file, true);
	if (!file.parse(map.Peek()))
		return {};
	map->finalizeImportedPolygons(false);
	return map;
}

void WorldHeightMap::finalizeImportedPolygons(Bool install)
{
	if (m_standingWater.empty() && m_triggerAreas.empty())
		return;
	std::vector<PolygonTrigger *> polygons;
	polygons.reserve(m_standingWater.size() + m_triggerAreas.size());
	for (const auto &polygon : m_standingWater)
		polygons.push_back(polygon.get());
	for (const auto &polygon : m_triggerAreas)
		polygons.push_back(polygon.get());
	PolygonTrigger::assignImportedIDs(polygons);
	if (install)
	{
		std::rotate(polygons.begin(), polygons.begin() + m_standingWater.size(), polygons.end());
		PolygonTrigger::installImportedPolygons(polygons);
		for (auto &polygon : m_standingWater)
			polygon.release();
		for (auto &polygon : m_triggerAreas)
			polygon.release();
		m_standingWater.clear();
		m_triggerAreas.clear();
	}
}
/**
* WorldHeightMap - read a height map from a file.
* Format is  Chunky.
*
*	Input: ChunkInputStream,
*
*/
WorldHeightMap::WorldHeightMap(ChunkInputStream *pStrm, Bool logicalDataOnly, Bool legacyBlendOnly):
	m_width(0), m_height(0),  m_dataSize(0), m_data(nullptr), m_cellFlipState(nullptr), m_seismicUpdateFlag(nullptr), m_seismicZVelocities(nullptr),
	m_drawOriginX(0),	m_cellCliffState(nullptr), m_drawOriginY(0),
	m_numTextureClasses(0),
	m_drawWidthX(NORMAL_DRAW_WIDTH), m_drawHeightY(NORMAL_DRAW_HEIGHT),
	m_tileNdxes(nullptr), m_blendTileNdxes(nullptr), m_extraBlendTileNdxes(nullptr), m_cliffInfoNdxes(nullptr),
	m_terrainTexHeight(1), m_alphaTexHeight(1),
#ifdef EVAL_TILING_MODES
	m_tileMode(TILE_4x4),
#endif
	m_numCliffInfo(1),
	m_terrainTex(nullptr), m_alphaTerrainTex(nullptr), m_numBitmapTiles(0), m_numBlendedTiles(1)
{

	DataChunkInput file( pStrm );
	const AsciiString& root = AsciiString::TheEmptyString;

	if (logicalDataOnly) {
		RegisterLogicalMapParsers(file, false);
		freeListOfMapObjects(); // just in case.
		PolygonTrigger::deleteTriggers(); // just in case.
		TheSidesList->clear();
	}	else {
		file.registerVersionedParser("HeightMapData", root, ParseHeightMapDataChunk, K_HEIGHT_MAP_VERSION_5, true);
		file.registerVersionedParser("BlendTileData", root, ParseBlendTileDataChunk,
			legacyBlendOnly ? K_BLEND_TILE_VERSION_8 : 18, true);
#ifdef EVAL_TILING_MODES
		file.registerVersionedParser("FUNKY_TILING", root, ParseFunkyTilingDataChunk, 1);
#endif
		file.registerVersionedParser("GlobalLighting", root, ParseLightingDataChunk, K_LIGHTING_VERSION_8, true);
		if (!legacyBlendOnly && std::getenv("GENERALSX_ASSET_SMOKE_TERRAIN"))
			file.registerVersionedParser("StandingWaterAreas", root, ParseStandingWaterAreas, 2);
	}
	try
	{
		if (!file.parse(this))
			throw ERROR_CORRUPT_FILE_FORMAT;
		if (const DataChunkInfo *unsupported = file.getUnsupportedRequiredChunk())
		{
			fprintf(stderr, "Map load stopped at %s v%u after reading %d sides and %d teams\n",
				unsupported->label.str(), static_cast<unsigned>(unsupported->version),
				TheSidesList->getNumSides(), TheSidesList->getNumTeams());
			fflush(stderr);
			throw ERROR_CORRUPT_FILE_FORMAT;
		}
		if (!logicalDataOnly && !m_tileNdxes)
			throw ERROR_CORRUPT_FILE_FORMAT;
		if (!logicalDataOnly && m_bfmeLighting)
			applyBfmeLighting();
		if (logicalDataOnly)
			finalizeImportedPolygons(true);
	}
	catch (...)
	{
		releaseMapData();
		if (logicalDataOnly)
		{
			freeListOfMapObjects();
			PolygonTrigger::deleteTriggers();
			TheSidesList->clear();
		}
		throw;
	}
	// patch bad maps.
	if (!logicalDataOnly) {
		for (Int i = 0; i < m_dataSize; i++) {
			if (m_cliffInfoNdxes[i]<0 || m_cliffInfoNdxes[i]>= m_numCliffInfo) {
				m_cliffInfoNdxes[i] = 0;
			}
			if (m_blendTileNdxes[i]<0 || m_blendTileNdxes[i]>= m_numBlendedTiles) {
				m_blendTileNdxes[i] = 0;
			}
			if (m_extraBlendTileNdxes[i]<0 || m_extraBlendTileNdxes[i]>= m_numBlendedTiles) {
				m_extraBlendTileNdxes[i] = 0;
			}
		}
	}

	if (m_drawWidthX > m_width) {
		m_drawWidthX = m_width;
	}
	if (m_drawHeightY > m_height) {
		m_drawHeightY = m_height;
	}

	TheSidesList->validateSides();
	setupAlphaTiles();
}

Bool WorldHeightMap::isTerrainFlat(const IRegion2D& bounds) const
{
	DEBUG_ASSERTCRASH(getLogicalBounds().contains(bounds), ("WorldHeightMap::isTerrainFlat must use a region contained by the logical map bounds."));

	const Int width = bounds.width();
	const Int height = bounds.height();
	if (width <= 0 || height <= 0)
		return true;

	const UnsignedShort* firstRow = m_data + (bounds.lo.y + m_borderSize) * m_width + bounds.lo.x + m_borderSize;
	const UnsignedShort referenceHeight = firstRow[0];
	for (Int j = 0; j < height; j++)
	{
		const UnsignedShort* row = firstRow + j * m_width;
		if (!std::all_of(row, row + width, [referenceHeight](UnsignedShort sample) { return sample == referenceHeight; }))
			return false;
	}

	return true;
}

/** Optimized version of method to get triangle flip state of a terrain cell.  Use this
*	instead of getAlphaUVData() whenever possible.
*/
Bool WorldHeightMap::getFlipState(Int xIndex, Int yIndex) const
{
	if (xIndex<0 || yIndex<0) return false;
	if (yIndex>=m_height) return false;
	if (xIndex>=m_width) return false;
	if (!m_cellFlipState) return false;
	return m_cellFlipState[yIndex*m_flipStateWidth + (xIndex >> 3)] & (1<<(xIndex&0x7));
}

/** Sets the value of the flip state bit.
*/
void WorldHeightMap::setFlipState(Int xIndex, Int yIndex, Bool value)
{
	if (xIndex<0 || yIndex<0) return ;
	if (yIndex>=m_height) return ;
	if (xIndex>=m_width) return ;
	if (!m_cellFlipState) return ;
	UnsignedByte *curVal = &m_cellFlipState[yIndex*m_flipStateWidth + (xIndex >> 3)];
	if (value) {
		*curVal |= (1<<(xIndex&0x7));
	}	else {
		*curVal &= ~(1<<(xIndex&0x7));
	}
}

/** Clears all flip state bits.
*/
void WorldHeightMap::clearFlipStates() {
	if (m_cellFlipState) {
		memset(m_cellFlipState,0,m_flipStateWidth*m_height);	//clear all flags
	}
}

//////////////////////////////////////////////////////////////////////////////m_SeismicUpdateFlag
Bool WorldHeightMap::getSeismicUpdateFlag(Int xIndex, Int yIndex) const
{
	if (xIndex<0 || yIndex<0) return false;
	if (yIndex>=m_height) return false;
	if (xIndex>=m_width) return false;
	if (!m_seismicUpdateFlag) return false;
	return m_seismicUpdateFlag[yIndex*m_seismicUpdateWidth + (xIndex >> 3)] & (1<<(xIndex&0x7));
}
void WorldHeightMap::setSeismicUpdateFlag(Int xIndex, Int yIndex, Bool value)
{
	if (xIndex<0 || yIndex<0) return ;
	if (yIndex>=m_height) return ;
	if (xIndex>=m_width) return ;
	if (!m_seismicUpdateFlag) return ;
	UnsignedByte *curVal = &m_seismicUpdateFlag[yIndex*m_seismicUpdateWidth + (xIndex >> 3)];
	if (value) {
		*curVal |= (1<<(xIndex&0x7));
	}	else {
		*curVal &= ~(1<<(xIndex&0x7));
	}
}
void WorldHeightMap::clearSeismicUpdateFlags()
{
	if (m_seismicUpdateFlag) {
		memset(m_seismicUpdateFlag,0,m_seismicUpdateWidth*m_height);	//clear all flags
	}
}

///////////////////////////////////////////////m_SeismicZVelocities
Real WorldHeightMap::getSeismicZVelocity(Int xIndex, Int yIndex) const
{
	if (xIndex<0 || yIndex<0) return false;
	if (yIndex>=m_height) return false;
	if (xIndex>=m_width) return false;
	if (!m_seismicZVelocities) return false;
	return m_seismicZVelocities[yIndex*m_width + xIndex];
}
void WorldHeightMap::setSeismicZVelocity(Int xIndex, Int yIndex, Real value)
{
	if (xIndex<0 || yIndex<0) return ;
	if (yIndex>=m_height) return ;
	if (xIndex>=m_width) return ;
	if (!m_seismicZVelocities) return ;
	m_seismicZVelocities[yIndex*m_width + xIndex] = value;
}
void WorldHeightMap::fillSeismicZVelocities( Real value )
{
	if (!m_seismicZVelocities) return ;
  for (Int idx = 0; idx < m_width*m_height; ++idx)
    m_seismicZVelocities[idx] = value;
}

Real WorldHeightMap::getBilinearSampleSeismicZVelocity( Int x, Int y)
{
	if ( x < 0 || y < 0 ) return 0;
	if ( y >= m_height ) return 0;
	if ( x >= m_width ) return 0;
	if (!m_seismicZVelocities) return 0;

  Real collector = 0.0f;
  Real divisor = 0.0f;

  collector += m_seismicZVelocities[ y * m_width + x ];
  ++divisor;

  if ( y > 0 )
  {
    collector += m_seismicZVelocities[ (y-1) * m_width + x ];//bottom
    ++divisor;

    if( x > 0 )
    {
      collector += m_seismicZVelocities[ (y-1) * m_width + (x-1) ];//lower left
      ++divisor;
    }
    if ( x < m_width-1 )
    {
      collector += m_seismicZVelocities[ (y-1) * m_width + (x+1) ];//lower right
      ++divisor;
    }
  }
  if ( y < m_height-1 )
  {
    collector += m_seismicZVelocities[ (y+1) * m_width + x ];//top
    ++divisor;

    if( x > 0 )
    {
      collector += m_seismicZVelocities[ (y+1) * m_width + (x-1) ];//upper left
      ++divisor;
    }
    if ( x < m_width-1 )
    {
      collector += m_seismicZVelocities[ (y+1) * m_width + (x+1) ];//upper right
      ++divisor;
    }
  }
  if( x > 0 )
  {
    collector += m_seismicZVelocities[ y * m_width + (x-1) ];//left
    ++divisor;
  }
  if ( x < m_width-1 )
  {
    collector += m_seismicZVelocities[ y * m_width + (x+1) ];//right
    ++divisor;
  }

  collector /= divisor;

  return collector;

}

/** Get whether the cell is a cliff cell (impassable to ground vehicles).
*/
Bool WorldHeightMap::getCliffState(Int xIndex, Int yIndex) const
{
	if (xIndex<0 || yIndex<0) return false;
	if (yIndex>=m_height) return false;
	if (xIndex>=m_width) return false;
	if (!m_cellCliffState) return false;
	return m_cellCliffState[yIndex*m_flipStateWidth + (xIndex >> 3)] & (1<<(xIndex&0x7));
}

//=============================================================================
// setCliffState
//=============================================================================
/** Sets the cliff state for a given cell. */
//=============================================================================
void WorldHeightMap::setCliffState(Int xIndex, Int yIndex, Bool state)
{
	if (xIndex<0 || yIndex<0) return;
	if (yIndex>=m_height) return;
	if (xIndex>=m_width) return;
	if (!m_cellCliffState) return;
	UnsignedByte	flagByte = m_cellCliffState[yIndex*m_flipStateWidth + (xIndex >> 3)];
	UnsignedByte flagMask = (1<<(xIndex&0x7));
	if (state) {
		flagByte |= flagMask;
	} else {
		flagByte &= (~flagMask);
	}
	m_cellCliffState[yIndex*m_flipStateWidth + (xIndex >> 3)] = flagByte;
}

Bool WorldHeightMap::ParseWorldDictDataChunk(DataChunkInput &file, DataChunkInfo *info, void *userData)
{
	Dict d = file.readDict();
	*MapObject::getWorldDict() = d;
	Bool exists;
	Int theWeather = MapObject::getWorldDict()->getInt(TheKey_weather, &exists);
	if (exists) {
		TheWritableGlobalData->m_weather = (Weather) theWeather;
	}
	return true;
}

/**
* WorldHeightMap::ParseLightingDataChunk - read a global lights chunk.
* Format is the newer CHUNKY format.
*	See WHeightMapEdit.cpp for the writer.
*	Input: DataChunkInput
*
*/
static RGBColor readMapColor(DataChunkInput &file)
{
	return {file.readReal(), file.readReal(), file.readReal()};
}

static GlobalData::TerrainLighting readMapLight(DataChunkInput &file)
{
	return {
		readMapColor(file),
		readMapColor(file),
		{file.readReal(), file.readReal(), file.readReal()}
	};
}

void WorldHeightMap::readBfmeLighting(DataChunkInput &file, unsigned version)
{
	auto lighting = std::make_unique<BfmeLighting>();
	const Bool hasNoCloudFactor = version >= K_LIGHTING_VERSION_8;
	constexpr unsigned days = TIME_OF_DAY_COUNT - TIME_OF_DAY_FIRST;
	const unsigned expectedSize = 3 * sizeof(Int) + (days * MAX_GLOBAL_LIGHTS * 3 * 9 + 1 + 9) * sizeof(Real)
		+ (hasNoCloudFactor ? 3 * sizeof(Real) : 0);
	if (file.getChunkDataSizeLeft() != expectedSize)
		throw ERROR_CORRUPT_FILE_FORMAT;

	const Int timeOfDay = file.readInt();
	if (timeOfDay < TIME_OF_DAY_FIRST || timeOfDay >= TIME_OF_DAY_COUNT)
		throw ERROR_CORRUPT_FILE_FORMAT;
	lighting->timeOfDay = static_cast<TimeOfDay>(timeOfDay);
	for (Int day = TIME_OF_DAY_FIRST; day < TIME_OF_DAY_COUNT; ++day)
	{
		lighting->terrain[day][0] = readMapLight(file);
		for (auto &light : lighting->objects[day])
			light = readMapLight(file);
		for (Int slot = 1; slot < MAX_GLOBAL_LIGHTS; ++slot)
			lighting->terrain[day][slot] = readMapLight(file);
		for (auto &light : lighting->infantry[day])
			light = readMapLight(file);
	}
	lighting->overbright = file.readReal();
	lighting->filterEnabled = file.readInt();
	for (auto &color : lighting->filterColors)
		color = readMapColor(file);
	lighting->shadowColor = file.readInt();
	if (hasNoCloudFactor)
		lighting->noCloudFactor = readMapColor(file);
	m_bfmeLighting = std::move(lighting);
}

static GlobalData::TerrainLighting scaleMapLight(GlobalData::TerrainLighting light, Real scale)
{
	light.ambient *= scale;
	light.diffuse *= scale;
	return light;
}

void WorldHeightMap::applyBfmeLighting() const
{
	const Real scale = m_bfmeLighting->overbright > 1.0f ? 2.0f : 1.0f;
	for (Int day = TIME_OF_DAY_FIRST; day < TIME_OF_DAY_COUNT; ++day)
	{
		for (Int slot = 0; slot < MAX_GLOBAL_LIGHTS; ++slot)
		{
			TheWritableGlobalData->m_terrainLighting[day][slot] = scaleMapLight(m_bfmeLighting->terrain[day][slot], scale);
			TheWritableGlobalData->m_terrainObjectsLighting[day][slot] = scaleMapLight(m_bfmeLighting->objects[day][slot], scale);
		}
	}
	TheWritableGlobalData->setTimeOfDay(m_bfmeLighting->timeOfDay);
	if (TheW3DShadowManager)
		TheW3DShadowManager->setShadowColor(m_bfmeLighting->shadowColor);
}

std::vector<UnsignedInt> WorldHeightMap::getBfmeLightingAudit() const
{
	if (!m_bfmeLighting)
		return {};

	const auto &lighting = *m_bfmeLighting;
	std::vector<UnsignedInt> fields{
		static_cast<UnsignedInt>(lighting.timeOfDay), std::bit_cast<UnsignedInt>(lighting.overbright),
		lighting.filterEnabled
	};
	const auto appendColor = [&fields](const RGBColor &color)
	{
		for (Real channel : {color.red, color.green, color.blue})
			fields.push_back(std::bit_cast<UnsignedInt>(channel));
	};
	for (const auto &color : lighting.filterColors)
		appendColor(color);
	fields.push_back(lighting.shadowColor);
	appendColor(lighting.noCloudFactor);
	for (Int day = TIME_OF_DAY_FIRST; day < TIME_OF_DAY_COUNT; ++day)
	{
		for (const auto *lights : {&lighting.terrain[day], &lighting.objects[day], &lighting.infantry[day]})
		{
			for (const auto &light : *lights)
			{
				appendColor(light.ambient);
				appendColor(light.diffuse);
				for (Real component : {light.lightPos.x, light.lightPos.y, light.lightPos.z})
					fields.push_back(std::bit_cast<UnsignedInt>(component));
			}
		}
	}
	return fields;
}

Bool WorldHeightMap::ParseLogicalLightingDataChunk(DataChunkInput &file, DataChunkInfo *info, void *userData)
{
	if (info->version >= K_LIGHTING_VERSION_7)
	{
		try
		{
			static_cast<WorldHeightMap *>(userData)->readBfmeLighting(file, info->version);
		}
		catch (ErrorCode)
		{
			if (!INIAudit::isEnabled())
				throw;
			INIAudit::record("error", "Invalid BFME lighting map chunk", info->label.str(), 0);
		}
	}
	else if (info->version > K_LIGHTING_VERSION_3)
		file.skipUnsupportedRequiredChunk(*info);
	return true;
}

Bool WorldHeightMap::ParseLightingDataChunk(DataChunkInput &file, DataChunkInfo *info, void *userData)
{
	if (info->version > K_LIGHTING_VERSION_3)
		return ParseLogicalLightingDataChunk(file, info, userData);

		TheWritableGlobalData->m_timeOfDay = (TimeOfDay)file.readInt();
		Int i;
		GlobalData::TerrainLighting	initLightValues	= { { 0,0,0},{0,0,0},{0,0,-1.0f}};

		// initialize the directions of the lights to not be totally invalid, in case old maps are read
		for (i=0; i<4; i++) {
			for (Int j=0;j<MAX_GLOBAL_LIGHTS; j++) {
				TheWritableGlobalData->m_terrainObjectsLighting[i+TIME_OF_DAY_FIRST][j]=initLightValues;
				TheWritableGlobalData->m_terrainLighting[i+TIME_OF_DAY_FIRST][j]=initLightValues;
			}
		}

		for (i=0; i<4; i++) {
			TheWritableGlobalData->m_terrainLighting[i+TIME_OF_DAY_FIRST][0].ambient.red = file.readReal();
			TheWritableGlobalData->m_terrainLighting[i+TIME_OF_DAY_FIRST][0].ambient.green = file.readReal();
			TheWritableGlobalData->m_terrainLighting[i+TIME_OF_DAY_FIRST][0].ambient.blue = file.readReal();
			TheWritableGlobalData->m_terrainLighting[i+TIME_OF_DAY_FIRST][0].diffuse.red = file.readReal();
			TheWritableGlobalData->m_terrainLighting[i+TIME_OF_DAY_FIRST][0].diffuse.green = file.readReal();
			TheWritableGlobalData->m_terrainLighting[i+TIME_OF_DAY_FIRST][0].diffuse.blue = file.readReal();
			TheWritableGlobalData->m_terrainLighting[i+TIME_OF_DAY_FIRST][0].lightPos.x = file.readReal();
			TheWritableGlobalData->m_terrainLighting[i+TIME_OF_DAY_FIRST][0].lightPos.y = file.readReal();
			TheWritableGlobalData->m_terrainLighting[i+TIME_OF_DAY_FIRST][0].lightPos.z = file.readReal();

			TheWritableGlobalData->m_terrainObjectsLighting[i+TIME_OF_DAY_FIRST][0].ambient.red = file.readReal();
			TheWritableGlobalData->m_terrainObjectsLighting[i+TIME_OF_DAY_FIRST][0].ambient.green = file.readReal();
			TheWritableGlobalData->m_terrainObjectsLighting[i+TIME_OF_DAY_FIRST][0].ambient.blue = file.readReal();
			TheWritableGlobalData->m_terrainObjectsLighting[i+TIME_OF_DAY_FIRST][0].diffuse.red = file.readReal();
			TheWritableGlobalData->m_terrainObjectsLighting[i+TIME_OF_DAY_FIRST][0].diffuse.green = file.readReal();
			TheWritableGlobalData->m_terrainObjectsLighting[i+TIME_OF_DAY_FIRST][0].diffuse.blue = file.readReal();
			TheWritableGlobalData->m_terrainObjectsLighting[i+TIME_OF_DAY_FIRST][0].lightPos.x = file.readReal();
			TheWritableGlobalData->m_terrainObjectsLighting[i+TIME_OF_DAY_FIRST][0].lightPos.y = file.readReal();
			TheWritableGlobalData->m_terrainObjectsLighting[i+TIME_OF_DAY_FIRST][0].lightPos.z = file.readReal();

			if (info->version >= K_LIGHTING_VERSION_2) {
				for (Int j=1; j<3; j++)	//added support for 2 extra object lights
				{
					TheWritableGlobalData->m_terrainObjectsLighting[i+TIME_OF_DAY_FIRST][j].ambient.red = file.readReal();
					TheWritableGlobalData->m_terrainObjectsLighting[i+TIME_OF_DAY_FIRST][j].ambient.green = file.readReal();
					TheWritableGlobalData->m_terrainObjectsLighting[i+TIME_OF_DAY_FIRST][j].ambient.blue = file.readReal();
					TheWritableGlobalData->m_terrainObjectsLighting[i+TIME_OF_DAY_FIRST][j].diffuse.red = file.readReal();
					TheWritableGlobalData->m_terrainObjectsLighting[i+TIME_OF_DAY_FIRST][j].diffuse.green = file.readReal();
					TheWritableGlobalData->m_terrainObjectsLighting[i+TIME_OF_DAY_FIRST][j].diffuse.blue = file.readReal();
					TheWritableGlobalData->m_terrainObjectsLighting[i+TIME_OF_DAY_FIRST][j].lightPos.x = file.readReal();
					TheWritableGlobalData->m_terrainObjectsLighting[i+TIME_OF_DAY_FIRST][j].lightPos.y = file.readReal();
					TheWritableGlobalData->m_terrainObjectsLighting[i+TIME_OF_DAY_FIRST][j].lightPos.z = file.readReal();
				}
			}
			if (info->version >= K_LIGHTING_VERSION_3) {
				for (Int j=1; j<3; j++)	//added support for 2 extra terrain lights
				{
					TheWritableGlobalData->m_terrainLighting[i+TIME_OF_DAY_FIRST][j].ambient.red = file.readReal();
					TheWritableGlobalData->m_terrainLighting[i+TIME_OF_DAY_FIRST][j].ambient.green = file.readReal();
					TheWritableGlobalData->m_terrainLighting[i+TIME_OF_DAY_FIRST][j].ambient.blue = file.readReal();
					TheWritableGlobalData->m_terrainLighting[i+TIME_OF_DAY_FIRST][j].diffuse.red = file.readReal();
					TheWritableGlobalData->m_terrainLighting[i+TIME_OF_DAY_FIRST][j].diffuse.green = file.readReal();
					TheWritableGlobalData->m_terrainLighting[i+TIME_OF_DAY_FIRST][j].diffuse.blue = file.readReal();
					TheWritableGlobalData->m_terrainLighting[i+TIME_OF_DAY_FIRST][j].lightPos.x = file.readReal();
					TheWritableGlobalData->m_terrainLighting[i+TIME_OF_DAY_FIRST][j].lightPos.y = file.readReal();
					TheWritableGlobalData->m_terrainLighting[i+TIME_OF_DAY_FIRST][j].lightPos.z = file.readReal();
				}
			}
		}
		if (!file.atEndOfChunk()) {
			UnsignedInt shadowColor = file.readInt();
			if (TheW3DShadowManager) {
				TheW3DShadowManager->setShadowColor(shadowColor);
			}
		}
	DEBUG_ASSERTCRASH(file.atEndOfChunk(), ("Unexpected data left over."));
	return true;
}

/**
* WorldHeightMap::ParseObjectsDataChunk - read a height map chunk.
* Format is the newer CHUNKY format.
*	See WHeightMapEdit.cpp for the writer.
*	Input: DataChunkInput
*
*/
Bool WorldHeightMap::ParseObjectsDataChunk(DataChunkInput &file, DataChunkInfo *info, void *userData)
{
	if (file.getUnsupportedRequiredChunk())
		return true;
	file.m_currentObject = nullptr;
	file.registerVersionedParser("Object", info->label, ParseObjectDataChunk, K_OBJECTS_VERSION_3);
	return (file.parse(userData));
}

Bool WorldHeightMap::isValidMapSize(Int width, Int height, Int borderSize)
{
	if (width <= 0 || width > MAX_MAP_DIMENSION)
		return false;
	if (height <= 0 || height > MAX_MAP_DIMENSION)
		return false;
	if (width * height > MAX_MAP_AREA)
		return false;
	if (borderSize < 0 || borderSize > (width - 1) / 2 || borderSize > (height - 1) / 2)
		return false;
	return true;
}

/**
* WorldHeightMap::ParseHeightMapData - read a height map chunk.
* Format is the newer CHUNKY format.
*	See WHeightMapEdit.cpp for the writer.
*	Input: DataChunkInput
*
*/
Bool WorldHeightMap::ParseHeightMapDataChunk(DataChunkInput &file, DataChunkInfo *info, void *userData)
{
	auto *map = static_cast<WorldHeightMap *>(userData);
	map->readHeightData(file, info->version, false);
	const Int bytesPerRow = (map->m_width + 7) / 8;
	map->m_seismicUpdateWidth = bytesPerRow;
	map->m_seismicUpdateFlag = MSGNEW("WorldHeightMap::m_seismicUpdateFlag") UnsignedByte[bytesPerRow * map->m_height];
	map->clearSeismicUpdateFlags();
	map->m_seismicZVelocities = MSGNEW("WorldHeightMap::m_seismicZVelocities") Real[map->m_dataSize];
	map->fillSeismicZVelocities(0);
	DEBUG_ASSERTCRASH(file.atEndOfChunk(), ("Unexpected data left over."));
	return true;
}

Bool WorldHeightMap::ParseSizeOnlyInChunk(DataChunkInput &file, DataChunkInfo *info, void *userData)
{
	static_cast<WorldHeightMap *>(userData)->readHeightData(file, info->version, true);
	return true;
}

void WorldHeightMap::readHeightData(DataChunkInput &file, unsigned version, Bool logicalDataOnly)
{
	if (m_data || (version == K_HEIGHT_MAP_VERSION_5 && file.getChunkDataSizeLeft() < 5 * sizeof(Int)))
		throw ERROR_CORRUPT_FILE_FORMAT;
	m_width = file.readInt();
	m_height = file.readInt();
	if (version >= K_HEIGHT_MAP_VERSION_3) {
		m_borderSize = file.readInt();
	} else {
		m_borderSize = 0;
	}

	if (!isValidMapSize(m_width, m_height, m_borderSize)) {
		throw ERROR_CORRUPT_FILE_FORMAT;
	}

	if (version >= K_HEIGHT_MAP_VERSION_4) {
		Int numBorders = file.readInt();
		const auto remaining = file.getChunkDataSizeLeft();
		if (numBorders < 0 || remaining < sizeof(Int) ||
			static_cast<unsigned>(numBorders) > (remaining - sizeof(Int)) / (2 * sizeof(Int)))
			throw ERROR_CORRUPT_FILE_FORMAT;
		m_boundaries.resize(numBorders);
		for (int i = 0; i < numBorders; ++i) {
			m_boundaries[i].x = file.readInt();
			m_boundaries[i].y = file.readInt();
		}
	} else {
		m_boundaries.resize(1);
		m_boundaries[0].x = m_width - 2 * m_borderSize;
		m_boundaries[0].y = m_height - 2 * m_borderSize;
	}

	m_dataSize = file.readInt();
	m_heightSampleBytes = HeightMapSamples::bytesPerSample(version);
	m_heightScale = HeightMapSamples::scale(m_heightSampleBytes, MAP_HEIGHT_SCALE);
	const unsigned payloadBytes = static_cast<unsigned>(m_dataSize) * m_heightSampleBytes;
	const unsigned remaining = file.getChunkDataSizeLeft();
	if (m_dataSize != m_width * m_height || payloadBytes > remaining ||
		(m_heightSampleBytes == 2 && payloadBytes != remaining))
		throw ERROR_CORRUPT_FILE_FORMAT;

	std::vector<UnsignedByte> bytes(payloadBytes);
	file.readArrayOfBytes(reinterpret_cast<char *>(bytes.data()), payloadBytes);
	m_data = MSGNEW("WorldHeightMap::m_data") UnsignedShort[m_dataSize];
	HeightMapSamples::decode(bytes, {m_data, static_cast<std::size_t>(m_dataSize)}, m_heightSampleBytes);

	if (version == K_HEIGHT_MAP_VERSION_1) {
		Int newWidth = (m_width+1)/2;
		Int newHeight = (m_height+1)/2;
		Int i, j;
		for (i=0; i<newHeight; i++) {
			for (j=0; j<newWidth; j++) {
				m_data[i*newWidth+j] = m_data[2*i*m_width+2*j];
			}
		}
		if (logicalDataOnly)
		{
			m_width = newWidth;
			m_height = newHeight;
		}
	}
}

/**
* WorldHeightMap::ParseBlendTileDataChunk - read a blend tile info chunk.
* Format is the newer CHUNKY format.
*	See WHeightMapEdit.cpp for the writer.
*	Input: DataChunkInput
*
*/
Bool WorldHeightMap::ParseBlendTileDataChunk(DataChunkInput &file, DataChunkInfo *info, void *userData)
{
	WorldHeightMap *pThis = (WorldHeightMap *)userData;
	return pThis->ParseBlendTileData(file, info, true);
}

static Bool supportsBlendTileVersion(DataChunkVersionType version)
{
	return (version >= K_BLEND_TILE_VERSION_1 && version <= K_BLEND_TILE_VERSION_8) || version == 11 || version == 14 ||
		version == 15 || version == 16 || version == 18;
}

Bool WorldHeightMap::ParseLogicalBlendTileDataChunk(DataChunkInput &file, DataChunkInfo *info, void *userData)
{
	if (!supportsBlendTileVersion(info->version))
	{
		file.skipUnsupportedRequiredChunk(*info);
		return true;
	}
	auto *map = static_cast<WorldHeightMap *>(userData);
	if (!map->m_data) {
		if (info->version > K_BLEND_TILE_VERSION_8)
			file.skipUnsupportedRequiredChunk(*info);
		return true;
	}
	if (info->version <= K_BLEND_TILE_VERSION_8 && !map->m_readAuditBlendTiles)
		return true;
	return map->ParseBlendTileData(file, info, false);
}

namespace
{
class BfmePolygonReader
{
	DataChunkInput &m_file;

public:
	explicit BfmePolygonReader(DataChunkInput &file) : m_file(file) {}

	template<class T>
	T read()
	{
		T value;
		if (sizeof(value) > m_file.getChunkDataSizeLeft())
			throw ERROR_CORRUPT_FILE_FORMAT;
		m_file.readArrayOfBytes(reinterpret_cast<char *>(&value), sizeof(value));
		return value;
	}

	AsciiString readString()
	{
		const UnsignedShort size = read<UnsignedShort>();
		if (size >= AsciiString::MAX_LEN || size > m_file.getChunkDataSizeLeft())
			throw ERROR_CORRUPT_FILE_FORMAT;
		AsciiString value;
		if (size != 0)
		{
			char *bytes = value.getBufferForRead(size);
			m_file.readArrayOfBytes(bytes, size);
			bytes[size] = '\0';
		}
		return value;
	}

	ICoord3D readPoint()
	{
		const Real x = read<Real>();
		const Real y = read<Real>();
		const Real extent = MAX_MAP_DIMENSION * MAP_XY_FACTOR;
		if (!(x >= -extent && x <= extent && y >= -extent && y <= extent))
			throw ERROR_CORRUPT_FILE_FORMAT;
		return {static_cast<Int>(x), static_cast<Int>(y), 0};
	}
};

struct WaterEdge
{
	ICoord3D low;
	ICoord3D high;

	double xAt(double y) const
	{
		return low.x + (y - low.y) * (high.x - low.x) / (high.y - low.y);
	}
};

std::vector<std::array<Coord2D, 4>> tessellateStandingWater(const PolygonTrigger &polygon)
{
	constexpr size_t maxPatches = 16384;
	std::vector<WaterEdge> edges;
	std::vector<double> levels;
	const Int count = polygon.getNumPoints();
	for (Int i = 0; i < count; ++i)
	{
		const ICoord3D &a = *polygon.getPoint(i);
		const ICoord3D &b = *polygon.getPoint((i + 1) % count);
		levels.push_back(a.y);
		if (a.y != b.y)
			edges.push_back(a.y < b.y ? WaterEdge{a, b} : WaterEdge{b, a});
	}
	for (size_t i = 0; i < edges.size(); ++i)
	{
		const WaterEdge &a = edges[i];
		for (size_t j = 0; j < i; ++j)
		{
			const WaterEdge &b = edges[j];
			const double low = std::max(a.low.y, b.low.y);
			const double high = std::min(a.high.y, b.high.y);
			if (low >= high)
				continue;
			const double start = a.xAt(low) - b.xAt(low);
			const double end = a.xAt(high) - b.xAt(high);
			if ((start < 0 && end > 0) || (start > 0 && end < 0))
			{
				if (levels.size() >= maxPatches)
					throw ERROR_CORRUPT_FILE_FORMAT;
				levels.push_back(low + (high - low) * start / (start - end));
			}
		}
	}

	std::sort(levels.begin(), levels.end());
	levels.erase(std::unique(levels.begin(), levels.end()), levels.end());
	std::vector<std::array<Coord2D, 4>> patches;
	std::vector<std::pair<double, size_t>> crossings;
	for (size_t level = 1; level < levels.size(); ++level)
	{
		const double low = levels[level - 1];
		const double high = levels[level];
		const double mid = (low + high) * 0.5;
		if (static_cast<Real>(low) == static_cast<Real>(high))
			continue;
		crossings.clear();
		for (size_t i = 0; i < edges.size(); ++i)
			if (mid > edges[i].low.y && mid < edges[i].high.y)
				crossings.emplace_back(edges[i].xAt(mid), i);
		std::sort(crossings.begin(), crossings.end());
		for (size_t i = 1; i < crossings.size(); i += 2)
		{
			if (static_cast<Real>(crossings[i - 1].first) == static_cast<Real>(crossings[i].first))
				continue;
			if (patches.size() >= maxPatches)
				throw ERROR_CORRUPT_FILE_FORMAT;
			const WaterEdge &left = edges[crossings[i - 1].second];
			const WaterEdge &right = edges[crossings[i].second];
			const Real lowY = static_cast<Real>(low);
			const Real highY = static_cast<Real>(high);
			patches.push_back({{
				{static_cast<Real>(left.xAt(low)), lowY}, {static_cast<Real>(right.xAt(low)), lowY},
				{static_cast<Real>(right.xAt(high)), highY}, {static_cast<Real>(left.xAt(high)), highY}
			}});
		}
	}
	return patches;
}
}

void WorldHeightMap::MapPolygonDeleter::operator()(PolygonTrigger *polygon) const
{
	deleteInstance(polygon);
}

Bool WorldHeightMap::ParseTriggerAreas(DataChunkInput &file, DataChunkInfo *info, void *userData)
{
	auto *map = static_cast<WorldHeightMap *>(userData);
	if (map->getHeightSampleBytes() != 2 || info->version != 1)
		return true;

	std::vector<MapPolygon> polygons;
	try
	{
		if (info->dataSize > std::numeric_limits<Int>::max())
			throw ERROR_CORRUPT_FILE_FORMAT;
		BfmePolygonReader reader(file);
		const UnsignedInt count = reader.read<UnsignedInt>();
		constexpr UnsignedInt minAreaBytes = 3 * sizeof(UnsignedInt) + 2 * sizeof(UnsignedShort);
		constexpr UnsignedInt maxAreas = 4096;
		if (count > maxAreas || count > file.getChunkDataSizeLeft() / minAreaBytes)
			throw ERROR_CORRUPT_FILE_FORMAT;
		polygons.reserve(count);
		for (UnsignedInt area = 0; area < count; ++area)
		{
			const AsciiString name = reader.readString();
			const AsciiString layer = reader.readString();
			const UnsignedInt id = reader.read<UnsignedInt>();
			const UnsignedInt points = reader.read<UnsignedInt>();
			constexpr UnsignedInt maxPoints = 4096;
			const UnsignedInt remaining = file.getChunkDataSizeLeft();
			const UnsignedInt footer = sizeof(UnsignedInt) + (count - area - 1) * minAreaBytes;
			if (id >= std::numeric_limits<Int>::max() || points > maxPoints || remaining < footer ||
				points > (remaining - footer) / (2 * sizeof(Real)))
				throw ERROR_CORRUPT_FILE_FORMAT;

			MapPolygon polygon(newInstance(PolygonTrigger)(points, id));
			polygon->setTriggerName(name);
			polygon->setLayerName(layer);
			polygon->setBfmeTriggerAreaId(id);
			for (UnsignedInt point = 0; point < points; ++point)
				polygon->addPoint(reader.readPoint());
			if (reader.read<UnsignedInt>() != 0)
				throw ERROR_CORRUPT_FILE_FORMAT;
			polygons.push_back(std::move(polygon));
		}
		if (file.getChunkDataSizeLeft() != 0)
			throw ERROR_CORRUPT_FILE_FORMAT;
	}
	catch (ErrorCode)
	{
		if (!INIAudit::isEnabled())
			throw;
		INIAudit::record("error", "Invalid trigger areas map chunk", info->label.str(), 0);
		return true;
	}
	for (auto &polygon : polygons)
		map->m_triggerAreas.push_back(std::move(polygon));
	return true;
}

Bool WorldHeightMap::ParseStandingWaterAreas(DataChunkInput &file, DataChunkInfo *info, void *userData)
{
	auto *map = static_cast<WorldHeightMap *>(userData);
	if (map->getHeightSampleBytes() != 2)
		return true;

	std::vector<MapPolygon> polygons;
	try
	{
		if (info->dataSize > std::numeric_limits<Int>::max())
			throw ERROR_CORRUPT_FILE_FORMAT;
		BfmePolygonReader reader(file);
		const UnsignedInt count = reader.read<UnsignedInt>();
		constexpr UnsignedInt minAreaBytes = 4 * sizeof(UnsignedInt) + sizeof(UnsignedByte) +
			6 * sizeof(UnsignedShort);
		if (count > file.getChunkDataSizeLeft() / minAreaBytes)
			throw ERROR_CORRUPT_FILE_FORMAT;
		for (UnsignedInt area = 0; area < count; ++area)
		{
			const UnsignedInt id = reader.read<UnsignedInt>();
			const AsciiString name = reader.readString();
			const AsciiString layer = reader.readString();
			PolygonTrigger::StandingWaterSettings settings;
			settings.sourceId = id;
			settings.uvScrollSpeed = reader.read<Real>();
			const UnsignedByte additive = reader.read<UnsignedByte>();
			settings.additive = additive != 0;
			settings.bumpTexture = reader.readString();
			settings.skyTexture = reader.readString();
			const UnsignedInt points = reader.read<UnsignedInt>();
			constexpr UnsignedInt maxWaterPoints = 4096;
			if (id >= std::numeric_limits<Int>::max() || additive > 1 || !std::isfinite(settings.uvScrollSpeed) ||
				points > maxWaterPoints || points > file.getChunkDataSizeLeft() / (2 * sizeof(Real)))
				throw ERROR_CORRUPT_FILE_FORMAT;

			MapPolygon polygon(newInstance(PolygonTrigger)(points, id));
			polygon->setTriggerName(name);
			polygon->setLayerName(layer);
			for (UnsignedInt point = 0; point < points; ++point)
				polygon->addPoint(reader.readPoint());
			const UnsignedInt height = reader.read<UnsignedInt>();
			if (height > std::numeric_limits<Int>::max())
				throw ERROR_CORRUPT_FILE_FORMAT;
			settings.fxShader = reader.readString();
			settings.depthColors = reader.readString();
			if (points < 3)
				continue;
			for (Int point = 0; point < polygon->getNumPoints(); ++point)
			{
				ICoord3D vertex = *polygon->getPoint(point);
				vertex.z = static_cast<Int>(height);
				polygon->setPoint(vertex, point);
			}
			settings.patches = tessellateStandingWater(*polygon);
			if (settings.patches.empty())
				continue;
			polygon->setStandingWater(std::move(settings));
			polygons.push_back(std::move(polygon));
		}
		if (file.getChunkDataSizeLeft() != 0)
			throw ERROR_CORRUPT_FILE_FORMAT;
	}
	catch (ErrorCode)
	{
		if (!INIAudit::isEnabled())
			throw;
		INIAudit::record("error", "Invalid standing water map chunk", info->label.str(), 0);
		return true;
	}
	for (auto &polygon : polygons)
		map->m_standingWater.push_back(std::move(polygon));
	return true;
}

Bool WorldHeightMap::writeBlendTileChecksums(const char *path) const
{
	if (!m_tileNdxes)
		return false;
	FILE *output = std::fopen(path, "w");
	if (!output)
		return false;
	auto writeLayer = [output](const char *name, const auto *values, Int count, unsigned bytes) {
		unsigned long long hash = 14695981039346656037ull;
		for (Int i = 0; i < count; ++i) {
			unsigned value = values[i];
			for (unsigned j = 0; j < bytes; ++j) {
				hash = (hash ^ (value & 255u)) * 1099511628211ull;
				value >>= 8;
			}
		}
		std::fprintf(output, "%s\t%d\t%016llx\n", name, count, hash);
	};
	std::fprintf(output, "tables\t%d\t%d\t%d\t%d\n", m_numBitmapTiles, m_numBlendedTiles,
		m_numCliffInfo, m_numTextureClasses);
	writeLayer("tile", m_tileNdxes, m_dataSize, 2);
	writeLayer("blend", m_blendTileNdxes, m_dataSize, 4);
	writeLayer("extraBlend", m_extraBlendTileNdxes, m_dataSize, 4);
	writeLayer("cliff", m_cliffInfoNdxes, m_dataSize, 4);
	writeLayer("impassable", m_cellCliffState, m_flipStateWidth * m_height, 1);
	writeLayer("impassableToPlayers", m_impassableToPlayers.data(), m_impassableToPlayers.size(), 1);
	writeLayer("passageWidths", m_passageWidths.data(), m_passageWidths.size(), 1);
	writeLayer("taintable", m_taintable.data(), m_taintable.size(), 1);
	writeLayer("extraPassable", m_extraPassable.data(), m_extraPassable.size(), 1);
	writeLayer("flammability", m_flammability.data(), m_flammability.size(), 1);
	writeLayer("visibility", m_visibility.data(), m_visibility.size(), 1);
	const Bool ok = !std::ferror(output);
	return std::fclose(output) == 0 && ok;
}

/** Function to read in the tiles for a texture class. */
void WorldHeightMap::readTexClass(TXTextureClass *texClass, TileData **tileData, Bool bfme)
{
	File *theFile = nullptr;

	// get the file from the description in TheTerrainTypes
	TerrainType *terrain = TheTerrainTypes ? TheTerrainTypes->findTerrain(texClass->name) : nullptr;
	AsciiString texturePath;
	if (terrain==nullptr)
	{
#ifdef LOAD_TEST_ASSETS
		theFile = TheFileSystem->openFile( texClass->name.str(), File::READ|File::BINARY);
#endif
	}
	else
	{
		texturePath.format("%s%s", TERRAIN_TGA_DIR_PATH, terrain->getTexture().str());
		theFile = TheFileSystem->openFile(texturePath.str(), File::READ | File::BINARY);
	}

	Bool loaded = false;
	if (theFile != nullptr) {
		GDIFileStream theStream(theFile);
		InputStream *pStr = &theStream;
		Bool isLegacyGrid = false;
		Bool halfTile = false;
		Int numTiles = WorldHeightMap::countTiles(pStr, bfme ? &halfTile : nullptr, texClass->numTiles, &isLegacyGrid);
		theFile->seek(0, File::START);
		if (numTiles >= texClass->numTiles) {
			numTiles = texClass->numTiles;
			Int width;
			for (width = 10; width >= 1; width--) {
				if (numTiles >= width*width) {
					numTiles = width*width;
					break;
				}
			}
			loaded = WorldHeightMap::readTiles(pStr, tileData + texClass->firstTile, width, isLegacyGrid);
		}
		theFile->close();
	}
	if (bfme && (!loaded || std::getenv("GENERALSX_ASSET_SMOKE_TERRAIN"))) {
		std::fprintf(stderr, "Terrain texture %s: %s (%s)\n", texClass->name.str(),
			texturePath.isEmpty() ? "no Terrain definition" : texturePath.str(), loaded ? "loaded" : "unavailable");
		std::fflush(stderr);
	}
}

/**
* WorldHeightMap::ParseBlendTileData - read a blend tile info chunk.
* Format is the newer CHUNKY format.
*	See WHeightMapEdit.cpp for the writer.
*	Input: DataChunkInput
*
*/
Bool WorldHeightMap::ParseBlendTileData(DataChunkInput &file, DataChunkInfo *info, Bool loadTextures)
{
	if (!supportsBlendTileVersion(info->version))
	{
		file.skipUnsupportedRequiredChunk(*info);
		return true;
	}
	int i, j;
	Int len = file.readInt();
	if (m_tileNdxes || m_dataSize != len || len <= 0) {
		throw ERROR_CORRUPT_FILE_FORMAT	;
	}
	m_tileNdxes = MSGNEW("WorldHeightMap_ParseBlendTileData") Short[m_dataSize];
	m_cliffInfoNdxes = MSGNEW("WorldHeightMap_ParseBlendTileData") Int[m_dataSize]{};
	m_blendTileNdxes = MSGNEW("WorldHeightMap_ParseBlendTileData") Int[m_dataSize]{};
	m_extraBlendTileNdxes = MSGNEW("WorldHeightMap_ParseBlendTileData") Int[m_dataSize]{};
	// Note - we have one less cell than the width & height. But for paranoia, allocate
	// extra row. jba.
	//
	Int numBytesX = (m_width+7)/8;	//how many bytes to fit all bitflags
	Int numBytesY = m_height;

	m_flipStateWidth=numBytesX;

	m_cellFlipState	= MSGNEW("WorldHeightMap_getTerrainTexture") UnsignedByte[numBytesX*numBytesY];
	m_cellCliffState	= MSGNEW("WorldHeightMap_getTerrainTexture") UnsignedByte[numBytesX*numBytesY];
	memset(m_cellFlipState,0,numBytesX*numBytesY);	//clear all flags
	memset(m_cellCliffState,0,numBytesX*numBytesY);	//clear all flags

	file.readArrayOfBytes((char*)m_tileNdxes, m_dataSize*sizeof(Short));
	const Bool wideIndices = info->version >= 14;
	auto readIndices = [&](Int *indices) {
		const unsigned bytes = wideIndices ? 4 : 2;
		if (static_cast<unsigned>(m_dataSize) > file.getChunkDataSizeLeft() / bytes)
			throw ERROR_CORRUPT_FILE_FORMAT;
		if (wideIndices) {
			file.readArrayOfBytes(reinterpret_cast<char *>(indices), m_dataSize * sizeof(Int));
		} else {
			std::vector<Short> legacy(m_dataSize);
			file.readArrayOfBytes(reinterpret_cast<char *>(legacy.data()), m_dataSize * sizeof(Short));
			for (Int cell = 0; cell < m_dataSize; ++cell)
				indices[cell] = info->version <= K_BLEND_TILE_VERSION_8
					? static_cast<Int>(legacy[cell]) : static_cast<UnsignedShort>(legacy[cell]);
		}
	};
	readIndices(m_blendTileNdxes);
	if (info->version >= K_BLEND_TILE_VERSION_6) {
		readIndices(m_extraBlendTileNdxes);
		if (loadTextures && !TheGlobalData->m_use3WayTerrainBlends)
			std::fill_n(m_extraBlendTileNdxes, m_dataSize, 0);
	}
	if (info->version >= K_BLEND_TILE_VERSION_5)
		readIndices(m_cliffInfoNdxes);
	if (info->version >= K_BLEND_TILE_VERSION_7) {
		if (info->version==K_BLEND_TILE_VERSION_7) {
			Int byteWidth = (m_width+1)/8; // previous incorrect length that got used to save the file.  jba. [4/3/2003]
			UnsignedByte *data = new UnsignedByte[m_height*byteWidth];
			file.readArrayOfBytes((char*)data, m_height*byteWidth);
			for (j=0; j<m_height; j++) {
				for (i=0; i<byteWidth; i++) {
					m_cellCliffState[j*m_flipStateWidth + i] = data[j*byteWidth + i];
				}
			}
			delete[] data;
		} else {
			file.readArrayOfBytes((char*)m_cellCliffState, m_height*m_flipStateWidth);
		}
	} else {
		initCliffFlagsFromHeights();
	}
	auto readLayer = [&](std::vector<UnsignedByte> &layer, Int size) {
		if (static_cast<unsigned>(size) > file.getChunkDataSizeLeft())
			throw ERROR_CORRUPT_FILE_FORMAT;
		layer.resize(size);
		file.readArrayOfBytes(reinterpret_cast<char *>(layer.data()), size);
	};
	const Int packedSize = m_height * m_flipStateWidth;
	if (info->version >= 10) readLayer(m_impassableToPlayers, packedSize);
	if (info->version >= 11) readLayer(m_passageWidths, packedSize);
	if (info->version >= 14) readLayer(m_taintable, packedSize);
	if (info->version >= 15) readLayer(m_extraPassable, packedSize);
	if (info->version >= 16) readLayer(m_flammability, m_dataSize);
	if (info->version >= 17) readLayer(m_visibility, packedSize);
	const Bool retail = RetailFileLayers::enabled();
	const Int sourceTileLimit = retail ? ROTWK_TERRAIN_TILE_LIMIT : 16384;
	const Int edgeTileLimit = retail ? ROTWK_TERRAIN_TILE_LIMIT : NUM_SOURCE_TILES;
	const Int textureClassLimit = retail ? ROTWK_TERRAIN_CLASS_LIMIT : NUM_TEXTURE_CLASSES;
	m_numBitmapTiles = file.readInt();
	if (m_numBitmapTiles < 0 || m_numBitmapTiles > sourceTileLimit)
		throw ERROR_CORRUPT_FILE_FORMAT;
	m_sourceTiles.resize(m_numBitmapTiles);
	m_numBlendedTiles = file.readInt();
	if (m_numBlendedTiles < 1)
		throw ERROR_CORRUPT_FILE_FORMAT;
	if (info->version >= K_BLEND_TILE_VERSION_5) {
		m_numCliffInfo = file.readInt();
	} else {
		m_numCliffInfo = 1;	// cliffInfo[0] is the default info.
	}
	if (m_numCliffInfo < 0)
		throw ERROR_CORRUPT_FILE_FORMAT;
	m_numCliffInfo = std::max(1, m_numCliffInfo);
	m_numTextureClasses	= file.readInt();
	if (m_numTextureClasses < 0 || m_numTextureClasses > textureClassLimit)
		throw ERROR_CORRUPT_FILE_FORMAT;
	for (i=0; i<m_numTextureClasses; i++) {
		m_textureClasses[i].globalTextureClass = -1;
		m_textureClasses[i].firstTile = file.readInt();
		m_textureClasses[i].numTiles = file.readInt();
		m_textureClasses[i].width = file.readInt();

		// legacy GDF data
		// used to read "m_textureClasses[i].isGDF = file.readInt();"
	/*	Int legacy = */ file.readInt();

		m_textureClasses[i].name = file.readAsciiString();
		const auto &texture = m_textureClasses[i];
		if (texture.firstTile < 0 || texture.numTiles < 0 || texture.firstTile > m_numBitmapTiles ||
			texture.numTiles > m_numBitmapTiles - texture.firstTile || texture.width < 0 ||
			static_cast<long long>(texture.width) * texture.width > texture.numTiles)
			throw ERROR_CORRUPT_FILE_FORMAT;
		if (loadTextures && texture.numTiles)
			readTexClass(&m_textureClasses[i], m_sourceTiles.data(), info->version > K_BLEND_TILE_VERSION_8);
	}
	m_numEdgeTextureClasses = 0;
	m_numEdgeTiles = 0;
	if (info->version >= K_BLEND_TILE_VERSION_4) {
		m_numEdgeTiles	= file.readInt();
		m_numEdgeTextureClasses	= file.readInt();
		if (m_numEdgeTiles < 0 || m_numEdgeTiles > edgeTileLimit ||
			m_numEdgeTextureClasses < 0 || m_numEdgeTextureClasses > textureClassLimit)
			throw ERROR_CORRUPT_FILE_FORMAT;
		for (i=0; i<m_numEdgeTextureClasses; i++) {
			m_edgeTextureClasses[i].globalTextureClass = -1;
			m_edgeTextureClasses[i].firstTile = file.readInt();
			m_edgeTextureClasses[i].numTiles = file.readInt();
			m_edgeTextureClasses[i].width = file.readInt();
			m_edgeTextureClasses[i].name = file.readAsciiString();
			const auto &texture = m_edgeTextureClasses[i];
			if (texture.firstTile < 0 || texture.numTiles < 0 || texture.firstTile > edgeTileLimit ||
				texture.numTiles > edgeTileLimit - texture.firstTile)
				throw ERROR_CORRUPT_FILE_FORMAT;
			if (retail && (texture.firstTile > m_numEdgeTiles ||
				texture.numTiles > m_numEdgeTiles - texture.firstTile || texture.width < 0 ||
				static_cast<long long>(texture.width) * texture.width > texture.numTiles))
				throw ERROR_CORRUPT_FILE_FORMAT;
			if (loadTextures && texture.numTiles)
				readTexClass(&m_edgeTextureClasses[i], m_edgeTiles, retail && info->version > K_BLEND_TILE_VERSION_8);
		}
	}
	const unsigned blendBytes = 13 + (info->version >= K_BLEND_TILE_VERSION_3) +
		4 * (info->version >= K_BLEND_TILE_VERSION_4);
	if (static_cast<unsigned long long>(m_numBlendedTiles - 1) * blendBytes +
		static_cast<unsigned long long>(m_numCliffInfo - 1) * 38 > file.getChunkDataSizeLeft())
		throw ERROR_CORRUPT_FILE_FORMAT;
	m_blendedTiles.resize(m_numBlendedTiles);
	m_cliffInfo.resize(m_numCliffInfo);
	for (i=1; i<m_numBlendedTiles; i++) {
		Int flag;
		m_blendedTiles[i].blendNdx = file.readInt();
		m_blendedTiles[i].horiz = file.readByte();
		m_blendedTiles[i].vert = file.readByte();
		m_blendedTiles[i].rightDiagonal = file.readByte();
		m_blendedTiles[i].leftDiagonal = file.readByte();
		m_blendedTiles[i].inverted = file.readByte();
		//Allow clearing of extra blend tiles via ini and resaving of map.
		//Useful for flushing out initial maps made with buggy 3-way blending.
		if (loadTextures && !TheGlobalData->m_use3WayTerrainBlends)
			m_blendedTiles[i].inverted &= ~FLIPPED_MASK;

		if (info->version >= K_BLEND_TILE_VERSION_3) {
			m_blendedTiles[i].longDiagonal = file.readByte();
		} else {
			m_blendedTiles[i].longDiagonal = false;
		}
		if (info->version >= K_BLEND_TILE_VERSION_4) {
			m_blendedTiles[i].customBlendEdgeClass = file.readInt();
		} else {
			m_blendedTiles[i].customBlendEdgeClass = -1;
		}

		flag = file.readInt();
		DEBUG_ASSERTCRASH(flag==FLAG_VAL, ("Invalid format."));
		if (flag != FLAG_VAL) {
			throw ERROR_CORRUPT_FILE_FORMAT;
		}
	}
	if (info->version >= K_BLEND_TILE_VERSION_5) {
		for (i=1; i<m_numCliffInfo; i++) {
			m_cliffInfo[i].tileIndex = file.readInt();
			m_cliffInfo[i].u0 = file.readReal();
			m_cliffInfo[i].v0 = file.readReal();
			m_cliffInfo[i].u1 = file.readReal();
			m_cliffInfo[i].v1 = file.readReal();
			m_cliffInfo[i].u2 = file.readReal();
			m_cliffInfo[i].v2 = file.readReal();
			m_cliffInfo[i].u3 = file.readReal();
			m_cliffInfo[i].v3 = file.readReal();
			m_cliffInfo[i].flip = file.readByte();
			m_cliffInfo[i].mutant = file.readByte();
		}
	}
	// Resize me.
	if (info->version == K_BLEND_TILE_VERSION_1) {
		Int newWidth = (m_width+1)/2;
		Int newHeight = (m_height+1)/2;
		Int i, j;
		for (i=0; i<newHeight; i++) {
			for (j=0; j<newWidth; j++) {
				m_tileNdxes[i*newWidth+j] = m_tileNdxes[2*i*m_width+2*j];
				m_blendTileNdxes[i*newWidth+j] = 0;
				m_extraBlendTileNdxes[i*newWidth+j] = 0;
				m_cliffInfoNdxes[i*newWidth+j] = 0;
			}
		}
		m_numBlendedTiles = 1;
		m_numCliffInfo = 1;
		m_width= newWidth;
		m_height = newHeight;
		m_dataSize = m_width*m_height;
	}
	DEBUG_ASSERTCRASH(file.atEndOfChunk(), ("Unexpected data left over."));
	return true;
}


/**
* WorldHeightMap::ParseObjectData - read a object info chunk.
* Format is the newer CHUNKY format.
*	See WHeightMapEdit.cpp for the writer.
*	Input: DataChunkInput
*
*/
Bool WorldHeightMap::ParseObjectDataChunk(DataChunkInput &file, DataChunkInfo *info, void *userData)
{
	auto *pThis = static_cast<WorldHeightMap *>(userData);
	return pThis->ParseObjectData(file, info, userData, info->version >= K_OBJECTS_VERSION_2);
}

/**
* WorldHeightMap::ParseObjectData - read a object info chunk.
* Format is the newer CHUNKY format.
*	See WHeightMapEdit.cpp for the writer.
*	Input: DataChunkInput
*
*/
Bool WorldHeightMap::ParseObjectData(DataChunkInput &file, DataChunkInfo *info, void *userData, Bool readDict)
{
	MapObject *pPrevious = (MapObject *)file.m_currentObject;

	Coord3D loc;
	loc.x = file.readReal();
	loc.y = file.readReal();
	loc.z = file.readReal();

	Real minZ = -100*MAP_XY_FACTOR;
	Real maxZ = toWorldHeight(getMaxHeightValue() * 10);

	if (info->version <= K_OBJECTS_VERSION_2) {
		loc.z = 0;
	}

	Real angle = file.readReal();
	Int flags = file.readInt();
	AsciiString name = file.readAsciiString();
	Dict d;
	if (readDict)
	{
		d = file.readDict();
	}

	if (loc.z<minZ || loc.z>maxZ) {
		DEBUG_LOG(("Removing object at z height %f", loc.z));
		return true;
	}

	MapObject *pThisOne;

	// create the map object
	pThisOne = newInstance( MapObject )( loc, name, angle, flags, &d,
														TheThingFactory->findTemplate( name, FALSE ) );

//DEBUG_LOG(("obj %s owner %s",name.str(),d.getAsciiString(TheKey_originalOwner).str()));

	if (pThisOne->getProperties()->getType(TheKey_waypointID) == Dict::DICT_INT)
		pThisOne->setIsWaypoint();

	if (pThisOne->getProperties()->getType(TheKey_lightHeightAboveTerrain) == Dict::DICT_REAL)
		pThisOne->setIsLight();

	if (pThisOne->getProperties()->getType(TheKey_scorchType) == Dict::DICT_INT)
		pThisOne->setIsScorch();


	if (pPrevious) {
		DEBUG_ASSERTCRASH(MapObject::TheMapObjectListPtr != nullptr && pPrevious->getNext() == nullptr, ("Bad linkage."));
		pPrevious->setNextMap(pThisOne);
	}	else {
		DEBUG_ASSERTCRASH(MapObject::TheMapObjectListPtr == nullptr, ("Bad linkage."));
		MapObject::TheMapObjectListPtr = pThisOne;
	}
	file.m_currentObject = pThisOne;
	return true;
}



// Targa format:  Header

typedef struct {
	UnsignedByte	idLength;
	UnsignedByte	colorMapType; // 0 = rgb, 1 = indexed.
	UnsignedByte	imageType; //0x1 = indexed, 0x2 = rgb, 0x8 = rle.
	UnsignedByte	colorMapInfo[5]; // we ignore, only do rgb.
	Short			xOrigin;
	Short			yOrigin;
	Short			imageWidth;
	Short			imageHeight;
	UnsignedByte	pixelDepth;
	UnsignedByte	flags; //  &0x0F = alpha channel bits, &0x10 is right to left flag,
						   // 0x20 is top to bottom flag.  (0x0? is left to right, bottom to top)
						   // 0x3? is top to bottom, right to left.
} TTargaHeader;

// followed by idLength bytes of ascii data

// followed by pixel data

// followed by optional data.



// GeneralsX @feature mrkinglollipop 11/07/2026 Adds legacy-grid detection so TGAs packed at the pre-patch
// 64px cell size keep loading now that TILE_PIXEL_EXTENT is 256 (native HD grid cell).
// Unmodified base-game and custom-map TGAs are still packed as NxN grids of
// LEGACY_TILE_PIXEL_EXTENT (64px) cells -- the classic pre-patch tile size. Without a
// fallback, a legacy file's imageWidth/TILE_PIXEL_EXTENT floors to 0 and the file is
// silently rejected (0 tiles), breaking every unmodified map/mod.
//
// Detection rule (documented per plan): try the *native* 256px-cell grid first, but
// only trust it when it alone can satisfy the texture class's INI-declared tile count
// (expectedTileCount). This resolves the one genuinely ambiguous width -- 256px, which
// is simultaneously a valid native 1x1 HD grid AND a valid legacy 4x4 grid of 64px
// cells (16 tiles) -- because the base game genuinely ships 256px-wide legacy sheets
// (width census: 32, 64, 128, 256, 640). Any other width where both interpretations
// divide evenly is treated as legacy too (matches actual on-disk content) -- this is
// the "treat legacy files as 64-grids by default" simplification. The old "halfTile"
// sub-tile case (a lone 32x32 image = half of a legacy 64px cell) keeps keying off the
// legacy half-extent (32), not a new native half-extent, so it matches the same files
// it always did.
#define LEGACY_TILE_PIXEL_EXTENT 64

static Bool ChooseTileGrid(Short imageWidth, Short imageHeight, Int expectedTileCount,
														Int &outTileWidth, Int &outTileHeight, Bool &outIsLegacyGrid)
{
	Int nativeW = imageWidth / TILE_PIXEL_EXTENT;
	Int nativeH = imageHeight / TILE_PIXEL_EXTENT;
	Bool nativeValid = (nativeW>=1 && nativeW<=10 && nativeH>=1 && nativeH<=10);

	Int legacyW = imageWidth / LEGACY_TILE_PIXEL_EXTENT;
	Int legacyH = imageHeight / LEGACY_TILE_PIXEL_EXTENT;
	Bool legacyValid = (legacyW>=1 && legacyW<=10 && legacyH>=1 && legacyH<=10);

	Bool useLegacy;
	if (nativeValid && legacyValid) {
		// Ambiguous width: only trust native when the class can't possibly want more
		// tiles than the (smaller) native grid provides -- i.e. this really is a fresh
		// HD single/few-tile replacement, not an unmodified legacy sheet.
		useLegacy = !(expectedTileCount > 0 && expectedTileCount <= nativeW*nativeH);
	} else if (legacyValid) {
		useLegacy = true;
	} else if (nativeValid) {
		useLegacy = false;
	} else {
		return false;
	}

	outIsLegacyGrid = useLegacy;
	outTileWidth = useLegacy ? legacyW : nativeW;
	outTileHeight = useLegacy ? legacyH : nativeH;
	return true;
}

/// Count how many tiles come in from a targa file.
Int WorldHeightMap::countTiles(InputStream *pStr, Bool *halfTile, Int expectedTileCount, Bool *pIsLegacyGrid)
{
	TTargaHeader hdr;
	if (halfTile) {
		*halfTile = false;
	}
	if (pIsLegacyGrid) {
		*pIsLegacyGrid = false;
	}
	Int len = pStr->read(&hdr,sizeof(hdr));
	if (len!=sizeof(hdr)) return(0);

	if (hdr.colorMapType != 0) {
		return(0); // we don't do indexed at this time. jba.
	}
	if (hdr.imageType != 0x2 && hdr.imageType != 0xA) {
		return(0); // we don't do indexed at this time. jba.
	}

	if (hdr.pixelDepth < 24) return(false);
	if (hdr.pixelDepth > 32) return(false);

	Int tileWidth, tileHeight;
	Bool isLegacyGrid = false;
	if (ChooseTileGrid(hdr.imageWidth, hdr.imageHeight, expectedTileCount, tileWidth, tileHeight, isLegacyGrid)) {
		if (pIsLegacyGrid) *pIsLegacyGrid = isLegacyGrid;
		// 3x3 gives 9,
		// 2x2 gives 4,
		// 1x1 gives 1,
		// else 0;
		if (tileWidth>=10 && tileHeight >=10) return(100);
		if (tileWidth>=9 && tileHeight >=9) return(81);
		if (tileWidth>=8 && tileHeight >=8) return(64);
		if (tileWidth>=7 && tileHeight >=7) return(49);
		if (tileWidth>=6 && tileHeight >=6) return(36);
		if (tileWidth>=5 && tileHeight >=5) return(25);
		if (tileWidth>=4 && tileHeight >=4) return(16);
		if (tileWidth>=3 && tileHeight >=3) return(9);
		if (tileWidth>=2 && tileHeight >=2) return(4);
		if (tileWidth>=1 && tileHeight >=1) return(1);
	}
	// Neither native nor legacy full-cell grid fit -- last resort: a lone image half
	// as wide/tall as one LEGACY cell (32x32), always legacy-scattered.
	if (halfTile && hdr.imageHeight==LEGACY_TILE_PIXEL_EXTENT/2 && hdr.imageWidth==LEGACY_TILE_PIXEL_EXTENT/2) {
		*halfTile = true;
		if (pIsLegacyGrid) *pIsLegacyGrid = true;
		return 1;
	}
	return(0);
}
/*Break down a .tga file into a collection of tiles.  numRows * numRows total tiles.
	isLegacyGrid must be the interpretation countTiles() already chose for this same
	file (via ChooseTileGrid) -- readTiles does not re-derive it, so the two stay
	consistent even in the ambiguous-width case. */
Bool WorldHeightMap::readTiles(InputStream *pStr, TileData **tiles, Int numRows, Bool isLegacyGrid)
{
	TTargaHeader hdr;
	pStr->read(&hdr, sizeof(hdr));

	// Effective source-cell pixel size for this file's grid. scale is how many times
	// each decoded source pixel is nearest-neighbor replicated per axis into the
	// native TILE_PIXEL_EXTENT destination cell; scale==1 (no scatter, byte-identical
	// to the pre-patch loop) for native HD content.
	Int srcCellExtent = isLegacyGrid ? LEGACY_TILE_PIXEL_EXTENT : TILE_PIXEL_EXTENT;

	Int tileWidth = hdr.imageWidth/srcCellExtent;
	Int tileHeight = hdr.imageHeight/srcCellExtent;

	// Legacy "halfTile" sub-tile case: a lone image half as wide/tall as one legacy
	// cell (32x32) represents a single tile; scatter it across the full native tile.
	if (isLegacyGrid && hdr.imageHeight==srcCellExtent/2) {
		tileHeight = 1;
	}
	if (isLegacyGrid && hdr.imageWidth==srcCellExtent/2) {
		tileWidth = 1;
	}
	if (isLegacyGrid && (hdr.imageWidth==LEGACY_TILE_PIXEL_EXTENT/2 || hdr.imageHeight==LEGACY_TILE_PIXEL_EXTENT/2)) {
		srcCellExtent = LEGACY_TILE_PIXEL_EXTENT/2;
	}
	Int scale = TILE_PIXEL_EXTENT / srcCellExtent;

	if (tileWidth<numRows && tileHeight<numRows) {
		return(false);
	}
	Bool compressed = false;
	if (hdr.imageType & 0x08) {
		compressed = true;
	}
	int row = 0;
	int column = 0;
	int bytesPerPixel = (hdr.pixelDepth+7)/8;
	if (bytesPerPixel < 3) return(false);
	if (bytesPerPixel > 4) return(false);
	int i;
	for (i=0; i<numRows*numRows; i++) {
		if (tiles[i] == nullptr)
			tiles[i] = MSGNEW("WorldHeightMap_readTiles") TileData;
	}
	UnsignedByte buf[4];
	int repeatCount = 0;
//	Bool read = false;
	Bool running = false;
	for (row = 0; row < numRows*srcCellExtent; row++) {
		for (column=0; column<hdr.imageWidth; column++) {
			UnsignedByte r, g, b, a;
			if (row < hdr.imageHeight) {
				if (compressed && repeatCount==0) {
					UnsignedByte flag;
					pStr->read(&flag, 1);
					repeatCount = flag&0x7f;
					repeatCount++;
					if (flag&0x80) {
						running = true;
						pStr->read(buf, bytesPerPixel);
					} else {
						running = false;
					}
				}
				if (compressed) repeatCount--;
				if (!running) {
					pStr->read(buf, bytesPerPixel);
				}
				r = buf[2]; g = buf[1]; b = buf[0];
				if (bytesPerPixel==4) {
					a = buf[3];
				} else {
					a = 255;// solid alpha.
				}
			} else {
				r = g = b = a = 0;
			}
			if (column >= (numRows*srcCellExtent)) continue;
			int tileNdx = (column/srcCellExtent) + numRows*(row/srcCellExtent);
			int cellCol = column % srcCellExtent;
			int cellRow = row % srcCellExtent;

			UnsignedByte *tileBase = tiles[tileNdx]->getDataPtr();
			// Nearest-neighbor scatter: replicate this one decoded source pixel into
			// its scale x scale destination block. No row lookahead needed -- the
			// whole block is written immediately from the single decoded pixel.
			for (int dy=0; dy<scale; dy++) {
				int destRow = cellRow*scale+dy;
				for (int dx=0; dx<scale; dx++) {
					int destCol = cellCol*scale+dx;
					int pixelNdx = destCol + TILE_PIXEL_EXTENT*destRow;
					UnsignedByte *pixel = tileBase + pixelNdx*TILE_BYTES_PER_PIXEL;
					pixel[0] = b;
					pixel[1] = g;
					pixel[2] = r;
					pixel[3] = a;
				}
			}
		}
		DEBUG_ASSERTCRASH(repeatCount==0, ("Invalid tga."));
	}
	for (i=0; i<numRows*numRows; i++) {
		tiles[i]->updateMips();
	}
	return(true);
}



/** updateTileTexturePositions - assigns each tile a location in the texture.
*/
Int WorldHeightMap::updateTileTexturePositions(Int *edgeHeight)
{
	Int i, j;
	Int maxHeight = 0;
	Int row, column;

	const Int candidateExtents[] = { TILE_PIXEL_EXTENT, TILE_PIXEL_EXTENT_MIP1, TILE_PIXEL_EXTENT_MIP2 };
	const Int numCandidates = m_heightSampleBytes > 1 ? 3 : 1;
	m_tilePixelExtent = TILE_PIXEL_EXTENT;

	for (Int candidate = 0; candidate < numCandidates; ++candidate) {
		const Int tileExtent = candidateExtents[candidate];
		const Int tilesPerRow = TEXTURE_WIDTH / (tileExtent + TILE_OFFSET);
		std::vector<UnsignedByte> availableGrid(tilesPerRow * tilesPerRow, 1);

		for (i = 0; i < m_numBitmapTiles; i++) {
			if (m_sourceTiles[i]) {
				m_sourceTiles[i]->m_tileLocationInTexture.x = 0;
				m_sourceTiles[i]->m_tileLocationInTexture.y = 0;
			}
		}

		maxHeight = 0;
		Bool allPlaced = true;
		Int texClass;
		Int tileWidth;
		for (tileWidth = tilesPerRow; tileWidth > 0; tileWidth--) {
			for (texClass = 0; texClass < m_numTextureClasses; texClass++) {
				Int width = m_textureClasses[texClass].width;
				if (width != tileWidth) continue;
				Bool found = false;
				for (row = 0; row < tilesPerRow - width + 1 && !found; row++) {
					for (column = 0; column < tilesPerRow - width + 1 && !found; column++) {
						if (availableGrid[row * tilesPerRow + column]) {
							Bool open = true;
							for (i = 0; i < width && open; i++) {
								for (j = 0; j < width && open; j++) {
									if (!availableGrid[(row + j) * tilesPerRow + (column + i)]) {
										open = false;
									}
								}
							}
							if (open) found = true;
							break;
						}
					}
					if (found) break;
				}
				if (!found) {
					m_textureClasses[texClass].positionInTexture.x = 0;
					m_textureClasses[texClass].positionInTexture.y = 0;
					allPlaced = false;
					continue;
				}

				Int xOrigin = TILE_OFFSET / 2 + column * (tileExtent + TILE_OFFSET);
				Int yOrigin = TILE_OFFSET / 2 + row * (tileExtent + TILE_OFFSET);
				m_textureClasses[texClass].positionInTexture.x = xOrigin;
				m_textureClasses[texClass].positionInTexture.y = yOrigin;
				Int classHeight = yOrigin + width * tileExtent + TILE_OFFSET / 2;
				if (maxHeight < classHeight) maxHeight = classHeight;

				for (i = 0; i < width; i++) {
					for (j = 0; j < width; j++) {
						availableGrid[(row + j) * tilesPerRow + (column + i)] = 0;
						Int baseNdx = m_textureClasses[texClass].firstTile + i + j * width;
						if (!getSourceTile(baseNdx)) continue;
						Int x = xOrigin + i * tileExtent;
						Int y = yOrigin + (width - j - 1) * tileExtent;
						m_sourceTiles[baseNdx]->m_tileLocationInTexture.x = x;
						m_sourceTiles[baseNdx]->m_tileLocationInTexture.y = y;
					}
				}
			}
		}

		m_tilePixelExtent = tileExtent;
		if (allPlaced)
			break;
	}

	for (TileData *tile : m_edgeTiles) {
		if (tile) {
			tile->m_tileLocationInTexture.x = 0;
			tile->m_tileLocationInTexture.y = 0;
		}
	}

	Int maxEdgeHeight = 0;
	m_edgeTilePixelExtent = RetailFileLayers::enabled() ? TILE_PIXEL_EXTENT_MIP2 : TILE_PIXEL_EXTENT;
	const Int edgeStride = m_edgeTilePixelExtent + TILE_OFFSET;
	const Int edgeTilesPerRow = TEXTURE_WIDTH / edgeStride;
	std::vector<UnsignedByte> edgeGrid(edgeTilesPerRow * edgeTilesPerRow, 1);
	for (Int texClass = 0; texClass < m_numEdgeTextureClasses; texClass++) {
		Int width = m_edgeTextureClasses[texClass].width;
		Bool found = false;
		for (row = 0; row < edgeTilesPerRow - width + 1 && !found; row++) {
			for (column = 0; column < edgeTilesPerRow - width + 1 && !found; column++) {
				if (edgeGrid[row * edgeTilesPerRow + column]) {
					Bool open = true;
					for (i = 0; i < width && open; i++) {
						for (j = 0; j < width && open; j++) {
							if (!edgeGrid[(row + j) * edgeTilesPerRow + (column + i)]) {
								open = false;
							}
						}
					}
					if (open) found = true;
					break;
				}
			}
			if (found) break;
		}
		if (!found) {
			m_edgeTextureClasses[texClass].positionInTexture.x = 0;
			m_edgeTextureClasses[texClass].positionInTexture.y = 0;
			continue;
		}

		Int xOrigin = TILE_OFFSET / 2 + column * edgeStride;
		Int yOrigin = TILE_OFFSET / 2 + row * edgeStride;
		m_edgeTextureClasses[texClass].positionInTexture.x = xOrigin;
		m_edgeTextureClasses[texClass].positionInTexture.y = yOrigin;
		Int classHeight = yOrigin + width * m_edgeTilePixelExtent + TILE_OFFSET / 2;
		if (maxEdgeHeight < classHeight) maxEdgeHeight = classHeight;

		for (i = 0; i < width; i++) {
			for (j = 0; j < width; j++) {
				edgeGrid[(row + j) * edgeTilesPerRow + (column + i)] = 0;
				Int baseNdx = m_edgeTextureClasses[texClass].firstTile + i + j * width;
				TileData *tile = getEdgeTile(baseNdx);
				if (!tile) continue;
				tile->m_tileLocationInTexture.x = xOrigin + i * m_edgeTilePixelExtent;
				tile->m_tileLocationInTexture.y = yOrigin + (width - j - 1) * m_edgeTilePixelExtent;
			}
		}
	}
	if (edgeHeight) *edgeHeight = maxEdgeHeight;
	return maxHeight;
}

/** getUVData - Gets the texture coordinates to use.  See getTerrainTexture.
*/
void WorldHeightMap::getUVForNdx(Int tileNdx, float *minU, float *minV, float *maxU, float*maxV)
{
	const TileData *tile = getSourceTile(tileNdx >> 2);
	if (!tile) {
		// Missing texture.
		*minU = *minV = *maxU = *maxV = 0.0f;
		return;
	}
	ICoord2D pos = tile->m_tileLocationInTexture;
	*minU = pos.x;
	*minV = pos.y;
	*maxU = *minU + m_tilePixelExtent;
	*maxV = *minV + m_tilePixelExtent;
#ifdef EVAL_TILING_MODES
	if (m_tileMode == TILE_8x8) {
		*maxU = *minU+TILE_PIXEL_EXTENT/2.0f;
		*maxV = *minV+TILE_PIXEL_EXTENT/2.0f;
	} else if (m_tileMode == TILE_6x6) {
		*maxU = *minU+2.0f*TILE_PIXEL_EXTENT/3.0f;
		*maxV = *minV+2.0f*TILE_PIXEL_EXTENT/3.0f;
	} else {
		*maxU = *minU+TILE_PIXEL_EXTENT;
		*maxV = *minV+TILE_PIXEL_EXTENT;
	}
#endif
	*minU/=TEXTURE_WIDTH;
	*minV/=m_terrainTexHeight;
	*maxU/=TEXTURE_WIDTH;
	*maxV/=m_terrainTexHeight;

	// Tiles are 64x64 pixels, height grids map to 32x32.
	// So get the proper quadrant of the tile.
	Real midX = (*minU+*maxU)/2;
	Real midY = (*minV+*maxV)/2;
	if (tileNdx&2) {
		// y's are flipped.
		*maxV = midY;
	} else {
		*minV = midY;
	}
	if (tileNdx&1) {
		*minU = midX;
	} else {
		*maxU = midX;
	}
}

/** getUVData - Gets the texture coordinates to use.  See getTerrainTexture.
*/
void WorldHeightMap::getUVForBlend(Int edgeClass, Region2D *range)
{
	ICoord2D pos = m_edgeTextureClasses[edgeClass].positionInTexture;
	Int width = m_edgeTextureClasses[edgeClass].width;
	range->lo.x = (Real)pos.x/TEXTURE_WIDTH;
	range->lo.y = (Real)pos.y/m_alphaEdgeHeight;
	range->hi.x = ((Real)pos.x + width*m_edgeTilePixelExtent)/TEXTURE_WIDTH;
	range->hi.y = ((Real)pos.y + width*m_edgeTilePixelExtent)/m_alphaEdgeHeight;
}

/// Get whether something is cliff indexed with the offset that HeightMapRenderObjClass uses built in.
Bool WorldHeightMap::isCliffMappedTexture(Int x, Int y) {
	Int ndx = x+m_drawOriginX+m_width*(y+m_drawOriginY);
	if (ndx>=0 && ndx<m_dataSize) {
		return m_cliffInfoNdxes[ndx] != 0;
	}
	return false;
};

/** getUVData - Gets the texture coordinates to use.  See getTerrainTexture.
		xIndex and yIndex are the integer coordinates into the height map.
		U and V are the texture coordinates for the 4 corners of a height map cell.
*/
Bool WorldHeightMap::getUVData(Int xIndex, Int yIndex, float U[4], float V[4])
{
#define dont_SHOW_THE_TEXTURE_FOR_DEBUG 1
#if SHOW_THE_TEXTURE_FOR_DEBUG
		// This is debug code that just shows the generated texture laid on the terrain.
		// For debugging ;) jba.
		xIndex += m_drawOriginX;
		yIndex += m_drawOriginY;
		float nU= xIndex;
		float xU = xIndex+1;
		float nV = 48-yIndex-1;
		float xV = 48-yIndex;
		float k = 48;
		nU /= k;
		xU /= k;
		k = k*m_terrainTexHeight/TEXTURE_WIDTH;
		nV /= k;
		xV /= k;
		U[0] = nU; U[1] = xU; U[2] = xU; U[3] = nU;
		V[0] = xV; V[1] = xV; V[2] = nV; V[3] = nV;
		return(true);
#else
	xIndex += m_drawOriginX;
	yIndex += m_drawOriginY;
	Int ndx = (yIndex*m_width)+xIndex;
	if ((ndx<m_dataSize) && m_tileNdxes) {
		Short tileNdx = m_tileNdxes[ndx];
		return getUVForTileIndex(ndx, tileNdx, U, V);
	}
	return false;
#endif
}

/** getUVForTileIndex - Gets the texture coordinates to use.  See getTerrainTexture.
		ndx is the index into the linear height array.
		tileNdx is the index into the texture tiles array.
		U and V are the texture coordinates for the 4 corners of a height map cell.
*/

Bool WorldHeightMap::getUVForTileIndex(Int ndx, Short tileNdx, float U[4], float V[4])
{
	Real nU, nV, xU, xV;
	nU=nV=xU=xV = 0.0f;
	Int tilesPerRow = TEXTURE_WIDTH/(2*TILE_PIXEL_EXTENT+TILE_OFFSET);
	tilesPerRow *= 4;

	if ((ndx<m_dataSize) && m_tileNdxes) {
		getUVForNdx(tileNdx, &nU, &nV, &xU, &xV);
		U[0] = nU; U[1] = xU; U[2] = xU; U[3] = nU;
		V[0] = xV; V[1] = xV; V[2] = nV; V[3] = nV;
		if (TheGlobalData && !TheGlobalData->m_adjustCliffTextures) {
			return false;
		}
		if (nU==0.0) {
			return false; // missing texture.
		}
		if (m_cliffInfoNdxes[ndx]) {
			TCliffInfo info = m_cliffInfo[m_cliffInfoNdxes[ndx]];
			Bool tilesMatch = false;
			Int ndx1 = tileNdx>>2;
			Int ndx2 = info.tileIndex>>2;
			Int i;
			for (i=0; i<this->m_numTextureClasses; i++) {
				if (ndx1 >= m_textureClasses[i].firstTile && ndx1 < m_textureClasses[i].firstTile + m_textureClasses[i].numTiles) {
					tilesMatch = ndx2 >= m_textureClasses[i].firstTile && ndx2 < m_textureClasses[i].firstTile + m_textureClasses[i].numTiles;
					//tilesMatch = true;
					break;
				}
			}
			if (tilesMatch) {
				Real minU = m_textureClasses[i].positionInTexture.x;
				Real maxV = m_textureClasses[i].positionInTexture.y + m_textureClasses[i].width * m_tilePixelExtent;
				minU /= TEXTURE_WIDTH;
				maxV /= m_terrainTexHeight;
				Real vFactor = TEXTURE_WIDTH / m_terrainTexHeight;
				Real uvScale = (Real)m_tilePixelExtent / (Real)TILE_PIXEL_EXTENT;
				U[0] = info.u0 * uvScale + minU;
				U[1] = info.u1 * uvScale + minU;
				U[2] = info.u2 * uvScale + minU;
				U[3] = info.u3 * uvScale + minU;
				V[0] = info.v0 * uvScale * vFactor + maxV;
				V[1] = info.v1 * uvScale * vFactor + maxV;
				V[2] = info.v2 * uvScale * vFactor + maxV;
				V[3] = info.v3 * uvScale * vFactor + maxV;
				return info.flip;
			}
		}

// TheSuperHackers @info xezon 11/12/2025 The old uv adjustment for cliffs produces bad uv tiles on steep terrain
// and is also not helping performance. But we cannot just remove it, because it is required to render smooth
// steep diagonal slopes.
#define DO_OLD_UV
#ifdef DO_OLD_UV
// old uv adjustment for cliffs
		static Real STRETCH_LIMIT = 1.5f;	 // If it is stretching less than this, don't adjust.
		static Real TILE_LIMIT = 4.0;			// Our tiles are currently 4 cells wide & tall, so dont'
																			// adjust to more than 4.0.

		static Real TALL_STRETCH_LIMIT = 2.0f;
		static Real DIAMOND_STRETCH_LIMIT = 2.4f;
		const Real HEIGHT_SCALE = getHeightScale() / MAP_XY_FACTOR;

		Real nU, nV, xU, xV;
		nU=nV=xU=xV = 0.0f;
		Int tilesPerRow = TEXTURE_WIDTH/(2*TILE_PIXEL_EXTENT+TILE_OFFSET);
		tilesPerRow *= 4;


		getUVForNdx(tileNdx, &nU, &nV, &xU, &xV);
		U[0] = nU; U[1] = xU; U[2] = xU; U[3] = nU;
		V[0] = xV; V[1] = xV; V[2] = nV; V[3] = nV;
		if (TheGlobalData && !TheGlobalData->m_adjustCliffTextures) {
			return false;
		}
		if (nU==0.0) {
			return false; // missing texture.
		}
		// check for excessive heights.
		if (ndx < this->m_dataSize - m_width - 1) {
			Int h0 = m_data[ndx];
			Int h1 = m_data[ndx+1];
			Int h2 = m_data[ndx+m_width+1];
			Int h3 = m_data[ndx+m_width];
			Int minH, maxH;
			minH = maxH = h0;
			if (minH>h1) minH = h1;
			if (maxH<h1) maxH = h1;
			if (minH>h2) minH = h2;
			if (maxH<h2) maxH = h2;
			if (minH>h3) minH = h3;
			if (maxH<h3) maxH = h3;
//			Int avgH = (h1+h2+h3+h0+2)/4;
			Int deltaH = maxH-minH;
			Int below = 0;
			Int above = 0;
			Int belowLimit = minH+(2*deltaH+1)/3;
			Int aboveLimit = minH+(deltaH+1)/3;
			if (h0<belowLimit) below++;
			if (h1<belowLimit) below++;
			if (h2<belowLimit) below++;
			if (h3<belowLimit) below++;
			if (h0>aboveLimit) above++;
			if (h1>aboveLimit) above++;
			if (h2>aboveLimit) above++;
			if (h3>aboveLimit) above++;
			if (deltaH*HEIGHT_SCALE < STRETCH_LIMIT) {
				return false;
			}

			Short baseNdx = tileNdx>>2;
			Short texClass;
			for (texClass=0; texClass<m_numTextureClasses; texClass++) {
				if (m_textureClasses[texClass].firstTile<0) {
					continue;
				}
				// see if the blend tile is in a texture class, and get the right tile for xIndex, yIndex.
				if (baseNdx >= m_textureClasses[texClass].firstTile &&
					baseNdx < m_textureClasses[texClass].firstTile+m_textureClasses[texClass].numTiles) {
					break;
				}
			}
			if (texClass>= m_numTextureClasses) return false;
			Real nUb, nVb, xUb, xVb;
			nUb = m_textureClasses[texClass].positionInTexture.x;
			nVb = m_textureClasses[texClass].positionInTexture.y;
			xUb = nUb + m_textureClasses[texClass].width * m_tilePixelExtent;
			xVb = nVb + m_textureClasses[texClass].width * m_tilePixelExtent;
			nUb/=TEXTURE_WIDTH;
			nVb/=m_terrainTexHeight;
			xUb/=TEXTURE_WIDTH;
			xVb/=m_terrainTexHeight;
			// Now covers texture bounds.
			// too much stretch.
			Real divisor = TILE_LIMIT/(deltaH*HEIGHT_SCALE);
			if (divisor > TILE_LIMIT) divisor = TILE_LIMIT;
			if (divisor < 1.0f) divisor = 1.0f;
			Real deltaV = (xVb-nVb);
//			Real deltaU = (xUb-nUb);

			if (above != 1 && below != 1 && (above!=2 || below != 2)) {
				// diamond shaped.  Use default if it is not too stretched, as
				// the fix is not that appealing either.
				if (deltaH*HEIGHT_SCALE < DIAMOND_STRETCH_LIMIT) {
					return false;
				}
			}

			if (below==1 || above>below) {
				//(avgH > minH + (2*deltaH+2)/3)
				// we got one low guy.
				if (h0==minH) {
					V[0] = nV+deltaV/divisor;
				} else if (h1 == minH) {
					V[1] = nV+deltaV/divisor;
				}	else if (h2 == minH) {
					V[2] = xV-deltaV/divisor;
				} else if (h3 == minH) {
					V[3] = xV-deltaV/divisor;
				}
#if 0
				nU = nV = xU = xV = 1.0f;
				U[0] = nU; U[1] = xU; U[2] = xU; U[3] = nU;
				V[0] = xV; V[1] = xV; V[2] = nV; V[3] = nV;
				return false;
#endif
			}	else if (above==1 || below>above) {
				//(avgH < minH + (deltaH+1)/3)
				// we got one high guy
				if (h0==maxH) {
					V[0] = nV+deltaV/divisor;
				} else if (h1 == maxH) {
					V[1] = nV+deltaV/divisor;
				}	else if (h2 == maxH) {
					V[2] = xV-deltaV/divisor;
				} else if (h3 == maxH) {
					V[3] = xV-deltaV/divisor;
				}
#if 0
				nU = nV = xU = xV = 0.0f;
				U[0] = nU; U[1] = xU; U[2] = xU; U[3] = nU;
				V[0] = xV; V[1] = xV; V[2] = nV; V[3] = nV;
				return false;
#endif
			} else {
				// we got two up and two down.
				if (deltaH*HEIGHT_SCALE < TALL_STRETCH_LIMIT) {
					return false;
				}
#if 0
				nU = nV = xU = xV = 0.0f;
				U[0] = nU; U[1] = xU; U[2] = xU; U[3] = nU;
				V[0] = xV; V[1] = xV; V[2] = nV; V[3] = nV;
				return;
#endif
				Real dx = (h3-h2)*HEIGHT_SCALE;
				dx = sqrt(1+dx*dx); // length of the bottom of the cell
				Real dy =	(h3-h0)*HEIGHT_SCALE;
				dy = sqrt(1+dy*dy); // length of the left side.
				if (dx<STRETCH_LIMIT) dx = 1.0f; // don't make a seam unless there is great stretch.
				if (dy<STRETCH_LIMIT) dy = 1.0f; // don't make a seam unless there is great stretch.
				if (dx>TILE_LIMIT) dx = TILE_LIMIT; // don't tile past the texture's edge.
				if (dy>TILE_LIMIT) dy = TILE_LIMIT; // don't tile past the texture's edge.
				dx *= xU-nU;
				dy *= xV-nV;
				U[0] = nU; U[1] = nU+dx; U[2] = nU+dx; U[3] = nU;
				V[0] = nV+dy; V[1] = nV+dy; V[2] = nV; V[3] = nV;
				if (below==1) {
					below = 1;
				}
				// recalc for point 1.
				dx = (h1-h0)*HEIGHT_SCALE;
				dx = sqrt(1+dx*dx); // length of the bottom of the cell
				dy =	(h2-h1)*HEIGHT_SCALE;
				dy = sqrt(1+dy*dy); // length of the left side.
				if (dx<STRETCH_LIMIT) dx = 1.0f; // don't make a seam unless there is great stretch.
				if (dy<STRETCH_LIMIT) dy = 1.0f; // don't make a seam unless there is great stretch.
				if (dx>TILE_LIMIT) dx = TILE_LIMIT; // don't tile past the texture's edge.
				if (dy>TILE_LIMIT) dy = TILE_LIMIT; // don't tile past the texture's edge.
				dx *= xU-nU;
				dy *= xV-nV;
				U[1] = U[0]+dx;
				V[1] = V[3] + dy;
			}
			// Make sure we are within the texture;
			Real adjU = 0;
			Real adjV = 0;
			Int i;
			for (i=0; i<4; i++) {
				if (nVb - V[i] > adjV) adjV = nVb - V[i];
			}
			for (i=0; i<4; i++) {
				V[i] += adjV;
			}
			adjV = 0;
			for (i=0; i<4; i++) {
				if (U[i] - xUb > adjU) adjU = U[i]-xUb;
				if (V[i] - xVb > adjV) adjV = V[i]-xVb;
			}
			for (i=0; i<4; i++) {
				U[i] -= adjU;
				V[i] -= adjV;
			}
		}
		return true;
//
#endif

	}
	return false;
}

///@todo: Are the different "if" cases mutually exclusive?  If so, should add else statements.
Bool WorldHeightMap::getExtraAlphaUVData(Int xIndex, Int yIndex, float U[4], float V[4], UnsignedByte alpha[4], Bool *needFlip, Bool *cliff)
{
	Int ndx = (yIndex*m_width)+xIndex;
	*needFlip = FALSE;
	*cliff = FALSE;

	if ( (ndx>=0) && (ndx<m_dataSize) && m_tileNdxes) {
		Int blendNdx = m_extraBlendTileNdxes[ndx];
		if (blendNdx == 0) {
			return FALSE;
		} else {
			*cliff = getUVForTileIndex(ndx, m_blendedTiles[blendNdx].blendNdx, U, V);
			alpha[0] = alpha[1] = alpha[2] = alpha[3] = 0;
			if (m_blendedTiles[blendNdx].horiz) {
				// Horizontals don't need flipping unless forced because of 3way blend
				// and a diagonal in base blend layer.
				*needFlip = m_blendedTiles[blendNdx].inverted & FLIPPED_MASK;
				if (m_blendedTiles[blendNdx].inverted & INVERTED_MASK) {
					alpha[0] = alpha[3] = 255;
				} else {
					alpha[1] = alpha[2] = 255;
				}
			}
			if (m_blendedTiles[blendNdx].vert) {
				// Verticals don't need flipping unless forced because of 3way blend
				// and a diagonal in base blend layer.
				*needFlip = m_blendedTiles[blendNdx].inverted & FLIPPED_MASK;
				if (m_blendedTiles[blendNdx].inverted & INVERTED_MASK) {
					alpha[0] = alpha[1] = 255;
				} else {
					alpha[2] = alpha[3]  = 255;
				}
			}
			if (m_blendedTiles[blendNdx].rightDiagonal) {
				if (m_blendedTiles[blendNdx].inverted & INVERTED_MASK) {
					alpha[1] = 255;
					if (m_blendedTiles[blendNdx].longDiagonal) {
						alpha[0] = 255;
						alpha[2] = 255;
					}
				} else {
					// Uninverted right diagonals need flipping.
					*needFlip = TRUE;
					alpha[2] = 255;
					if (m_blendedTiles[blendNdx].longDiagonal) {
						alpha[1] = 255;
						alpha[3] = 255;
					}
				}
			}
			if (m_blendedTiles[blendNdx].leftDiagonal) {
				if (m_blendedTiles[blendNdx].inverted & INVERTED_MASK) {
					// Inverted left diagonals need flipping.
					*needFlip = TRUE;
					alpha[0] = 255;
					if (m_blendedTiles[blendNdx].longDiagonal) {
						alpha[1] = 255;
						alpha[3] = 255;
					}
				} else {
					alpha[3] = 255;
					if (m_blendedTiles[blendNdx].longDiagonal) {
						alpha[0] = 255;
						alpha[2] = 255;
					}
				}
			}
			if (m_blendedTiles[blendNdx].customBlendEdgeClass>=0) {
				alpha[0] = alpha[1] = alpha[2] = alpha[3] = 0;
				// No alpha blend, so never need to flip.
				*needFlip = FALSE;
			}
		}
	}

	return TRUE;
}

/** getAlphaUVData - Gets the texture coordinates to use with the alpha texture.
		xIndex and yIndex are the integer coordinates into the height map.
		U and V are the texture coordinates for the 4 corners of a height map cell.
		flip is set if we need to flip the diagonal across the cell to make the
		alpha coordinates blend properly.  Filling a square with 2 triangles is not symmetrical :)
*/
void WorldHeightMap::getAlphaUVData(Int xIndex, Int yIndex, float U[4], float V[4], UnsignedByte alpha[4], Bool *flip)
{
	xIndex += m_drawOriginX;
	yIndex += m_drawOriginY;
	Int ndx = (yIndex*m_width)+xIndex;
	Bool stretchedForCliff = false;
	Bool needFlip = false;

	if ((ndx<m_dataSize) && m_tileNdxes) {
		Int blendNdx = m_blendTileNdxes[ndx];
		if (blendNdx == 0) {
			stretchedForCliff = getUVForTileIndex(ndx, m_tileNdxes[ndx], U, V);
			alpha[0] = alpha[1] = alpha[2] = alpha[3] = 0;
			// No alpha blend, so never need to flip.
			needFlip = false;
		} else {
			stretchedForCliff = getUVForTileIndex(ndx, m_blendedTiles[blendNdx].blendNdx, U, V);
			alpha[0] = alpha[1] = alpha[2] = alpha[3] = 0;
			if (m_blendedTiles[blendNdx].horiz) {
				// Horizontals don't need flipping unless forced because of 3way blend.
				needFlip = m_blendedTiles[blendNdx].inverted & FLIPPED_MASK;
				if (m_blendedTiles[blendNdx].inverted & INVERTED_MASK) {
					alpha[0] = alpha[3] = 255;
				} else {
					alpha[1] = alpha[2] = 255;
				}
			}
			if (m_blendedTiles[blendNdx].vert) {
				// Verticals don't need flipping unless forced because of 3way blend.
				needFlip = m_blendedTiles[blendNdx].inverted & FLIPPED_MASK;
				if (m_blendedTiles[blendNdx].inverted & INVERTED_MASK) {
					alpha[0] = alpha[1] = 255;
				} else {
					alpha[2] = alpha[3]  = 255;
				}
			}
			if (m_blendedTiles[blendNdx].rightDiagonal) {
				if (m_blendedTiles[blendNdx].inverted & INVERTED_MASK) {
					alpha[1] = 255;
					if (m_blendedTiles[blendNdx].longDiagonal) {
						alpha[0] = 255;
						alpha[2] = 255;
					}
				} else {
					// Uninverted right diagonals need flipping.
					needFlip = true;
					alpha[2] = 255;
					if (m_blendedTiles[blendNdx].longDiagonal) {
						alpha[1] = 255;
						alpha[3] = 255;
					}
				}
			}
			if (m_blendedTiles[blendNdx].leftDiagonal) {
				if (m_blendedTiles[blendNdx].inverted & INVERTED_MASK) {
					// Inverted left diagonals need flipping.
					needFlip = true;
					alpha[0] = 255;
					if (m_blendedTiles[blendNdx].longDiagonal) {
						alpha[1] = 255;
						alpha[3] = 255;
					}
				} else {
					alpha[3] = 255;
					if (m_blendedTiles[blendNdx].longDiagonal) {
						alpha[0] = 255;
						alpha[2] = 255;
					}
				}
			}
			if (m_blendedTiles[blendNdx].customBlendEdgeClass>=0) {
				alpha[0] = alpha[1] = alpha[2] = alpha[3] = 0;
				// No alpha blend, so never need to flip.
				needFlip = false;
			}
		}
	}
	if (stretchedForCliff) {
		// If we had to stretch for clif, check heights.
		Int p0=getHeight(xIndex, yIndex);
		Int p1=getHeight(xIndex+1, yIndex);
		Int p2=getHeight(xIndex+1, yIndex+1);
		Int p3=getHeight(xIndex, yIndex+1);
		Int dz1 = abs(p0-p2);
		Int dz2 = abs(p1-p3);
		needFlip = dz1>dz2;
	}
#ifdef FLIP_TRIANGLES
	*flip = needFlip;
#endif
}

void WorldHeightMap::setTextureLOD(Int lod)
{
	if (m_terrainTex)
		m_terrainTex->setLOD(lod);
}

TextureClass *WorldHeightMap::getTerrainTexture()
{
	if (m_terrainTex == nullptr) {
		Int edgeHeight;
		Int height = updateTileTexturePositions(&edgeHeight);
		Int pow2Height = 1;
		while (pow2Height<height) {
			pow2Height *=2;
		}
		REF_PTR_RELEASE(m_terrainTex);
		m_terrainTex = MSGNEW("WorldHeightMap_getTerrainTexture") TerrainTextureClass(pow2Height);
		m_terrainTexHeight = m_terrainTex->update(this);
		char buf[64];
		sprintf(buf, "Base tex height %d", pow2Height);
		DEBUG_LOG((buf));
		REF_PTR_RELEASE(m_alphaTerrainTex);
		m_alphaTerrainTex = MSGNEW("WorldHeightMap_getTerrainTexture") AlphaTerrainTextureClass(m_terrainTex);

		pow2Height = 1;
		while (pow2Height<edgeHeight) {
			pow2Height *=2;
		}
		REF_PTR_RELEASE(m_alphaEdgeTex);
		m_alphaEdgeTex = MSGNEW("WorldHeightMap_getTerrainTexture") AlphaEdgeTextureClass(pow2Height);
		m_alphaEdgeHeight = m_alphaEdgeTex->update(this);

		//Generate lookup table for determining triangle order in each terrain cell.
		//Not the best place to put this but getAlphaUVData() requires a valid terrain
		//texture to return valid values.

		for (Int y=0; y<(m_height-1); y++)
			for (Int x=0; x<(m_width-1); x++)
			{
				UnsignedByte alpha[4];
				float UA[4], VA[4];
				Bool flipForBlend;

				getAlphaUVData(x, y, UA, VA, alpha, &flipForBlend);

				m_cellFlipState[y*m_flipStateWidth+(x>>3)] |= flipForBlend << (x & 0x7);
				DEBUG_ASSERTCRASH ((y*m_flipStateWidth+(x>>3)) < (m_flipStateWidth * m_height), ("Bad range"));
			}
	}

	return m_terrainTex;
}

TextureClass *WorldHeightMap::getAlphaTerrainTexture()
{
	if (m_alphaTerrainTex == nullptr) {
		getTerrainTexture();
	}
	return m_alphaTerrainTex;
}

TextureClass *WorldHeightMap::getEdgeTerrainTexture()
{
	if (m_alphaEdgeTex == nullptr) {
		getTerrainTexture();
	}
	return m_alphaEdgeTex;
}

TerrainTextureClass *WorldHeightMap::getFlatTexture(Int xCell, Int yCell, Int cellWidth, Int pixelsPerCell)
{
	if (WW3D::Get_Texture_Reduction()) {
		if (WW3D::Get_Texture_Reduction()>1) {
			pixelsPerCell /= 4;
		} else {
			pixelsPerCell /= 2;
		}
	}
	Int pow2Height = 1;
	while (pow2Height<cellWidth*pixelsPerCell) {
		pow2Height *=2;
	}
	TerrainTextureClass *newTexture = MSGNEW("WorldHeightMap_getTerrainTexture") TerrainTextureClass(pow2Height, pow2Height);
	newTexture->updateFlat(this, xCell, yCell, cellWidth, pixelsPerCell);
	return newTexture;
}

Region2D WorldHeightMap::getDrawRegion2D()
{
	// Get region in heightmap space
	const Int loX = getDrawOrgX() - getBorderSizeInline();
	const Int loY = getDrawOrgY() - getBorderSizeInline();
	const Int hiX = loX + getDrawWidth();
	const Int hiY = loY + getDrawHeight();

	// Convert to world space
	Region2D region;
	region.lo.x = loX * MAP_XY_FACTOR;
	region.lo.y = loY * MAP_XY_FACTOR;
	region.hi.x = hiX * MAP_XY_FACTOR;
	region.hi.y = hiY * MAP_XY_FACTOR;

	return region;
}

WorldHeightMap::DrawArea WorldHeightMap::createDrawArea(Int xOrg, Int yOrg)
{
	DrawArea area;
	area.sizeX = std::min(m_drawWidthX, m_width);
	area.sizeY = std::min(m_drawHeightY, m_height);
	area.originX = clamp(0, xOrg, m_width - area.sizeX);
	area.originY = clamp(0, yOrg, m_height - area.sizeY);

	return area;
}

Bool WorldHeightMap::setDrawArea(const DrawArea& area)
{
	Bool anythingDifferent =
		m_drawOriginX != area.originX ||
		m_drawOriginY != area.originY ||
		m_drawWidthX != area.sizeX ||
		m_drawHeightY != area.sizeY;

	if (anythingDifferent) {
		m_drawOriginX = area.originX;
		m_drawOriginY = area.originY;
		m_drawWidthX = area.sizeX;
		m_drawHeightY = area.sizeY;
		return true;
	}
	return false;
}

Bool WorldHeightMap::setDrawOrg(Int xOrg, Int yOrg)
{
	return setDrawArea(createDrawArea(xOrg, yOrg));
}


/** Gets global texture class. */
Int WorldHeightMap::getTextureClass(Int xIndex, Int yIndex, Bool baseClass)
{
	Int ndx = (yIndex*m_width)+xIndex;
	DEBUG_ASSERTCRASH((ndx>=0 && ndx<this->m_dataSize),("oops"));
	if (ndx<0 || ndx >= this->m_dataSize) return(-1);
	Int textureNdx = m_tileNdxes[ndx];
	if (!baseClass && (m_blendTileNdxes[ndx] != 0 || m_extraBlendTileNdxes[ndx] != 0)) {
		return(-1);  // blended, so not of the original class.
	}
	return getTextureClassFromNdx(textureNdx);
}


/** Sets all the cliff flags in map based on height. */
void WorldHeightMap::initCliffFlagsFromHeights()
{
	Int xIndex, yIndex;

	for (xIndex=0; xIndex<m_width-1; xIndex++) {
		for (yIndex=0; yIndex<m_height-1; yIndex++) {
			setCellCliffFlagFromHeights(xIndex, yIndex);
		}
	}
}

/** Sets the cliff flag for a cell based on height. */
void WorldHeightMap::setCellCliffFlagFromHeights(Int xIndex, Int yIndex)
{
	Real height1 = toWorldHeight(getHeight(xIndex, yIndex));
	Real height2 = toWorldHeight(getHeight(xIndex+1, yIndex));
	Real height3 = toWorldHeight(getHeight(xIndex, yIndex+1));
	Real height4 = toWorldHeight(getHeight(xIndex+1, yIndex+1));
	Real minZ = height1;
	if (minZ > height2) minZ = height2;
	if (minZ > height3) minZ = height3;
	if (minZ > height4) minZ = height4;
	Real maxZ = height1;
	if (maxZ < height2) maxZ = height2;
	if (maxZ < height3) maxZ = height3;
	if (maxZ < height4) maxZ = height4;
	const Real cliffRange = PATHFIND_CLIFF_SLOPE_LIMIT_F;
	Bool isCliff = (maxZ-minZ > cliffRange);
	setCliffState(xIndex, yIndex, isCliff);

}

/** Gets global texture class. */
Int WorldHeightMap::getTextureClassFromNdx(Int tileNdx)
{
	Int i;
	tileNdx = tileNdx>>2;
	for (i=0; i<m_numTextureClasses; i++) {
		if (m_textureClasses[i].firstTile<0) {
			continue;
		}
		// see if the blend tile is in a texture class, and get the right tile for xIndex, yIndex.
		if (tileNdx >= m_textureClasses[i].firstTile &&
			tileNdx < m_textureClasses[i].firstTile+m_textureClasses[i].numTiles) {
			return(m_textureClasses[i].globalTextureClass);
		}
	}
	return(-1);
}

TXTextureClass WorldHeightMap::getTextureFromIndex( Int textureIndex )
{
	return m_textureClasses[textureIndex];
}

void WorldHeightMap::getTerrainColorAt(Real x, Real y, RGBColor *pColor)
{
	Int xIndex = REAL_TO_INT_FLOOR(x/MAP_XY_FACTOR);
	Int yIndex = REAL_TO_INT_FLOOR(y/MAP_XY_FACTOR);
	xIndex += m_borderSize;
	yIndex += m_borderSize;
	pColor->red = pColor->green = pColor->blue = 0;
	if (xIndex<0) xIndex = 0;
	if (yIndex<0) yIndex = 0;
	if (xIndex >= m_width) xIndex = m_width-1;
	if (yIndex >= m_height) yIndex = m_height-1;
	Int ndx = (yIndex*m_width)+xIndex;
	if (ndx<0 || ndx >= this->m_dataSize) return;
	Int tileNdx = m_tileNdxes[ndx];
	tileNdx = tileNdx>>2;	 // We pack 4 grids into a tile.

	TileData *pTile = getSourceTile(tileNdx);
	if (pTile) {
		// pTile contains the bitmap data for 4 squares.
		// Get the data mipped down to one pixel for the tile.
		UnsignedByte *pData = pTile->getRGBDataForWidth(1);
		// Data is in microsoft bgra format.
		pColor->red = pData[2]/255.0;
		pColor->green = pData[1]/255.0;
		pColor->blue = pData[0]/255.0;
	}
}

AsciiString WorldHeightMap::getTerrainNameAt(Real x, Real y)
{
	Int xIndex = REAL_TO_INT_FLOOR(x/MAP_XY_FACTOR);
	Int yIndex = REAL_TO_INT_FLOOR(y/MAP_XY_FACTOR);
	xIndex += m_borderSize;
	yIndex += m_borderSize;
	if (xIndex<0) xIndex = 0;
	if (yIndex<0) yIndex = 0;
	if (xIndex >= m_width) xIndex = m_width-1;
	if (yIndex >= m_height) yIndex = m_height-1;
	Int ndx = (yIndex*m_width)+xIndex;
	if (ndx<0 || ndx >= this->m_dataSize) return AsciiString::TheEmptyString;
	Int tileNdx = m_tileNdxes[ndx];
	tileNdx = tileNdx>>2;	 // We pack 4 grids into a tile.

	Int i;
	for (i=0; i<this->m_numTextureClasses; i++) {
		if (tileNdx >= m_textureClasses[i].firstTile && tileNdx < m_textureClasses[i].firstTile + m_textureClasses[i].numTiles) {
			return(m_textureClasses[i].name);
		}
	}
	return AsciiString::TheEmptyString;
}


static UnsignedByte s_buffer[DATA_LEN_BYTES];
static UnsignedByte s_blendBuffer[DATA_LEN_BYTES];

UnsignedByte * WorldHeightMap::getPointerToTileData(Int xIndex, Int yIndex, Int width)
{
	Int ndx = (yIndex*m_width)+xIndex;
	if (yIndex<0 || xIndex<0 || xIndex>=m_width || yIndex>=m_height) {
		return nullptr;
	}
	if (ndx<0 || ndx>=m_dataSize) {
		return nullptr;
	}
	TBlendTileInfo *pBlend = nullptr;
	Short tileNdx = m_tileNdxes[ndx];
	if (getRawTileData(tileNdx, width, s_buffer, DATA_LEN_BYTES)) {
		Int blendTileNdx = m_blendTileNdxes[ndx];
		if (blendTileNdx>0 && blendTileNdx < m_numBlendedTiles) {
			pBlend = &m_blendedTiles[blendTileNdx];
			if (getRawTileData(pBlend->blendNdx, width, s_blendBuffer, DATA_LEN_BYTES)) {
				UnsignedByte *pAlpha = getRGBAlphaDataForWidth(width, pBlend);
				pAlpha += 3;  // skip over the rgb to the a.
				Int i, limit;
				limit = width*width;
				UnsignedByte *pBlendData = s_blendBuffer;
				UnsignedByte *pDestData = s_buffer;
				for (i=0; i<limit; i++) {
					Int r,g,b,a;
					b = *pBlendData++;
					g = *pBlendData++;
					r = *pBlendData++; pBlendData++;
					a = *pAlpha; pAlpha += 4;
					*pDestData++ = ((b*a)/255) + (((*pDestData)*(255-a))/255);
					*pDestData++ = ((g*a)/255) + (((*pDestData)*(255-a))/255);
					*pDestData++ = ((r*a)/255) + (((*pDestData)*(255-a))/255);
					*pDestData++ = 255; // just skip alpha.  not really used. jba.
				}
			}
		}
		return(s_buffer);
	}

	return(nullptr);
}

#define K_HORIZ 0
#define K_VERT 1
#define K_LDIAG 2
#define K_RDIAG 3
#define K_LLDIAG 4
#define K_LRDIAG 5
#define K_DIR_MOD 0x05
#define K_INV 6

UnsignedByte *WorldHeightMap::getRGBAlphaDataForWidth(Int width, TBlendTileInfo *pBlend)
{
	Int alphaTileNdx = 0;
	if (pBlend->horiz) {
		alphaTileNdx = K_HORIZ;
	} else if (pBlend->vert) {
		alphaTileNdx = K_VERT;
	} else if (pBlend->rightDiagonal) {
		alphaTileNdx = K_RDIAG;
		if (pBlend->longDiagonal) alphaTileNdx=K_LRDIAG;
	} else if (pBlend->leftDiagonal) {
		alphaTileNdx = K_LDIAG;
		if (pBlend->longDiagonal) alphaTileNdx=K_LLDIAG;
	}
	if (pBlend->inverted) {
		alphaTileNdx += K_INV;
	}
	return m_alphaTiles[alphaTileNdx]->getRGBDataForWidth(width);
}

void WorldHeightMap::setupAlphaTiles()
{
	TBlendTileInfo blendInfo;
	if (m_alphaTiles[0] != nullptr) return;
	Int k;
	for (k=0; k<NUM_ALPHA_TILES; k++) {
		memset(&blendInfo, 0, sizeof(blendInfo));
		Int baseK = k;
		if (k>=K_INV) {
			blendInfo.inverted = true;
			baseK -= K_INV;
		}
		switch(baseK) {
			case K_HORIZ : blendInfo.horiz = true; break;
			case K_VERT : blendInfo.vert = true; break;
			case K_LDIAG : blendInfo.leftDiagonal = true; break;
			case K_RDIAG : blendInfo.rightDiagonal = true; break;
			case K_LLDIAG : blendInfo.leftDiagonal = true; blendInfo.longDiagonal = true; break;
			case K_LRDIAG : blendInfo.rightDiagonal = true; blendInfo.longDiagonal = true; break;
		}
		m_alphaTiles[k] = new TileData;
		TileData *pTile = m_alphaTiles[k];

		Int i, j;
		UnsignedByte *pDest = pTile->getDataPtr();
		for (j=0; j<TILE_PIXEL_EXTENT; j++) {
			for (i=0; i<TILE_PIXEL_EXTENT; i++) {
				Int h = i;
				Int v = j;
				Int alpha = 255;  // 0 - 255.
				if (blendInfo.horiz) {
					if (!blendInfo.inverted) h = TILE_PIXEL_EXTENT-h-1;
					alpha = (alpha*h)/(TILE_PIXEL_EXTENT-1);
				} else if (blendInfo.vert) {
					if (!blendInfo.inverted) v = TILE_PIXEL_EXTENT-v-1;
					alpha = (alpha*v)/(TILE_PIXEL_EXTENT-1);
				} else if (blendInfo.rightDiagonal) {
					h = TILE_PIXEL_EXTENT-h-1;
					if (!blendInfo.inverted) v = TILE_PIXEL_EXTENT-v-1;
					v += h;				// angled
					if (blendInfo.longDiagonal) {
						v -= TILE_PIXEL_EXTENT;
					}
					alpha = (alpha*v)/(TILE_PIXEL_EXTENT-1);
				} else if (blendInfo.leftDiagonal) {
					if (!blendInfo.inverted) v = TILE_PIXEL_EXTENT-v-1;
					v += h;				// angled
					if (blendInfo.longDiagonal) {
						v -= TILE_PIXEL_EXTENT;
					}
					alpha = (alpha*v)/(TILE_PIXEL_EXTENT-1);
				}

				if (alpha > 255) alpha = 255;
				if (alpha<0) alpha = 0;
				alpha = 255-alpha;

				pDest += 3; // skip blue, green & red bytes.
				*pDest = alpha;		// alpha.
				//*pDest = 255;
				pDest++;
			}
		}
		pTile->updateMips();
	}
}


Bool  WorldHeightMap::getRawTileData(Short tileNdx, Int width,
																				 UnsignedByte *buffer, Int bufLen)
{
	TileData *pSrc = getSourceTile(tileNdx >> 2);
	if (bufLen < (width*width*TILE_BYTES_PER_PIXEL)) {
		return(false);
	}
	if (pSrc && pSrc->hasRGBDataForWidth(2*width)) {
		Int j;
		UnsignedByte *pSrcData = pSrc->getRGBDataForWidth(2*width);
		Int xOffset=0;
		Int yOffset=0;
		if (tileNdx & 1) xOffset = width;
		if (tileNdx & 2) yOffset = width;
		for (j=0; j<width; j++) {
			UnsignedByte *pDestData = buffer;
			pDestData += j*(width)*TILE_BYTES_PER_PIXEL;
			UnsignedByte *pSrc = pSrcData;
			pSrc += (j+yOffset)*width*TILE_BYTES_PER_PIXEL*2;
			pSrc += xOffset*TILE_BYTES_PER_PIXEL;
			memcpy(pDestData, pSrc, width*TILE_BYTES_PER_PIXEL);
		}
		return(true);
	}
	return(false);
}

