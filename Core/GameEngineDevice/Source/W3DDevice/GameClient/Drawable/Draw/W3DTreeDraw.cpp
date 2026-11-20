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

// FILE: W3DTreeDraw.cpp ////////////////////////////////////////////////////////////////////////
// Author: Colin Day, December 2001
// Desc:   Tree draw
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include <stdlib.h>
#include <cmath>

#include "Common/Dict.h"
#include "Common/NameKeyGenerator.h"
#include "Common/Thing.h"
#include "Common/INIAudit.h"
#include "Common/ModuleFactory.h"
#include "Common/RetailFileLayers.h"
#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"
#include "GameLogic/Object.h"
#include "GameClient/Drawable.h"
#include "GameClient/GameClient.h"
#include "W3DDevice/GameClient/Module/W3DTreeDraw.h"
#include "W3DDevice/GameClient/BaseHeightMap.h"


//-------------------------------------------------------------------------------------------------
W3DTreeDrawModuleData::W3DTreeDrawModuleData() :
m_framesToMoveInward(1),
m_framesToMoveOutward(1),
m_darkening(0.0f),
m_maxOutwardMovement(1.0f)
{

	// Topple parameters. [7/7/2003]
	const Real START_VELOCITY_PERCENT = 0.2f;
	const Real START_ACCEL_PERCENT = 0.01f;
	const Real VELOCITY_BOUNCE_PERCENT = 0.3f;			// multiply the velocity by this when you bounce
  const Real MINIMUM_TOPPLE_SPEED = 0.5f;         // Won't let trees fall slower than this
	m_toppleFX = nullptr;
	m_bounceFX = nullptr;
	m_stumpName.clear();
	m_killWhenToppled = true;
	m_doTopple = false;
	m_doShadow = false;
	m_initialVelocityPercent = START_VELOCITY_PERCENT;
	m_initialAccelPercent = START_ACCEL_PERCENT;
	m_bounceVelocityPercent = VELOCITY_BOUNCE_PERCENT;
  m_minimumToppleSpeed = MINIMUM_TOPPLE_SPEED;
	m_sinkFrames = 10*LOGICFRAMES_PER_SECOND;
	m_sinkDistance = 20.0f;

}

//-------------------------------------------------------------------------------------------------
W3DTreeDrawModuleData::~W3DTreeDrawModuleData()
{
}

//-------------------------------------------------------------------------------------------------
void W3DTreeDrawModuleData::buildFieldParse(MultiIniFieldParse& p)
{
  ModuleData::buildFieldParse(p);
	static const FieldParse dataFieldParse[] =
	{
		{ "ModelName", INI::parseAsciiString, nullptr, offsetof(W3DTreeDrawModuleData, m_modelName) },
		{ "TextureName", INI::parseAsciiString, nullptr, offsetof(W3DTreeDrawModuleData, m_textureName) },
		{ "MoveOutwardTime", INI::parseDurationUnsignedInt, nullptr, offsetof(W3DTreeDrawModuleData, m_framesToMoveOutward) },
		{ "MoveInwardTime", INI::parseDurationUnsignedInt, nullptr, offsetof(W3DTreeDrawModuleData, m_framesToMoveInward) },
		{ "MoveOutwardDistanceFactor", INI::parseReal, nullptr, offsetof(W3DTreeDrawModuleData, m_maxOutwardMovement) },
		{ "DarkeningFactor", INI::parseReal, nullptr, offsetof(W3DTreeDrawModuleData, m_darkening) },

// Topple parameters [7/7/2003]
		{ "ToppleFX",	INI::parseFXList, nullptr, offsetof( W3DTreeDrawModuleData, m_toppleFX ) },
		{ "BounceFX",	INI::parseFXList, nullptr, offsetof( W3DTreeDrawModuleData, m_bounceFX ) },
		{ "StumpName",	INI::parseAsciiString, nullptr, offsetof( W3DTreeDrawModuleData, m_stumpName ) },
		{ "KillWhenFinishedToppling",	INI::parseBool, nullptr, offsetof( W3DTreeDrawModuleData, m_killWhenToppled ) },
		{ "DoTopple",	INI::parseBool, nullptr, offsetof( W3DTreeDrawModuleData, m_doTopple ) },
		{ "InitialVelocityPercent",	INI::parsePercentToReal, nullptr, offsetof( W3DTreeDrawModuleData, m_initialVelocityPercent ) },
		{ "InitialAccelPercent",	INI::parsePercentToReal, nullptr, offsetof( W3DTreeDrawModuleData, m_initialAccelPercent ) },
		{ "BounceVelocityPercent",	INI::parsePercentToReal, nullptr, offsetof( W3DTreeDrawModuleData, m_bounceVelocityPercent ) },
    { "MinimumToppleSpeed",	INI::parsePositiveNonZeroReal, nullptr, offsetof( W3DTreeDrawModuleData, m_minimumToppleSpeed ) },
    { "SinkDistance",	INI::parsePositiveNonZeroReal, nullptr, offsetof( W3DTreeDrawModuleData, m_sinkDistance ) },
		{ "SinkTime", INI::parseDurationUnsignedInt, nullptr, offsetof(W3DTreeDrawModuleData, m_sinkFrames) },
		{ "DoShadow",	INI::parseBool, nullptr, offsetof( W3DTreeDrawModuleData, m_doShadow ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
  p.add(dataFieldParse);
	if (ModuleFactory::registersRotwkModules())
	{
		static const FieldParse retailFields[] =
		{
			{ "MorphTree", INI::parseAsciiString, nullptr, offsetof(W3DTreeDrawModuleData, m_morphTree) },
			{ "MorphTime", INI::parseDurationUnsignedInt, nullptr, offsetof(W3DTreeDrawModuleData, m_morphFrames) },
			{ "MorphFX", INI::parseFXList, nullptr, offsetof(W3DTreeDrawModuleData, m_morphFX) },
			{ "TaintedTree", INI::parseBool, nullptr, offsetof(W3DTreeDrawModuleData, m_taintedTree) },
			{ "FadeRate", INI::parseUnsignedInt, nullptr, offsetof(W3DTreeDrawModuleData, m_fadeRate) },
			{ "FadeTarget", INI::parseUnsignedInt, nullptr, offsetof(W3DTreeDrawModuleData, m_fadeTarget) },
			{ "FadeDistance", INI::parseReal, nullptr, offsetof(W3DTreeDrawModuleData, m_fadeDistance) },
			{ nullptr, nullptr, nullptr, 0 }
		};
		p.add(retailFields);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// PUBLIC FUNCTIONS ///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
W3DTreeDraw::W3DTreeDraw( Thing *thing, const ModuleData* moduleData ) : DrawModule( thing, moduleData )
{
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
W3DTreeDraw::~W3DTreeDraw()
{
	if (!RetailFileLayers::enabled())
		addToTreeBuffer();
}

const W3DTreeDrawModuleData* W3DTreeDraw::getMapDrawData(const ThingTemplate& thing)
{
	if (!RetailFileLayers::enabled() ||
		(!thing.isKindOf(KINDOF_TREE) && !thing.isKindOf(KINDOF_SHRUB) && !thing.isKindOf(KINDOF_OPTIMIZED_PROP)))
		return nullptr;

	const ModuleInfo& modules = thing.getDrawModuleInfo();
	for (Int i = 0; i < modules.getCount(); ++i)
	{
		const ModuleData* module = modules.getNthData(i);
		if (const auto* tree = module ? module->getAsW3DTreeDrawModuleData() : nullptr)
			return tree;
	}
	return nullptr;
}

Bool W3DTreeDraw::placeMapVegetation(const ThingTemplate& thing, const Dict& properties, const Coord3D& pos, Real angle)
{
	const W3DTreeDrawModuleData* data = getMapDrawData(thing);
	if (!data)
		return FALSE;
	if (!TheTerrainRenderObject)
		return TRUE;

	const Bool prop = !thing.isKindOf(KINDOF_TREE) && !thing.isKindOf(KINDOF_SHRUB);
	Bool hasScale;
	const NameKeyType scaleKey = TheNameKeyGenerator->nameToKey("objectPrototypeScale");
	const Real mapScale = properties.getReal(scaleKey, &hasScale);
	const Real scale = thing.getAssetScale() * (hasScale ? mapScale : 1.0f);
	const DrawableID id = TheGameClient->allocDrawableID();
	const Bool added = std::isfinite(scale) && scale > 0.0f && (prop
		? TheTerrainRenderObject->addProp(static_cast<Int>(id), pos, angle, scale, data->m_modelName)
		: TheTerrainRenderObject->addTree(id, pos, scale, angle, 0.0f, data));
	if (!added && INIAudit::isEnabled())
		INIAudit::record("error", "Unable to place map vegetation", thing.getName().str(), 0);
	if (std::getenv("GENERALSX_ASSET_SMOKE_VEGETATION"))
	{
		std::fprintf(stderr, "Asset smoke: vegetation %s %s %s %s at %.1f,%.1f scale %.4f\n",
			added ? "added" : "failed", prop ? "prop" : "tree", thing.getName().str(), data->m_modelName.str(),
			pos.x, pos.y, scale);
		std::fflush(stderr);
	}
	return TRUE;
}

//-------------------------------------------------------------------------------------------------
void W3DTreeDraw::addToTreeBuffer()
{
	const W3DTreeDrawModuleData *moduleData = getW3DTreeDrawModuleData();
	const Drawable *draw = getDrawable();

	DEBUG_ASSERTCRASH(draw->isPositioned(), ("W3DTreeDraw::addToTreeBuffer - This tree was not positioned!"));

	Real scale = draw->getScale();
	Real scaleRandomness = draw->getTemplate()->getInstanceScaleFuzziness();
	scaleRandomness = 0.0f; // We use the scale fuzziness inside WB to generate random scales, so they don't change at load time. jba. [4/22/2003]
	TheTerrainRenderObject->addTree(draw->getID(), *draw->getPosition(), scale, draw->getOrientation(), scaleRandomness, moduleData);
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void W3DTreeDraw::crc( Xfer *xfer )
{

	// extend base class
	DrawModule::crc( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void W3DTreeDraw::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	DrawModule::xfer( xfer );

	// no data to save here, nobody will ever notice

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void W3DTreeDraw::loadPostProcess()
{

	// extend base class
	DrawModule::loadPostProcess();

}

