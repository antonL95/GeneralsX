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

// RadiusDecal.cpp ///////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include <deque>

#define DEFINE_SHADOW_NAMES

#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/RetailFileLayers.h"
#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"
#include "GameClient/Display.h"
#include "GameClient/DisplayStringManager.h"
#include "GameClient/Drawable.h"
#include "GameClient/InGameUI.h"
#include "GameClient/Image.h"
#include "GameClient/Mouse.h"
#include "GameClient/RadiusDecal.h"
#include "GameClient/RetailWorldUI.h"
#include "GameClient/Shadow.h"
#include "GameClient/View.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/TerrainResourceBehavior.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"
#include "WWMath/wwmath.h"

namespace
{
struct NamedCursor
{
	AsciiString name;
	RadiusDecalTemplate decal;
};

std::deque<NamedCursor>& namedCursors()
{
	static std::deque<NamedCursor> cursors;
	return cursors;
}

Int namedCursorIndex(const char* name)
{
	auto& cursors = namedCursors();
	for (size_t index = 0; index < cursors.size(); ++index)
		if (cursors[index].name.compareNoCase(name) == 0)
			return static_cast<Int>(index);
	cursors.push_back({name, RadiusDecalTemplate()});
	return static_cast<Int>(cursors.size() - 1);
}
}

void RadiusDecalTemplate::parseCursorType(INI* ini, void *instance, void *store, const void *userData)
{
	if (!RetailFileLayers::enabled())
	{
		INI::parseIndexList(ini, instance, store, userData);
		return;
	}

	const char* name = ini->getNextToken();
	const auto names = static_cast<ConstCharPtrArray>(userData);
	for (Int index = 0; names[index]; ++index)
	{
		if (stricmp(name, names[index]) == 0)
		{
			*static_cast<RadiusCursorType*>(store) = static_cast<RadiusCursorType>(index);
			return;
		}
	}
	*static_cast<RadiusCursorType*>(store) = static_cast<RadiusCursorType>(RADIUSCURSOR_COUNT + namedCursorIndex(name));
}

void RadiusDecalTemplate::parseNamedCursor(INI* ini, void *instance, void *, const void *)
{
	const Int index = namedCursorIndex(ini->getNextToken());
	RadiusDecalTemplate decal;
	parseRadiusDecalTemplate(ini, instance, &decal, nullptr);
	namedCursors()[index].decal = decal;
}

const RadiusDecalTemplate* RadiusDecalTemplate::findNamedCursor(Int cursorType)
{
	const Int index = cursorType - RADIUSCURSOR_COUNT;
	const auto& cursors = namedCursors();
	return index >= 0 && static_cast<size_t>(index) < cursors.size() && cursors[index].decal.valid()
		? &cursors[index].decal : nullptr;
}

void drawTerrainResourceIndicator(View& view, const ThingTemplate* placement, const Drawable* selected,
	DisplayString*& label, WorldGroundDecal& decal)
{
	const Player* player = ThePlayerList->getLocalPlayer();
	const Object* object = nullptr;
	const ThingTemplate* type = placement;
	Coord3D position;
	if (placement)
	{
		ICoord2D cursor = TheMouse->getMouseStatus()->pos;
		if (TheInGameUI->isPlacementAnchored())
		{
			ICoord2D end;
			TheInGameUI->getPlacementPoints(&cursor, &end);
		}
		if (!view.screenToTerrain(&cursor, &position))
		{
			decal.clear();
			return;
		}
	}
	else
	{
		object = selected ? selected->getObject() : nullptr;
		if (!object || object->getControllingPlayer() != player || !object->isLogicallyVisible())
		{
			decal.clear();
			return;
		}
		type = object->getTemplate();
		position = *object->getPosition();
	}

	Real radius, efficiency;
	if (!player || !TerrainResourceBehavior::getDisplayInfo(*type, position, *player, object, radius, efficiency))
	{
		decal.clear();
		return;
	}

	if (object)
	{
		decal.clear();
		if (object->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION))
			RetailWorldUI::drawTerrainClaims(view, *player, object, radius);
		else
			RetailWorldUI::drawTerrainClaims(view, *player);
		return;
	}

	AsciiString texture = TheInGameUI->getTerrainClaimDecal(TRUE);
	if (const Image* image = TheMappedImageCollection ? TheMappedImageCollection->findImageByName(texture) : nullptr)
		texture = image->getFilename();
	decal.set(texture, position, radius * 2.0f, radius * 2.0f, 108);
	RetailWorldUI::drawTerrainClaims(view, *player);

	ICoord2D center;
	if (!view.worldToScreen(&position, &center))
		return;

	if (!label)
		label = TheDisplayStringManager->newDisplayString();
#if RTS_ZEROHOUR
	label->setFont(TheInGameUI->getPlacementClaimFont());
#else
	label->setFont(TheFontLibrary->getFont("Albertus MT", std::max(9u, TheDisplay->getHeight() / 60), FALSE));
#endif
	UnicodeString text;
	text.format(L"%.0f%%", std::max(0.0f, efficiency * 100.0f));
	if (label->getText().compare(text) != 0)
		label->setText(text);
	Int width, height;
	label->getSize(&width, &height);
	const Int x = center.x - width / 2;
	const Int y = center.y - height - 28;
#if RTS_ZEROHOUR
	const Color color = TheInGameUI->getPlacementClaimFontColor();
#else
	const Color color = GameMakeColor(131, 255, 94, 255);
#endif
	label->draw(x, y, color, GameMakeColor(13, 25, 12, 240));
}


// ------------------------------------------------------------------------------------------------
RadiusDecalTemplate::RadiusDecalTemplate() :
	m_shadowType(SHADOW_ALPHA_DECAL),
	m_minOpacity(1.0f),
	m_maxOpacity(1.0f),
	m_opacityThrobTime(LOGICFRAMES_PER_SECOND),
	m_color(0),
	m_onlyVisibleToOwningPlayer(true),
	m_name(AsciiString::TheEmptyString)
{
}

// ------------------------------------------------------------------------------------------------
void RadiusDecalTemplate::createRadiusDecal(const Coord3D& pos, Real radius, const Player* owningPlayer, RadiusDecal& result) const
{
	result.clear();

	if (owningPlayer == nullptr)
	{
		DEBUG_CRASH(("You MUST specify a non-null owningPlayer to createRadiusDecal. (srj)"));
		return;
	}

	if (m_name.isEmpty() || radius <= 0.0f)
		return;

	// it is now considered nonEmpty, regardless of the state of m_decal, etc
	result.m_empty = false;

	if (TheProjectedShadowManager && (!m_onlyVisibleToOwningPlayer ||
			owningPlayer->getPlayerIndex() == ThePlayerList->getLocalPlayer()->getPlayerIndex()))
	{
		Shadow::ShadowTypeInfo decalInfo;
		decalInfo.allowUpdates = FALSE;										// shadow texture will never update
		decalInfo.allowWorldAlign = TRUE;									// shadow image will wrap around world objects
		decalInfo.m_type = m_shadowType;
		strlcpy(decalInfo.m_ShadowName, m_name.str(), ARRAY_SIZE(decalInfo.m_ShadowName));		// name of your texture
		decalInfo.m_sizeX = radius*2;									// world space dimensions
		decalInfo.m_sizeY = radius*2;									// world space dimensions

		result.m_decal = TheProjectedShadowManager->addDecal(&decalInfo);
		if (result.m_decal)
		{
			result.m_decal->setAngle(0.0f);
			result.m_baseColor = m_color == 0 ? owningPlayer->getPlayerColor() : m_color;
			result.m_decal->setColor(result.m_baseColor);
			result.m_decal->setPosition(pos.x, pos.y, pos.z);
			result.m_template = this;
		}
		else
		{
			DEBUG_CRASH(("Unable to add decal %s",decalInfo.m_ShadowName));
		}
	}
}

// ------------------------------------------------------------------------------------------------
void RadiusDecalTemplate::xferRadiusDecalTemplate( Xfer *xfer )
{
  // version
  XferVersion currentVersion = 1;
  XferVersion version = currentVersion;
  xfer->xferVersion( &version, currentVersion );

	xfer->xferAsciiString(&m_name);
	xfer->xferUser(&m_shadowType, sizeof(m_shadowType));
	xfer->xferReal(&m_minOpacity);
  xfer->xferReal(&m_maxOpacity);
	xfer->xferUnsignedInt(&m_opacityThrobTime);
	xfer->xferColor(&m_color);
	xfer->xferBool(&m_onlyVisibleToOwningPlayer);
}

// ------------------------------------------------------------------------------------------------
/*static*/ void RadiusDecalTemplate::parseRadiusDecalTemplate(INI* ini, void *instance, void * store, const void* /*userData*/)
{
	static const FieldParse dataFieldParse[] =
	{
		{ "Texture",										INI::parseAsciiString,				nullptr,							offsetof( RadiusDecalTemplate, m_name ) },
		{ "Style",											INI::parseBitString32,				TheShadowNames,		offsetof( RadiusDecalTemplate, m_shadowType ) },
		{ "OpacityMin",									INI::parsePercentToReal,			nullptr,							offsetof( RadiusDecalTemplate, m_minOpacity ) },
		{ "OpacityMax",									INI::parsePercentToReal,			nullptr,							offsetof( RadiusDecalTemplate, m_maxOpacity) },
		{ "OpacityThrobTime",						INI::parseEngineDurationUnsignedInt, nullptr,				offsetof( RadiusDecalTemplate, m_opacityThrobTime ) },
		{ "Color",											INI::parseColorInt,						nullptr,							offsetof( RadiusDecalTemplate, m_color ), true },
		{ "OnlyVisibleToOwningPlayer",	INI::parseBool,								nullptr,							offsetof( RadiusDecalTemplate, m_onlyVisibleToOwningPlayer ) },
		{ nullptr, nullptr, nullptr, 0 }
	};

	ini->initFromINI(store, dataFieldParse);
}

// ------------------------------------------------------------------------------------------------
RadiusDecal::RadiusDecal() :
	m_template(nullptr),
	m_decal(nullptr),
	m_empty(true)
{
}

// ------------------------------------------------------------------------------------------------
RadiusDecal::RadiusDecal(const RadiusDecal& that) :
	m_template(nullptr),
	m_decal(nullptr),
	m_empty(true)
{
	DEBUG_CRASH(("not fully implemented"));
}

// ------------------------------------------------------------------------------------------------
RadiusDecal& RadiusDecal::operator=(const RadiusDecal& that)
{
	if (this != &that)
	{
		m_template = nullptr;
		if (m_decal)
			m_decal->release();
		m_decal = nullptr;
		m_empty = true;
		DEBUG_CRASH(("not fully implemented"));
	}
	return *this;
}

// ------------------------------------------------------------------------------------------------
void RadiusDecal::xferRadiusDecal( Xfer *xfer )
{
	/// @todo implement me
	if (xfer->getXferMode() == XFER_LOAD)
	{
		clear();
	}
}

// ------------------------------------------------------------------------------------------------
void RadiusDecal::clear()
{
	m_template = nullptr;
	if (m_decal)
	{
		m_decal->release();
	}
	m_decal = nullptr;
	m_empty = true;
}

// ------------------------------------------------------------------------------------------------
RadiusDecal::~RadiusDecal()
{
	clear();
}

// ------------------------------------------------------------------------------------------------
void RadiusDecal::update()
{
	if (m_decal && m_template)
	{
		UnsignedInt now = TheGameLogic->getFrame();
		Real theta = (2*PI) * (Real)(now % m_template->m_opacityThrobTime) / (Real)m_template->m_opacityThrobTime;
		Real percent = 0.5f * (Sin(theta) + 1.0f);
		Int opac;
		if( TheGameLogic->getDrawIconUI() )
		{
			opac = REAL_TO_INT((m_template->m_minOpacity + percent * (m_template->m_maxOpacity - m_template->m_minOpacity)) * 255.0f);
		}
		else
		{
			//Scripts turned this off, so don't show them!
			opac = 0;
		}
		m_decal->setOpacity(opac);
	}
}



void RadiusDecal::setOpacity( Real o )
{
	if (m_decal)
	{
		m_decal->setOpacity(REAL_TO_INT(255.0f * o));
	}
}

void RadiusDecal::setTargetingValidity(Bool valid)
{
	constexpr Color retailInvalidTargetColor = 0xff1414;
	if (m_decal)
		m_decal->setColor(valid ? m_baseColor : retailInvalidTargetColor);
}

// ------------------------------------------------------------------------------------------------
void RadiusDecal::setPosition(const Coord3D& pos)
{
	if (m_decal)
	{
		m_decal->setPosition(pos.x, pos.y, pos.z);	//world space position of center of decal
	}
}

WorldGroundDecal::~WorldGroundDecal()
{
	clear();
}

void WorldGroundDecal::clear()
{
	if (m_shadow)
		m_shadow->release();
	m_shadow = nullptr;
	m_texture.clear();
	m_pixels.clear();
}

void WorldGroundDecal::set(const AsciiString& texture, const Coord3D& position, Real width, Real height, Int opacity,
	Color color, Bool additive, Real angle)
{
	if (!TheProjectedShadowManager || texture.isEmpty() || width <= 0.0f || height <= 0.0f)
	{
		clear();
		return;
	}
	if (m_texture != texture || m_additive != additive)
	{
		clear();
		m_texture = texture;
		m_additive = additive;
		Shadow::ShadowTypeInfo info;
		info.m_type = additive ? SHADOW_ADDITIVE_DECAL : SHADOW_ALPHA_DECAL;
		info.allowWorldAlign = TRUE;
		info.m_sizeX = width;
		info.m_sizeY = height;
		strlcpy(info.m_ShadowName, texture.str(), ARRAY_SIZE(info.m_ShadowName));
		if (char* extension = strrchr(info.m_ShadowName, '.'))
			*extension = '\0';
		m_shadow = TheProjectedShadowManager->addDecal(&info);
	}
	if (m_shadow)
	{
		m_shadow->setSize(width, height);
		m_shadow->setPosition(position.x, position.y, position.z);
		m_shadow->setAngle(angle);
		m_shadow->setColor(color);
		m_shadow->setOpacity(opacity);
	}
}

void WorldGroundDecal::setComposited(const Coord3D& position, Real extent, Int opacity, Color color,
	std::vector<UnsignedInt>&& pixels, Int size)
{
	if (!TheProjectedShadowManager)
		return;
	if (!m_texture.isEmpty())
		clear();
	if (!m_shadow)
	{
		Shadow::ShadowTypeInfo info;
		info.m_type = SHADOW_ALPHA_DECAL;
		info.allowWorldAlign = TRUE;
		info.m_sizeX = extent;
		info.m_sizeY = extent;
		m_shadow = TheProjectedShadowManager->addDecal(&info, size);
	}
	if (!m_shadow)
		return;
	if (m_pixels != pixels)
	{
		if (!m_shadow->updateDecalPixels(pixels.data(), size))
		{
			clear();
			return;
		}
		m_pixels = std::move(pixels);
	}
	m_shadow->setSize(extent, extent);
	m_shadow->setPosition(position.x, position.y, position.z);
	m_shadow->setColor(color);
	m_shadow->setOpacity(opacity);
}
