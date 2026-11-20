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

// FILE: INI.cpp //////////////////////////////////////////////////////////////////////////////////
// Author: Colin Day, November 2001
// Desc:   INI Reader
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine
#define DEFINE_DEATH_NAMES

#include "WWMath/wwmath.h"
#include "Common/INI.h"
#include "GameLogic/BfmeMapScripts.h"
#include "Common/INIAudit.h"
#include "Common/INIBlockStructure.h"
#include "Common/RetailFileLayers.h"
#include "Common/RotwkDuration.h"
#include "Common/INIException.h"
#include "Common/INILegacyLines.h"
#include "Common/INIMath.h"
#include "Common/INIPreprocessor.h"
#include "Common/INIRetailTolerance.h"

#include <map>

#include "Common/DamageFX.h"
#include "Common/file.h"
#include "Common/FileSystem.h"
#include "Common/GameAudio.h"
#include "Common/KindOf.h"
#include "Common/ObjectStatusTypes.h"
#include "Common/Science.h"
#include "Common/SpecialPower.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "Common/Xfer.h"
#include "Common/XferCRC.h"

#include "GameClient/Anim2D.h"
#include "GameClient/Color.h"
#include "GameClient/FXList.h"
#include "GameClient/GameText.h"
#include "GameClient/Image.h"
#include "GameClient/ParticleSys.h"
#include "GameLogic/Armor.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/FPUControl.h"
#include "GameLogic/ObjectCreationList.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Weapon.h"

namespace
{
std::map<std::pair<std::string, std::string>, unsigned> retailSkips;
bool retailStartupLoadFinished = false;
unsigned retailLoadDepth = 0;

void printRetailSkips(const char* loadPath = nullptr)
{
	if (retailSkips.empty())
		return;

	if (loadPath)
		fprintf(stderr, "[Retail INI] Skips loading '%s'\n", loadPath);
	for (const auto& [entry, count] : retailSkips)
		fprintf(stderr, "[Retail INI] %s '%s': %u\n", entry.first.c_str(), entry.second.c_str(), count);
	fflush(stderr);
	retailSkips.clear();
}

class RetailLoadScope
{
public:
	explicit RetailLoadScope(const AsciiString& path)
		: m_path(path), m_active(retailStartupLoadFinished && RetailFileLayers::enabled() && !INIAudit::isEnabled())
	{
		if (m_active)
			++retailLoadDepth;
	}

	~RetailLoadScope()
	{
		if (m_active && --retailLoadDepth == 0)
			printRetailSkips(m_path.str());
	}

private:
	const AsciiString& m_path;
	const bool m_active;
};

bool isINIParseError()
{
	try
	{
		throw;
	}
	catch (INIErrorCode)
	{
		return true;
	}
	catch (...)
	{
		return false;
	}
}

}

#if __cplusplus >= 201611L
#define USE_STD_FROM_CHARS_PARSING 1
#else
#define USE_STD_FROM_CHARS_PARSING 0
#endif

#if USE_STD_FROM_CHARS_PARSING
#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <string_view>
#include <type_traits>
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////
// PRIVATE DATA ///////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

static Xfer *s_xfer = nullptr;

static Bool hashesPreprocessedLines()
{
	return RetailFileLayers::enabled() || INIAudit::isEnabled();
}

//-------------------------------------------------------------------------------------------------
/** This is the table of data types we can have in INI files.  To add a new data type
	* block make a new entry in this table and add an appropriate parsing function */
//-------------------------------------------------------------------------------------------------
extern void parseReallyLowMHz( INI* ini);		// yeah, so sue me (srj)
struct BlockParse
{
	const char *token;
	INIBlockParse parse;
};
static const BlockParse theTypeTable[] =
{
	{ "AIData",                         INI::parseAIDataDefinition },
	{ "Animation",                      INI::parseAnim2DDefinition },
	{ "Armor",                          INI::parseArmorDefinition },
	{ "AudioEvent",                     INI::parseAudioEventDefinition },
	{ "AudioSettings",                  INI::parseAudioSettingsDefinition },
	{ "BenchProfile",                   INI::parseBenchProfile },
	{ "Bridge",                         INI::parseTerrainBridgeDefinition },
	{ "Campaign",                       INI::parseCampaignDefinition },
	{ "ChallengeGenerals",              INI::parseChallengeModeDefinition },
	{ "CommandButton",                  INI::parseCommandButtonDefinition },
	{ "CommandMap",                     INI::parseMetaMapDefinition },
	{ "CommandSet",                     INI::parseCommandSetDefinition },
	{ "ControlBarResizer",              INI::parseControlBarResizerDefinition },
	{ "ControlBarScheme",               INI::parseControlBarSchemeDefinition },
	{ "CrateData",                      INI::parseCrateTemplateDefinition },
	{ "Credits",                        INI::parseCredits },
	{ "DamageFX",                       INI::parseDamageFXDefinition },
	{ "DialogEvent",                    INI::parseDialogDefinition },
	{ "DrawGroupInfo",                  INI::parseDrawGroupNumberDefinition },
	{ "DynamicGameLOD",                 INI::parseDynamicGameLODDefinition },
	{ "EvaEvent",                       INI::parseEvaEvent },
	{ "FXList",                         INI::parseFXListDefinition },
	{ "GameData",                       INI::parseGameDataDefinition },
	{ "HeaderTemplate",                 INI::parseHeaderTemplateDefinition },
	{ "InGameUI",                       INI::parseInGameUIDefinition },
	{ "LODPreset",                      INI::parseLODPreset },
	{ "Language",                       INI::parseLanguageDefinition },
	{ "Locomotor",                      INI::parseLocomotorTemplateDefinition },
	{ "MapCache",                       INI::parseMapCacheDefinition },
	{ "MapData",                        INI::parseMapDataDefinition },
	{ "MappedImage",                    INI::parseMappedImageDefinition },
	{ "MiscAudio",                      INI::parseMiscAudio },
	{ "Mouse",                          INI::parseMouseDefinition },
	{ "MouseCursor",                    INI::parseMouseCursorDefinition },
	{ "MultiplayerColor",               INI::parseMultiplayerColorDefinition },
	{ "MultiplayerSettings",            INI::parseMultiplayerSettingsDefinition },
	{ "MultiplayerStartingMoneyChoice", INI::parseMultiplayerStartingMoneyChoiceDefinition },
	{ "MusicTrack",                     INI::parseMusicTrackDefinition },
	{ "Object",                         INI::parseObjectDefinition },
	{ "ObjectCreationList",             INI::parseObjectCreationListDefinition },
	{ "ChildObject",                    INI::parseChildObjectDefinition },
	{ "ObjectReskin",                   INI::parseObjectReskinDefinition },
	{ "OnlineChatColors",               INI::parseOnlineChatColorDefinition },
	{ "ParticleSystem",                 INI::parseParticleSystemDefinition },
	{ "PlayerTemplate",                 INI::parsePlayerTemplateDefinition },
	{ "Rank",                           INI::parseRankDefinition },
	{ "ReallyLowMHz",                   parseReallyLowMHz },
	{ "Road",                           INI::parseTerrainRoadDefinition },
	{ "Science",                        INI::parseScienceDefinition },
	{ "ScriptAction",                   ScriptEngine::parseScriptAction },
	{ "ScriptCondition",                ScriptEngine::parseScriptCondition },
	{ "ShellMenuScheme",                INI::parseShellMenuSchemeDefinition },
	{ "SpecialPower",                   INI::parseSpecialPowerDefinition },
	{ "StaticGameLOD",                  INI::parseStaticGameLODDefinition },
	{ "Terrain",                        INI::parseTerrainDefinition },
	{ "Upgrade",                        INI::parseUpgradeDefinition },
	{ "Video",                          INI::parseVideoDefinition },
	{ "WaterSet",                       INI::parseWaterSettingDefinition },
	{ "WaterTransparency",              INI::parseWaterTransparencyDefinition },
	{ "Weapon",                         INI::parseWeaponTemplateDefinition },
	{ "Weather",                        INI::parseWeatherDefinition },
	{ "WebpageURL",                     INI::parseWebpageURLDefinition },
	{ "WindowTransition",               INI::parseWindowTransitions },
};


///////////////////////////////////////////////////////////////////////////////////////////////////
// PRIVATE FUNCTIONS //////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
Bool INI::isValidINIFilename( const char *filename )
{
	if( filename == nullptr )
		return FALSE;

	return endsWithNoCase(filename, ".ini");
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// PUBLIC FUNCTIONS ///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
INI::INI()
{

	m_readBuffer = nullptr;
	m_readBufferNext = 0;
	m_readBufferUsed = 0;
	m_lineStart = 0;
	m_filename					= "None";
	m_loadType					= INI_LOAD_INVALID;
	m_lineNum						= 0;
	m_buffer[0]					= 0;
	m_endOfFile					= FALSE;
	m_extraBlockToken = nullptr;
	m_extraBlockParse = nullptr;
#ifdef DEBUG_CRASHING
	m_curBlockStart[0]	= 0;
#endif

}

//-------------------------------------------------------------------------------------------------
UnsignedInt INI::loadFileDirectory( AsciiString fileDirName, INILoadType loadType, Xfer *pXfer, LoadFlags loadFlags )
{
	const RetailLoadScope retailLoad(fileDirName);
	// GeneralsX @feature BenderAI 20/02/2026 Debug hang investigation
	fprintf(stderr, "[INI] loadFileDirectory('%s') START\n", fileDirName.str());
	fflush(stderr);
	
	UnsignedInt filesRead = 0;

	AsciiString iniDir = fileDirName;
	AsciiString iniFile = fileDirName;

	char ext[] = ".ini";

	if (iniDir.endsWithNoCase(ext))
	{
		iniDir.truncateBy(ARRAY_SIZE(ext)-1);
	}

	if (!iniFile.endsWithNoCase(ext))
	{
		iniFile.concat(ext);
	}

	fprintf(stderr, "[INI] loadFileDirectory - checking iniFile: '%s'\n", iniFile.str());
	fflush(stderr);
	
	if (TheFileSystem->doesFileExist(iniFile.str()))
	{
		fprintf(stderr, "[INI] loadFileDirectory - loading iniFile: '%s' START\n", iniFile.str());
		fflush(stderr);
		filesRead += load(iniFile, loadType, pXfer);
		fprintf(stderr, "[INI] loadFileDirectory - loading iniFile: '%s' END\n", iniFile.str());
		fflush(stderr);
	}

	// Load any additional ini files from a "filename" directory and its subdirectories.
	fprintf(stderr, "[INI] loadFileDirectory - calling loadDirectory('%s') START\n", iniDir.str());
	fflush(stderr);
	filesRead += loadDirectory(iniDir, loadType, pXfer, loadFlags & ~LoadFlags_ExpectFileFound);
	fprintf(stderr, "[INI] loadFileDirectory - calling loadDirectory('%s') END\n", iniDir.str());
	fflush(stderr);

	// Expect to open and load at least one file.
	const Bool expectFileFound = (loadFlags & LoadFlags_ExpectFileFound) != 0;
	if (expectFileFound && filesRead == 0)
	{
		if (RetailFileLayers::enabled() && !INIAudit::isEnabled() && INIRetailTolerance::isOptionalRetailINI(fileDirName.str()))
		{
			recordSkipped("missing", fileDirName.str());
			return 0;
		}
		fprintf(stderr, "[INI] ERROR: No files read from directory '%s'\n", fileDirName.str());
		fflush(stderr);
		throw INI_CANT_OPEN_FILE;
	}

	fprintf(stderr, "[INI] loadFileDirectory('%s') END - filesRead=%d\n", fileDirName.str(), filesRead);
	fflush(stderr);
	return filesRead;
}

//-------------------------------------------------------------------------------------------------
/** Load all INI files in the specified directory (and subdirectories if indicated).
	* If we are to load subdirectories, we will load them *after* we load all the
	* files in the current directory */
//-------------------------------------------------------------------------------------------------
UnsignedInt INI::loadDirectory( AsciiString dirName, INILoadType loadType, Xfer *pXfer, LoadFlags loadFlags )
{
	const AsciiString loadPath = dirName;
	const RetailLoadScope retailLoad(loadPath);
	// GeneralsX @feature BenderAI 20/02/2026 Debug hang investigation
	fprintf(stderr, "[INI] loadDirectory('%s') START\n", dirName.str());
	fflush(stderr);
	
	UnsignedInt filesRead = 0;

	// sanity
	if( dirName.isEmpty() )
	{
		fprintf(stderr, "[INI] ERROR: Empty directory name in loadDirectory\n");
		fflush(stderr);
		throw INI_INVALID_DIRECTORY;
	}

	const Bool subdirs = (loadFlags & LoadFlags_SearchSubDirs) != 0;
	FilenameList filenameList;
	dirName.concat('\\');
	TheFileSystem->getFileListInDirectory(dirName, "*.ini", filenameList, subdirs);
	// Load the INI files in the dir now, in a sorted order.  This keeps things the same between machines
	// in a network game.
	FilenameList::const_iterator it = filenameList.begin();
	while (it != filenameList.end())
	{
		AsciiString tempname;
		tempname = (*it).str() + dirName.getLength();

		if ((tempname.find('\\') == nullptr) && (tempname.find('/') == nullptr)) {
			// this file doesn't reside in a subdirectory, load it first.
			filesRead += load( *it, loadType, pXfer );
		}
		++it;
	}

	it = filenameList.begin();
	while (it != filenameList.end())
	{
		AsciiString tempname;
		tempname = (*it).str() + dirName.getLength();

		if ((tempname.find('\\') != nullptr) || (tempname.find('/') != nullptr)) {
			filesRead += load( *it, loadType, pXfer );
		}
		++it;
	}

	// Expect to open and load at least one file.
	const Bool expectFileFound = (loadFlags & LoadFlags_ExpectFileFound) != 0;
	if (expectFileFound && filesRead == 0)
	{
		throw INI_CANT_OPEN_FILE;
	}

	return filesRead;
}

static bool readIncludedFile(const std::string& path, std::string& contents)
{
	File* file = TheFileSystem->openFile(path.c_str(), File::READ);
	if (file == nullptr)
		return false;
	const Int size = file->size();
	char* data = file->readEntireAndClose();
	contents.assign(data, size);
	delete[] data;
	return true;
}

static INIPreprocessor& getPreprocessor()
{
	static INIPreprocessor preprocessor(readIncludedFile);
	return preprocessor;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void INI::prepFile( AsciiString filename, INILoadType loadType )
{
	// if we have a file open already -- we can't do another one
	if( m_readBuffer != nullptr )
	{

		DEBUG_CRASH(( "INI::load, cannot open file '%s', file already open", filename.str() ));
		throw INI_FILE_ALREADY_OPEN;

	}

	// open the file
	File* file = TheFileSystem->openFile(filename.str(), File::READ);
	if( file == nullptr )
	{

		DEBUG_CRASH(( "INI::load, cannot open file '%s'", filename.str() ));
		throw INI_CANT_OPEN_FILE;

	}

	const Int size = file->size();
	char* raw = file->readEntireAndClose();
	const std::string text(raw, size);
	if (s_xfer != nullptr && !hashesPreprocessedLines())
	{
		INILegacyLines::forEach(std::string_view(text), [](std::string_view line) {
			s_xfer->xferUser(const_cast<char*>(line.data()), static_cast<Int>(line.size()));
		});
	}
	delete[] raw;
	prepText(filename, text, loadType);
}

void INI::prepText( AsciiString filename, const std::string& text, INILoadType loadType )
{
	if( m_readBuffer != nullptr )
	{
		DEBUG_CRASH(( "INI::load, cannot open file '%s', file already open", filename.str() ));
		throw INI_FILE_ALREADY_OPEN;
	}

	INIPreprocessor& preprocessor = getPreprocessor();
	const std::string expanded = preprocessor.process(filename.str(), text, RetailFileLayers::enabled() || INIAudit::isEnabled());
	for (const std::string& warning : preprocessor.getWarnings())
	{
		fprintf(stderr, "[INI] %s\n", warning.c_str());
	}
	fflush(stderr);
	preprocessor.clearWarnings();

	m_readBufferNext = 0;
	m_lineStart = 0;
	m_readBufferUsed = static_cast<unsigned>(expanded.size());
	m_readBuffer = new char[expanded.size() + 1];
	memcpy(m_readBuffer, expanded.data(), expanded.size());
	m_readBuffer[expanded.size()] = 0;

	// save our filename
	m_filename = filename;

	// save our load type
	m_loadType = loadType;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void INI::unPrepFile()
{
	// delete the buffer
	delete[] m_readBuffer;
	m_readBuffer = nullptr;
	m_readBufferNext = 0;
	m_readBufferUsed = 0;

	m_filename = "None";
	m_loadType = INI_LOAD_INVALID;
	m_lineNum = 0;
	m_endOfFile = FALSE;
	s_xfer = nullptr;
	m_extraBlockToken = nullptr;
	m_extraBlockParse = nullptr;
}

//-------------------------------------------------------------------------------------------------
static INIBlockParse findBlockParse(const char* token)
{
	if (!INIAudit::isEnabled() && RetailFileLayers::enabled() && strcmp(token, "PlayerAIType") == 0)
		return BfmeMapScripts::parsePlayerAIType;
	for (size_t i = 0; i < ARRAY_SIZE(theTypeTable); ++i)
	{
		if (strcmp(theTypeTable[i].token, token) == 0)
		{
			return theTypeTable[i].parse;
		}
	}

	return nullptr;
}

//-------------------------------------------------------------------------------------------------
static const FieldParse* findFieldParse(const FieldParse* parseTable, const char* token)
{
	const FieldParse* parse = parseTable;
	for (; parse->token; ++parse)
	{
		if (strcmp( parse->token, token ) == 0)
			return parse;
	}
	return parse->parse ? parse : nullptr;
}

//-------------------------------------------------------------------------------------------------
/** Load and parse an INI file */
//-------------------------------------------------------------------------------------------------
UnsignedInt INI::load( AsciiString filename, INILoadType loadType, Xfer *pXfer )
{
	const RetailLoadScope retailLoad(filename);
	// GeneralsX @feature BenderAI 20/02/2026 Debug hang investigation
	fprintf(stderr, "[INI] load('%s') START\n", filename.str());
	fflush(stderr);
	
	setFPMode(); // so we have consistent Real values for GameLogic -MDC

	s_xfer = pXfer;
	fprintf(stderr, "[INI] load - calling prepFile('%s') START\n", filename.str());
	fflush(stderr);
	prepFile(filename, loadType);
	fprintf(stderr, "[INI] load - prepFile completed\n");
	fflush(stderr);

	return parsePreparedFile(filename);
}

UnsignedInt INI::loadText( AsciiString name, const char *text, INILoadType loadType,
	const char *extraBlockToken, INIBlockParse extraBlockParse )
{
	setFPMode();

	s_xfer = nullptr;
	prepText(name, text ? std::string(text) : std::string(), loadType);
	m_extraBlockToken = extraBlockToken;
	m_extraBlockParse = extraBlockParse;
	return parsePreparedFile(name);
}

static Bool auditBlockRanges()
{
	static const char* enabled = std::getenv("GENERALSX_INI_AUDIT_RANGES");
	return enabled && *enabled;
}

UnsignedInt INI::parsePreparedFile( AsciiString filename )
{
	try
	{

		// read all lines in the file
		DEBUG_ASSERTCRASH( m_endOfFile == FALSE, ("INI::load, EOF at the beginning!") );
		UnsignedInt lineCount = 0;
		Bool resync = FALSE;
		const Bool audit = INIAudit::isEnabled();
		const Bool retail = RetailFileLayers::enabled() && !audit;
		const Bool traceRanges = audit && auditBlockRanges();
		while( m_endOfFile == FALSE )
		{
			lineCount++;
			if ((lineCount % 100) == 0) {
				fprintf(stderr, "[INI] load - processed %d lines\n", lineCount);
				fflush(stderr);
			}
			
			// read this line
			readLine();

			AsciiString currentLine = m_buffer;
			const Bool atLineStart = m_buffer[0] != ' ';

			// the first word is the type of data we're processing
			const char *token = strtok( m_buffer, getSeps() );
			if( token )
			{
				INIBlockParse parse = findBlockParse(token);
				if (parse == nullptr && m_extraBlockToken && strcmp(m_extraBlockToken, token) == 0)
					parse = m_extraBlockParse;
				if (resync)
				{
					if (parse == nullptr || !atLineStart)
						continue;
					resync = FALSE;
				}
				const UnsignedInt blockLine = getLineNum();
				if (parse)
				{
					#ifdef DEBUG_CRASHING
					static_assert(ARRAY_SIZE(m_curBlockStart) >= ARRAY_SIZE(m_buffer), "Incorrect array size");
					strcpy(m_curBlockStart, m_buffer);
					#endif
					const std::string blockName = retail ? token : "";
					try {
						INIAudit::Scope auditScope(currentLine.str());
						(*parse)( this );
					} catch (const INIException& error) {
						if (!audit && !retail)
							throw;
						if (audit)
							INIAudit::recordWithContext("error", currentLine.str(), error.mFailureMessage ? error.mFailureMessage : "", m_filename.str(), blockLine);
						else
							recordSkipped("error", blockName.c_str());
						resync = TRUE;
					} catch (...) {
						if (audit)
						{
							INIAudit::recordWithContext("error", currentLine.str(), "block parse failed", m_filename.str(), blockLine);
							resync = TRUE;
						}
						else if (retail && isINIParseError())
						{
							recordSkipped("error", blockName.c_str());
							resync = TRUE;
						}
						else
						{
							DEBUG_CRASH(("Error parsing block '%s' in INI file '%s'", token, m_filename.str()) );
							char buff[1024];
							snprintf(buff, ARRAY_SIZE(buff), "Error parsing INI file '%s' (Line: '%s')\n",
								m_filename.str(), currentLine.str());

							throw INIException(buff);
						}
					}
					if ((audit || retail) && blockLine == getLineNum())
					{
						INIBlockStructure::LineHead head;
						if (INIBlockStructure::parseLineHead(currentRawLineView(), head) &&
							!INIBlockStructure::isSingleLineTopLevel(head.keyword))
							skipBlock(nullptr);
						resync = FALSE;
					}
					#ifdef DEBUG_CRASHING
						strcpy(m_curBlockStart, "NO_BLOCK");
					#endif
				}
				else if (audit || retail)
				{
					recordSkipped("block", token);
					INIBlockStructure::LineHead head;
					if (INIBlockStructure::parseLineHead(currentRawLineView(), head) &&
						!INIBlockStructure::isEnd(head) && !INIBlockStructure::isSingleLineTopLevel(head.keyword))
						skipBlock(token);
				}
				else
				{
					DEBUG_CRASH( ("[LINE: %d - FILE: '%s'] Unknown block '%s'",
														 getLineNum(), getFilename().str(), token ) );
					throw INI_UNKNOWN_TOKEN;
				}

				if (traceRanges)
					INIAudit::recordWithContext("block-range", currentLine.str(), std::to_string(getLineNum()), m_filename.str(), blockLine);
			}

		}
		fprintf(stderr, "[INI] load - processed total %d lines\n", lineCount);
		fflush(stderr);
	}
	catch (...)
	{
		fprintf(stderr, "[INI] ERROR in load('%s') - exception caught\n", filename.str());
		fflush(stderr);
		unPrepFile();

		// propagate the exception.
		throw;
	}

	unPrepFile();

	fprintf(stderr, "[INI] load('%s') END\n", filename.str());
	fflush(stderr);
	return 1;
}

void INI::recordSkipped(const char* kind, const char* detail) const
{
	if (INIAudit::isEnabled())
		INIAudit::record(kind, detail, m_filename.str(), getLineNum());
	else if (RetailFileLayers::enabled())
		++retailSkips[{kind, detail}];
}

void INI::finishRetailStartupLoad()
{
	printRetailSkips();
	retailStartupLoadFinished = true;
}

Bool INI::skipBlockWithoutOwner(const void* owner, const char* blockType)
{
	if (owner != nullptr || (!INIAudit::isEnabled() && !RetailFileLayers::enabled()))
		return FALSE;
	if (INIAudit::isEnabled())
		INIAudit::record("order", std::string(blockType) + " parsed before the subsystem that owns it exists", m_filename.str(), getLineNum());
	else
		recordSkipped("order", blockType);
	skipBlock(blockType);
	return TRUE;
}

std::string_view INI::currentRawLineView() const
{
	if (m_readBuffer == nullptr || m_readBufferNext < m_lineStart)
		return {};
	std::string_view line(m_readBuffer + m_lineStart, m_readBufferNext - m_lineStart);
	while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
		line.remove_suffix(1);
	return line;
}

std::string INI::currentRawLine() const
{
	return std::string(currentRawLineView());
}

std::string INI::readRawBlock(const char* endToken, const char* const* nestedBlocks)
{
	std::string body;
	Int depth = 0;
	while (!isEOF())
	{
		readLine();
		const char* token = strtok(m_buffer, getSeps());
		if (token && stricmp(token, endToken) == 0 && !getNextTokenOrNull())
		{
			if (depth == 0)
				return body;
			--depth;
		}
		else if (token && nestedBlocks)
		{
			for (const char* const* block = nestedBlocks; *block; ++block)
			{
				if (stricmp(token, *block) == 0)
				{
					++depth;
					break;
				}
			}
		}
		body += currentRawLineView();
		body += '\n';
	}
	throw INI_MISSING_END_TOKEN;
}

Bool INI::readBlockBody(const char* enclosing, std::string* body)
{
	INIBlockStructure::BlockEnd block(enclosing);
	while (!isEOF())
	{
		readLine();
		const auto line = currentRawLineView();
		if (block.consume(line))
			return TRUE;
		if (body)
		{
			*body += line;
			*body += '\n';
		}
	}
	return FALSE;
}

std::string INI::readStructuredBlock(const char* enclosing)
{
	std::string body;
	if (!readBlockBody(enclosing, &body))
		throw INI_MISSING_END_TOKEN;
	return body;
}

void INI::skipBlock(const char* enclosing)
{
	const auto header = currentRawLineView();
	INIBlockStructure::LineHead head{};
	INIBlockStructure::parseLineHead(header, head);
	const std::string keyword(enclosing ? std::string_view(enclosing) : head.keyword);
	const UnsignedInt headerLine = getLineNum();
	if (readBlockBody(keyword.c_str(), nullptr))
	{
		if (INIAudit::isEnabled() && auditBlockRanges())
			INIAudit::recordWithContext("skip-range", std::string(header), std::to_string(getLineNum()), m_filename.str(), headerLine);
		return;
	}

	if (INIAudit::isEnabled())
		INIAudit::record("missing-end", std::string(header), m_filename.str(), headerLine);
	else
		recordSkipped("missing-end", keyword.c_str());
}

//-------------------------------------------------------------------------------------------------
/** Read a line from the already open file.  Any comments will be removed and
	* therefore ignored from any given line
	* 
	* TheSuperHackers @performance xezon 18/01/2026 The file contents are now read directly from a
	* full File Ram buffer into the INI Line Buffer without a third buffer in between.
	*/
//-------------------------------------------------------------------------------------------------
void INI::readLine()
{
	// sanity
	DEBUG_ASSERTCRASH( m_readBuffer, ("readLine(), read buffer is null") );

	if (m_endOfFile)
	{
		*m_buffer = 0;
	}
	else
	{
		m_lineStart = m_readBufferNext;
		// read up till the newline or semicolon character, or until out of space
		char *p = m_buffer;
		while (p != m_buffer+INI_MAX_CHARS_PER_LINE)
		{
			// test end of read buffer
			if (m_readBufferNext==m_readBufferUsed)
			{
				m_endOfFile = true;
				*p = 0;
				break;
			}

			// get next character
			*p = m_readBuffer[m_readBufferNext++];

			// check for new line
			if (*p == '\n')
			{
				*p = 0;
				break;
			}

			DEBUG_ASSERTCRASH(*p != '\t', ("tab characters are not allowed in INI files (%s). please check your editor settings. Line Number %d", m_filename.str(), getLineNum()));

			// if this is a semicolon, that represents the start of a comment
			if (*p == ';')
			{
				*p = 0;
			}

			// make whitespace characters actual spaces
			else if (*p > 0 && *p < 32)
			{
				*p = ' ';
			}

			p++;
		}

		*p = 0;

		// increase our line count
		m_lineNum++;

		// check for at the max
		if ( p == m_buffer+INI_MAX_CHARS_PER_LINE )
		{
			DEBUG_CRASH( ("Buffer too small (%d) and was truncated, increase INI_MAX_CHARS_PER_LINE", INI_MAX_CHARS_PER_LINE) );
		}
	}

	if (s_xfer && hashesPreprocessedLines())
	{
		s_xfer->xferUser( m_buffer, sizeof( char ) * strlen( m_buffer ) );
		//DEBUG_LOG(("Xfer val is now 0x%8.8X in %s, line %s", ((XferCRC *)s_xfer)->getCRC(), m_filename.str(), m_buffer));
	}
}

//-------------------------------------------------------------------------------------------------
/** Parse UnsignedByte from buffer and assign at location 'store' */
//-------------------------------------------------------------------------------------------------
void INI::parseUnsignedByte( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	const char *token = ini->getNextToken();
	Int value = scanInt(token);
	if (value < 0 || value > 255)
	{
		DEBUG_CRASH(("Bad value INI::parseUnsignedByte"));
		throw ERROR_BUG;
	}
	*(Byte *)store = (Byte)value;
}

//-------------------------------------------------------------------------------------------------
/** Parse signed short from buffer and assign at location 'store' */
//-------------------------------------------------------------------------------------------------
void INI::parseShort( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	const char *token = ini->getNextToken();
	Int value = scanInt(token);
	if (value < -32768 || value > 32767)
	{
		DEBUG_CRASH(("Bad value INI::parseShort"));
		throw ERROR_BUG;
	}
	*(Short *)store = (Short)value;
}

//-------------------------------------------------------------------------------------------------
/** Parse unsigned short from buffer and assign at location 'store' */
//-------------------------------------------------------------------------------------------------
void INI::parseUnsignedShort( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	const char *token = ini->getNextToken();
	Int value = scanInt(token);
	if (value < 0 || value > 65535)
	{
		DEBUG_CRASH(("Bad value INI::parseUnsignedShort"));
		throw ERROR_BUG;
	}
	*(UnsignedShort *)store = (UnsignedShort)value;
}

//-------------------------------------------------------------------------------------------------
/** Parse integer from buffer and assign at location 'store' */
//-------------------------------------------------------------------------------------------------
void INI::parseInt( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	const char *token = ini->getNextToken();
	*(Int *)store = scanInt(token);

}

//-------------------------------------------------------------------------------------------------
/** Parse unsigned integer from buffer and assign at location 'store' */
//-------------------------------------------------------------------------------------------------
void INI::parseUnsignedInt( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	const char *token = ini->getNextToken();
	*(UnsignedInt *)store = scanUnsignedInt(token);

}

//-------------------------------------------------------------------------------------------------
/** Parse real from buffer and assign at location 'store' */
//-------------------------------------------------------------------------------------------------
void INI::parseReal( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	const char *token = ini->getNextToken();
	*(Real *)store = scanReal(token);

}

//-------------------------------------------------------------------------------------------------
/** Parse real from buffer and assign at location 'store' */
//-------------------------------------------------------------------------------------------------
void INI::parsePositiveNonZeroReal( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	const char *token = ini->getNextToken();
	*(Real *)store = scanReal(token);
	if (*(Real *)store <= 0.0f)
	{
		DEBUG_CRASH(("invalid Real value %f -- expected > 0",*(Real*)store));
		throw INI_INVALID_DATA;
	}

}

//-------------------------------------------------------------------------------------------------
/** Parse a degree value (0 to 360) and store the radian value of that degree
	* in a Real */
//-------------------------------------------------------------------------------------------------
void INI::parseAngleReal( INI *ini, void * /*instance*/,
																			void *store, const void *userData )
{
	const char *token = ini->getNextToken();

	const Real RADS_PER_DEGREE = PI / 180.0f;
	*(Real *)store = scanReal( token ) * RADS_PER_DEGREE;

}

//-------------------------------------------------------------------------------------------------
/** Parse an angular velocity in degrees-per-sec and store the rads-per-frame value of that degree
	* in a Real */
//-------------------------------------------------------------------------------------------------
void INI::parseAngularVelocityReal( INI *ini, void * /*instance*/,
																			void *store, const void *userData )
{
	const char *token = ini->getNextToken();

	// scan the int and convert to radian and store as a real
	*(Real *)store = ConvertAngularVelocityInDegreesPerSecToRadsPerFrame(scanReal( token ));

}

//-------------------------------------------------------------------------------------------------
/** Parse Bool from buffer and assign at location 'store'.  The buffer token must
	* be in the form of a string "Yes" or "No" (case is ignored) */
//-------------------------------------------------------------------------------------------------
void INI::parseBool( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	*(Bool*)store = INI::scanBool(ini->getNextToken());
}

//-------------------------------------------------------------------------------------------------
/** Parse Bool from buffer; if true, or in MASK, otherwise and out MASK. The buffer token must
	* be in the form of a string "Yes" or "No" (case is ignored) */
//-------------------------------------------------------------------------------------------------
void INI::parseBitInInt32( INI *ini, void *instance, void *store, const void* userData )
{
	UnsignedInt* s = (UnsignedInt*)store;
	// GeneralsX @build BenderAI 12/02/2026 64-bit safe pointer cast
	UnsignedInt mask = static_cast<UnsignedInt>(reinterpret_cast<uintptr_t>(userData));

	if (INI::scanBool(ini->getNextToken()))
		*s |= mask;
	else
		*s &= ~mask;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
/*static*/ Bool INI::scanBool(const char* token)
{
	// translate string yes/no into TRUE/FALSE
	if( stricmp( token, "yes" ) == 0 )
		return TRUE;
	else if( stricmp( token, "no" ) == 0 )
		return FALSE;
	else
	{
		DEBUG_CRASH(("invalid boolean token %s -- expected Yes or No",token));
		throw INI_INVALID_DATA;
		return false;	// keep compiler happy
	}

}

//-------------------------------------------------------------------------------------------------
/** Parse an *ASCII* string from buffer and assign at location 'store' */
//-------------------------------------------------------------------------------------------------
void INI::parseAsciiString( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	AsciiString* asciiString = (AsciiString *)store;
	*asciiString = ini->getNextAsciiString();
}

//-------------------------------------------------------------------------------------------------
/** Parse an *ASCII* string from buffer and assign at location 'store'. Has better support for quoted strings.
We don't really need this function, but parseString() is broken and we want to leave it broken to
maintain existing code.
 */
//-------------------------------------------------------------------------------------------------
void INI::parseQuotedAsciiString( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	AsciiString* asciiString = (AsciiString *)store;
	*asciiString = ini->getNextQuotedAsciiString();
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void INI::parseAsciiStringVector( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	std::vector<AsciiString>* asv = (std::vector<AsciiString>*)store;
	asv->clear();
	for (const char *token = ini->getNextTokenOrNull(); token; token = ini->getNextTokenOrNull())
	{
		asv->push_back(token);
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void INI::parseAsciiStringVectorAppend( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	std::vector<AsciiString>* asv = (std::vector<AsciiString>*)store;
	// nope, don't clear. duh.
	// asv->clear();
	for (const char *token = ini->getNextTokenOrNull(); token; token = ini->getNextTokenOrNull())
	{
		asv->push_back(token);
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
/* static */void INI::parseScienceVector( INI *ini, void * /*instance*/, void *store, const void *userData )
{
	ScienceVec* asv = (ScienceVec*)store;
	asv->clear();
	for (const char *token = ini->getNextTokenOrNull(); token; token = ini->getNextTokenOrNull())
	{
		if (stricmp(token, "None") == 0)
		{
			asv->clear();
			return;
		}
		asv->push_back(INI::scanScience( token ));
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
AsciiString INI::getNextQuotedAsciiString()
{
	AsciiString result;
	char buff[INI_MAX_CHARS_PER_LINE];
	buff[0] = '\0';

	const char *token = getNextTokenOrNull();	// if null, just leave an empty string
	if (token != nullptr)
	{
		if (token[0] != '\"')
		{
			// if token is simply "
			result.set( token );	// Start following the "
		}
		else
		{	int strLen=0;
			Bool done=FALSE;
			if ((strLen=strlen(token)) > 1)
			{
				strlcpy(buff, &token[1], ARRAY_SIZE(buff));	//skip the starting quote
				//Check for end of quoted string.  Checking here fixes cases where quoted string on same line with other data.
				if (buff[strLen-2]=='"')	//skip ending quote if present
				{	buff[strLen-2]='\0';
					done=TRUE;
				}
			}

			if (!done)
			{
				token = getNextToken(getSepsQuote());

				if (strlen(token) > 1 && token[1] != '\t')
				{
					strlcat(buff, " ", ARRAY_SIZE(buff));
					strlcat(buff, token, ARRAY_SIZE(buff));
				}
				else
				{	Int buflen=strlen(buff);
					if (buff[buflen-1]=='\"')
						buff[buflen-1]='\0';
				}
			}
			result.set(buff);
		}
	}
	return result;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
AsciiString INI::getNextAsciiString()
{
	AsciiString result;

	const char *token = getNextTokenOrNull();	// if null, just leave an empty string
	if (token != nullptr)
	{
		if (token[0] != '\"')
		{
			// if token is simply "
			result.set( token );	// Start following the "
		}
		else
		{
			static char buff[INI_MAX_CHARS_PER_LINE];
			buff[0] = 0;
			if (strlen(token) > 1)
			{
				strlcpy(buff, &token[1], ARRAY_SIZE(buff));
			}

			token = getNextTokenOrNull(getSepsQuote());
			if (token) {
				if (strlen(token) > 1 && token[1] != '\t')
				{
					strlcat(buff, " ", ARRAY_SIZE(buff));
				}
				strlcat(buff, token, ARRAY_SIZE(buff));
				result.set(buff);
			} else {
				Int len = strlen(buff);
				if (len && buff[len-1] == '"') {
					// strip off trailing quote jba. [2/12/2003]
					buff[len-1] = 0;
				}
				result.set(buff);
			}
		}
	}
	return result;
}

//-------------------------------------------------------------------------------------------------
/** Parse a string label, get the *translated* actual text from the label and store
	* into a *UNICODE* string. */
//-------------------------------------------------------------------------------------------------
void INI::parseAndTranslateLabel( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	const char *token = ini->getNextToken();

	// translate
	UnicodeString translated = TheGameText->fetch( token );
	if( translated.isEmpty() )
		throw INI_INVALID_DATA;

	// save the translated text
	UnicodeString *theString = (UnicodeString *)store;
	theString->set( translated.str() );

}

//-------------------------------------------------------------------------------------------------
/** Parse a string label assumed as an image as part of the image collection.  Translate
	* to an image pointer for storage */
//-------------------------------------------------------------------------------------------------
void INI::parseMappedImage( INI *ini, void * /*instance*/, void *store, const void *userData )
{
	const char *token = ini->getNextToken();

	if( TheMappedImageCollection )
	{
		typedef const Image* ConstImagePtr;
		*(ConstImagePtr*)store = TheMappedImageCollection->findImageByName( token );
	}

	//KM: If we are in the worldbuilder, we want to parse commandbuttons for informational purposes,
	//but we don't care about the images -- because we never access them. In RTS/GUIEdit, they always
	//exist -- and in those cases, it will never call this code anyways because it'll throw long before.
	//else
	//	throw INI_UNKNOWN_ERROR;

}

// ------------------------------------------------------------------------------------------------
/** Parse a string label assumed as a Anim2D template name.  Translate that name to an
	* actual template pointer for storage */
// ------------------------------------------------------------------------------------------------
/*static*/ void INI::parseAnim2DTemplate( INI *ini, void *instance, void *store, const void *userData )
{
	const char *token = ini->getNextToken();

	if( TheAnim2DCollection )
	{
		Anim2DTemplate **anim2DTemplate = (Anim2DTemplate **)store;
		*anim2DTemplate = TheAnim2DCollection->findTemplate( AsciiString( token ) );
	}
	else
	{

		DEBUG_CRASH(( "INI::parseAnim2DTemplate - TheAnim2DCollection is null" ));
		throw INI_UNKNOWN_ERROR;

	}

}

//-------------------------------------------------------------------------------------------------
/** Parse a percent in int or real form such as "23%" or "95.4%" and assign
	* to location 'store' as a number from 0.0 to 1.0 */
//-------------------------------------------------------------------------------------------------
void INI::parsePercentToReal( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	const char *token = ini->getNextToken(ini->getSepsPercent());
	Real *theReal = (Real *)store;
	*theReal = scanPercentToReal(token);

}

//-------------------------------------------------------------------------------------------------
/** 'store' points to an 32 bit unsigned integer.  We will zero that integer, parse each token
	* in the buffer, if the token is in the userData table of strings, we will set the
	* according bit flag for it */
//-------------------------------------------------------------------------------------------------
void INI::parseBitString8( INI* ini, void * /*instance*/, void *store, const void* userData )
{
	UnsignedInt tmp = *(Byte*)store;
	INI::parseBitString32(ini, nullptr, &tmp, userData);
	if (tmp & 0xffffff00)
	{
		DEBUG_CRASH(("Bad bitstring list INI::parseBitString8"));
		throw ERROR_BUG;
	}
	*(Byte*)store = (Byte)tmp;
}

//-------------------------------------------------------------------------------------------------
/** 'store' points to an 32 bit unsigned integer.  We will zero that integer, parse each token
	* in the buffer, if the token is in the userData table of strings, we will set the
	* according bit flag for it */
//-------------------------------------------------------------------------------------------------
void INI::parseBitString32( INI* ini, void * /*instance*/, void *store, const void* userData )
{
	ConstCharPtrArray flagList = (ConstCharPtrArray)userData;
	UnsignedInt *bits = (UnsignedInt *)store;

	if( flagList == nullptr || flagList[ 0 ] == nullptr)
	{
		DEBUG_ASSERTCRASH( flagList, ("INTERNAL ERROR! parseBitString32: No flag list provided!") );
		throw INI_INVALID_NAME_LIST;
	}

	Bool foundNormal = false;
	Bool foundAddOrSub = false;

	// loop through all tokens
	for (const char *token = ini->getNextTokenOrNull(); token; token = ini->getNextTokenOrNull())
	{
		if (stricmp(token, "NONE") == 0)
		{
			if (foundNormal || foundAddOrSub)
			{
				DEBUG_CRASH(("you may not mix normal and +- ops in bitstring lists"));
				throw INI_INVALID_NAME_LIST;
			}
			*bits = 0;
			break;
		}

		if (token[0] == '+')
		{
			if (foundNormal)
			{
				DEBUG_CRASH(("you may not mix normal and +- ops in bitstring lists"));
				throw INI_INVALID_NAME_LIST;
			}
			Int bitIndex = INI::scanIndexList(token+1, flagList);	// this throws if the token is not found
			*bits |= (1 << bitIndex);
			foundAddOrSub = true;
		}
		else if (token[0] == '-')
		{
			if (foundNormal)
			{
				DEBUG_CRASH(("you may not mix normal and +- ops in bitstring lists"));
				throw INI_INVALID_NAME_LIST;
			}
			Int bitIndex = INI::scanIndexList(token+1, flagList);	// this throws if the token is not found
			*bits &= ~(1 << bitIndex);
			foundAddOrSub = true;
		}
		else
		{
			if (foundAddOrSub)
			{
				DEBUG_CRASH(("you may not mix normal and +- ops in bitstring lists"));
				throw INI_INVALID_NAME_LIST;
			}

			if (!foundNormal)
				*bits = 0;

			Int bitIndex = INI::scanIndexList(token, flagList);	// this throws if the token is not found
			*bits |= (1 << bitIndex);
			foundNormal = true;
		}
	}
}

//-------------------------------------------------------------------------------------------------
/** Parse a color in the form of
	*
	* RGB_COLOR = R:100 G:114 B:245
	* and store in "RGBColor" structure pointed to by 'store' */
//-------------------------------------------------------------------------------------------------
void INI::parseRGBColor( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	const char* names[3] = { "R", "G", "B" };
	Int colors[3];
	for( Int i = 0; i < 3; i++ )
	{
		colors[i] = scanInt(ini->getNextSubToken(names[i]));
		if( colors[ i ] < 0 )
			throw INI_INVALID_DATA;
		if( colors[ i ] > 255 )
			throw INI_INVALID_DATA;
	}

	// assign the color components to the "RGBColor" pointer at 'store'
	RGBColor *theColor = (RGBColor *)store;
	theColor->red		= (Real)colors[ 0 ] / 255.0f;
	theColor->green = (Real)colors[ 1 ] / 255.0f;
	theColor->blue	= (Real)colors[ 2 ] / 255.0f;

}

//-------------------------------------------------------------------------------------------------
/** Parse a color in the form of
	*
	* RGB_COLOR = R:100 G:114 B:245 [A:233]
	* and store in "RGBAColorInt" structure pointed to by 'store' */
//-------------------------------------------------------------------------------------------------
void INI::parseRGBAColorInt( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	const char* names[4] = { "R", "G", "B", "A" };
	Int colors[4];
	for( Int i = 0; i < 4; i++ )
	{
		const char* token = ini->getNextTokenOrNull(ini->getSepsColon());
		if (token == nullptr)
		{
			if (i < 3)
			{
				throw INI_INVALID_DATA;
			}
			else
			{
				// it's ok for A to be omitted.
				colors[i] = 255;
			}
		}
		else
		{
			// if present, the token must match.
			if (stricmp(token, names[i]) != 0)
			{
				throw INI_INVALID_DATA;
			}
			colors[i] = scanInt(ini->getNextToken(ini->getSepsColon()));
		}
		if( colors[ i ] < 0 )
			throw INI_INVALID_DATA;
		if( colors[ i ] > 255 )
			throw INI_INVALID_DATA;
	}

	//
	// assign the color components to the "RGBColorInt" pointer at 'store', keep
	// the numbers as between 0 and 255
	//
	RGBAColorInt *theColor = (RGBAColorInt *)store;
	theColor->red		= colors[ 0 ];
	theColor->green = colors[ 1 ];
	theColor->blue	= colors[ 2 ];
	theColor->alpha = colors[ 3 ];

}

//-------------------------------------------------------------------------------------------------
/** Parse a color in the form of
	*
	* RGB_COLOR = R:100 G:114 B:245 [A:233]
	* and store in "Color" structure pointed to by 'store' */
//-------------------------------------------------------------------------------------------------
void INI::parseColorInt( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	const char* names[4] = { "R", "G", "B", "A" };
	Int colors[4];
	for( Int i = 0; i < 4; i++ )
	{
		const char* token = ini->getNextTokenOrNull(ini->getSepsColon());
		if (token == nullptr)
		{
			if (i < 3)
			{
				throw INI_INVALID_DATA;
			}
			else
			{
				// it's ok for A to be omitted.
				colors[i] = 255;
			}
		}
		else
		{
			// if present, the token must match.
			if (stricmp(token, names[i]) != 0)
			{
				throw INI_INVALID_DATA;
			}
			colors[i] = scanInt(ini->getNextToken(ini->getSepsColon()));
		}
		if( colors[ i ] < 0 )
			throw INI_INVALID_DATA;
		if( colors[ i ] > 255 )
			throw INI_INVALID_DATA;
	}

	//
	// assign the color components to the "Color" pointer at 'store', keep
	// the numbers as between 0 and 255
	//
	Color *theColor = (Color *)store;
	*theColor = GameMakeColor(colors[0], colors[1], colors[2], colors[3]);

}

//-------------------------------------------------------------------------------------------------
/** Parse a 3D coordinate of reals in the form of:
	* FIELD_NAME = X:400 Y:-214.3 Z:8.6 */
//-------------------------------------------------------------------------------------------------
void INI::parseCoord3D( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	Coord3D *theCoord = (Coord3D *)store;

	theCoord->x = scanReal(ini->getNextSubToken("X"));
	theCoord->y = scanReal(ini->getNextSubToken("Y"));
	theCoord->z = scanReal(ini->getNextSubToken("Z"));

}

//-------------------------------------------------------------------------------------------------
/** Parse a 2D coordinate of reals in the form of:
	* FIELD_NAME = X:400 Y:-214.3 */
//-------------------------------------------------------------------------------------------------
void INI::parseCoord2D( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	Coord2D *theCoord = (Coord2D *)store;

	theCoord->x = scanReal(ini->getNextSubToken("X"));
	theCoord->y = scanReal(ini->getNextSubToken("Y"));

}

//-------------------------------------------------------------------------------------------------
/** Parse a 2D coordinate of Ints in the form of:
	* FIELD_NAME = X:400 Y:-214 */
//-------------------------------------------------------------------------------------------------
void INI::parseICoord2D( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	ICoord2D *theCoord = (ICoord2D *)store;

	theCoord->x = scanInt(ini->getNextSubToken("X"));
	theCoord->y = scanInt(ini->getNextSubToken("Y"));

}

//-------------------------------------------------------------------------------------------------
/** Parse an audio event and assign to the 'AudioEventRTS*' at store */
//-------------------------------------------------------------------------------------------------
void INI::parseDynamicAudioEventRTS( INI *ini, void * /*instance*/, void *store, const void* userData )
{
	const char *token = ini->getNextToken();
	RefCountPtr<DynamicAudioEventRTS>* theSound = (RefCountPtr<DynamicAudioEventRTS>*)store;

	// translate the string into a sound
	if (stricmp(token, "NoSound") == 0)
	{
		theSound->Clear();
	}
	else
	{
		if (*theSound == nullptr)
			*theSound = Create_No_Add_Ref(newInstance(DynamicAudioEventRTS));
		(*theSound)->setEventName(AsciiString(token));
	}

	if (*theSound)
		TheAudio->getInfoForAudioEvent(theSound->Peek());
}

//-------------------------------------------------------------------------------------------------
/** Parse an audio event and assign to the 'AudioEventRTS*' at store */
//-------------------------------------------------------------------------------------------------
void INI::parseAudioEventRTS( INI *ini, void * /*instance*/, void *store, const void* userData )
{
	const char *token = ini->getNextToken();

	AudioEventRTS *theSound = (AudioEventRTS*)store;

	// translate the string into a sound
	if (stricmp(token, "NoSound") != 0) {
		theSound->setEventName(AsciiString(token));
	}

	TheAudio->getInfoForAudioEvent(theSound);
}

//-------------------------------------------------------------------------------------------------
/** Parse an ThingTemplate and assign to the 'ThingTemplate *' at store */
//-------------------------------------------------------------------------------------------------
void INI::parseThingTemplate( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	const char *token = ini->getNextToken();

	if (!TheThingFactory)
	{
		DEBUG_CRASH(("TheThingFactory not inited yet"));
		throw ERROR_BUG;
	}

	typedef const ThingTemplate *ConstThingTemplatePtr;
	ConstThingTemplatePtr* theThingTemplate = (ConstThingTemplatePtr*)store;

	if (stricmp(token, "None") == 0)
	{
		*theThingTemplate = nullptr;
	}
	else
	{
		const ThingTemplate *tt = TheThingFactory->findTemplate(token);	// could be null!
		DEBUG_ASSERTCRASH(tt, ("ThingTemplate %s not found!",token));
		// assign it, even if null!
		*theThingTemplate = tt;
	}

}

//-------------------------------------------------------------------------------------------------
/** Parse an ArmorTemplate and assign to the 'ArmorTemplate *' at store */
//-------------------------------------------------------------------------------------------------
void INI::parseArmorTemplate( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	const char *token = ini->getNextToken();

	typedef const ArmorTemplate *ConstArmorTemplatePtr;
	ConstArmorTemplatePtr* theArmorTemplate = (ConstArmorTemplatePtr*)store;

	if (stricmp(token, "None") == 0)
	{
		*theArmorTemplate = nullptr;
	}
	else
	{
		const ArmorTemplate *tt = TheArmorStore->findArmorTemplate(token);	// could be null!
		DEBUG_ASSERTCRASH(tt, ("ArmorTemplate %s not found!",token));
		// assign it, even if null!
		*theArmorTemplate = tt;
	}

}

//-------------------------------------------------------------------------------------------------
/** Parse an WeaponTemplate and assign to the 'WeaponTemplate *' at store */
//-------------------------------------------------------------------------------------------------
void INI::parseWeaponTemplate( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	const char *token = ini->getNextToken();

	typedef const WeaponTemplate *ConstWeaponTemplatePtr;
	ConstWeaponTemplatePtr* theWeaponTemplate = (ConstWeaponTemplatePtr*)store;

	const WeaponTemplate *tt = TheWeaponStore->findWeaponTemplate(token);	// could be null!
	DEBUG_ASSERTCRASH(tt || stricmp(token, "None") == 0, ("WeaponTemplate %s not found!",token));
	// assign it, even if null!
	*theWeaponTemplate = tt;

}

//-------------------------------------------------------------------------------------------------
/** Parse an FXList and assign to the 'FXList *' at store */
//-------------------------------------------------------------------------------------------------
void INI::parseFXList( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	const char *token = ini->getNextToken();

	typedef const FXList *ConstFXListPtr;
	ConstFXListPtr* theFXList = (ConstFXListPtr*)store;

	const FXList *fxl = TheFXListStore->findFXList(token);	// could be null!
	DEBUG_ASSERTCRASH(fxl != nullptr || stricmp(token, "None") == 0, ("FXList %s not found!",token));
	// assign it, even if null!
	*theFXList = fxl;

}

//-------------------------------------------------------------------------------------------------
/** Parse a particle system and assign to 'ParticleSystemTemplate *' at store */
//-------------------------------------------------------------------------------------------------
void INI::parseParticleSystemTemplate( INI *ini, void * /*instance*/, void *store, const void *userData )
{
	const char *token = ini->getNextToken();

	const ParticleSystemTemplate *pSystemT = TheParticleSystemManager->findTemplate( AsciiString( token ) );
	DEBUG_ASSERTCRASH( pSystemT || stricmp( token, "None" ) == 0, ("ParticleSystem %s not found!",token) );

	typedef const ParticleSystemTemplate* ConstParticleSystemTemplatePtr;
	ConstParticleSystemTemplatePtr* theParticleSystemTemplate = (ConstParticleSystemTemplatePtr*)store;

	*theParticleSystemTemplate = pSystemT;

}

//-------------------------------------------------------------------------------------------------
/** Parse an DamageFX and assign to the 'DamageFX *' at store */
//-------------------------------------------------------------------------------------------------
void INI::parseDamageFX( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	const char *token = ini->getNextToken();

	typedef const DamageFX *ConstDamageFXPtr;
	ConstDamageFXPtr* theDamageFX = (ConstDamageFXPtr*)store;

	if (stricmp(token, "None") == 0)
	{
		*theDamageFX = nullptr;
	}
	else
	{
		const DamageFX *fxl = TheDamageFXStore->findDamageFX(token);	// could be null!
		DEBUG_ASSERTCRASH(fxl, ("DamageFX %s not found!",token));
		// assign it, even if null!
		*theDamageFX = fxl;
	}

}

//-------------------------------------------------------------------------------------------------
/** Parse an ObjectCreationList and assign to the 'ObjectCreationList *' at store */
//-------------------------------------------------------------------------------------------------
void INI::parseObjectCreationList( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	const char *token = ini->getNextToken();

	typedef const ObjectCreationList *ConstObjectCreationListPtr;
	ConstObjectCreationListPtr* theObjectCreationList = (ConstObjectCreationListPtr*)store;

	const ObjectCreationList *ocl = TheObjectCreationListStore->findObjectCreationList(token);	// could be null!
	DEBUG_ASSERTCRASH(ocl || stricmp(token, "None") == 0, ("ObjectCreationList %s not found!",token));
	// assign it, even if null!
	*theObjectCreationList = ocl;

}

//-------------------------------------------------------------------------------------------------
/** Parse a upgrade template string and store as template pointer */
//-------------------------------------------------------------------------------------------------
void INI::parseUpgradeTemplate( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	const char *token = ini->getNextToken();

	if (!TheUpgradeCenter)
	{
		DEBUG_CRASH(("TheUpgradeCenter not inited yet"));
		throw ERROR_BUG;
	}

	const UpgradeTemplate *uu = TheUpgradeCenter->findUpgrade( token );
	DEBUG_ASSERTCRASH( uu || stricmp( token, "None" ) == 0, ("Upgrade %s not found!",token) );

	typedef const UpgradeTemplate* ConstUpgradeTemplatePtr;
	ConstUpgradeTemplatePtr* theUpgradeTemplate = (ConstUpgradeTemplatePtr *)store;
	*theUpgradeTemplate = uu;
}

//-------------------------------------------------------------------------------------------------
/** Parse a special power template string and store as template pointer */
//-------------------------------------------------------------------------------------------------
void INI::parseSpecialPowerTemplate( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	const char *token = ini->getNextToken();

	if (!TheSpecialPowerStore)
	{
		DEBUG_CRASH(("TheSpecialPowerStore not inited yet"));
		throw ERROR_BUG;
	}

	const SpecialPowerTemplate *sPowerT = TheSpecialPowerStore->findSpecialPowerTemplate( AsciiString( token ) );
	if( !sPowerT && stricmp( token, "None" ) != 0 )
	{
		DEBUG_CRASH( ("[LINE: %d in '%s'] Specialpower %s not found!", ini->getLineNum(), ini->getFilename().str(), token) );
	}

	typedef const SpecialPowerTemplate* ConstSpecialPowerTemplatePtr;
	ConstSpecialPowerTemplatePtr* theSpecialPowerTemplate = (ConstSpecialPowerTemplatePtr *)store;
	*theSpecialPowerTemplate = sPowerT;
}

//-------------------------------------------------------------------------------------------------
/** Parse a science string and store as science type */
//-------------------------------------------------------------------------------------------------
/* static */void INI::parseScience( INI *ini, void * /*instance*/, void *store, const void *userData )
{
	const char *token = ini->getNextToken();

	if (!TheScienceStore)
	{
		DEBUG_CRASH(("TheScienceStore not inited yet"));
		throw ERROR_BUG;
	}

	*((ScienceType *)store) = INI::scanScience(token);

}

//-------------------------------------------------------------------------------------------------
/** Parse a single string token, check for that token in the index list
	* of names provided and store the index into that list.
	*
	* NOTE: Is is assumed that we are going to store the index into
	*				a 4 byte integer.  This works well for INT and ENUM definitions */
//-------------------------------------------------------------------------------------------------
void INI::parseIndexList( INI* ini, void * /*instance*/, void *store, const void* userData )
{
	ConstCharPtrArray nameList = (ConstCharPtrArray)userData;
	*(Int *)store = scanIndexList(ini->getNextToken(), nameList);
}

//-------------------------------------------------------------------------------------------------
/** Parse a single string token, check for that token in the index list
	* of names provided and store the index into that list.
	*
	* NOTE: Is is assumed that we are going to store the index into
	*				a 4 byte integer.  This works well for INT and ENUM definitions */
//-------------------------------------------------------------------------------------------------
void INI::parseByteSizedIndexList( INI* ini, void * /*instance*/, void *store, const void* userData )
{
	ConstCharPtrArray nameList = (ConstCharPtrArray)userData;
	Int value = scanIndexList(ini->getNextToken(), nameList);
	if (value < 0 || value > 255)
	{
		DEBUG_CRASH(("Bad index list INI::parseByteSizedIndexList"));
		throw ERROR_BUG;
	}
	*(Byte *)store = (Byte)value;
}

//-------------------------------------------------------------------------------------------------
/** Parse a single string token, check for that token in the index list
	* of names provided and store the associated value into that list.
	*
	* NOTE: Is is assumed that we are going to store the index into
	*				a 4 byte integer.  This works well for INT and ENUM definitions */
//-------------------------------------------------------------------------------------------------
void INI::parseLookupList( INI* ini, void * /*instance*/, void *store, const void* userData )
{
	ConstLookupListRecArray lookupList = (ConstLookupListRecArray)userData;
	*(Int *)store = scanLookupList(ini->getNextToken(), lookupList);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// PRIVATE FUNCTIONS //////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////


//-------------------------------------------------------------------------------------------------
void MultiIniFieldParse::add(const FieldParse* f, UnsignedInt e)
{
	if (m_count < MAX_MULTI_FIELDS)
	{
		m_fieldParse[m_count] = f;
		m_extraOffset[m_count] = e;
		++m_count;
	}
	else
	{
		DEBUG_CRASH(("too many multi-fields in INI::initFromINIMultiProc"));
		throw ERROR_BUG;
	}
}

//-------------------------------------------------------------------------------------------------
void INI::initFromINI( void *what, const FieldParse* parseTable )
{
	MultiIniFieldParse p;
	p.add(parseTable);
	initFromINIMulti(what, p);
}

//-------------------------------------------------------------------------------------------------
void INI::initFromINIMultiProc( void *what, BuildMultiIniFieldProc proc )
{
	MultiIniFieldParse p;
	(*proc)(p);
	initFromINIMulti(what, p);
}

//-------------------------------------------------------------------------------------------------
void INI::initFromINIMulti( void *what, const MultiIniFieldParse& parseTableList )
{
	Bool done = FALSE;
	const Bool audit = INIAudit::isEnabled();
	const Bool retail = RetailFileLayers::enabled() && !audit;
	INIBlockStructure::LineHead enclosing{};
	if (audit || retail)
		INIBlockStructure::parseLineHead(currentRawLineView(), enclosing);

	if( what == nullptr )
	{
		DEBUG_CRASH( ("INI::initFromINI - Invalid parameters supplied!") );
		throw INI_INVALID_PARAMS;
	}

	// read each of the data fields
	while( !done )
	{

		// read next line
		readLine();
		const std::string auditLine = audit ? std::string(m_buffer) : std::string();
		const auto rawLine = (audit || retail) ? currentRawLineView() : std::string_view{};

		// check for end token
		const char* field = strtok( m_buffer, INI::getSeps() );
		if( field )
		{

			if( stricmp( field, getEndToken() ) == 0 )
			{
				done = TRUE;
			}
			else
			{
				Bool found = false;
				for (int ptIdx = 0; ptIdx < parseTableList.getCount(); ++ptIdx)
				{
					const FieldParse* fieldParse = findFieldParse(parseTableList.getNthFieldParse(ptIdx), field);
					if (fieldParse && fieldParse->parse)
					{
						// parse this block and check for parse errors
						const UnsignedInt fieldLine = getLineNum();
						const std::string fieldName = retail ? field : "";
						Bool failed = FALSE;
						try {
							INIAudit::Scope auditScope(auditLine.c_str());
							const void* userData = fieldParse->token ? fieldParse->userData : field;
							fieldParse->parse(this, what, (char*)what + fieldParse->offset + parseTableList.getNthExtraOffset(ptIdx), userData);
						} catch (const INIException& error) {
							if (!audit && !retail)
								throw;
							if (audit)
								INIAudit::record("field-error", auditLine + " | " + (error.mFailureMessage ? error.mFailureMessage : ""), m_filename.str(), fieldLine);
							else
								recordSkipped("field-error", fieldName.c_str());
							failed = TRUE;
						} catch (...) {
							if (audit)
								INIAudit::record("field-error", auditLine, m_filename.str(), fieldLine);
							else if (retail && isINIParseError())
								recordSkipped("field-error", fieldName.c_str());
							else
							{
								DEBUG_CRASH( ("[LINE: %d - FILE: '%s'] Error reading field '%s' of block '%s'",
																	 INI::getLineNum(), INI::getFilename().str(), field, m_curBlockStart) );
								char buff[1024];
								snprintf(buff, ARRAY_SIZE(buff), "[LINE: %d - FILE: '%s'] Error reading field '%s'\n",
									INI::getLineNum(), INI::getFilename().str(), field);
								throw INIException(buff);
							}
							failed = TRUE;
						}
						if (failed && !fieldParse->isScalar && fieldLine == getLineNum())
						{
							INIBlockStructure::LineHead head;
							if (INIBlockStructure::parseLineHead(rawLine, head) &&
								(!INIBlockStructure::opensRawBlock(head).empty() || INIBlockStructure::opensBlock(head, enclosing.keyword)))
								skipBlock(nullptr);
						}

						found = true;
						break;

					}
				}

				if (!found && (audit || retail))
				{
					INIBlockStructure::LineHead head;
					const Bool nested = INIBlockStructure::parseLineHead(rawLine, head) &&
						(!INIBlockStructure::opensRawBlock(head).empty() || INIBlockStructure::opensBlock(head, enclosing.keyword));
					recordSkipped(nested ? "subblock" : "field", field);
					if (nested)
						skipBlock(nullptr);
					found = true;
				}
				if (!found)
				{
					DEBUG_CRASH( ("[LINE: %d - FILE: '%s'] Unknown field '%s' in block '%s'",
														 INI::getLineNum(), INI::getFilename().str(), field, m_curBlockStart) );
				}

			}

		}

		// sanity check for reaching end of file with no closing end token
		if( done == FALSE && INI::isEOF() == TRUE )
		{
			if (!audit && !retail)
				DEBUG_CRASH( ("Error parsing block '%s', in INI file '%s'.  Missing '%s' token",
													 m_curBlockStart, getFilename().str(), getEndToken()) );
			throw INI_MISSING_END_TOKEN;
		}

	}

}

//-------------------------------------------------------------------------------------------------
/*static*/ const char* INI::getNextToken(const char* seps)
{
	const char *token = ::strtok(nullptr, seps);
	if (!token)
		throw INI_INVALID_DATA;
	return token;
}

//-------------------------------------------------------------------------------------------------
/*static*/ const char* INI::getNextTokenOrNull(const char* seps)
{
	const char *token = ::strtok(nullptr, seps);
	return token;
}

//-------------------------------------------------------------------------------------------------
/*static*/ ScienceType INI::scanScience(const char* token)
{
	return TheScienceStore->friend_lookupScience( token );
}

#if USE_STD_FROM_CHARS_PARSING

template <typename Type>
Type scanType(const char* tokenString)
{
	std::string_view token(tokenString);
	DEBUG_ASSERTCRASH(!token.empty(), ("token is not expected to be empty"));

	// Unlike sscanf, std::from_chars cannot parse "+".
	// Skip it for from_chars while preserving the original token for the fallback.
	const size_t tokenOffset = (token[0] == '+');
	if (tokenOffset && token.size() > 1 && token[1] == '-')
		throw INI_INVALID_DATA;

	if constexpr (std::is_floating_point_v<Type>)
	{
#if defined(__APPLE__)
		const std::string tokenStr(token.data() + tokenOffset, token.size() - tokenOffset);
		char *end = nullptr;
		errno = 0;
		const double result = std::strtod(tokenStr.c_str(), &end);

		if (end == tokenStr.c_str())
		{
			throw INI_INVALID_DATA;
		}

		if (!std::isfinite(result) && errno != ERANGE)
		{
			throw INI_INVALID_DATA;
		}

		const double maxValue = static_cast<double>(std::numeric_limits<Type>::max());
		if (result > maxValue)
		{
			fprintf(stderr, "[INI] Numeric token '%.*s' out of range, saturating to limit\n",
				static_cast<int>(token.size()), token.data());
			fflush(stderr);
			return std::numeric_limits<Type>::max();
		}
		if (result < -maxValue)
		{
			fprintf(stderr, "[INI] Numeric token '%.*s' out of range, saturating to limit\n",
				static_cast<int>(token.size()), token.data());
			fflush(stderr);
			return -std::numeric_limits<Type>::max();
		}

		return static_cast<Type>(result);
#else
		Type result{};
		const auto [ptr, ec] = std::from_chars(token.data() + tokenOffset, token.data() + token.size(), result);

		if (ec != std::errc{})
		{
			if (ec == std::errc::result_out_of_range)
			{
				char* end;
				const Type value = std::strtof(tokenString, &end);
				if (end == tokenString)
					throw INI_INVALID_DATA;
				return value;
			}

			fprintf(stderr, "[INI] Cannot parse numeric token '%.*s': %s\n",
				static_cast<int>(token.size()), token.data(),
				"invalid number");
			fflush(stderr);
			throw INI_INVALID_DATA;
		}

		if (!std::isfinite(result))
		{
			throw INI_INVALID_DATA;
		}

		return result;
#endif
	}
	else
	{
		// TheSuperHackers @info std::from_chars cannot parse "-1" as uint32 so the result needs to be int64 for integers.
		std::conditional_t<std::is_integral_v<Type>, Int64, Type> result{};
		const auto [ptr, ec] = std::from_chars(token.data() + tokenOffset, token.data() + token.size(), result);

		if (ec != std::errc{})
		{
			if (ec == std::errc::result_out_of_range)
			{
				// GeneralsX @bugfix fbraz 25/09/2026 Saturate overflowing integers to field limits to match retail behavior for mods (#297).
				fprintf(stderr, "[INI] Numeric token '%.*s' out of range, saturating to limit\n",
					static_cast<int>(token.size()), token.data());
				fflush(stderr);

				if (!token.empty() && token[tokenOffset] == '-')
				{
					return std::numeric_limits<Type>::min();
				}
				return std::numeric_limits<Type>::max();
			}

			// GeneralsX @bugfix Copilot 20/09/2026 Keep numeric conversion failures visible in release builds.
			fprintf(stderr, "[INI] Cannot parse numeric token '%.*s': %s\n",
				static_cast<int>(token.size()), token.data(),
				"invalid number");
			fflush(stderr);
			throw INI_INVALID_DATA;
		}

		if constexpr (std::is_unsigned_v<Type>)
		{
			// For unsigned integers, negative values like -1 are sentinels (~0U) and should wrap via static_cast.
			// Positive values exceeding Type's range saturate to max.
			if (result > static_cast<Int64>(std::numeric_limits<Type>::max()))
			{
				// GeneralsX @bugfix fbraz 25/09/2026 Saturate overflowing integers to field limits to match retail behavior for mods (#297).
				fprintf(stderr, "[INI] Numeric token '%.*s' exceeds max value, saturating to limit\n",
					static_cast<int>(token.size()), token.data());
				fflush(stderr);
				return std::numeric_limits<Type>::max();
			}
		}
		else if constexpr (std::is_signed_v<Type>)
		{
			if (result > static_cast<Int64>(std::numeric_limits<Type>::max()))
			{
				// GeneralsX @bugfix fbraz 25/09/2026 Saturate overflowing integers to field limits to match retail behavior for mods (#297).
				fprintf(stderr, "[INI] Numeric token '%.*s' exceeds max value, saturating to limit\n",
					static_cast<int>(token.size()), token.data());
				fflush(stderr);
				return std::numeric_limits<Type>::max();
			}
			if (result < static_cast<Int64>(std::numeric_limits<Type>::min()))
			{
				// GeneralsX @bugfix fbraz 25/09/2026 Saturate overflowing integers to field limits to match retail behavior for mods (#297).
				fprintf(stderr, "[INI] Numeric token '%.*s' exceeds min value, saturating to limit\n",
					static_cast<int>(token.size()), token.data());
				fflush(stderr);
				return std::numeric_limits<Type>::min();
			}
		}

		return static_cast<Type>(result);
	}
}

#endif

//-------------------------------------------------------------------------------------------------
template <typename Type>
static Type scanMath(INIMathFunction function, Type (*scan)(const char*))
{
	try
	{
		return evaluateINIMath<Type>(function, []() { return INI::getNextToken(); }, scan);
	}
	catch (const INIMathError& error)
	{
		fprintf(stderr, "[INI] %s\n", error.what());
		fflush(stderr);
		throw INI_INVALID_DATA;
	}
}

/*static*/ Int INI::scanInt(const char* token)
{
	const INIMathFunction math = findINIMathFunction(token);
	if (math != INI_MATH_NONE)
		return scanMath<Int>(math, &INI::scanInt);

#if USE_STD_FROM_CHARS_PARSING == 1
	return scanType<Int>(token);
#else
	Int value;
	if (sscanf( token, "%d", &value ) != 1)
		throw INI_INVALID_DATA;

#if USE_STD_FROM_CHARS_PARSING == -1
	if (value != scanType<Int>(token))
		throw INI_INVALID_DATA;
#endif

	return value;
#endif
}

//-------------------------------------------------------------------------------------------------
/*static*/ UnsignedInt INI::scanUnsignedInt(const char* token)
{
	const INIMathFunction math = findINIMathFunction(token);
	if (math != INI_MATH_NONE)
		return scanMath<UnsignedInt>(math, &INI::scanUnsignedInt);

#if USE_STD_FROM_CHARS_PARSING == 1
	return scanType<UnsignedInt>(token);
#else
	UnsignedInt value;
	if (sscanf( token, "%u", &value ) != 1)	// unsigned int is %u, not %d
		throw INI_INVALID_DATA;

#if USE_STD_FROM_CHARS_PARSING == -1
	if (value != scanType<UnsignedInt>(token))
		throw INI_INVALID_DATA;
#endif

	return value;
#endif
}

//-------------------------------------------------------------------------------------------------
/*static*/ Real INI::scanReal(const char* token)
{
	const INIMathFunction math = findINIMathFunction(token);
	if (math != INI_MATH_NONE)
		return scanMath<Real>(math, &INI::scanReal);

#if USE_STD_FROM_CHARS_PARSING == 1
	return scanType<Real>(token);
#else
	Real value;
	if (sscanf( token, "%f", &value ) != 1)
		throw INI_INVALID_DATA;

#if USE_STD_FROM_CHARS_PARSING == -1
	if (value != scanType<Real>(token))
		throw INI_INVALID_DATA;
#endif

	return value;
#endif
}

//-------------------------------------------------------------------------------------------------
/*static*/ Real INI::scanPercentToReal(const char* token)
{
	return scanReal(token) / 100.0f;
}

//-------------------------------------------------------------------------------------------------
/*static*/ Int INI::scanIndexList(const char* token, ConstCharPtrArray nameList)
{
	if( nameList == nullptr || nameList[ 0 ] == nullptr )
	{

		DEBUG_CRASH( ("INTERNAL ERROR! scanIndexList, invalid name list") );
		throw INI_INVALID_NAME_LIST;

	}

	// search for matching name
	Int count = 0;
	for(ConstCharPtrArray name = nameList; *name; name++, count++ )
	{
		if( stricmp( *name, token ) == 0 )
		{
			return count;
		}
	}

	if (nameList == ObjectStatusMaskType::getBitNames())
	{
		if (stricmp(token, "USING_ABILITY") == 0)
			return OBJECT_STATUS_IS_USING_ABILITY;
		if (strnicmp(token, "RIDER", 5) == 0 && token[5] >= '1' && token[5] <= '8' && token[6] == '\0')
			return OBJECT_STATUS_RIDER1 + token[5] - '1';
	}
#if !RTS_GENERALS
	if (nameList == KindOfMaskType::getBitNames() && stricmp(token, "AIRFIELD") == 0)
		return KINDOF_FS_AIRFIELD;
#endif

	DEBUG_CRASH(("token %s is not a valid member of the index list",token));
	throw INI_INVALID_DATA;
	return 0;	// never executed, but keeps compiler happy

}
//-------------------------------------------------------------------------------------------------
/*static*/ Int INI::scanLookupList(const char* token, ConstLookupListRecArray lookupList)
{
	if( lookupList == nullptr || lookupList[ 0 ].name == nullptr )
	{
		DEBUG_CRASH( ("INTERNAL ERROR! scanLookupList, invalid name list") );
		throw INI_INVALID_NAME_LIST;
	}

	// search for matching name
	for( const LookupListRec* lookup = &lookupList[0]; lookup->name; lookup++ )
	{
		if( stricmp( lookup->name, token ) == 0 )
		{
			return lookup->value;
		}
	}

	DEBUG_CRASH(("token %s is not a valid member of the lookup list",token));
	throw INI_INVALID_DATA;
	return 0;	// never executed, but keeps compiler happy

}

//-------------------------------------------------------------------------------------------------
const char* INI::getNextSubToken(const char* expected)
{
	const char* token = getNextToken(getSepsColon());
	if (stricmp(token, expected) != 0)
		throw INI_INVALID_DATA;
	return getNextToken(getSepsColon());
}

//-------------------------------------------------------------------------------------------------
/**
 * Parse a "random variable".
 * The format is "FIELD = low high [distribution]".
 */
void INI::parseGameClientRandomVariable( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	GameClientRandomVariable *var = static_cast<GameClientRandomVariable *>(store);

	const char* token;

	token = ini->getNextToken();
	Real low = INI::scanReal(token);

	token = ini->getNextToken();
	Real high = INI::scanReal(token);

	// if omitted, assume uniform
	GameClientRandomVariable::DistributionType type = GameClientRandomVariable::UNIFORM;
	token = ini->getNextTokenOrNull();
	if (token)
		type = (GameClientRandomVariable::DistributionType)INI::scanIndexList(token, GameClientRandomVariable::DistributionTypeNames);

	// set the range of the random variable
	var->setRange( low, high, type );
}

//-------------------------------------------------------------------------------------------------
// parse a duration in msec and convert to duration in frames
void INI::parseDurationReal( INI *ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	Real val = scanReal(ini->getNextToken());
	*(Real *)store = ConvertDurationFromMsecsToFrames(val);
}

//-------------------------------------------------------------------------------------------------
// parse a duration in msec and convert to duration in integral number of frames, (unsignedint) rounding UP
void INI::parseDurationUnsignedInt( INI *ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	if (RetailFileLayers::enabled() || INIAudit::isEnabled())
	{
		const UnsignedInt val = scanUnsignedInt(ini->getNextToken());
		*(UnsignedInt *)store = RotwkDuration::engineFrames(RotwkDuration::retailFrames(val), LOGICFRAMES_PER_SECOND);
		return;
	}
	parseEngineDurationUnsignedInt(ini, nullptr, store, nullptr);
}

void INI::parseEngineDurationUnsignedInt( INI *ini, void *, void *store, const void* )
{
	const UnsignedInt val = scanUnsignedInt(ini->getNextToken());
	*(UnsignedInt *)store = (UnsignedInt)WWMath::Ceil(ConvertDurationFromMsecsToFrames((Real)val));
}

// ------------------------------------------------------------------------------------------------
// parse a duration in msec and convert to duration in integral number of frames, (unsignedshort) rounding UP
void INI::parseDurationUnsignedShort( INI *ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	UnsignedInt frames;
	parseDurationUnsignedInt(ini, nullptr, &frames, nullptr);
	*(UnsignedShort *)store = static_cast<UnsignedShort>(frames);
}

//-------------------------------------------------------------------------------------------------
// parse acceleration in (dist/sec) and convert to (dist/frame)
void INI::parseVelocityReal( INI *ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	const char *token = ini->getNextToken();
	Real val = scanReal(token);
	*(Real *)store = ConvertVelocityInSecsToFrames(val);
}

//-------------------------------------------------------------------------------------------------
// parse acceleration in (dist/sec^2) and convert to (dist/frame^2)
void INI::parseAccelerationReal( INI *ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	const char *token = ini->getNextToken();
	Real val = scanReal(token);
	*(Real *)store = ConvertAccelerationInSecsToFrames(val);
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void INI::parseVeterancyLevelFlags(INI* ini, void* /*instance*/, void* store, const void* /*userData*/)
{
	VeterancyLevelFlags flags = VETERANCY_LEVEL_FLAGS_ALL;
	for (const char* token = ini->getNextToken(); token; token = ini->getNextTokenOrNull())
	{
		if (stricmp(token, "ALL") == 0)
		{
			flags = VETERANCY_LEVEL_FLAGS_ALL;
			continue;
		}
		else if (stricmp(token, "NONE") == 0)
		{
			flags = VETERANCY_LEVEL_FLAGS_NONE;
			continue;
		}
		else if (token[0] == '+')
		{
			VeterancyLevel dt = (VeterancyLevel)INI::scanIndexList(token+1, TheVeterancyNames);
			flags = setVeterancyLevelFlag(flags, dt);
			continue;
		}
		else if (token[0] == '-')
		{
			VeterancyLevel dt = (VeterancyLevel)INI::scanIndexList(token+1, TheVeterancyNames);
			flags = clearVeterancyLevelFlag(flags, dt);
			continue;
		}
		else
		{
			throw INI_UNKNOWN_TOKEN;
		}
	}
	*(VeterancyLevelFlags*)store = flags;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void INI::parseSoundsList( INI* ini, void *instance, void *store, const void* /*userData*/ )
{
	std::vector<AsciiString> *vec = (std::vector<AsciiString>*) store;
	vec->clear();

	constexpr const char* Seps = " \t,=";
	for (const char* token = ini->getNextTokenOrNull(Seps); token; token = ini->getNextTokenOrNull(Seps))
	{
		vec->push_back(token);
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void INI::parseDamageTypeFlags(INI* ini, void* /*instance*/, void* store, const void* /*userData*/)
{
	DamageTypeFlags flags = DAMAGE_TYPE_FLAGS_ALL;

	for (const char* token = ini->getNextToken(); token; token = ini->getNextTokenOrNull())
	{
		if (stricmp(token, "ALL") == 0)
		{
			flags = DAMAGE_TYPE_FLAGS_ALL;
			continue;
		}
		if (stricmp(token, "NONE") == 0)
		{
			flags = DAMAGE_TYPE_FLAGS_NONE;
			continue;
		}
		if (token[0] == '+')
		{
			DamageType dt = (DamageType)scanIndexList(token+1, DamageTypeFlags::getBitNames());
			flags = setDamageTypeFlag(flags, dt);
			continue;
		}
		if (token[0] == '-')
		{
			DamageType dt = (DamageType)scanIndexList(token+1, DamageTypeFlags::getBitNames());
			flags = clearDamageTypeFlag(flags, dt);
			continue;
		}
		throw INI_UNKNOWN_TOKEN;
	}
	*(DamageTypeFlags*)store = flags;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void INI::parseDeathTypeFlags(INI* ini, void* /*instance*/, void* store, const void* /*userData*/)
{
	DeathTypeFlags flags = DEATH_TYPE_FLAGS_ALL;
	for (const char* token = ini->getNextToken(); token; token = ini->getNextTokenOrNull())
	{
		if (stricmp(token, "ALL") == 0)
		{
			flags = DEATH_TYPE_FLAGS_ALL;
			continue;
		}
		if (stricmp(token, "NONE") == 0)
		{
			flags = DEATH_TYPE_FLAGS_NONE;
			continue;
		}
		if (token[0] == '+')
		{
			DeathType dt = (DeathType)INI::scanIndexList(token+1, TheDeathNames);
			flags = setDeathTypeFlag(flags, dt);
			continue;
		}
		if (token[0] == '-')
		{
			DeathType dt = (DeathType)INI::scanIndexList(token+1, TheDeathNames);
			flags = clearDeathTypeFlag(flags, dt);
			continue;
		}
		throw INI_UNKNOWN_TOKEN;
	}
	*(DeathTypeFlags*)store = flags;
}

//-------------------------------------------------------------------------------------------------
// parse the line and return whether the given line is a Block declaration of the form
// [whitespace] blockType [whitespace] blockName [EOL]
// both blockType and blockName are case insensitive
Bool INI::isDeclarationOfType( AsciiString blockType, AsciiString blockName, char *bufferToCheck )
{
	if (!bufferToCheck || blockType.isEmpty() || blockName.isEmpty())
		return false;

	const char* tempBuff = bufferToCheck;

	while (isspace(*tempBuff))
		++tempBuff;

	const int blockTypeLength = blockType.getLength();
	if (strnicmp(tempBuff, blockType.str(), blockTypeLength) != 0)
		return false;

	tempBuff += blockTypeLength;

	if (!isspace(*tempBuff++))
		return false;

	while (isspace(*tempBuff))
		++tempBuff;

	const int blockNameLength = blockName.getLength();
	if (strnicmp(tempBuff, blockName.str(), blockNameLength) != 0)
		return false;

	tempBuff += blockNameLength;

	while (isspace(*tempBuff))
		++tempBuff;

	if (*tempBuff != '\0')
		return false;

	return true;
}

//-------------------------------------------------------------------------------------------------
// parse the line and return whether the given line is a Block declaration of the form
// [whitespace] end [EOL]
Bool INI::isEndOfBlock( char *bufferToCheck )
{
	Bool retVal = true;
	if (!bufferToCheck) {
		return false;
	}

	// DO NOT RETURN EARLY FROM THIS FUNCTION (beyond this point)
	// we have to restore the bufferToCheck to its previous state before returning, so
	// it is important to get through all the checks.

	static const char* endString = "End";
	int endStringLength = strlen(endString);
	char restoreChar;
	char *tempBuff = bufferToCheck;


	while (isspace(*tempBuff)) {
		++tempBuff;
	}

	if (strlen(tempBuff) > endStringLength) {
		restoreChar = tempBuff[endStringLength];
		tempBuff[endStringLength] = 0;

		if (stricmp(endString, tempBuff) != 0) {
			retVal = false;
		}

		tempBuff[endStringLength] = restoreChar;
		tempBuff = tempBuff + endStringLength;
	} else {
		retVal = false;
	}

	while (*tempBuff && retVal) {
		retVal = isspace(*tempBuff);
		++tempBuff;
	}

	return retVal;
}
