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

//----------------------------------------------------------------------------
//
//                       Westwood Studios Pacific.
//
//                       Confidential Information
//                Copyright(C) 2001 - All Rights Reserved
//
//----------------------------------------------------------------------------
//
// Project:   WSYS Library
//
// Module:    IO
//
// File name: IO_FileSystem.cpp
//
// Created:   4/23/01
//
//----------------------------------------------------------------------------

//----------------------------------------------------------------------------
//         Includes
//----------------------------------------------------------------------------

#include "PreRTS.h"
#include "Common/file.h"
#include "Common/FileSystem.h"

#include "Common/ArchiveFileSystem.h"
#include "Common/GameAudio.h"
#include "Common/GlobalData.h"
#include "Common/LocalFileSystem.h"
#include "Common/PerfTimer.h"
#include "Common/RetailFileLayers.h"

#include "Lib/PathUtil.h"

#include <array>
#include <string>

namespace
{
	std::array<AsciiString, 2> retailLooseRoots(const char* filename)
	{
		if (!RetailFileLayers::enabled() || !filename || filename[0] == '/' ||
			filename[0] == '\\' || strchr(filename, ':'))
			return {};

		return {TheGlobalData->m_modDir, RetailFileLayers::languageDirectory()};
	}

	void insertVirtualFilename(FilenameList& filenames, const char* path)
	{
		std::string normalized(path);
		std::replace(normalized.begin(), normalized.end(), '/', '\\');
		if (normalized.starts_with(".\\"))
			normalized.erase(0, 2);
		filenames.insert(AsciiString(normalized.c_str()));
	}
}

bool RetailFileLayers::hasLooseOverride(const char* filename)
{
	for (auto path : retailLooseRoots(filename))
	{
		if (path.isEmpty())
			continue;
		path.concat(filename);
		if (TheLocalFileSystem->doesFileExist(path.str()))
			return true;
	}
	return false;
}

DECLARE_PERF_TIMER(FileSystem)

//----------------------------------------------------------------------------
//         Externals
//----------------------------------------------------------------------------



//----------------------------------------------------------------------------
//         Defines
//----------------------------------------------------------------------------



//----------------------------------------------------------------------------
//         Private Types
//----------------------------------------------------------------------------



//----------------------------------------------------------------------------
//         Private Data
//----------------------------------------------------------------------------



//----------------------------------------------------------------------------
//         Public Data
//----------------------------------------------------------------------------

//===============================
// TheFileSystem
//===============================
/**
  *	This is the FileSystem's singleton class. All file access
	* should be through TheFileSystem, unless code needs to use an explicit
	* File or FileSystem derivative.
	*
	* Using TheFileSystem->open and File exclusively for file access, particularly
	* in library or modular code, allows applications to transparently implement
	* file access as they see fit. This is particularly important for code that
	* needs to be shared between applications, such as games and tools.
	*/
//===============================

FileSystem	*TheFileSystem = nullptr;

//----------------------------------------------------------------------------
//         Private Prototypes
//----------------------------------------------------------------------------

//----------------------------------------------------------------------------
//         Private Functions
//----------------------------------------------------------------------------


//----------------------------------------------------------------------------
//         Public Functions
//----------------------------------------------------------------------------


//============================================================================
// FileSystem::FileSystem
//============================================================================

FileSystem::FileSystem()
{

}

//============================================================================
// FileSystem::~FileSystem
//============================================================================

FileSystem::~FileSystem()
{

}

//============================================================================
// FileSystem::init
//============================================================================

void		FileSystem::init()
{
	TheLocalFileSystem->init();
	TheArchiveFileSystem->init();
}

//============================================================================
// FileSystem::update
//============================================================================

void		FileSystem::update()
{
	USE_PERF_TIMER(FileSystem)
	TheLocalFileSystem->update();
	TheArchiveFileSystem->update();
}

//============================================================================
// FileSystem::reset
//============================================================================

void		FileSystem::reset()
{
	USE_PERF_TIMER(FileSystem)
	TheLocalFileSystem->reset();
	TheArchiveFileSystem->reset();
}

//============================================================================
// FileSystem::open
//============================================================================

File*		FileSystem::openFile( const Char *filename, Int access, size_t bufferSize, FileInstance instance )
{
	USE_PERF_TIMER(FileSystem)
	if (RetailFileLayers::enabled() && instance != 0)
		return nullptr;
	File *file = nullptr;

	// GeneralsX @bugfix felipebraz 02/10/2026 Allow mod archives (e.g. !*.big, custom SkirmishScripts)
	// to override stock loose files that otherwise shadow mod contents.
	const Bool hasModOverride = !RetailFileLayers::enabled() && (TheArchiveFileSystem != nullptr) &&
	                            !(access & File::WRITE) &&
	                            TheArchiveFileSystem->hasModArchiveOverride(filename);

	FileInstance remainingInstance = instance;

	if (hasModOverride)
	{
		FileInstance modArchiveCount = TheArchiveFileSystem->getFileCount(filename, ArchiveFileSystem::ARCHIVE_FILTER_MOD_ONLY);
		if (remainingInstance < modArchiveCount)
		{
			return TheArchiveFileSystem->openFile( filename, access, remainingInstance, ArchiveFileSystem::ARCHIVE_FILTER_MOD_ONLY );
		}
		remainingInstance -= modArchiveCount;
	}

	if ( TheLocalFileSystem != nullptr )
	{
		if (!(access & (File::WRITE | File::CREATE)))
		{
			for (auto path : retailLooseRoots(filename))
			{
				if (path.isEmpty())
					continue;
				path.concat(filename);
				if (File* looseFile = TheLocalFileSystem->openFile(path.str(), access, bufferSize))
					return looseFile;
			}
		}
		if (remainingInstance != 0)
		{
			if (TheLocalFileSystem->doesFileExist(filename))
			{
				--remainingInstance;
			}
		}
		else
		{
			file = TheLocalFileSystem->openFile( filename, access, bufferSize );

#if ENABLE_FILESYSTEM_EXISTENCE_CACHE
			if (file != nullptr && (file->getAccess() & File::CREATE))
			{
				FastCriticalSectionClass::LockClass lock(m_fileExistMutex);
				FileExistMap::iterator it = m_fileExist.find(FileExistMap::key_type::temporary(filename));
				if (it != m_fileExist.end())
				{
					++it->second.instanceExists;
					if (it->second.instanceDoesNotExist != ~FileInstance(0))
						++it->second.instanceDoesNotExist;
				}
				else
				{
					m_fileExist[filename];
				}
			}
#endif
		}
	}

	if ( (TheArchiveFileSystem != nullptr) && (file == nullptr) )
	{
		// Non-mod (ordinary) archives fallback if mod overrides were active, or all archives if not.
		ArchiveFileSystem::ArchiveFilter filter = hasModOverride ? ArchiveFileSystem::ARCHIVE_FILTER_NON_MOD_ONLY : ArchiveFileSystem::ARCHIVE_FILTER_ALL;
		file = TheArchiveFileSystem->openFile( filename, 0, remainingInstance, filter );
	}

	return file;
}

//============================================================================
// FileSystem::doesFileExist
//============================================================================

Bool FileSystem::doesFileExist(const Char *filename, FileInstance instance) const
{
	USE_PERF_TIMER(FileSystem)
	if (RetailFileLayers::enabled() && instance != 0)
		return FALSE;

#if ENABLE_FILESYSTEM_EXISTENCE_CACHE
	{
		FastCriticalSectionClass::LockClass lock(m_fileExistMutex);
		FileExistMap::const_iterator it = m_fileExist.find(FileExistMap::key_type::temporary(filename));
		if (it != m_fileExist.end())
		{
			// Must test instanceDoesNotExist first!
			if (instance >= it->second.instanceDoesNotExist)
				return FALSE;
			if (instance <= it->second.instanceExists)
				return TRUE;
		}
	}
#endif

	const FileInstance requestedInstance = instance;
	FileInstance remainingInstance = instance;

	// GeneralsX @bugfix felipebraz 02/10/2026 Prioritize mod archive existence if an override is active.
	const Bool hasModOverride = !RetailFileLayers::enabled() && (TheArchiveFileSystem != nullptr) &&
	                            TheArchiveFileSystem->hasModArchiveOverride(filename);

	if (hasModOverride)
	{
		FileInstance modArchiveCount = TheArchiveFileSystem->getFileCount(filename, ArchiveFileSystem::ARCHIVE_FILTER_MOD_ONLY);
		if (remainingInstance < modArchiveCount)
		{
#if ENABLE_FILESYSTEM_EXISTENCE_CACHE
			{
				FastCriticalSectionClass::LockClass lock(m_fileExistMutex);
				FileExistMap::mapped_type& value = m_fileExist[filename];
				value.instanceExists = max(value.instanceExists, requestedInstance);
			}
#endif
			return TRUE;
		}
		remainingInstance -= modArchiveCount;
	}

	if (RetailFileLayers::hasLooseOverride(filename))
		return TRUE;

	if (TheLocalFileSystem->doesFileExist(filename))
	{
		if (remainingInstance == 0)
		{
#if ENABLE_FILESYSTEM_EXISTENCE_CACHE
			{
				FastCriticalSectionClass::LockClass lock(m_fileExistMutex);
				FileExistMap::mapped_type& value = m_fileExist[filename];
				value.instanceExists = max(value.instanceExists, requestedInstance);
			}
#endif
			return TRUE;
		}

		--remainingInstance;
	}

	ArchiveFileSystem::ArchiveFilter filter = hasModOverride ? ArchiveFileSystem::ARCHIVE_FILTER_NON_MOD_ONLY : ArchiveFileSystem::ARCHIVE_FILTER_ALL;
	if (TheArchiveFileSystem->doesFileExist(filename, remainingInstance, filter))
	{
#if ENABLE_FILESYSTEM_EXISTENCE_CACHE
		{
			FastCriticalSectionClass::LockClass lock(m_fileExistMutex);
			FileExistMap::mapped_type& value = m_fileExist[filename];
			value.instanceExists = max(value.instanceExists, requestedInstance);
		}
#endif
		return TRUE;
	}

#if ENABLE_FILESYSTEM_EXISTENCE_CACHE
	{
		FastCriticalSectionClass::LockClass lock(m_fileExistMutex);
		FileExistMap::mapped_type& value = m_fileExist[filename];
		value.instanceDoesNotExist = min(value.instanceDoesNotExist, requestedInstance);
	}
#endif
	return FALSE;
}

//============================================================================
// FileSystem::getFileListInDirectory
//============================================================================
void FileSystem::getFileListInDirectory(const AsciiString& directory, const AsciiString& searchName, FilenameList &filenameList, Bool searchSubdirectories) const
{
	USE_PERF_TIMER(FileSystem)
	if (RetailFileLayers::enabled())
	{
		for (const auto& root : retailLooseRoots(directory.str()))
		{
			if (root.isEmpty())
				continue;
			FilenameList layer;
			TheLocalFileSystem->getFileListInDirectory(directory, root, searchName, layer, searchSubdirectories);
			for (const auto& path : layer)
				insertVirtualFilename(filenameList, path.str() + root.getLength());
		}
		FilenameList install;
		TheLocalFileSystem->getFileListInDirectory("", directory, searchName, install, searchSubdirectories);
		TheArchiveFileSystem->getFileListInDirectory("", directory, searchName, install, searchSubdirectories);
		for (const auto& path : install)
			insertVirtualFilename(filenameList, path.str());
		return;
	}
	TheLocalFileSystem->getFileListInDirectory(AsciiString::TheEmptyString, directory, searchName, filenameList, searchSubdirectories);
	TheArchiveFileSystem->getFileListInDirectory(AsciiString::TheEmptyString, directory, searchName, filenameList, searchSubdirectories);
}

//============================================================================
// FileSystem::getFileInfo
//============================================================================
Bool FileSystem::getFileInfo(const AsciiString& filename, FileInfo *fileInfo, FileInstance instance) const
{
	USE_PERF_TIMER(FileSystem)

	// TheSuperHackers @todo Add file info cache?

	if (fileInfo == nullptr) {
		return FALSE;
	}
	memset(fileInfo, 0, sizeof(*fileInfo));
	if (RetailFileLayers::enabled() && instance != 0)
		return FALSE;

	FileInstance remainingInstance = instance;

	// GeneralsX @bugfix felipebraz 02/10/2026 Prioritize mod archive metadata if an override is active.
	const Bool hasModOverride = !RetailFileLayers::enabled() && (TheArchiveFileSystem != nullptr) &&
	                            TheArchiveFileSystem->hasModArchiveOverride(filename.str());

	if (hasModOverride)
	{
		FileInstance modArchiveCount = TheArchiveFileSystem->getFileCount(filename.str(), ArchiveFileSystem::ARCHIVE_FILTER_MOD_ONLY);
		if (remainingInstance < modArchiveCount)
		{
			return TheArchiveFileSystem->getFileInfo(filename, fileInfo, remainingInstance, ArchiveFileSystem::ARCHIVE_FILTER_MOD_ONLY);
		}
		remainingInstance -= modArchiveCount;
	}

	for (auto path : retailLooseRoots(filename.str()))
	{
		if (path.isEmpty())
			continue;
		path.concat(filename);
		if (TheLocalFileSystem->getFileInfo(path, fileInfo))
			return TRUE;
	}

	if (TheLocalFileSystem->getFileInfo(filename, fileInfo)) {
		if (remainingInstance == 0) {
			return TRUE;
		}

		--remainingInstance;
	}

	ArchiveFileSystem::ArchiveFilter filter = hasModOverride ? ArchiveFileSystem::ARCHIVE_FILTER_NON_MOD_ONLY : ArchiveFileSystem::ARCHIVE_FILTER_ALL;
	if (TheArchiveFileSystem->getFileInfo(filename, fileInfo, remainingInstance, filter)) {
		return TRUE;
	}

	return FALSE;
}

//============================================================================
// FileSystem::createDirectory
//============================================================================
Bool FileSystem::createDirectory(AsciiString directory)
{
	USE_PERF_TIMER(FileSystem)
	if (TheLocalFileSystem != nullptr) {
		return TheLocalFileSystem->createDirectory(directory);
	}
	return FALSE;
}

//============================================================================
// FileSystem::normalizePath
//============================================================================
AsciiString FileSystem::normalizePath(const AsciiString& path)
{
	return TheLocalFileSystem->normalizePath(path);
}

//============================================================================
// FileSystem::isPathInDirectory
//============================================================================
Bool FileSystem::isPathInDirectory(const AsciiString& testPath, const AsciiString& basePath)
{
	AsciiString testPathNormalized = TheFileSystem->normalizePath(testPath);
	AsciiString basePathNormalized = TheFileSystem->normalizePath(basePath);

	if (basePathNormalized.isEmpty())
	{
		DEBUG_CRASH(("Unable to normalize base directory path '%s'.", basePath.str()));
		return false;
	}
	else if (testPathNormalized.isEmpty())
	{
		DEBUG_CRASH(("Unable to normalize file path '%s'.", testPath.str()));
		return false;
	}

#ifdef _WIN32
	const char* pathSep = "\\";
#else
	const char* pathSep = "/";
#endif

	if (!basePathNormalized.endsWith(pathSep))
	{
		basePathNormalized.concat(pathSep);
	}

#ifdef _WIN32
	if (!testPathNormalized.startsWithNoCase(basePathNormalized))
#else
	if (!testPathNormalized.startsWith(basePathNormalized))
#endif
	{
		return false;
	}

	return true;
}

//============================================================================
// FileSystem::removeExtension - Ascii handling variant
//============================================================================
bool FileSystem::removeExtension(AsciiString& path)
{
	if (const Char* ext = getExtension(path.str()))
	{
		path.truncateTo(ext - path.str());
		return true;
	}

	return false;
}

//============================================================================
// FileSystem::removeExtension - Unicode handling variant
//============================================================================
bool FileSystem::removeExtension(UnicodeString& path)
{
	if (const WideChar* ext = getExtension(path.str()))
	{
		path.truncateTo(ext - path.str());
		return true;
	}

	return false;
}

//============================================================================
// FileSystem::appendPathSeparator
//============================================================================
void FileSystem::appendPathSeparator(AsciiString& path)
{
	if (path.isNotEmpty() && !isPathSeparator(path.back()))
	{
		path.concat(getNativePathSeparator());
	}
}
