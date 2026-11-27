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

// FILE: GameFont.cpp /////////////////////////////////////////////////////////////////////////////
// Created:    Colin Day, June 2001
// Desc:       Access to our representation for fonts
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameClient/GameFont.h"
#include "Common/FileSystem.h"
#include "Common/INI.h"
#include "Common/INIException.h"
#include "GameClient/UiScale.h"

// PUBLIC DATA ////////////////////////////////////////////////////////////////////////////////////
FontLibrary *TheFontLibrary = nullptr;

///////////////////////////////////////////////////////////////////////////////////////////////////
// PRIVATE FUNCTIONS //////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
/** Link a font to the font list */
//-------------------------------------------------------------------------------------------------
void FontLibrary::linkFont( GameFont *font )
{

	// sanity
	if( font == nullptr )
		return;

	// link it
	font->next = m_fontList;
	m_fontList = font;

	// increment linked count
	m_count++;

}

//-------------------------------------------------------------------------------------------------
/** Unlink a font from the font list */
//-------------------------------------------------------------------------------------------------
void FontLibrary::unlinkFont( GameFont *font )
{
	GameFont *other = nullptr;

	// sanity
	if( font == nullptr )
		return;

	// sanity check and make sure this font is actually in this library
	for( other = m_fontList; other; other = other->next )
		if( other == font )
			break;
	if( other == nullptr )
	{

		DEBUG_CRASH(( "Font '%s' not found in library", font->nameString.str() ));
		return;

	}

	// scan for the font pointing to the one we're going to unlink
	for( other = m_fontList; other; other = other->next )
		if( other->next == font )
			break;

	//
	// if nothing was fount this was at the head of the list, otherwise
	// remove from chain
	//
	if( other == nullptr )
		m_fontList = font->next;
	else
		other->next = font->next;

	// clean up this font we just unlinked just to be cool!
	font->next = nullptr;

	// we now have one less font on the list
	m_count--;

}

//-------------------------------------------------------------------------------------------------
/** Delete all font data, DO NOT throw an exception ... the destructor uses this */
//-------------------------------------------------------------------------------------------------
void FontLibrary::deleteAllFonts()
{
	GameFont *font;

	// release all the fonts
	while( m_fontList )
	{

		// get temp pointer to this font
		font = m_fontList;

		// remove font from the list, this will change m_fontList
		unlinkFont( font );

		// release font data
		releaseFontData( font );

		// delete the font list element
		deleteInstance(font);

	}

}

///////////////////////////////////////////////////////////////////////////////////////////////////
// PUBLIC FUNCTIONS ///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
FontLibrary::FontLibrary()
{

	m_fontList = nullptr;
	m_count = 0;

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
FontLibrary::~FontLibrary()
{

	// delete all font data
	deleteAllFonts();

}

//-------------------------------------------------------------------------------------------------
/** Initialize what we need to in the font library */
//-------------------------------------------------------------------------------------------------
void FontLibrary::init()
{
	m_fontAntialiasing.clear();
	const char *path = "Data\\INI\\FontSettings.ini";
	if (TheFileSystem && TheFileSystem->doesFileExist(path))
	{
		INI ini;
		ini.load(path, INI_LOAD_OVERWRITE, nullptr, "FontDefaultSettings", parseFontDefaults);
	}
}

void FontLibrary::parseFontDefaults(INI *ini)
{
	static const FieldParse fields[] = {
		{ "Antialiased", INI::parseBool, nullptr, 0 },
		{ nullptr, nullptr, nullptr, 0 }
	};
	AsciiString name = ini->getNextQuotedAsciiString();
	name.toLower();
	const char *size = ini->getNextTokenOrNull();
	const Int pointSize = size ? INI::scanInt(size) : 0;
	if (size && pointSize <= 0)
		throw INIException("FontDefaultSettings point size must be positive");
	Bool antialiased = TRUE;
	ini->initFromINI(&antialiased, fields);
	TheFontLibrary->m_fontAntialiasing[{name, pointSize}] = antialiased;
}

Bool FontLibrary::isFontAntialiased(AsciiString name, Int pointSize) const
{
	name.toLower();
	const auto sized = m_fontAntialiasing.find({name, pointSize});
	if (sized != m_fontAntialiasing.end())
		return sized->second;
	const auto family = m_fontAntialiasing.find({name, 0});
	return family == m_fontAntialiasing.end() || family->second;
}

//-------------------------------------------------------------------------------------------------
/** Reset the fonts for this font library */
//-------------------------------------------------------------------------------------------------
void FontLibrary::reset()
{

	// delete all font data
	deleteAllFonts();

}

//-------------------------------------------------------------------------------------------------
/** Get a font from our list, if we don't have that font loaded we will
	* attempt to load it */
//-------------------------------------------------------------------------------------------------
GameFont *FontLibrary::getFont( AsciiString name, Int pointSize, Bool bold )
{
	// TheSuperHackers @fix No longer creates fonts with zero size and clamps oversized fonts.
	// The upper bound caps glyph buffer memory when a resolution-scaled point size gets very large.
	enum { FONT_POINT_SIZE_MAX = 512 };
	if (pointSize < 1)
	{
		return nullptr;
	}

	FontRequest substituted;
	if (!m_substitutions.empty() &&
		m_substitutions.resolve(FontRequest{name.str(), pointSize, bold != FALSE}, substituted))
	{
		name = substituted.name.c_str();
		pointSize = substituted.size;
		bold = substituted.bold ? TRUE : FALSE;
	}

	if (pointSize > FONT_POINT_SIZE_MAX)
	{
		pointSize = FONT_POINT_SIZE_MAX;
	}

	return findOrLoadFont( name, pointSize, 0, bold );
}

GameFont *FontLibrary::getFontPixels( AsciiString name, Int pixelSize, Bool bold )
{
	enum { FONT_PIXEL_SIZE_MAX = 683 };
	if (pixelSize < 1)
	{
		return nullptr;
	}
	if (pixelSize > FONT_PIXEL_SIZE_MAX)
	{
		pixelSize = FONT_PIXEL_SIZE_MAX;
	}
	return findOrLoadFont( name, 0, pixelSize, bold );
}

GameFont *FontLibrary::getFontVariant( const GameFont *font, Bool bold )
{
	if (font == nullptr || (font->pointSize < 1 && font->pixelSize < 1))
	{
		return nullptr;
	}
	return findOrLoadFont( font->nameString, font->pointSize, font->pixelSize, bold );
}

GameFont *FontLibrary::getUiFont( AsciiString name, Int designSize, Bool bold, Int screenWidth, Int screenHeight )
{
	if (designSize < 1)
	{
		return nullptr;
	}

	FontRequest substituted;
	if (!m_substitutions.empty() &&
		m_substitutions.resolve(FontRequest{name.str(), designSize, bold != FALSE}, substituted))
	{
		name = substituted.name.c_str();
		designSize = substituted.size;
		bold = substituted.bold ? TRUE : FALSE;
	}

	return getFontPixels( name, UiScale::fontPixelSize( designSize, screenWidth, screenHeight ), bold );
}

static Bool sharesFace( const GameFont *font, const std::vector<const GameFont *>& inUse )
{
	for( const GameFont *used : inUse )
	{
		if( used == font ||
				( used->pixelSize == font->pixelSize && used->pointSize == font->pointSize && used->nameString == font->nameString ) )
			return TRUE;
	}
	return FALSE;
}

void FontLibrary::releaseUnusedPixelFonts( const std::vector<const GameFont *>& inUse )
{
	GameFont *next = nullptr;
	for( GameFont *font = m_fontList; font; font = next )
	{
		next = font->next;
		if( font->pixelSize <= 0 || sharesFace( font, inUse ) )
			continue;
		unlinkFont( font );
		releaseFontData( font );
		deleteInstance( font );
	}
	releaseUnusedFontData();
}

GameFont *FontLibrary::findOrLoadFont( const AsciiString& name, Int pointSize, Int pixelSize, Bool bold )
{
	GameFont *font;

	// search for font in list
	for( font = m_fontList; font; font = font->next )
	{

		if( font->pointSize == pointSize &&
				font->pixelSize == pixelSize &&
				font->bold == bold &&
				font->nameString == name
			)
			return font;  // found

	}

	// font not found, allocate a new font element
	font = newInstance(GameFont);
	if( font == nullptr )
	{

		DEBUG_CRASH(( "getFont: Unable to allocate new font list element" ));
		return nullptr;

	}

	// copy font data over to new element
	font->nameString = name;
	font->pointSize = pointSize;
	font->pixelSize = pixelSize;
	font->bold = bold;
	font->fontData = nullptr;

	//DEBUG_LOG(("Font: Loading font '%s' %d point", font->nameString.str(), font->pointSize));
	// load the device specific data pointer
	if( loadFontData( font ) == FALSE )
	{

		DEBUG_CRASH(( "getFont: Unable to load font data pointer '%s'", name.str() ));
		deleteInstance(font);
		return nullptr;

	}

	// tie font into list
	linkFont( font );

	// all is done and loaded
	return font;

}
