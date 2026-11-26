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

// FILE: INIAudioEventInfo.cpp ////////////////////////////////////////////////////////////////////////
// Author: Colin Day, July 2002
// Desc:   Parsing AudioEvent, MusicTrack and DialogEvent INI entries
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/INI.h"
#include "Common/GameAudio.h"
#include "Common/AudioEventInfo.h"
#include "Common/AudioRandomValue.h"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <cstring>
#include <limits>

WeightedAudioName::WeightedAudioName(const char* token) : name(token)
{
	const char* suffix = std::strrchr(token, ':');
	if (!suffix || suffix == token)
		return;
	const char* end = token + std::strlen(token);
	Int parsed;
	const auto result = std::from_chars(suffix + 1, end, parsed);
	if (result.ec != std::errc() || result.ptr != end || parsed < 1)
		return;
	authoredName = name;
	name.set(token, static_cast<Int>(suffix - token));
	weight = parsed;
}

const AsciiString& WeightedAudioName::getName(Bool useAuthoredName) const
{
	return useAuthoredName && !authoredName.isEmpty() ? authoredName : name;
}

Int pickWeightedAudioName(const std::vector<WeightedAudioName>& names, Int previous)
{
	const Int count = static_cast<Int>(names.size());
	if (count < 2)
		return count - 1;
	if (previous < 0 || previous >= count)
		previous = -1;

	std::uint64_t total = 0;
	for (Int i = 0; i < count; ++i)
		if (i != previous)
			total += names[i].weight;

	std::uint64_t draw;
	if (total <= std::numeric_limits<Int>::max())
		draw = GameAudioRandomValue(0, static_cast<Int>(total) - 1);
	else
		draw = std::min(total - 1, static_cast<std::uint64_t>(GameAudioRandomValueReal(0.0f, 1.0f) * static_cast<double>(total)));

	for (Int i = 0; i < count; ++i)
	{
		if (i == previous)
			continue;
		const auto weight = static_cast<std::uint64_t>(names[i].weight);
		if (draw < weight)
			return i;
		draw -= weight;
	}
	return -1;
}

AudioEventInfo::~AudioEventInfo()
{

}
// @todo: Get these functions unified.

//-------------------------------------------------------------------------------------------------
void INI::parseMusicTrackDefinition( INI* ini )
{
	AsciiString name;
	AudioEventInfo *track;

	// read the name
	const char* c = ini->getNextToken();
	name.set( c );

	track = TheAudio->newAudioEventInfo( name );
	if (!track) {
		return;
	}

	AudioEventInfo *defaultInfo = TheAudio->findAudioEventInfo("DefaultMusicTrack");
	if (defaultInfo) {
		(*track) = (*defaultInfo);
		TheAudio->addTrackName( name );
	}

	track->m_audioName = name;
	track->m_soundType = AT_Music;

	// parse the ini definition
	ini->initFromINI( track, track->getFieldParse() );
}

//-------------------------------------------------------------------------------------------------
void INI::parseAudioEventDefinition( INI* ini )
{
	AsciiString name;
	AudioEventInfo *track;

	// read the name
	const char* c = ini->getNextToken();
	name.set( c );

	track = TheAudio->newAudioEventInfo( name );
	if (!track) {
		return;
	}

	AudioEventInfo *defaultInfo = TheAudio->findAudioEventInfo("DefaultSoundEffect");
	if (defaultInfo) {
		(*track) = (*defaultInfo);
	}

	track->m_audioName = name;
	track->m_soundType = AT_SoundEffect;

	// parse the ini definition
	ini->initFromINI( track, track->getFieldParse() );
}

//-------------------------------------------------------------------------------------------------
void INI::parseDialogDefinition( INI* ini )
{
	AsciiString name;
	AudioEventInfo *track;

	// read the name
	const char* c = ini->getNextToken();
	name.set( c );

	track = TheAudio->newAudioEventInfo( name );
	if (!track) {
		return;
	}

	AudioEventInfo *defaultInfo = TheAudio->findAudioEventInfo("DefaultDialog");
	if (defaultInfo) {
		(*track) = (*defaultInfo);
	}

	track->m_audioName = name;
	track->m_soundType = AT_Streaming;

	// parse the ini definition
	ini->initFromINI( track, track->getFieldParse() );
}

void AudioEventInfo::parseAmbientStream(INI* ini)
{
	const AsciiString name = ini->getNextAsciiString();
	AudioEventInfo* stream = TheAudio->newAudioEventInfo(name);
	if (!stream)
		return;
	if (const AudioEventInfo* defaults = TheAudio->findAudioEventInfo("DefaultAmbientStream"); defaults && defaults != stream)
		*stream = *defaults;
	stream->m_audioName = name;
	stream->m_soundType = AT_AmbientStream;
	ini->initFromINI(stream, stream->getFieldParse());
}


//-------------------------------------------------------------------------------------------------
static void parseDelay( INI* ini, void *instance, void *store, const void* /*userData*/ );
static void parsePitchShift( INI* ini, void *instance, void *store, const void* /*userData*/ );

static void parsePerFilePitchShift(INI* ini, void* instance, void*, const void*)
{
	AudioEventInfo& info = *static_cast<AudioEventInfo*>(instance);
	const Real low = ini->scanReal(ini->getNextToken());
	const Real high = ini->scanReal(ini->getNextToken());
	if (!(low > -100.0f && high >= low))
		throw INI_INVALID_DATA;
	info.m_perFilePitchMin = 1.0f + low * 0.01f;
	info.m_perFilePitchMax = 1.0f + high * 0.01f;
}

//-------------------------------------------------------------------------------------------------
static constexpr bool WeightedSoundList = true;

const FieldParse AudioEventInfo::m_audioEventInfo[] =
{
	{ "PlayPercent", INI::parsePercentToReal, nullptr, offsetof(AudioEventInfo, m_playFraction) },
	{ "PerFileVolumeShift", INI::parsePercentToReal, nullptr, offsetof(AudioEventInfo, m_perFileVolumeShift) },
	{ "PerFilePitchShift", parsePerFilePitchShift, nullptr, 0 },
	{ "Filename",							INI::parseAsciiString,		nullptr,								offsetof( AudioEventInfo, m_filename) },
	{ "Volume",								INI::parsePercentToReal,	nullptr,								offsetof( AudioEventInfo, m_volume ) },
	{ "VolumeShift",					INI::parsePercentToReal,	nullptr,								offsetof( AudioEventInfo, m_volumeShift ) },
	{ "MinVolume",						INI::parsePercentToReal,	nullptr,								offsetof( AudioEventInfo, m_minVolume ) },
	{ "PitchShift",						parsePitchShift,					nullptr,								0 },
	{ "Delay",								parseDelay,								nullptr,								0 },
	{ "Limit",								INI::parseInt,						nullptr,								offsetof( AudioEventInfo, m_limit ) },
	{ "LoopCount",						INI::parseInt,						nullptr,								offsetof( AudioEventInfo, m_loopCount ) },
	{ "Priority",							INI::parseIndexList,			theAudioPriorityNames,	offsetof( AudioEventInfo, m_priority ) },
	{ "Type",									INI::parseBitString32,		theSoundTypeNames,			offsetof( AudioEventInfo, m_type ) },
	{ "Control",							INI::parseBitString32,		theAudioControlNames,	offsetof( AudioEventInfo, m_control ) },
	{ "Sounds",								INI::parseSoundsList,			&WeightedSoundList,								offsetof( AudioEventInfo, m_sounds ) },
	{ "SoundsNight",					INI::parseSoundsList,			&WeightedSoundList,								offsetof( AudioEventInfo, m_soundsNight ) },
	{ "SoundsEvening",				INI::parseSoundsList,			&WeightedSoundList,								offsetof( AudioEventInfo, m_soundsEvening ) },
	{ "SoundsMorning",				INI::parseSoundsList,			&WeightedSoundList,								offsetof( AudioEventInfo, m_soundsMorning ) },
	{ "Attack",								INI::parseSoundsList,			&WeightedSoundList,								offsetof( AudioEventInfo, m_attackSounds ) },
	{ "Decay",								INI::parseSoundsList,			&WeightedSoundList,								offsetof( AudioEventInfo, m_decaySounds ) },
	{ "MinRange",							INI::parseReal,						nullptr,								offsetof( AudioEventInfo, m_minDistance) },
	{ "MaxRange",							INI::parseReal,						nullptr,								offsetof( AudioEventInfo, m_maxDistance) },
	{ "LowPassCutoff",				INI::parsePercentToReal,	nullptr,								offsetof( AudioEventInfo, m_lowPassFreq) },
	{ nullptr, nullptr, nullptr, 0 }
};

//-------------------------------------------------------------------------------------------------
static void parseDelay( INI* ini, void *instance, void *store, const void* /*userData*/ )
{
	AudioEventInfo *attribs = (AudioEventInfo*) store;

	Int min = ini->scanInt(ini->getNextToken());
	Int max = ini->scanInt(ini->getNextToken());

	DEBUG_ASSERTCRASH( min >= 0 && max >= min, ("Bad delay values for audio event %s", attribs->m_audioName.str()));
	attribs->m_delayMax = max;
	attribs->m_delayMin = min;
}

//-------------------------------------------------------------------------------------------------
static void parsePitchShift( INI* ini, void *instance, void *store, const void* /*userData*/ )
{
	AudioEventInfo *attribs = (AudioEventInfo*) store;

	Real min = ini->scanReal(ini->getNextToken());
	Real max = ini->scanReal(ini->getNextToken());

	DEBUG_ASSERTCRASH( min > -100 && max >= min, ("Bad pitch shift values for audio event %s", attribs->m_audioName.str()));
	attribs->m_pitchShiftMin = 1.0f + min/100;
	attribs->m_pitchShiftMax = 1.0f + max/100;
}

// STATIC DEFINIITIONS ////////////////////////////////////////////////////////////////////////////

const char *const theAudioPriorityNames[] =
{
	"LOWEST",
	"LOW",
	"NORMAL",
	"HIGH",
	"CRITICAL",
	nullptr
};
static_assert(ARRAY_SIZE(theAudioPriorityNames) == AP_COUNT + 1, "Incorrect array size");

const char *const theSoundTypeNames[] =
{
	"UI",
	"WORLD",
	"SHROUDED",
	"GLOBAL",
	"VOICE",
	"PLAYER",
	"ALLIES",
	"ENEMIES",
	"EVERYONE",
	"FAKE",
	"DEFAULT",
	nullptr
};

const char *const theAudioControlNames[] =
{
	"LOOP",
	"RANDOM",
	"ALL",
	"POSTDELAY",
	"INTERRUPT",
	"SEQUENTIAL",
	"RANDOMSTART",
	"FADE_ON_KILL",
	"FADE_ON_START",
	nullptr
};

