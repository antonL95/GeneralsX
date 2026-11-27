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

// FILE: W3DGameFont.cpp //////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
//
//                       Westwood Studios Pacific.
//
//                       Confidential Information
//                Copyright (C) 2001 - All Rights Reserved
//
//-----------------------------------------------------------------------------
//
// Project:    RTS3
//
// File name:  W3DGameFont.cpp
//
// Created:    Colin Day, June 2001
//
// Desc:       W3D implementation for managing font definitions
//
//-----------------------------------------------------------------------------
///////////////////////////////////////////////////////////////////////////////

// SYSTEM INCLUDES ////////////////////////////////////////////////////////////
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// USER INCLUDES //////////////////////////////////////////////////////////////
#include "Common/Debug.h"
#include "W3DDevice/GameClient/W3DGameFont.h"
#include "WW3D2/ww3d.h"
#include "WW3D2/assetmgr.h"
#include "WW3D2/render2dsentence.h"
#include "WW3D2/fontregistry.h"
#include "Common/file.h"
#include "Common/FileSystem.h"
#include "Common/GlobalData.h"
#include "Common/Registry.h"
#include "Common/RetailFileLayers.h"
#include "GameClient/FontSubstitution.h"
#include "GameClient/GlobalLanguage.h"
#include "GameClient/UiTest.h"

#include <algorithm>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace
{
using FontLookup = std::tuple<std::string, Int, Bool, Bool, bool>;
std::set<FontLookup> FailedFontLookups;

FontCharsClass *GetFontChars(const char *name, Int size, Bool pixels, Bool bold)
{
	const bool bundledFallback = FontRegistryClass::Is_Strict_Family(name) &&
		FontRegistryClass::Allows_Bundled_Fallback();
	const FontLookup request{name, size, pixels, bold, bundledFallback};
	if (FailedFontLookups.count(request) != 0)
		return nullptr;

	FontCharsClass *font = pixels
		? WW3DAssetManager::Get_Instance()->Get_FontChars_Pixels(name, size, bold)
		: WW3DAssetManager::Get_Instance()->Get_FontChars(name, size, bold);
	if (font == nullptr)
		FailedFontLookups.insert(request);
	return font;
}

// GeneralsX @bugfix GitHubCopilot 20/05/2026 Resolve a usable Unicode fallback font on macOS/Linux when localized font names are unavailable.
// GeneralsX @bugfix GitHubCopilot 29/05/2026 Prevent circular Unicode fallback when the localized unicode family equals the base font family.
FontCharsClass *LoadUnicodeFallbackFont(Int size, Bool pixels, Bool bold, const char *base_name)
{
	const char *preferred_name = nullptr;

	if (TheGlobalLanguageData && TheGlobalLanguageData->m_unicodeFontName.isNotEmpty()) {
		preferred_name = TheGlobalLanguageData->m_unicodeFontName.str();
	}

	// Build candidate list: the localized preferred name first (may be a limited-coverage font like "Arial"),
	// then known-good Unicode fonts. We iterate all candidates and pick the first that loads AND has a
	// different family from the base font. This avoids returning a font that lacks Cyrillic coverage
	// (e.g., Arial on macOS) when a better universal font like "Arial Unicode MS" is available.
	// GeneralsX @feature felipebraz 26/09/2026 Include Liberation Sans in Unicode fallback candidate list
	static const char *kFallbackUnicodeFonts[] = {
		"Arial Unicode MS",
		"Arial Unicode",
		"Liberation Sans",
		"Liberation Serif",
		"Arial",
		"Helvetica Neue",
		"Helvetica",
		"Noto Sans",
		"Noto Sans CJK SC",
		"Noto Sans CJK JP",
		"DejaVu Sans"
	};

	for (const char *font_name : kFallbackUnicodeFonts) {
		if (base_name != nullptr && strcmp(font_name, base_name) == 0)
			continue;
		if (preferred_name != nullptr && strcmp(font_name, preferred_name) == 0)
			continue;

		FontCharsClass *font = GetFontChars(font_name, size, pixels, bold);
		if (font != nullptr) {
			return font;
		}
	}

	// Now try the localized preferred name as a last resort (it may load on some platforms)
	if (preferred_name != nullptr && (base_name == nullptr || strcmp(preferred_name, base_name) != 0)) {
		FontCharsClass *font = GetFontChars(preferred_name, size, pixels, bold);
		if (font != nullptr) {
			return font;
		}
	}

	return nullptr;
}

// GeneralsX @feature felipebraz 26/09/2026 Load Font Awesome Brands as dedicated brand icon fallback font
static inline bool IsBrandFontName(const char *name)
{
	if (name == nullptr) return false;
	return (strcmp(name, "fa-brands-400") == 0 ||
	        strcmp(name, "Font Awesome 6 Brands Regular") == 0 ||
	        strcmp(name, "Font Awesome 6 Brands") == 0 ||
	        strcmp(name, "FontAwesome6Brands-Regular") == 0);
}

FontCharsClass *LoadBrandFallbackFont(Int size, Bool pixels, Bool bold, const char *base_name)
{
	static const char *kBrandFonts[] = {
		"Font Awesome 6 Brands Regular",
		"Font Awesome 6 Brands",
		"fa-brands-400",
		nullptr
	};

	for (int i = 0; kBrandFonts[i]; ++i) {
		if (base_name != nullptr && strcmp(kBrandFonts[i], base_name) == 0)
			continue;

		FontCharsClass *font = GetFontChars(kBrandFonts[i], size, pixels, bold);
		if (font != nullptr) {
			// GeneralsX @bugfix felipebraz 27/09/2026 Ensure font has the required OS brand glyphs (Apple, Windows, Linux)
			if (font->Has_Glyph(0xF179) && font->Has_Glyph(0xF17A) && font->Has_Glyph(0xF17C)) {
				return font;
			}
			font->Release_Ref();
		}
	}

	return nullptr;
}

Bool ReadGameFile(const char *path, std::vector<unsigned char> &data)
{
	data.clear();
	File *file = TheFileSystem->openFile(path, File::READ | File::BINARY);
	if (file == nullptr)
		return FALSE;
	const Int size = file->size();
	if (size > 0)
	{
		data.resize(size);
		if (file->read(data.data(), size) != size)
			data.clear();
	}
	file->close();
	return !data.empty();
}

std::string_view AsText(const std::vector<unsigned char> &data)
{
	return std::string_view(reinterpret_cast<const char *>(data.data()), data.size());
}

Bool ReadLanguageFile(std::vector<unsigned char> &data)
{
	AsciiString language = GetRegistryLanguage();
	language.toLower();
	AsciiString modPath;
	modPath.format("%slang/%s/language.ini", TheGlobalData->m_modDir.str(), language.str());
	return ReadGameFile(modPath.str(), data) || ReadGameFile("language.ini", data);
}

void RegisterInstallFont(const AsciiString &path)
{
	std::vector<unsigned char> data;
	if (!ReadGameFile(path.str(), data))
	{
		fprintf(stderr, "[Font] install font '%s' not found\n", path.str());
		return;
	}
	if (const InstalledFontFace *face = FontRegistryClass::Register_Memory_Font(path.str(), std::move(data)))
		fprintf(stderr, "[Font] install font '%s' registered as '%s' %s\n", path.str(), face->Family.c_str(), face->Style.c_str());
}

void LoadInstallFonts()
{
	std::vector<unsigned char> language;
	std::vector<std::string> localFiles;
	if (ReadLanguageFile(language))
		localFiles = parseLocalFontFiles(AsText(language));
	for (const std::string &file : localFiles)
		RegisterInstallFont(AsciiString(file.c_str()));

	FilenameList fonts;
	TheFileSystem->getFileListInDirectory(AsciiString::TheEmptyString, "*.ttf", fonts, FALSE);
	TheFileSystem->getFileListInDirectory(AsciiString::TheEmptyString, "*.otf", fonts, FALSE);
	for (const AsciiString &file : fonts)
	{
		const bool listed = std::any_of(localFiles.begin(), localFiles.end(),
			[&](const std::string &local) { return fontNamesEqualNoCase(local, file.str()); });
		if (!listed)
			RegisterInstallFont(file);
	}
	fprintf(stderr, "[Font] %d install fonts from %d LocalFontFile entries and %d archive fonts\n",
		FontRegistryClass::Count(), static_cast<int>(localFiles.size()), static_cast<int>(fonts.size()));
}

FontSubstitutionTable LoadFontSubstitutions()
{
	std::vector<unsigned char> data;
	if (!ReadGameFile("data\\ini\\fontsubstitution.ini", data))
		return FontSubstitutionTable();

	std::vector<std::string> warnings;
	FontSubstitutionTable table = FontSubstitutionTable::parse(AsText(data), &warnings);
	for (const std::string &warning : warnings)
		fprintf(stderr, "[Font] fontsubstitution.ini %s\n", warning.c_str());
	fprintf(stderr, "[Font] %d font substitutions with %d sizes\n",
		static_cast<int>(table.fontCount()), static_cast<int>(table.ruleCount()));
	return table;
}
}

// DEFINES ////////////////////////////////////////////////////////////////////

// PRIVATE TYPES //////////////////////////////////////////////////////////////

// PRIVATE DATA ///////////////////////////////////////////////////////////////

// PUBLIC DATA ////////////////////////////////////////////////////////////////

// PRIVATE PROTOTYPES /////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// PRIVATE FUNCTIONS //////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void W3DFontLibrary::init()
{
	FontLibrary::init();
	FailedFontLookups.clear();

	if (!RetailFileLayers::enabled() && !UiTest::isActive())
		return;

	FontRegistryClass::Add_Strict_Family("Albertus MT");
	FontRegistryClass::Add_Strict_Family("Omnia LT Std");
	FontRegistryClass::Add_Strict_Family("SachaWynter");
	FontRegistryClass::Add_Strict_Family("SachaWynterTight");
	if (FontRegistryClass::Count() == 0)
		LoadInstallFonts();
	const FontSubstitutionTable substitutions = LoadFontSubstitutions();
	for (const std::string &name : substitutions.targetNames())
		FontRegistryClass::Add_Strict_Family(name);
	setSubstitutions(substitutions);
	fflush(stderr);
}

// W3DFontLibrary::loadFontData ===============================================
/** Load a font */
//=============================================================================
Bool W3DFontLibrary::loadFontData( GameFont *font )
{
	// sanity
	if( font == nullptr )
		return FALSE;

	const char* name = font->nameString.str();
	const Bool pixels = font->pixelSize > 0;
	const Int size = pixels ? font->pixelSize : font->pointSize;
	const Bool bold = font->bold;

	// get the font data from the asset manager
	FontCharsClass *fontChar = GetFontChars( name, size, pixels, bold );

	if( fontChar == nullptr )
	{
		DEBUG_CRASH(( "Unable to find font '%s' in Asset Manager", name ));
		return FALSE;
	}

	// assign font data
	fontChar->Set_Antialiased(isFontAntialiased(font->nameString, font->pointSize));
	font->fontData = fontChar;
	font->height = fontChar->Get_Char_Height();

	// load Unicode of same point size
	// GeneralsX @bugfix fbraz 03/06/2026 Prevent circular AlternateUnicodeFont chain.
	// Fonts in the fallback candidate list that DON'T need a fallback (full-coverage fonts)
	// should not get an AlternateUnicodeFont set, otherwise Get_Char_Data enters infinite
	// recursion: e.g. Arial → Arial Unicode MS → Arial → ...
	{
		FontRegistryClass::ProbeScope probe;
		// GeneralsX @bugfix felipebraz 29/09/2026 Brand font aliases must skip Unicode fallback to preserve native glyph lookup
		bool skipFallback = IsBrandFontName(name);
		// Skip fallback for fonts that already have full Unicode coverage themselves
		static const char *kFullCoverageFonts[] = {
			"Arial Unicode MS",
			"Arial Unicode",
			"DejaVu Sans",
			nullptr
		};
		if (!skipFallback) {
			for (int i = 0; kFullCoverageFonts[i]; i++) {
				if (strcmp(name, kFullCoverageFonts[i]) == 0) {
					skipFallback = true;
					break;
				}
			}
		}
		if (!skipFallback) {
			fontChar->AlternateUnicodeFont = LoadUnicodeFallbackFont(size, pixels, bold, name);
		}

		// GeneralsX @feature felipebraz 26/09/2026 Provide brand glyph fallback for UI fonts
		bool isBrandFont = IsBrandFontName(name);
		if (!isBrandFont) {
			fontChar->AlternateBrandFont = LoadBrandFallbackFont(size, pixels, bold, name);
		}
	}

	return TRUE;
}

// W3DFontLibrary::releaseFontData ============================================
/** Release font data */
//=============================================================================
void W3DFontLibrary::releaseFontData( GameFont *font )
{

	// presently we don't need to do anything because fonts are handled in
	// the W3D asset manager which is all taken for of us
	if (font && font->fontData)
	{
		if(((FontCharsClass *)(font->fontData))->AlternateBrandFont)
			((FontCharsClass *)(font->fontData))->AlternateBrandFont->Release_Ref();
		if(((FontCharsClass *)(font->fontData))->AlternateUnicodeFont)
			((FontCharsClass *)(font->fontData))->AlternateUnicodeFont->Release_Ref();
		((FontCharsClass *)(font->fontData))->Release_Ref();

		font->fontData = nullptr;
	}

}

void W3DFontLibrary::releaseUnusedFontData()
{
	WW3DAssetManager::Get_Instance()->Release_Unused_Pixel_FontChars();
}

// PUBLIC FUNCTIONS ///////////////////////////////////////////////////////////

