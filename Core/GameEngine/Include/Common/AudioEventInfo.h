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

// FILE: AudioEventInfo.h /////////////////////////////////////////////////////////////////////////
// AudioEventInfo structure
// Author: John K. McDonald, March 2002

#pragma once

#include "Common/AsciiString.h"
#include "Common/GameMemory.h"
#include "Common/STLTypedefs.h"

// DEFINES
#define NO_INTENSIVE_AUDIO_DEBUG

// FORWARD DECLARATIONS ///////////////////////////////////////////////////////////////////////////
struct FieldParse;
class INI;

struct WeightedAudioName
{
	explicit WeightedAudioName(const char* token);
	const AsciiString& getName(Bool useAuthoredName = FALSE) const;

	AsciiString name;
	AsciiString authoredName;
	Int weight = 1000;
};

Int pickWeightedAudioName(const std::vector<WeightedAudioName>& names, Int previous = -1);

// USEFUL DECLARATIONS ////////////////////////////////////////////////////////////////////////////
enum AudioType CPP_11(: Int)
{
	AT_Music,
	AT_Streaming,
	AT_SoundEffect,
	AT_AmbientStream
};

extern const char* const theAudioPriorityNames[];
enum AudioPriority CPP_11(: Int)
{
	AP_LOWEST,
	AP_LOW,
	AP_NORMAL,
	AP_HIGH,
	AP_CRITICAL,

	AP_COUNT
};

extern const char *const theSoundTypeNames[];
enum SoundType CPP_11(: Int)
{
	ST_UI										= 0x0001,
	ST_WORLD								= 0x0002,
	ST_SHROUDED							= 0x0004,
	ST_GLOBAL								= 0x0008,
	ST_VOICE								= 0x0010,
	ST_PLAYER								= 0x0020,
	ST_ALLIES								= 0x0040,
	ST_ENEMIES							= 0x0080,
	ST_EVERYONE							= 0x0100,
	ST_FAKE = 1 << 9,
	ST_DEFAULT = 1 << 10,
};

extern const char *const theAudioControlNames[];
enum AudioControl CPP_11(: Int)
{
	AC_LOOP									= 0x0001,
	AC_RANDOM								= 0x0002,
	AC_ALL									= 0x0004,
	AC_POSTDELAY						= 0x0008,
	AC_INTERRUPT						= 0x0010,
	AC_SEQUENTIAL = 1 << 5,
	AC_RANDOMSTART = 1 << 6,
	AC_FADE_ON_KILL = 1 << 7,
	AC_FADE_ON_START = 1 << 8,
};

class DynamicAudioEventInfo;

struct AudioEventInfo : public MemoryPoolObject
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( AudioEventInfo, "AudioEventInfo" )

public:
	static void parseAmbientStream(INI* ini);
	AsciiString m_audioName;	// This name matches the name of the AudioEventRTS
	AsciiString m_filename;		// For music tracks, this is the filename of the track

	Real m_volume;						// Desired volume of this audio
	Real m_volumeShift;				// Desired volume shift of the audio
	Real m_minVolume;					// Clamped minimum value, useful when muting sound effects
	Real m_playFraction = 1.0f;
	Real m_pitchShiftMin;			// minimum pitch shift value
	Real m_pitchShiftMax;			// maximum pitch shift value
	Int m_delayMin;						// minimum delay before we'll fire up another one of these
	Int m_delayMax;						// maximum delay before we'll fire up another one of these
	Int m_limit;							// Limit to the number of these sounds that can be fired up simultaneously
	Int m_loopCount;					// number of times to loop this sound

	AudioPriority m_priority;	// Priority of this sound
	UnsignedInt m_type;								// Type of sound
	UnsignedInt m_control;						// control of sound

	std::vector<WeightedAudioName> m_soundsMorning;
	std::vector<WeightedAudioName> m_sounds;
	std::vector<WeightedAudioName> m_soundsNight;
	std::vector<WeightedAudioName> m_soundsEvening;

	std::vector<WeightedAudioName> m_attackSounds;
	std::vector<WeightedAudioName> m_decaySounds;

	Real m_lowPassFreq;			// When performing low pass filters, what is the maximum frequency heard, expressed as a percentage?
	Real m_minDistance;			// less than this distance and the sound behaves as though it is at minDistance
	Real m_maxDistance;			// greater than this distance and the sound behaves as though it is muted

	AudioType m_soundType;
	Real m_perFileVolumeShift = 0.0f;
	Real m_perFilePitchMin = 1.0f;
	Real m_perFilePitchMax = 1.0f;


  // DynamicAudioEventInfo interfacing functions
  virtual Bool isLevelSpecific() const { return false; } ///< If true, this sound is only defined on the current level and can be deleted when that level ends
  virtual DynamicAudioEventInfo * getDynamicAudioEventInfo() { return nullptr; }  ///< If this object is REALLY a DynamicAudioEventInfo, return a pointer to the derived class
  virtual const DynamicAudioEventInfo * getDynamicAudioEventInfo() const { return nullptr; } ///< If this object is REALLY a DynamicAudioEventInfo, return a pointer to the derived class

  /// Is this a permanent sound? That is, if I start this sound up, will it ever end
  /// "on its own" or only if I explicitly kill it?
  Bool isPermanentSound() const { return BitIsSet( m_control, AC_LOOP ) && (m_loopCount == 0 );  }

	static const FieldParse m_audioEventInfo[];		///< the parse table for INI definition
	const FieldParse *getFieldParse() const { return m_audioEventInfo; }
};
