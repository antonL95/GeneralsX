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

// FILE: GameFont.h ///////////////////////////////////////////////////////////////////////////////
// Created:    Colin Day, June 2001
// Desc:       Game representations for fonts
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <map>
#include <utility>

#include "Common/SubsystemInterface.h"
#include "Common/AsciiString.h"
#include "Common/GameMemory.h"
#include "GameClient/FontSubstitution.h"

#include <vector>

class INI;

//-------------------------------------------------------------------------------------------------
/** A font for use in the device independent game */
//-------------------------------------------------------------------------------------------------
class GameFont : public MemoryPoolObject
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(GameFont, "GameFont")
public:
	GameFont*			next;  ///< for library use
	AsciiString		nameString;
	Int						pointSize;		 ///< point size of font
	Int						pixelSize;
	Int						height;				///< pixel height of font
	void*					fontData;  ///< font data to be filled out for device specific font
	Bool					bold;				 ///< is this font bold
};
EMPTY_DTOR(GameFont)

//-------------------------------------------------------------------------------------------------
/** Interface to access fonts for the system */
//-------------------------------------------------------------------------------------------------
class FontLibrary : public SubsystemInterface
{

public:


public:

	FontLibrary();
	virtual ~FontLibrary() override;

	virtual void init() override;
	virtual void reset() override;
	virtual void update() override { }

	GameFont *getFont( AsciiString name, Int pointSize, Bool bold );  ///< get a font pointer
	GameFont *getFontPixels( AsciiString name, Int pixelSize, Bool bold );
	Bool isFontAntialiased(AsciiString name, Int pointSize) const;
	GameFont *getFontVariant( const GameFont *font, Bool bold );
	GameFont *getUiFont( AsciiString name, Int designSize, Bool bold, Int screenWidth, Int screenHeight );
	void releaseUnusedPixelFonts( const std::vector<const GameFont *>& inUse );

	void setSubstitutions( const FontSubstitutionTable& table ) { m_substitutions = table; }
	const FontSubstitutionTable& getSubstitutions() const { return m_substitutions; }

	GameFont *firstFont();  ///< return first font
	GameFont *nextFont( GameFont *font );  ///< get next font in library

	Int getCount();  ///< return how many fonts are loaded in this lib

protected:

	void deleteAllFonts();				///< delete all fonts in this library
	void linkFont( GameFont *font );		///< add to font list
	void unlinkFont( GameFont *font );  ///< remove font from list

	/// load the font data pointer based on everything else we already have set
	virtual Bool loadFontData( GameFont *font ) = 0;
	static void parseFontDefaults(INI *ini);
	std::map<std::pair<AsciiString, Int>, Bool> m_fontAntialiasing;
	/// release the font data pointer
	virtual void releaseFontData( GameFont *font ) { };
	virtual void releaseUnusedFontData() { }

	GameFont *findOrLoadFont( const AsciiString& name, Int pointSize, Int pixelSize, Bool bold );

	GameFont *m_fontList;  ///< list of fonts we have loaded
	Int m_count;  ///< number of unique fonts loaded in this lib
	FontSubstitutionTable m_substitutions;

};

// INLINING ///////////////////////////////////////////////////////////////////////////////////////
inline Int FontLibrary::getCount() { return m_count; }
inline GameFont *FontLibrary::firstFont() { return m_fontList; }
inline GameFont *FontLibrary::nextFont( GameFont *font )
{
	if( font )
		return font->next;
	return nullptr;
}

// EXTERNALS //////////////////////////////////////////////////////////////////////////////////////
extern FontLibrary *TheFontLibrary;  ///< font library external
