/*
**	Command & Conquer Generals(tm)
**	Copyright 2026 Stephan Vedder
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
//                                                                            //
//  (c) 2001-2003 Electronic Arts Inc.                                       //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

// FILE: MiniAudioManager.cpp
/*---------------------------------------------------------------------------*/
/* Project:    RTS3                                                          */
/* File name:  MiniAudioManager.cpp                                          */
/* Created:    Stephan Vedder (feliwir), June 2026                           */
/*             Adapted for GeneralsX Core architecture                       */
/* Desc:       Implementation for the MiniAudioManager, interfacing with    */
/*             the miniaudio Sound System.                                   */
/* Reference:  https://github.com/feliwir/CnC_Generals_Zero_Hour            */
/*---------------------------------------------------------------------------*/

#include "Lib/BaseType.h"
#define MINIAUDIO_IMPLEMENTATION
#include "MiniAudioDevice/MiniAudioManager.h"
#include "MiniAudioDevice/MiniAudioReverb.h"
#include "MiniAudioDevice/MiniAudioStream.h"

#include "Common/AudioAffect.h"
#include "Common/AudioHandleSpecialValues.h"
#include "Common/AudioRandomValue.h"
#include "Common/AudioRequest.h"
#include "Common/AudioSettings.h"
#include "Common/AsciiString.h"
#include "Common/AudioEventInfo.h"
#include "Common/AudioRandomValue.h"
#include "Common/FileSystem.h"
#include "Common/GameCommon.h"
#include "Common/GameSounds.h"
#include "Common/CRCDebug.h"
#include "Common/GlobalData.h"

#include "GameClient/DebugDisplay.h"
#include "GameClient/Drawable.h"
#include "GameClient/GameClient.h"
#include "GameClient/VideoPlayer.h"
#include "GameClient/View.h"

#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"
// GeneralsX @bugfix fbraz3 24/06/2026 Save and restore FPU precision mode when calling audio manager entrypoints.
#include "GameLogic/FPUControl.h"

#include "Common/file.h"
#include "VideoDevice/FFmpeg/FFmpegFile.h"

#include <atomic>
#include <cmath>
#include <random>

extern "C" {
#include <libavcodec/avcodec.h>
}

#ifdef _INTERNAL
//#pragma optimize("", off)
#endif

enum { INFINITE_LOOP_COUNT = 1000000 };

// Callback functions for miniaudio's virtual file system
static ma_result vfsFileOpen(ma_vfs *pVFS, const char *filename, ma_uint32 mode, ma_vfs_file *pFile);
static ma_result vfsFileClose(ma_vfs *pVFS, ma_vfs_file file);
static ma_result vfsFileInfo(ma_vfs *pVFS, ma_vfs_file file, ma_file_info *pInfo);
static ma_result vfsFileRead(ma_vfs *pVFS, ma_vfs_file file, void *pBuffer, size_t bytesToRead, size_t *pBytesRead);
static ma_result vfsFileTell(ma_vfs *pVFS, ma_vfs_file file, ma_int64 *pCursor);
static ma_result vfsFileSeek(ma_vfs *pVFS, ma_vfs_file file, ma_int64 offset, ma_seek_origin origin);

static ObjectID audioOwnerObject(AudioEventRTS *event)
{
	if (!event)
		return INVALID_ID;
	if (event->getOwnerType() == OT_Object)
		return event->getObjectID();
	if (event->getOwnerType() == OT_Drawable && TheGameClient) {
		Drawable *drawable = TheGameClient->findDrawableByID(event->getDrawableID());
		if (drawable && drawable->getObject())
			return drawable->getObject()->getID();
	}
	return INVALID_ID;
}

namespace
{
struct AudioPcm
{
	std::vector<uint8_t> bytes;
	ma_format format = ma_format_unknown;
	ma_uint32 rate = 0;
	ma_uint32 channels = 0;

	ma_uint32 frameBytes() const { return ma_get_bytes_per_frame(format, channels); }
	ma_uint64 frames() const { return bytes.size() / frameBytes(); }
};

Bool decodeAudioPcm(const AsciiString& filename, AudioPcm& pcm)
{
	File* file = TheFileSystem->openFile(filename.str());
	if (!file)
		return FALSE;

	FFmpegFile decoder;
	if (!decoder.open(file))
		return FALSE;

	decoder.setFrameCallback([&](AVFrame* frame, int, int streamType, void*) {
		if (streamType != AVMEDIA_TYPE_AUDIO)
			return;

		const auto format = static_cast<AVSampleFormat>(frame->format);
		switch (av_get_packed_sample_fmt(format))
		{
			case AV_SAMPLE_FMT_U8: pcm.format = ma_format_u8; break;
			case AV_SAMPLE_FMT_S16: pcm.format = ma_format_s16; break;
			case AV_SAMPLE_FMT_S32: pcm.format = ma_format_s32; break;
			case AV_SAMPLE_FMT_FLT: pcm.format = ma_format_f32; break;
			default: return;
		}
		pcm.rate = frame->sample_rate;
		pcm.channels = frame->ch_layout.nb_channels;
		const int frameSize = av_samples_get_buffer_size(nullptr, pcm.channels, frame->nb_samples, format, 1);
		if (frameSize <= 0)
			return;

		if (av_sample_fmt_is_planar(format))
		{
			const int sampleBytes = av_get_bytes_per_sample(format);
			for (int sample = 0; sample < frame->nb_samples; ++sample)
				for (ma_uint32 channel = 0; channel < pcm.channels; ++channel)
				{
					const uint8_t* start = frame->extended_data[channel] + sample * sampleBytes;
					pcm.bytes.insert(pcm.bytes.end(), start, start + sampleBytes);
				}
		}
		else
			pcm.bytes.insert(pcm.bytes.end(), frame->data[0], frame->data[0] + frameSize);
	});
	while (decoder.decodePacket()) {}
	return pcm.rate > 0 && pcm.channels > 0 && pcm.channels <= MA_MAX_CHANNELS &&
		!pcm.bytes.empty() && pcm.bytes.size() % pcm.frameBytes() == 0;
}
}

class AudioLoopSource : public ma_data_source_base
{
public:
	struct Segment
	{
		PortionToPlay portion;
		ma_uint64 offset;
		ma_uint64 frames;
	};

	AudioLoopSource(const AudioPcm& first, Bool sequential, Int count, Int initialIndex, Bool record)
		: m_format(first.format), m_rate(first.rate), m_channels(first.channels),
		  m_frameBytes(first.frameBytes()), m_sequential(sequential), m_remaining(count),
		  m_index(initialIndex), m_record(record),
		  m_rng(sequential ? std::minstd_rand::default_seed :
			  static_cast<unsigned int>(GameAudioRandomValue(1, std::minstd_rand::max() - 1)))
	{
		static const ma_data_source_vtable callbacks = {read, nullptr, getFormat, nullptr, nullptr, nullptr, 0};
		ma_data_source_config config = ma_data_source_config_init();
		config.vtable = &callbacks;
		ma_data_source_init(&config, this);
	}

	~AudioLoopSource() { ma_data_source_uninit(this); }

	Bool init(AudioEventRTS& event, const AudioPcm& first)
	{
		const auto size = event.getAudioEventInfo()->m_sounds.size();
		if (m_index >= size || m_remaining < 0)
			return FALSE;
		m_variants.resize(size);
		m_portion = event.getNextPlayPortion();
		for (size_t i = 0; i < size; ++i)
		{
			if (i == m_index && m_portion == PP_Sound)
				m_variants[i] = first.bytes;
			else if (!load(event.getSoundVariantFilename(i), m_variants[i]))
				return FALSE;
		}
		if (m_portion == PP_Attack)
			m_attack = first.bytes;
		if (m_remaining > 0 && !event.getDecayFilename().isEmpty() && !load(event.getDecayFilename(), m_decay))
			return FALSE;
		if (m_record)
		{
			segments.push_back({m_portion, 0, currentClip().size() / m_frameBytes});
			m_segmentRecorded = TRUE;
		}
		return TRUE;
	}

	ma_uint32 rate() const { return m_rate; }
	ma_uint32 channels() const { return m_channels; }
	ma_uint32 frameBytes() const { return m_frameBytes; }
	Bool playingDecay() const { return m_playingDecay.load(std::memory_order_relaxed); }
	std::vector<Segment> segments;

private:
	Bool load(const AsciiString& filename, std::vector<uint8_t>& into)
	{
		AudioPcm clip;
		if (!decodeAudioPcm(filename, clip))
			return FALSE;
		if (clip.format == m_format && clip.rate == m_rate && clip.channels == m_channels)
		{
			into = std::move(clip.bytes);
			return TRUE;
		}
		const auto frames = ma_convert_frames(nullptr, 0, m_format, m_channels, m_rate,
			clip.bytes.data(), clip.frames(), clip.format, clip.channels, clip.rate);
		into.resize(frames * m_frameBytes);
		const auto written = ma_convert_frames(into.data(), frames, m_format, m_channels, m_rate,
			clip.bytes.data(), clip.frames(), clip.format, clip.channels, clip.rate);
		into.resize(written * m_frameBytes);
		return written > 0;
	}

	const std::vector<uint8_t>& currentClip() const
	{
		if (m_portion == PP_Attack)
			return m_attack;
		if (m_portion == PP_Decay)
			return m_decay;
		return m_variants[m_index];
	}

	void advance()
	{
		if (m_portion == PP_Attack)
			m_portion = PP_Sound;
		else if (m_portion == PP_Decay)
			m_portion = PP_Done;
		else if (m_remaining > 0 && --m_remaining == 0)
		{
			if (m_decay.empty())
				m_portion = PP_Done;
			else
			{
				m_portion = PP_Decay;
				m_playingDecay.store(TRUE, std::memory_order_relaxed);
			}
		}
		else if (m_sequential)
			m_index = (m_index + 1) % m_variants.size();
		else if (m_variants.size() > 1)
		{
			const auto next = std::uniform_int_distribution<size_t>(0, m_variants.size() - 2)(m_rng);
			m_index = next + (next >= m_index);
		}
		m_clipOffset = 0;
		m_segmentRecorded = FALSE;
	}

	static ma_result read(ma_data_source* source, void* output, ma_uint64 requested, ma_uint64* count)
	{
		ScopedFPUGuard fpuGuard;
		auto& loop = *static_cast<AudioLoopSource*>(static_cast<ma_data_source_base*>(source));
		*count = 0;
		while (*count < requested && loop.m_portion != PP_Done)
		{
			const auto& clip = loop.currentClip();
			const ma_uint64 length = clip.size() / loop.m_frameBytes;
			if (loop.m_record && !loop.m_segmentRecorded)
			{
				loop.segments.push_back({loop.m_portion, loop.m_outputOffset, length});
				loop.m_segmentRecorded = TRUE;
			}
			const auto frames = std::min(requested - *count, length - loop.m_clipOffset);
			std::memcpy(static_cast<uint8_t*>(output) + *count * loop.m_frameBytes,
				clip.data() + loop.m_clipOffset * loop.m_frameBytes, frames * loop.m_frameBytes);
			*count += frames;
			loop.m_clipOffset += frames;
			loop.m_outputOffset += frames;
			if (loop.m_clipOffset == length)
				loop.advance();
		}
		return *count ? MA_SUCCESS : MA_AT_END;
	}

	static ma_result getFormat(ma_data_source* source, ma_format* format, ma_uint32* channels,
		ma_uint32* rate, ma_channel* channelMap, size_t capacity)
	{
		const auto& loop = *static_cast<AudioLoopSource*>(static_cast<ma_data_source_base*>(source));
		if (format) *format = loop.m_format;
		if (channels) *channels = loop.m_channels;
		if (rate) *rate = loop.m_rate;
		if (channelMap)
			ma_channel_map_init_standard(ma_standard_channel_map_default, channelMap, capacity, loop.m_channels);
		return MA_SUCCESS;
	}

	ma_format m_format;
	ma_uint32 m_rate, m_channels, m_frameBytes;
	Bool m_sequential;
	Int m_remaining;
	size_t m_index;
	Bool m_record;
	std::minstd_rand m_rng;
	PortionToPlay m_portion = PP_Done;
	std::vector<std::vector<uint8_t>> m_variants;
	std::vector<uint8_t> m_attack;
	std::vector<uint8_t> m_decay;
	ma_uint64 m_clipOffset = 0;
	ma_uint64 m_outputOffset = 0;
	Bool m_segmentRecorded = FALSE;
	std::atomic<Bool> m_playingDecay{FALSE};
};

//-------------------------------------------------------------------------------------------------
MiniAudioManager::MiniAudioManager() :
	m_playbackDeviceCount(0),
	m_selectedPlaybackDevice(PROVIDER_ERROR),
	m_selectedSpeakerType(0),
	m_lastSelectedPlaybackDevice(PROVIDER_ERROR),
	m_binkHandle(NULL),
	m_pref3DProvider(AsciiString::TheEmptyString),
	m_prefSpeaker(AsciiString::TheEmptyString)
{
}

//-------------------------------------------------------------------------------------------------
MiniAudioManager::~MiniAudioManager()
{
	DEBUG_ASSERTCRASH(m_binkHandle == NULL, ("Leaked a Bink handle. Chuybregts"));
	releaseHandleForBink();
	closeDevice();

	DEBUG_ASSERTCRASH(this == TheAudio, ("Umm...\n"));
	TheAudio = NULL;
}

//-------------------------------------------------------------------------------------------------
#if defined(_DEBUG) || defined(_INTERNAL)
AudioHandle MiniAudioManager::addAudioEvent(const AudioEventRTS *eventToAdd)
{
	ScopedFPUGuard fpuGuard;
	if (TheGlobalData->m_preloadReport) {
		if (!eventToAdd->getEventName().isEmpty()) {
			m_allEventsLoaded.insert(eventToAdd->getEventName());
		}
	}

	return AudioManager::addAudioEvent(eventToAdd);
}
#endif

#if defined(_DEBUG) || defined(_INTERNAL)
//-------------------------------------------------------------------------------------------------
void MiniAudioManager::audioDebugDisplay(DebugDisplayInterface *dd, void *, FILE *fp)
{
	std::list<PlayingAudio *>::iterator it;

	Coord3D lookPos;
	TheTacticalView->getPosition(&lookPos);
	lookPos.z = TheTerrainLogic->getGroundHeight(lookPos.x, lookPos.y);
	const Coord3D *mikePos = TheAudio->getListenerPosition();
	Coord3D distanceVector = TheTacticalView->get3DCameraPosition();
	distanceVector.sub(mikePos);

	if (dd)
	{
		dd->printf("MiniAudio version: %s    ", ma_version_string());
		dd->printf("Sound: %s    ", (isOn(AudioAffect_Sound) ? "Yes" : "No"));
		dd->printf("3DSound: %s    ", (isOn(AudioAffect_Sound3D) ? "Yes" : "No"));
		dd->printf("Speech: %s    ", (isOn(AudioAffect_Speech) ? "Yes" : "No"));
		dd->printf("Music: %s\n", (isOn(AudioAffect_Music) ? "Yes" : "No"));
		dd->printf("Channels Available: ");
		dd->printf("%d Sounds    ", m_sound->getAvailableSamples());
		dd->printf("Volume: ");
		dd->printf("Sound: %d    ", REAL_TO_INT(m_soundVolume * 100.0f));
		dd->printf("3DSound: %d    ", REAL_TO_INT(m_sound3DVolume * 100.0f));
		dd->printf("Speech: %d    ", REAL_TO_INT(m_speechVolume * 100.0f));
		dd->printf("Music: %d\n", REAL_TO_INT(m_musicVolume * 100.0f));
		dd->printf("Current 3D Provider: %s    ", TheAudio->getProviderName(getSelectedProvider()).str());
		dd->printf("Current Speaker Type: %s\n", TheAudio->translateUnsignedIntToSpeakerType(TheAudio->getSpeakerType()).str());
		dd->printf("Looking at: (%d,%d,%d) -- Microphone at: (%d,%d,%d)\n",
			(Int)lookPos.x, (Int)lookPos.y, (Int)lookPos.z, (Int)mikePos->x, (Int)mikePos->y, (Int)mikePos->z);
		dd->printf("Camera distance from microphone: %d -- Zoom Volume: %d%%\n",
			(Int)distanceVector.length(), (Int)(TheAudio->getZoomVolume() * 100.0f));
		dd->printf("-----------------------------------------------------------\n");
		dd->printf("Playing Audio:\n");

		int i = 0;
		for (it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it) {
			PlayingAudio *playing = *it;
			if (!playing || !playing->m_audioEventRTS) continue;
			AsciiString fn = playing->m_audioEventRTS->getFilename();
			fn = fn.reverseFind('\\') + 1;
			Real volume = 100.0f * getEffectiveVolume(playing->m_audioEventRTS);
			dd->printf("%2d: %-20s - (%s) Volume: %d\n", i++,
				playing->m_audioEventRTS->getEventName().str(), fn.str(), REAL_TO_INT(volume));
		}
	}

	if (fp)
	{
		fprintf(fp, "MiniAudio version: %s\n", ma_version_string());
	}
}
#endif

//-------------------------------------------------------------------------------------------------
void MiniAudioManager::init()
{
	ScopedFPUGuard fpuGuard;
	AudioManager::init();
#ifdef INTENSE_DEBUG
	return;
#endif
	openDevice();
}

//-------------------------------------------------------------------------------------------------
void MiniAudioManager::postProcessLoad()
{
	AudioManager::postProcessLoad();
}

//-------------------------------------------------------------------------------------------------
void MiniAudioManager::reset()
{
#if defined(_DEBUG) || defined(_INTERNAL)
	dumpAllAssetsUsed();
	m_allEventsLoaded.clear();
#endif

	AudioManager::reset();
	m_completedMusicByHandle.clear();
	m_completedMusicByName.clear();
	stopAllAudioImmediately();
	if (m_reverb)
		m_reverb->setRoom(RetailScriptAudio::None, 0.0f);
	// GeneralsX @bugfix coolswood 18/07/2026 Restart sound groups in reset() to fix silent audio after loadGame.
	// stopAllAudioImmediately() releases individual PlayingAudio entries but does not touch the four
	// ma_sound_group nodes. pauseAudio() and stopAudio() used to stop these groups (e.g. via the ESC
	// menu used to pick "Load"), so every new sound stayed silent after loadGame() until the game was
	// paused and resumed again. They now pause or stop only the playing sounds and leave the groups
	// running; resumeAudio() still starts the groups, so this call keeps audio working after a save
	// load even if a group was stopped some other way. Guarded by the per-type *On flags: if the
	// device never opened (openDevice bailed out), the groups were never ma_sound_group_init'd, so
	// touching them would be unsafe.
	if (m_deviceOpened && (m_soundOn || m_sound3DOn || m_speechOn || m_musicOn)) {
		resumeAudio(AudioAffect_All);
	}
}

//-------------------------------------------------------------------------------------------------
void MiniAudioManager::update()
{
	ScopedFPUGuard fpuGuard;

	AudioManager::update();
	if (m_reverb)
	{
		const auto room = getScriptAudio().getRoom();
		const Real gain = getAudioSettings()->m_roomReverbGain[room];
		if (m_reverb->setRoom(room, std::isfinite(gain) ? gain : 0.0f))
			m_volumeHasChanged = TRUE;
	}
	setDeviceListenerPosition();
	processRequestList();
	processPlayingList();
	playReadyContinuations();
	processFadingList();
	processStoppedList();
}

//-------------------------------------------------------------------------------------------------
// GeneralsX @bugfix elmeriniemela 17/10/2026 Match playing sounds and pending play requests to the audio
// types in 'which', so that pauseAudio(), resumeAudio() and stopAudio() only affect those types.
static Bool isEventAffectedBy(AudioEventRTS *event, Bool positional, AudioAffect which)
{
	if (!event || !event->getAudioEventInfo())
		return FALSE;

	switch (event->getAudioEventInfo()->m_soundType)
	{
	case AT_Music:     return BitIsSet(which, AudioAffect_Music);
	case AT_Streaming: return BitIsSet(which, AudioAffect_Speech);
	default:
		if (positional)
			return BitIsSet(which, AudioAffect_Sound3D);
		return BitIsSet(which, AudioAffect_Sound);
	}
}

static Bool isAffectedBy(PlayingAudio *playing, AudioAffect which)
{
	if (!playing || !playing->m_audioEventRTS)
		return FALSE;
	return isEventAffectedBy(playing->m_audioEventRTS.Peek(), playing->m_type == PAT_3DSample, which);
}

//-------------------------------------------------------------------------------------------------
void MiniAudioManager::stopAudio(AudioAffect which)
{
	// GeneralsX @bugfix meerzulee 19/07/2026 FP env guard for audio entry point (see #215)
	ScopedFPUGuard fpuGuard;
	// GeneralsX @bugfix elmeriniemela 17/10/2026 Like the Miles backend, stop the sounds that are playing now.
	// Stopping the whole sound group would also silence every sound started later.
	for (auto it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it) {
		if (isAffectedBy(*it, which))
			(*it)->m_requestStop = true;
	}
}

//-------------------------------------------------------------------------------------------------
void MiniAudioManager::pauseAudio(AudioAffect which)
{
	// GeneralsX @bugfix meerzulee 19/07/2026 FP env guard for audio entry point (see #215)
	ScopedFPUGuard fpuGuard;
	// GeneralsX @bugfix elmeriniemela 17/10/2026 Like the Miles backend, pause only the sounds that are playing
	// now. Sounds started while the game is paused (for example the buttons of the pause menu) still play.
	for (auto it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it) {
		PlayingAudio *playing = *it;
		if (!isAffectedBy(playing, which) || !playing->m_sound || playing->m_paused)
			continue;
		if (ma_sound_is_playing(playing->m_sound)) {
			ma_sound_stop(playing->m_sound);
			playing->m_paused = true;
		}
	}

	// GeneralsX @bugfix elmeriniemela 17/10/2026 Get rid of PLAY audio requests of the paused types only.
	// Requests of other types (for example speech queued in the same frame as SUSPEND_BACKGROUND_SOUNDS)
	// must still reach processRequestList().
	for (auto ait = m_audioRequests.begin(); ait != m_audioRequests.end(); ) {
		AudioRequest *req = (*ait);
		if (req && req->m_request == AR_Play && req->m_pendingEvent
			&& isEventAffectedBy(req->m_pendingEvent.Peek(), req->m_pendingEvent->isPositionalAudio(), which)) {
			deleteInstance(req);
			ait = m_audioRequests.erase(ait);
		}
		else {
			++ait;
		}
	}
}

//-------------------------------------------------------------------------------------------------
void MiniAudioManager::resumeAudio(AudioAffect which)
{
	resumeAudioFaded(which, 0);
}

void MiniAudioManager::resumeAudioFaded(AudioAffect which, UnsignedInt milliseconds)
{
	// GeneralsX @bugfix meerzulee 19/07/2026 FP env guard for audio entry point (see #215)
	ScopedFPUGuard fpuGuard;
	// GeneralsX @bugfix elmeriniemela 17/10/2026 Restart the sounds that pauseAudio() paused.
	for (auto it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it) {
		PlayingAudio *playing = *it;
		if (!isAffectedBy(playing, which) || !playing->m_sound || !playing->m_paused)
			continue;
		if (milliseconds > 0)
			ma_sound_set_fade_in_milliseconds(playing->m_sound, 0.0f, 1.0f, milliseconds);
		ma_sound_start(playing->m_sound);
		playing->m_paused = false;
	}

	if (m_deviceOpened)
	{
		if (BitIsSet(which, AudioAffect_Sound))
			ma_sound_group_start(&m_soundGroup);
		if (BitIsSet(which, AudioAffect_Sound3D))
			ma_sound_group_start(&m_sound3DGroup);
		if (BitIsSet(which, AudioAffect_Speech))
			ma_sound_group_start(&m_speechGroup);
		if (BitIsSet(which, AudioAffect_Music))
			ma_sound_group_start(&m_musicGroup);
	}
}

void MiniAudioManager::pauseAmbient(Bool shouldPause)
{
}

//-------------------------------------------------------------------------------------------------
void MiniAudioManager::stopAllAmbientsBy(Object *obj)
{
}

//-------------------------------------------------------------------------------------------------
void MiniAudioManager::stopAllAmbientsBy(Drawable *draw)
{
}

//-------------------------------------------------------------------------------------------------
void MiniAudioManager::playAudioEvent(AudioRequest *req)
{
	AudioEventRTS *event = req->m_pendingEvent.Peek();
	if (!event) {
		return;
	}

	const AudioEventInfo *info = event->getAudioEventInfo();
	if (!info) {
		return;
	}
	const ObjectID voiceOwner = audioOwnerObject(event);
	const AudioHandle voiceGroup = event->getMultiSoundGroup();
	if ((info->m_type & ST_VOICE) && voiceOwner != INVALID_ID && isObjectPlayingVoice(voiceOwner, voiceGroup)) {
		if (!(info->m_control & AC_INTERRUPT)) {
			if (m_traceEnabled)
				traceEvent("voice-suppress", voiceOwner, event->getPlayingHandle(), event->getEventName());
			return;
		}
		stopObjectVoice(voiceOwner, voiceGroup);
	}

	AsciiString fileToPlay = event->getFilename();
	if (info->m_soundType == AT_SoundEffect) {
		if (event->getNextPlayPortion() == PP_Attack)
			fileToPlay = event->getAttackFilename();
		else if (event->getNextPlayPortion() == PP_Decay)
			fileToPlay = event->getDecayFilename();
	}
	DEBUG_LOG(("MINIAUDIO: playAudioEvent '%s' type=%d file='%s'\n",
		event->getEventName().str(), info->m_soundType, fileToPlay.str()));

	std::list<PlayingAudio *>::iterator it;

	AudioHandle handleToKill = event->getHandleToKill();
	PlayingAudio *audio = allocatePlayingAudio();
	audio->m_audioEventRTS = req->m_pendingEvent;

	// Kill existing sound if handleToKill is set
	if (handleToKill) {
		for (it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it) {
			PlayingAudio *playing = (*it);
			if (!playing) continue;
			if (playing->m_audioEventRTS && playing->m_audioEventRTS->getPlayingHandle() == handleToKill)
			{
				releasePlayingAudio(playing);
				m_playingSounds.erase(it);
				break;
			}
		}
	}
	event->setHandleToKill(0);

	ma_sound_group *groupToUse = NULL;
	ma_uint32 flags = MA_SOUND_FLAG_NO_SPATIALIZATION | MA_SOUND_FLAG_NO_PITCH;
	const Real requestedPitch = event->getPitchShift();
	const Real pitch = std::isfinite(requestedPitch) && requestedPitch > 0.0f ? requestedPitch : 1.0f;
	switch (info->m_soundType)
	{
	case AT_Music:
		groupToUse = &m_musicGroup;
		break;
	case AT_Streaming:
		groupToUse = &m_speechGroup;
		break;
	case AT_AmbientStream:
	case AT_SoundEffect:
		if (event->isPositionalAudio()) {
			groupToUse = &m_sound3DGroup;
			flags = 0; // Allow spatialization for 3D
		}
		else {
			groupToUse = &m_soundGroup;
		}
		break;
	}

	if (pitch != 1.0f)
		flags &= ~MA_SOUND_FLAG_NO_PITCH;
	if (!m_deviceOpened && !m_headlessVirtualPlayback) {
		releasePlayingAudio(audio);
		return;
	}

	if (fileToPlay.isEmpty()) {
		releasePlayingAudio(audio);
		return;
	}

	AudioPcm pcm;
	if (!decodeAudioPcm(fileToPlay, pcm)) {
		if (m_headlessVirtualPlayback) {
			std::fprintf(stderr, "AUDIO TRACE cannot decode event=%s\n", event->getEventName().str());
			std::abort();
		}
		DEBUG_LOG(("MiniAudio: Failed to decode: %s\n", fileToPlay.str()));
		releasePlayingAudio(audio);
		return;
	}

	const auto portionToPlay = event->getNextPlayPortion();
	if (!req->m_isContinuation && info->m_soundType == AT_SoundEffect && (info->m_control & AC_LOOP) &&
		info->m_delayMin <= 0 && info->m_delayMax <= 0 && !event->getIsLogicalAudio() &&
		(portionToPlay == PP_Attack || portionToPlay == PP_Sound))
	{
		const Bool sequential = (info->m_control & AC_SEQUENTIAL) && !(info->m_control & AC_RANDOM);
		auto loop = std::make_unique<AudioLoopSource>(pcm, sequential, event->getLoopCount(),
			event->getPlayingAudioIndex(), m_headlessVirtualPlayback && m_traceEnabled);
		if (loop->init(*event, pcm))
			audio->m_loopSource = loop.release();
	}

	const auto pcmFrames = pcm.frames();
	const bool ambientStream = info->m_soundType == AT_AmbientStream;
	const ma_uint64 startFrame = ambientStream && pcmFrames > 1 && (info->m_control & AC_RANDOMSTART)
		? static_cast<ma_uint64>(static_cast<double>(pcmFrames - 1) * GameAudioRandomValueReal(0.0f, 1.0f))
		: 0;
	if (m_headlessVirtualPlayback)
	{
		audio->m_virtualDurationMs = static_cast<Real>(pcm.frames()) * static_cast<Real>(MSEC_PER_SECOND) / (static_cast<Real>(pcm.rate) * pitch);
		audio->m_virtualElapsedMs = static_cast<Real>(startFrame) * static_cast<Real>(MSEC_PER_SECOND) / (static_cast<Real>(pcm.rate) * pitch);
		audio->m_virtualLastFrame = TheGameLogic ? TheGameLogic->getFrame() : 0;
		audio->m_type = info->m_soundType == AT_Music || info->m_soundType == AT_Streaming ? PAT_Stream :
			(event->isPositionalAudio() ? PAT_3DSample : ambientStream ? PAT_Stream : PAT_Sample);
		audio->m_fadeIn = event->getShouldFade();
		m_playingSounds.push_back(audio);
		if (m_traceEnabled) {
			if (audio->m_loopSource) {
				traceLoopSegments(*event, *audio->m_loopSource);
			} else {
				std::fprintf(stderr, "AUDIO DURATION handle=%u ms=%.3f bytes=%zu rate=%u channels=%u samplebytes=%u\n",
					static_cast<UnsignedInt>(event->getPlayingHandle()), audio->m_virtualDurationMs,
					pcm.bytes.size(), pcm.rate, pcm.channels, ma_get_bytes_per_sample(pcm.format));
				const ObjectID owner = event->getOwnerType() == OT_Object ? event->getObjectID() : INVALID_ID;
				const char *portion = info->m_soundType != AT_SoundEffect ? nullptr :
					(event->getNextPlayPortion() == PP_Attack ? "attack" :
					(event->getNextPlayPortion() == PP_Decay ? "decay" : "sound"));
				traceEvent(req->m_isContinuation ? "segment" : "start", owner, event->getPlayingHandle(), event->getEventName(), portion);
			}
		}
		return;
	}

	ma_data_source* source = audio->m_loopSource;
	if (!source)
	{
		auto* buffer = static_cast<ma_audio_buffer*>(malloc(sizeof(ma_audio_buffer)));
		const auto config = ma_audio_buffer_config_init(pcm.format, pcm.channels, pcm.frames(), pcm.bytes.data(), nullptr);
		if (!buffer || ma_audio_buffer_init_copy(&config, buffer) != MA_SUCCESS)
		{
			free(buffer);
			releasePlayingAudio(audio);
			return;
		}
		buffer->ref.sampleRate = pcm.rate;
		audio->m_audioBuffer = buffer;
		source = buffer;
	}

	ma_sound *sound = (ma_sound *)malloc(sizeof(ma_sound));
	if (!sound || ma_sound_init_from_data_source(&m_engine, source, flags, groupToUse, sound) != MA_SUCCESS) {
		free(sound);
		releasePlayingAudio(audio);
		return;
	}

	audio->m_sound = sound;
	audio->m_dryGroup = groupToUse;
	if (pitch != 1.0f)
		ma_sound_set_pitch(sound, pitch);

	switch (info->m_soundType)
	{
	case AT_Music:
	case AT_Streaming:
	{
		if ((info->m_soundType == AT_Streaming) && event->getUninterruptible()) {
			stopAllSpeech();
		}
		audio->m_type = PAT_Stream;
		if ((info->m_soundType == AT_Streaming) && event->getUninterruptible()) {
			setDisallowSpeech(TRUE);
		}
		// GeneralsX @bugfix elmeriniemela 17/10/2026 Like the Miles backend (INFINITE_LOOP_COUNT), music
		// tracks repeat until they are stopped.
		if (info->m_soundType == AT_Music && !event->getPlayMusicOnce()) {
			ma_sound_set_looping(sound, MA_TRUE);
		}
		break;
	}

	case AT_AmbientStream:
	case AT_SoundEffect:
	{
		if (event->isPositionalAudio()) {
			audio->m_type = PAT_3DSample;
			if (event->getAudioEventInfo()->m_type & ST_GLOBAL) {
				ma_sound_set_min_distance(sound, TheAudio->getAudioSettings()->m_globalMinRange);
				ma_sound_set_max_distance(sound, TheAudio->getAudioSettings()->m_globalMaxRange);
			}
			else {
				ma_sound_set_min_distance(sound, event->getAudioEventInfo()->m_minDistance);
				ma_sound_set_max_distance(sound, event->getAudioEventInfo()->m_maxDistance);
			}
			const Coord3D *pos = event->getCurrentPosition();
			if (pos) {
				ma_sound_set_position(sound, pos->x, pos->y, pos->z);
			}

		}
		else {
			audio->m_type = ambientStream ? PAT_Stream : PAT_Sample;

		}
		break;
	}
	}

	// GeneralsX @bugfix Mr. Meeseeks 19/06/2026 - Initialize volume using adjustPlayingVolume
	adjustPlayingVolume(audio);
	if ((event->getPlayMusicOnce() && event->getShouldFade()) ||
		((info->m_control & AC_FADE_ON_START) && !req->m_isContinuation))
	{
		const auto duration = static_cast<ma_uint64>(getAudioSettings()->m_fadeAudioFrames) * MSEC_PER_SECOND / LOGICFRAMES_PER_SECOND;
		ma_sound_set_fade_in_milliseconds(sound, 0.0f, 1.0f, duration);
	}

	if (startFrame > 0)
		ma_sound_seek_to_pcm_frame(sound, startFrame);
	const ma_result result = ma_sound_start(sound);
	if (result != MA_SUCCESS) {
		DEBUG_LOG(("MiniAudio: Failed to start sound: %d for '%s'\n", result, fileToPlay.str()));
		releasePlayingAudio(audio);
		return;
	}

	m_playingSounds.push_back(audio);
	if (m_traceEnabled)
	{
		const ObjectID owner = event->getOwnerType() == OT_Object ? event->getObjectID() : INVALID_ID;
		const char *portion = info->m_soundType != AT_SoundEffect ?
			(event->getLogicalTrackName().isNotEmpty() ? event->getLogicalTrackName().str() : nullptr) :
			(event->getNextPlayPortion() == PP_Attack ? "attack" :
			(event->getNextPlayPortion() == PP_Decay ? "decay" : "sound"));
		traceEvent(req->m_isContinuation ? "segment" : "start", owner, event->getPlayingHandle(), event->getEventName(),
			portion);
	}
}

void MiniAudioManager::traceLoopSegments(AudioEventRTS& event, AudioLoopSource& loop) const
{
	const ObjectID owner = event.getOwnerType() == OT_Object ? event.getObjectID() : INVALID_ID;
	const auto handle = static_cast<UnsignedInt>(event.getPlayingHandle());
	for (const auto& segment : loop.segments)
	{
		const char* portion = segment.portion == PP_Attack ? "attack" :
			(segment.portion == PP_Decay ? "decay" : "sound");
		const auto duration = static_cast<Real>(segment.frames) * static_cast<Real>(MSEC_PER_SECOND) / loop.rate();
		std::fprintf(stderr, "AUDIO DURATION handle=%u ms=%.3f bytes=%llu rate=%u channels=%u samplebytes=%u\n",
			handle, duration, static_cast<unsigned long long>(segment.frames * loop.frameBytes()),
			loop.rate(), loop.channels(), loop.frameBytes() / loop.channels());
		traceEvent(segment.offset == 0 ? "start" : "segment", owner, event.getPlayingHandle(), event.getEventName(), portion);
		std::fprintf(stderr, "AUDIO PCM SEGMENT event=%s handle=%u start=%llu length=%llu\n",
			event.getEventName().str(), handle, static_cast<unsigned long long>(segment.offset),
			static_cast<unsigned long long>(segment.frames));
	}
	std::fflush(stderr);
	loop.segments.clear();
}

//-------------------------------------------------------------------------------------------------
void MiniAudioManager::stopAudioEvent(AudioHandle handle)
{
	// GeneralsX @bugfix meerzulee 19/07/2026 FP env guard for audio entry point (see #215)
	ScopedFPUGuard fpuGuard;
	std::list<PlayingAudio *>::iterator it;

	// Handle special music stop commands
	if (handle == AHSV_StopTheMusic || handle == AHSV_StopTheMusicFade) {
		for (it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it) {
			PlayingAudio *audio = (*it);
			if (!audio || !audio->m_audioEventRTS) continue;

			const AudioEventInfo *stopInfo = audio->m_audioEventRTS->getAudioEventInfo();
			if (stopInfo && stopInfo->m_soundType == AT_Music)
			{
				if (handle == AHSV_StopTheMusicFade)
					m_fadingAudio.push_back(audio);
				else
					releasePlayingAudio(audio);
				m_playingSounds.erase(it);
				break;
			}
		}
	}
	for (auto request = m_audioRequests.begin(); request != m_audioRequests.end(); ++request) {
		AudioRequest *pending = *request;
		if (!pending || pending->m_request != AR_Play || !pending->m_pendingEvent ||
			pending->m_pendingEvent->getPlayingHandle() != handle)
			continue;
		AudioEventRTS *event = pending->m_pendingEvent.Peek();
		const AudioEventInfo *info = event->getAudioEventInfo();
		if (pending->m_isContinuation && info && (info->m_control & AC_LOOP) &&
			!event->getDecayFilename().isEmpty() && event->getNextPlayPortion() != PP_Decay) {
			event->setNextPlayPortion(PP_Decay);
			event->decrementDelay(event->getDelay());
		} else {
			deleteInstance(pending);
			m_audioRequests.erase(request);
		}
		return;
	}

	// Find and mark the audio event for stopping
	for (it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it) {
		PlayingAudio *audio = (*it);
		if (!audio || !audio->m_audioEventRTS) continue;
		if (audio->m_audioEventRTS->getPlayingHandle() == handle) {
			const AudioEventInfo* info = audio->m_audioEventRTS->getAudioEventInfo();
			if (info && (info->m_control & AC_FADE_ON_KILL)) {
				m_fadingAudio.push_back(audio);
				m_playingSounds.erase(it);
			}
			else {
				audio->m_requestStop = true;
			}
			break;
		}
	}
}

//-------------------------------------------------------------------------------------------------
void MiniAudioManager::killAudioEventImmediately(AudioHandle audioEvent)
{
	// GeneralsX @bugfix meerzulee 19/07/2026 FP env guard for audio entry point (see #215)
	ScopedFPUGuard fpuGuard;
	if (const auto found = m_multiAudioHandles.find(audioEvent); found != m_multiAudioHandles.end()) {
		const std::vector<AudioHandle> children = found->second.children;
		m_multiAudioHandles.erase(found);
		for (const AudioHandle child : children)
			killAudioEventImmediately(child);
		return;
	}
	// First look for it in the request list.
	std::list<AudioRequest *>::iterator ait;
	for (ait = m_audioRequests.begin(); ait != m_audioRequests.end(); ait++)
	{
		AudioRequest *req = (*ait);
		if (req && req->m_request == AR_Play && req->m_handleToInteractOn == audioEvent)
		{
			deleteInstance(req);
			ait = m_audioRequests.erase(ait);
			return;
		}
	}

	std::list<PlayingAudio *>::iterator it;
	for (it = m_playingSounds.begin(); it != m_playingSounds.end(); it++)
	{
		PlayingAudio *audio = (*it);
		if (!audio) continue;
		if (audio->m_audioEventRTS && audio->m_audioEventRTS->getPlayingHandle() == audioEvent)
		{
			releasePlayingAudio(audio);
			m_playingSounds.erase(it);
			return;
		}
	}
}

//-------------------------------------------------------------------------------------------------
void MiniAudioManager::pauseAudioEvent(AudioHandle handle)
{
	// GeneralsX @bugfix meerzulee 19/07/2026 FP env guard for audio entry point (see #215)
	ScopedFPUGuard fpuGuard;
	// TODO: implement individual sound pause
}

//-------------------------------------------------------------------------------------------------
PlayingAudio *MiniAudioManager::allocatePlayingAudio(void)
{
	PlayingAudio *aud = NEW PlayingAudio;
	return aud;
}

//-------------------------------------------------------------------------------------------------
void MiniAudioManager::releaseMiniAudioHandles(PlayingAudio *release)
{
	// Uninit sound BEFORE audio buffer — sound holds reference to buffer
	if (release->m_sound) {
		ma_sound_uninit(release->m_sound);
		free(release->m_sound);
		release->m_sound = NULL;
	}
	if (release->m_reverbSends)
	{
		ma_splitter_node_uninit(release->m_reverbSends.get(), nullptr);
		release->m_reverbSends.reset();
	}
	if (release->m_audioBuffer) {
		ma_audio_buffer_uninit(release->m_audioBuffer);
		free(release->m_audioBuffer);
		release->m_audioBuffer = NULL;
	}
	delete release->m_loopSource;
	release->m_loopSource = nullptr;
	release->m_type = PAT_INVALID;
}

//-------------------------------------------------------------------------------------------------
void MiniAudioManager::releasePlayingAudio(PlayingAudio *release)
{
	if (!release) return;
	if (m_traceEnabled && !release->m_continuationPending && release->m_audioEventRTS) {
		AudioEventRTS *event = release->m_audioEventRTS.Peek();
		const ObjectID owner = event->getOwnerType() == OT_Object ? event->getObjectID() : INVALID_ID;
		traceEvent("stop", owner, event->getPlayingHandle(), event->getEventName());
	}

	const AudioEventInfo *releaseInfo = release->m_audioEventRTS
		? release->m_audioEventRTS->getAudioEventInfo() : nullptr;

	// GeneralsX @bugfix elmeriniemela 17/10/2026 Allow speech again once the uninterruptible speech that
	// disallowed it is released. notifyOfAudioCompletion() is never called for finished sounds in this backend.
	if (releaseInfo && releaseInfo->m_soundType == AT_Streaming && getDisallowSpeech()
		&& release->m_audioEventRTS->getUninterruptible())
		setDisallowSpeech(FALSE);

	if (releaseInfo && releaseInfo->m_soundType == AT_SoundEffect && release->m_sound) {
		if (release->m_type == PAT_Sample) {

		}
		else if (release->m_type == PAT_3DSample) {

		}
		// PAT_Stream and PAT_INVALID don't notify
	}

	releaseMiniAudioHandles(release);
	delete release;
}

//-------------------------------------------------------------------------------------------------
void MiniAudioManager::stopAllAudioImmediately(void)
{
	// GeneralsX @bugfix meerzulee 19/07/2026 FP env guard for audio entry point (see #215)
	ScopedFPUGuard fpuGuard;
	std::list<PlayingAudio *>::iterator it;
	PlayingAudio *playing;

	for (it = m_playingSounds.begin(); it != m_playingSounds.end(); ) {
		playing = *it;
		it = m_playingSounds.erase(it); // advance before release
		if (playing) releasePlayingAudio(playing);
	}

	for (it = m_fadingAudio.begin(); it != m_fadingAudio.end(); ) {
		playing = *it;
		it = m_fadingAudio.erase(it);
		if (playing) releasePlayingAudio(playing);
	}

	for (it = m_stoppedAudio.begin(); it != m_stoppedAudio.end(); ) {
		playing = *it;
		it = m_stoppedAudio.erase(it);
		if (playing) releasePlayingAudio(playing);
	}
}

//-------------------------------------------------------------------------------------------------
void MiniAudioManager::freeAllMiniAudioHandles(void)
{
	ScopedFPUGuard fpuGuard;
	if (m_deviceOpened)
		ma_engine_stop(&m_engine);
}

//-------------------------------------------------------------------------------------------------
void MiniAudioManager::adjustPlayingVolume(PlayingAudio *audio)
{
	if (!audio || !audio->m_audioEventRTS || !audio->m_sound) return;
	ma_sound_set_volume(audio->m_sound, getEffectiveVolume(audio->m_audioEventRTS.Peek()));
	adjustReverbSends(audio);
}

void MiniAudioManager::adjustReverbSends(PlayingAudio* audio)
{
	if (!m_reverb || !audio->m_sound || !audio->m_audioEventRTS)
		return;
	AudioEventRTS& event = *audio->m_audioEventRTS.Peek();
	const AudioEventInfo* info = event.getAudioEventInfo();
	if (!info)
		return;
	const Bool positional = event.isPositionalAudio();
	const auto gain = [](Real value) { return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 1.0f; };
	const Real dry = positional ? 1.0f : gain(info->m_dryLevel);
	const Real wet = gain(info->m_reverbEffectLevel) *
		getScriptAudio().suppressionAt(positional ? event.getCurrentPosition() : nullptr);
	m_reverb->setSends(audio->m_sound, audio->m_dryGroup, audio->m_reverbSends, dry, wet);
}

//-------------------------------------------------------------------------------------------------
void MiniAudioManager::stopAllSpeech(void)
{
	// GeneralsX @bugfix meerzulee 19/07/2026 FP env guard for audio entry point (see #215)
	ScopedFPUGuard fpuGuard;
	std::list<PlayingAudio *>::iterator it;
	for (it = m_playingSounds.begin(); it != m_playingSounds.end(); ) {
		PlayingAudio *playing = (*it);
		if (!playing) { ++it; continue; }

		if (playing->m_audioEventRTS && playing->m_audioEventRTS->getAudioEventInfo() &&
			playing->m_audioEventRTS->getAudioEventInfo()->m_soundType == AT_Streaming) {
			it = m_playingSounds.erase(it);
			releasePlayingAudio(playing);
		}
		else {
			++it;
		}
	}
}

//-------------------------------------------------------------------------------------------------
AsciiString MiniAudioManager::nextMusicTrack(void)
{
	AsciiString trackName;
	for (auto it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it) {
		PlayingAudio *playing = *it;
		if (playing && playing->m_audioEventRTS && playing->m_audioEventRTS->getAudioEventInfo() &&
			playing->m_audioEventRTS->getAudioEventInfo()->m_soundType == AT_Music) {
			trackName = playing->m_audioEventRTS->getEventName();
		}
	}
	TheAudio->removeAudioEvent(AHSV_StopTheMusic);
	trackName = nextTrackName(trackName);
	AudioEventRTS newTrack(trackName);
	TheAudio->addAudioEvent(&newTrack);
	return trackName;
}

//-------------------------------------------------------------------------------------------------
AsciiString MiniAudioManager::prevMusicTrack(void)
{
	AsciiString trackName;
	for (auto it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it) {
		PlayingAudio *playing = *it;
		if (playing && playing->m_audioEventRTS && playing->m_audioEventRTS->getAudioEventInfo() &&
			playing->m_audioEventRTS->getAudioEventInfo()->m_soundType == AT_Music) {
			trackName = playing->m_audioEventRTS->getEventName();
		}
	}
	TheAudio->removeAudioEvent(AHSV_StopTheMusic);
	trackName = prevTrackName(trackName);
	AudioEventRTS newTrack(trackName);
	TheAudio->addAudioEvent(&newTrack);
	return trackName;
}

//-------------------------------------------------------------------------------------------------
Bool MiniAudioManager::isMusicPlaying(void) const
{
	// GeneralsX @bugfix elmeriniemela 17/10/2026 The music group stays started when music is paused or
	// stopped, so check for a music track that is actually playing.
	for (auto it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it) {
		PlayingAudio *playing = *it;
		if (!playing || !playing->m_sound || playing->m_paused || playing->m_requestStop)
			continue;
		const AudioEventInfo *info = playing->m_audioEventRTS ? playing->m_audioEventRTS->getAudioEventInfo() : nullptr;
		if (info && info->m_soundType == AT_Music && ma_sound_is_playing(playing->m_sound))
			return TRUE;
	}
	return FALSE;
}

//-------------------------------------------------------------------------------------------------
Bool MiniAudioManager::hasMusicTrackCompleted(const AsciiString &trackName, Int numberOfTimes) const
{
	AsciiString key = trackName;
	key.toLower();
	Int completions = 0;
	const auto done = m_completedMusicByName.find(key);
	if (done != m_completedMusicByName.end())
		completions = done->second;
	for (const PlayingAudio *playing : m_playingSounds) {
		if (playing && !playing->m_requestStop && playing->m_audioEventRTS && playing->m_audioEventRTS->getAudioEventInfo() &&
			playing->m_audioEventRTS->getEventName().compareNoCase(trackName) == 0 &&
			playing->m_audioEventRTS->getAudioEventInfo()->m_soundType == AT_Music)
			completions += playing->m_completedLoops;
	}
	return completions > 0 && completions >= numberOfTimes;
}

//-------------------------------------------------------------------------------------------------
AsciiString MiniAudioManager::getMusicTrackName(void) const
{
	for (auto ait = m_audioRequests.begin(); ait != m_audioRequests.end(); ++ait) {
		if ((*ait)->m_request != AR_Play || !(*ait)->m_pendingEvent) continue;
		if ((*ait)->m_pendingEvent->getAudioEventInfo()->m_soundType == AT_Music)
			return (*ait)->m_pendingEvent->getEventName();
	}
	for (auto it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it) {
		PlayingAudio *playing = *it;
		if (playing && playing->m_audioEventRTS && playing->m_audioEventRTS->getAudioEventInfo() &&
			playing->m_audioEventRTS->getAudioEventInfo()->m_soundType == AT_Music) {
			return playing->m_audioEventRTS->getEventName();
		}
	}
	return AsciiString::TheEmptyString;
}

//-------------------------------------------------------------------------------------------------
void MiniAudioManager::openDevice(void)
{
	if (!TheGlobalData->m_audioOn) {
		return;
	}

	ma_result result;
	ma_resource_manager_config resourceManagerConfig;
	ma_context_config contextConfig;
	ma_engine_config engineConfig;
	static ma_vfs_callbacks vfs = {};
	vfs.onOpen = vfsFileOpen;
	vfs.onClose = vfsFileClose;
	vfs.onInfo = vfsFileInfo;
	vfs.onRead = vfsFileRead;
	vfs.onTell = vfsFileTell;
	vfs.onSeek = vfsFileSeek;

	resourceManagerConfig = ma_resource_manager_config_init();
	resourceManagerConfig.pVFS = (ma_vfs *)&vfs;
	result = ma_resource_manager_init(&resourceManagerConfig, &m_resourceManager);
	if (result != MA_SUCCESS) {
		DEBUG_LOG(("MiniAudio: Failed to initialize resource manager: %d\n", result));
		setOn(false, AudioAffect_All);
		return;
	}

	result = ma_log_init(NULL, &m_log);
	if (result != MA_SUCCESS) {
		DEBUG_LOG(("MiniAudio: Failed to initialize log: %d\n", result));
		setOn(false, AudioAffect_All);
		return;
	}

	auto on_log = [](void *pUserData, ma_uint32 logLevel, const char *message) {
		DEBUG_LOG(("miniaudio [%s]: %s", ma_log_level_to_string(logLevel), message));
	};
	ma_log_register_callback(&m_log, ma_log_callback_init(on_log, NULL));

	contextConfig = ma_context_config_init();
	contextConfig.pLog = &m_log;

	result = ma_context_init(NULL, 0, &contextConfig, &m_context);
	if (result != MA_SUCCESS) {
		DEBUG_LOG(("MiniAudio: Failed to initialize context: %d\n", result));
		setOn(false, AudioAffect_All);
		return;
	}

	result = ma_context_get_devices(&m_context, &m_playbackDevices, &m_playbackDeviceCount, NULL, NULL);
	if (result != MA_SUCCESS) {
		DEBUG_LOG(("MiniAudio: Failed to enumerate devices: %d\n", result));
		ma_context_uninit(&m_context);
		setOn(false, AudioAffect_All);
		return;
	}

	engineConfig = ma_engine_config_init();
	engineConfig.pResourceManager = &m_resourceManager;

	result = ma_engine_init(&engineConfig, &m_engine);
	if (result != MA_SUCCESS) {
		DEBUG_LOG(("MiniAudio: Failed to initialize engine: %d\n", result));
		setOn(false, AudioAffect_All);
		return;
	}

	ma_sound_group_init(&m_engine, 0, NULL, &m_musicGroup);
	ma_sound_group_init(&m_engine, 0, NULL, &m_soundGroup);
	ma_sound_group_init(&m_engine, 0, NULL, &m_sound3DGroup);
	ma_sound_group_init(&m_engine, 0, NULL, &m_speechGroup);
	m_reverb = std::make_unique<MiniAudioReverb>(m_engine);
	m_deviceOpened = TRUE;

	fprintf(stderr, "AUDIO: MiniAudio backend loaded - version %s, device: %s, playback devices: %d\n",
		ma_version_string(),
		m_playbackDeviceCount > 0 ? m_playbackDevices[0].name : "default",
		m_playbackDeviceCount);

	TheAudio->refreshCachedVariables();
}

//-------------------------------------------------------------------------------------------------
void MiniAudioManager::closeDevice(void)
{
	ScopedFPUGuard fpuGuard;
	stopAllAudioImmediately();
	if (!m_deviceOpened)
		return;
	m_deviceOpened = FALSE;

	m_reverb.reset();
	ma_sound_group_uninit(&m_speechGroup);
	ma_sound_group_uninit(&m_sound3DGroup);
	ma_sound_group_uninit(&m_soundGroup);
	ma_sound_group_uninit(&m_musicGroup);
	ma_engine_uninit(&m_engine);
	ma_resource_manager_uninit(&m_resourceManager);
	ma_context_uninit(&m_context);
	ma_log_uninit(&m_log);
}

//-------------------------------------------------------------------------------------------------
Bool MiniAudioManager::isCurrentlyPlaying(AudioHandle handle)
{
	if (const auto found = m_multiAudioHandles.find(handle); found != m_multiAudioHandles.end()) {
		for (const AudioHandle child : found->second.children)
			if (isCurrentlyPlaying(child))
				return TRUE;
		return FALSE;
	}
	for (auto it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it) {
		PlayingAudio *playing = *it;
		if (playing && playing->m_audioEventRTS && playing->m_audioEventRTS->getPlayingHandle() == handle)
			return true;
	}
	for (auto ait = m_audioRequests.begin(); ait != m_audioRequests.end(); ++ait) {
		AudioRequest *req = *ait;
		if (req && req->m_pendingEvent && req->m_pendingEvent->getPlayingHandle() == handle)
			return true;
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
void MiniAudioManager::notifyOfAudioCompletion(UnsignedInt audioCompleted, UnsignedInt flags)
{
	ScopedFPUGuard fpuGuard;
	PlayingAudio *playing = findPlayingAudioFrom(audioCompleted, flags);
	if (!playing) return;
	if (!playing->m_audioEventRTS || !playing->m_audioEventRTS->getAudioEventInfo()) return;

	if (getDisallowSpeech() && playing->m_audioEventRTS->getAudioEventInfo()->m_soundType == AT_Streaming)
		setDisallowSpeech(FALSE);

	AudioEventRTS *event = playing->m_audioEventRTS.Peek();
	const AudioEventInfo *info = event->getAudioEventInfo();
	if (info->m_soundType != AT_SoundEffect)
		return;
	if (playing->m_loopSource && (!playing->m_requestStop || playing->m_loopSource->playingDecay()))
		return;
	if (playing->m_requestStop) {
		if (!(info->m_control & AC_LOOP) || event->getDecayFilename().isEmpty() ||
			event->getNextPlayPortion() == PP_Decay)
			return;
		event->setNextPlayPortion(PP_Decay);
	} else if ((info->m_control & AC_LOOP) && event->getNextPlayPortion() == PP_Sound) {
		event->decreaseLoopCount();
		if (event->hasMoreLoops())
			event->generateFilename();
		else
			event->advanceNextPlayPortion();
	} else {
		event->advanceNextPlayPortion();
	}
	if (event->getNextPlayPortion() == PP_Done)
		return;
	AudioRequest *req = allocateAudioRequest();
	req->m_pendingEvent = playing->m_audioEventRTS;
	req->m_isContinuation = TRUE;
	appendAudioRequest(req);
	playing->m_continuationPending = TRUE;
}

//-------------------------------------------------------------------------------------------------
PlayingAudio *MiniAudioManager::findPlayingAudioFrom(UnsignedInt audioCompleted, UnsignedInt)
{
	for (auto it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it) {
		PlayingAudio *playing = *it;
		if (playing && playing->m_audioEventRTS && playing->m_audioEventRTS->getPlayingHandle() == audioCompleted)
			return playing;
	}
	return NULL;
}

//-------------------------------------------------------------------------------------------------
UnsignedInt MiniAudioManager::getProviderCount(void) const
{
	return m_playbackDeviceCount;
}

//-------------------------------------------------------------------------------------------------
AsciiString MiniAudioManager::getProviderName(UnsignedInt providerNum) const
{
	if (isOn(AudioAffect_Sound3D) && providerNum < m_playbackDeviceCount)
		return m_playbackDevices[providerNum].name;
	return AsciiString::TheEmptyString;
}

//-------------------------------------------------------------------------------------------------
UnsignedInt MiniAudioManager::getProviderIndex(AsciiString providerName) const
{
	for (UnsignedInt i = 0; i < m_playbackDeviceCount; ++i) {
		if (providerName == m_playbackDevices[i].name)
			return i;
	}
	return PROVIDER_ERROR;
}

//-------------------------------------------------------------------------------------------------
void MiniAudioManager::selectProvider(UnsignedInt providerNdx) {}
void MiniAudioManager::unselectProvider(void) {}

//-------------------------------------------------------------------------------------------------
UnsignedInt MiniAudioManager::getSelectedProvider(void) const
{
	return m_selectedPlaybackDevice;
}

//-------------------------------------------------------------------------------------------------
void MiniAudioManager::setSpeakerType(UnsignedInt speakerType) {}

//-------------------------------------------------------------------------------------------------
UnsignedInt MiniAudioManager::getSpeakerType(void)
{
	return 0;
}

//-------------------------------------------------------------------------------------------------
void *MiniAudioManager::getHandleForBink(void)
{
	if (!m_binkHandle) {
		m_binkHandle = NEW MiniAudioStream;
		// Give the stream access to the engine
		static_cast<MiniAudioStream *>(m_binkHandle)->setEngine(&m_engine);
	}
	return m_binkHandle;
}

//-------------------------------------------------------------------------------------------------
void MiniAudioManager::releaseHandleForBink(void)
{
	if (m_binkHandle) {
		delete (MiniAudioStream *)m_binkHandle;
		m_binkHandle = NULL;
	}
}

//-------------------------------------------------------------------------------------------------
UnsignedInt MiniAudioManager::getNum2DSamples(void) const { return 0; }
UnsignedInt MiniAudioManager::getNum3DSamples(void) const { return 0; }
UnsignedInt MiniAudioManager::getNumStreams(void) const { return 0; }

//-------------------------------------------------------------------------------------------------
Bool MiniAudioManager::doesViolateLimit(AudioEventRTS *event) const
{
	Int limit = event->getAudioEventInfo()->m_limit;
	if (limit == 0) return false;

	Int totalCount = 0;
	Int totalRequestCount = 0;

	for (auto it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it) {
		if (!(*it)->m_audioEventRTS) continue;
		if ((*it)->m_audioEventRTS->getEventName() == event->getEventName()) {
			if (totalCount == 0)
				event->setHandleToKill((*it)->m_audioEventRTS->getPlayingHandle());
			++totalCount;
		}
	}
	for (auto arIt = m_audioRequests.begin(); arIt != m_audioRequests.end(); ++arIt) {
		AudioRequest *req = (*arIt);
		if (req && req->m_pendingEvent && req->m_pendingEvent.Peek() != event &&
			req->m_pendingEvent->getEventName() == event->getEventName()) {
			totalRequestCount++;
			totalCount++;
		}
	}

	if (event->getAudioEventInfo()->m_control & AC_INTERRUPT) {
		if (totalRequestCount < limit) {
			Int totalPlayingCount = totalCount - totalRequestCount;
			if (totalRequestCount + totalPlayingCount < limit) {
				event->setHandleToKill(0);
				return false;
			}
			return false;
		}
	}

	if (totalCount < limit) {
		event->setHandleToKill(0);
		return false;
	}
	return true;
}

//-------------------------------------------------------------------------------------------------
Bool MiniAudioManager::isPlayingAlready(AudioEventRTS *event) const
{
	for (auto it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it) {
		if (!(*it)->m_audioEventRTS) continue;
		if ((*it)->m_audioEventRTS->getEventName() == event->getEventName())
			return true;
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
Bool MiniAudioManager::isObjectPlayingVoice(UnsignedInt objID) const
{
	return isObjectPlayingVoice(objID, 0);
}

Bool MiniAudioManager::isObjectPlayingVoice(UnsignedInt objID, AudioHandle exemptGroup) const
{
	if (objID == 0) return false;
	for (auto it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it) {
		if (!(*it)->m_audioEventRTS) continue;
		if (exemptGroup != 0 && (*it)->m_audioEventRTS->getMultiSoundGroup() == exemptGroup) continue;
		const AudioEventInfo *info = (*it)->m_audioEventRTS->getAudioEventInfo();
		if (info && audioOwnerObject((*it)->m_audioEventRTS.Peek()) == objID && (info->m_type & ST_VOICE))
			return true;
	}
	return false;
}

void MiniAudioManager::stopObjectVoice(UnsignedInt objID)
{
	stopObjectVoice(objID, 0);
}

void MiniAudioManager::stopObjectVoice(UnsignedInt objID, AudioHandle exemptGroup)
{
	for (auto it = m_playingSounds.begin(); it != m_playingSounds.end();) {
		PlayingAudio *playing = *it;
		AudioEventRTS *event = playing && playing->m_audioEventRTS ? playing->m_audioEventRTS.Peek() : nullptr;
		const AudioEventInfo *info = event ? event->getAudioEventInfo() : nullptr;
		const Bool exempt = exemptGroup != 0 && event && event->getMultiSoundGroup() == exemptGroup;
		if (!exempt && info && (info->m_type & ST_VOICE) && audioOwnerObject(event) == objID) {
			if (m_traceEnabled)
				traceEvent("interrupt", static_cast<ObjectID>(objID), event->getPlayingHandle(), event->getEventName());
			it = m_playingSounds.erase(it);
			releasePlayingAudio(playing);
		} else {
			++it;
		}
	}
}

Int MiniAudioManager::getMusicTrackCompletionCount(AudioHandle handle) const
{
	const auto it = m_completedMusicByHandle.find(handle);
	return it != m_completedMusicByHandle.end() ? it->second : 0;
}

void MiniAudioManager::advanceVirtualMusic(Real ms)
{
	if (!m_headlessVirtualPlayback || ms <= 0.0f)
		return;
	for (PlayingAudio *playing : m_playingSounds)
	{
		if (!playing || playing->m_sound || playing->m_paused || playing->m_requestStop || !playing->m_audioEventRTS)
			continue;
		const AudioEventInfo *info = playing->m_audioEventRTS->getAudioEventInfo();
		if (info && info->m_soundType == AT_Music)
			playing->m_virtualElapsedMs += ms;
	}
}

void MiniAudioManager::dumpAudioTraceStatus() const
{
	if (!m_traceEnabled)
		return;
	Int activeVoices = 0;
	Int activeLoops = 0;
	AsciiString musicName;
	Int musicLoops = 0;
	for (const PlayingAudio *playing : m_playingSounds)
	{
		if (!playing || playing->m_requestStop || !playing->m_audioEventRTS)
			continue;
		const AudioEventInfo *info = playing->m_audioEventRTS->getAudioEventInfo();
		if (!info)
			continue;
		if (info->m_soundType == AT_Music)
		{
			musicName = playing->m_audioEventRTS->getEventName();
			musicLoops = playing->m_completedLoops;
		}
		else if (info->m_type & ST_VOICE)
		{
			++activeVoices;
		}
		else if (info->m_control & AC_LOOP)
		{
			++activeLoops;
		}
	}
	const UnsignedInt frame = TheGameLogic ? TheGameLogic->getFrame() : 0u;
	std::fprintf(stderr, "AUDIO STATUS frame=%u voices=%d loops=%d music=%s completed=%d\n",
		frame, activeVoices, activeLoops, musicName.isEmpty() ? "none" : musicName.str(), musicLoops);
	std::fflush(stderr);
}

//-------------------------------------------------------------------------------------------------
AudioEventRTS *MiniAudioManager::findLowestPrioritySound(AudioEventRTS *event)
{
	AudioPriority priority = event->getAudioEventInfo()->m_priority;
	if (priority == AP_LOWEST) return NULL;

	AudioEventRTS *lowestPriorityEvent = NULL;
	AudioPriority lowestPriority = AP_LOWEST;

	// GeneralsX @bugfix elmeriniemela 17/10/2026 m_playingSounds also holds music and speech. Like the
	// Miles backend, only replace sound effects of the same kind (2D or 3D) as the new sound.
	const PlayingAudioType wantedType = event->isPositionalAudio() ? PAT_3DSample : PAT_Sample;

	for (auto it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it) {
		if ((*it)->m_type != wantedType) continue;
		AudioEventRTS *itEvent = (*it)->m_audioEventRTS.Peek();
		if (!itEvent) continue;
		const AudioEventInfo *itInfo = itEvent->getAudioEventInfo();
		if (!itInfo) continue;
		AudioPriority itPriority = itInfo->m_priority;
		if (itPriority < priority) {
			if (!lowestPriorityEvent || lowestPriority > itPriority) {
				lowestPriorityEvent = itEvent;
				lowestPriority = itPriority;
				if (lowestPriority == AP_LOWEST) return lowestPriorityEvent;
			}
		}
	}
	return lowestPriorityEvent;
}

//-------------------------------------------------------------------------------------------------
Bool MiniAudioManager::isPlayingLowerPriority(AudioEventRTS *event) const
{
	AudioPriority priority = event->getAudioEventInfo()->m_priority;
	if (priority == AP_LOWEST) return false;

	// GeneralsX @bugfix elmeriniemela 17/10/2026 m_playingSounds also holds music and speech. Like the
	// Miles backend, only replace sound effects of the same kind (2D or 3D) as the new sound.
	const PlayingAudioType wantedType = event->isPositionalAudio() ? PAT_3DSample : PAT_Sample;

	for (auto it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it) {
		if ((*it)->m_type != wantedType) continue;
		if (!(*it)->m_audioEventRTS) continue;
		const AudioEventInfo *info = (*it)->m_audioEventRTS->getAudioEventInfo();
		if (info && info->m_priority < priority) {
			event->setHandleToKill((*it)->m_audioEventRTS->getPlayingHandle());
			return true;
		}
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
Bool MiniAudioManager::killLowestPrioritySoundImmediately(AudioEventRTS *event)
{
	// GeneralsX @bugfix meerzulee 19/07/2026 FP env guard for audio entry point (see #215)
	ScopedFPUGuard fpuGuard;
	AudioEventRTS *lowestPriorityEvent = findLowestPrioritySound(event);
	if (!lowestPriorityEvent) return FALSE;

	for (auto it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it) {
		PlayingAudio *playing = (*it);
		if (!playing) continue;
		if (playing->m_audioEventRTS && playing->m_audioEventRTS.Peek() == lowestPriorityEvent) {
			releasePlayingAudio(playing);
			m_playingSounds.erase(it);
			return TRUE;
		}
	}
	return FALSE;
}

//-------------------------------------------------------------------------------------------------
void MiniAudioManager::adjustVolumeOfPlayingAudio(AsciiString eventName, Real newVolume)
{
	// GeneralsX @bugfix meerzulee 19/07/2026 FP env guard for audio entry point (see #215)
	ScopedFPUGuard fpuGuard;
	for (auto it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it) {
		PlayingAudio *playing = *it;
		if (playing && playing->m_audioEventRTS && playing->m_audioEventRTS->getEventName() == eventName) {
			playing->m_audioEventRTS->setVolume(newVolume);
			adjustPlayingVolume(playing);
		}
	}
}

//-------------------------------------------------------------------------------------------------
void MiniAudioManager::removePlayingAudio(AsciiString eventName)
{
	// GeneralsX @bugfix meerzulee 19/07/2026 FP env guard for audio entry point (see #215)
	ScopedFPUGuard fpuGuard;
	for (auto it = m_playingSounds.begin(); it != m_playingSounds.end(); ) {
		PlayingAudio *playing = *it;
		if (playing && playing->m_audioEventRTS && playing->m_audioEventRTS->getEventName() == eventName) {
			it = m_playingSounds.erase(it);
			releasePlayingAudio(playing);
		}
		else { ++it; }
	}
}

//-------------------------------------------------------------------------------------------------
void MiniAudioManager::removeAllDisabledAudio(void)
{
	// GeneralsX @bugfix meerzulee 19/07/2026 FP env guard for audio entry point (see #215)
	ScopedFPUGuard fpuGuard;
	for (auto it = m_playingSounds.begin(); it != m_playingSounds.end(); ) {
		PlayingAudio *playing = *it;
		if (playing && playing->m_audioEventRTS && playing->m_audioEventRTS->getVolume() == 0.0f) {
			it = m_playingSounds.erase(it);
			releasePlayingAudio(playing);
		}
		else { ++it; }
	}
}

//-------------------------------------------------------------------------------------------------
void MiniAudioManager::processRequestList(void)
{
	for (auto it = m_audioRequests.begin(); it != m_audioRequests.end(); ) {
		AudioRequest *req = (*it);
		if (req == NULL) { continue; }

		if (!shouldProcessRequestThisFrame(req)) {
			adjustRequest(req);
			++it;
			continue;
		}

		if (!req->m_requiresCheckForSample || checkForSample(req))
			processRequest(req);

		deleteInstance(req);
		it = m_audioRequests.erase(it);
	}
}

void MiniAudioManager::playReadyContinuations(void)
{
	for (auto it = m_audioRequests.begin(); it != m_audioRequests.end(); ) {
		AudioRequest *req = *it;
		if (!req || !req->m_isContinuation || req->m_requiresCheckForSample || !shouldProcessRequestThisFrame(req)) {
			++it;
			continue;
		}
		processRequest(req);
		deleteInstance(req);
		it = m_audioRequests.erase(it);
	}
}

//-------------------------------------------------------------------------------------------------
void MiniAudioManager::processPlayingList(void)
{
	for (auto it = m_playingSounds.begin(); it != m_playingSounds.end(); ) {
		PlayingAudio *playing = (*it);
		if (!playing) {
			it = m_playingSounds.erase(it);
			continue;
		}
		const AudioEventInfo *info = playing->m_audioEventRTS ? playing->m_audioEventRTS->getAudioEventInfo() : nullptr;
		Bool finished = FALSE;
		if (info && !playing->m_paused && !playing->m_requestStop) {
			if (!playing->m_sound && playing->m_loopSource) {
				auto& loop = *playing->m_loopSource;
				const UnsignedInt frame = TheGameLogic ? TheGameLogic->getFrame() : playing->m_virtualLastFrame;
				if (frame > playing->m_virtualLastFrame)
				{
					const auto scaledFrames = static_cast<ma_uint64>(frame - playing->m_virtualLastFrame) * loop.rate()
						+ playing->m_virtualSampleRemainder;
					auto remaining = scaledFrames / LOGICFRAMES_PER_SECOND;
					playing->m_virtualSampleRemainder = scaledFrames % LOGICFRAMES_PER_SECOND;
					uint8_t pcm[4096];
					while (remaining > 0)
					{
						const auto requested = std::min(remaining, static_cast<ma_uint64>(sizeof(pcm) / loop.frameBytes()));
						ma_uint64 count = 0;
						ma_data_source_read_pcm_frames(&loop, pcm, requested, &count);
						if (count < requested)
						{
							finished = TRUE;
							break;
						}
						remaining -= count;
					}
					if (m_traceEnabled)
						traceLoopSegments(*playing->m_audioEventRTS, loop);
				}
				playing->m_virtualLastFrame = frame;
			} else if (!playing->m_sound && m_headlessVirtualPlayback) {
				const UnsignedInt frame = TheGameLogic ? TheGameLogic->getFrame() : playing->m_virtualLastFrame;
				if (frame >= playing->m_virtualLastFrame)
					playing->m_virtualElapsedMs += (frame - playing->m_virtualLastFrame) * MSEC_PER_LOGICFRAME_REAL;
				playing->m_virtualLastFrame = frame;
				if (info->m_soundType == AT_Music && !playing->m_audioEventRTS->getPlayMusicOnce()) {
					while (playing->m_virtualElapsedMs >= playing->m_virtualDurationMs) {
						playing->m_virtualElapsedMs -= playing->m_virtualDurationMs;
						++playing->m_completedLoops;
					}
				} else {
					finished = playing->m_virtualElapsedMs >= playing->m_virtualDurationMs;
				}
			} else if (playing->m_sound && info->m_soundType == AT_Music) {
				ma_uint64 cursor = 0;
				if (ma_sound_get_cursor_in_pcm_frames(playing->m_sound, &cursor) == MA_SUCCESS) {
					if (cursor < playing->m_lastCursor)
						++playing->m_completedLoops;
					playing->m_lastCursor = cursor;
				}
			}
		}

		// Check if sound was explicitly marked to stop
		const Bool ended = !playing->m_requestStop && (finished || (playing->m_sound && !playing->m_paused && !ma_sound_is_playing(playing->m_sound)));
		if (playing->m_requestStop || finished || (playing->m_sound && !playing->m_paused && !ma_sound_is_playing(playing->m_sound))) {
			if (ended && info && info->m_soundType == AT_Music && playing->m_audioEventRTS) {
				const Int completions = playing->m_completedLoops + 1;
				AsciiString key = playing->m_audioEventRTS->getEventName();
				key.toLower();
				m_completedMusicByName[key] += completions;
				m_completedMusicByHandle[playing->m_audioEventRTS->getPlayingHandle()] = completions;
			}
			if (playing->m_sound) ma_sound_stop(playing->m_sound);
			if (playing->m_audioEventRTS)
				notifyOfAudioCompletion(playing->m_audioEventRTS->getPlayingHandle(), playing->m_type);
			it = m_playingSounds.erase(it);
			releasePlayingAudio(playing);
			continue;
		}

		if (m_volumeHasChanged || (playing->m_audioEventRTS && playing->m_audioEventRTS->hasRetailEmitter())) {
			adjustPlayingVolume(playing);
		}
		else if (m_reverb && m_reverb->isActive() && playing->m_type == PAT_3DSample)
			adjustReverbSends(playing);

		// Update 3D position for positional sounds
		if (playing->m_type == PAT_3DSample && playing->m_audioEventRTS) {
			const Coord3D *pos = getCurrentPositionFromEvent(playing->m_audioEventRTS.Peek());
			if (pos && playing->m_sound) {
				ma_sound_set_position(playing->m_sound, pos->x, pos->y, pos->z);
			}
		}

		++it;
	}

	if (m_volumeHasChanged) {
		m_volumeHasChanged = false;

		if (TheVideoPlayer) {
			TheVideoPlayer->setVolume(getMoviePlaybackVolume());
		}
	}
}

//-------------------------------------------------------------------------------------------------
void MiniAudioManager::processFadingList(void)
{
	for (auto it = m_fadingAudio.begin(); it != m_fadingAudio.end(); ) {
		PlayingAudio *playing = *it;
		if (!playing) {
			it = m_fadingAudio.erase(it);
			continue;
		}

		if (playing->m_framesFaded >= getAudioSettings()->m_fadeAudioFrames) {
			playing->m_requestStop = true;
			it = m_fadingAudio.erase(it);
			releasePlayingAudio(playing);
			continue;
		}

		++playing->m_framesFaded;
		Real volume = getEffectiveVolume(playing->m_audioEventRTS.Peek());
		volume *= (1.0f - 1.0f * playing->m_framesFaded / getAudioSettings()->m_fadeAudioFrames);

		if (playing->m_sound)
		{
			ma_sound_set_volume(playing->m_sound, volume);
			adjustReverbSends(playing);
		}

		++it;
	}
}

//-------------------------------------------------------------------------------------------------
void MiniAudioManager::processStoppedList(void)
{
	for (auto it = m_stoppedAudio.begin(); it != m_stoppedAudio.end(); ) {
		PlayingAudio *playing = *it;
		it = m_stoppedAudio.erase(it);
		if (playing) releasePlayingAudio(playing);
	}
}

//-------------------------------------------------------------------------------------------------
Bool MiniAudioManager::shouldProcessRequestThisFrame(AudioRequest *req) const
{
	if (!req->m_pendingEvent) return true;
	if (req->m_pendingEvent->getDelay() < MSEC_PER_LOGICFRAME_REAL) return true;
	return false;
}

//-------------------------------------------------------------------------------------------------
void MiniAudioManager::adjustRequest(AudioRequest *req)
{
	if (!req->m_pendingEvent) return;
	req->m_pendingEvent->decrementDelay(MSEC_PER_LOGICFRAME_REAL);
	req->m_requiresCheckForSample = true;
}

//-------------------------------------------------------------------------------------------------
Bool MiniAudioManager::checkForSample(AudioRequest *req)
{
	if (!req->m_pendingEvent) return true;
	if (req->m_pendingEvent->getAudioEventInfo() == NULL)
		getInfoForAudioEvent(req->m_pendingEvent.Peek());
	if (req->m_pendingEvent->getAudioEventInfo()->m_type != AT_SoundEffect)
		return true;
	return m_sound->canPlayNow(req->m_pendingEvent.Peek());
}

//-------------------------------------------------------------------------------------------------
void MiniAudioManager::setHardwareAccelerated(Bool accel)
{
	AudioManager::setHardwareAccelerated(accel);
}

//-------------------------------------------------------------------------------------------------
void MiniAudioManager::setSpeakerSurround(Bool surround)
{
	AudioManager::setSpeakerSurround(surround);
}

//-------------------------------------------------------------------------------------------------
Real MiniAudioManager::getFileLengthMS(AsciiString strToLoad) const
{
	// GeneralsX @bugfix meerzulee 19/07/2026 FP env guard for audio entry point (see #215)
	ScopedFPUGuard fpuGuard;
	if (strToLoad.isEmpty()) return 0.0f;

	static ma_vfs_callbacks vfs = {};
	vfs.onOpen = vfsFileOpen;
	vfs.onClose = vfsFileClose;
	vfs.onInfo = vfsFileInfo;
	vfs.onRead = vfsFileRead;
	vfs.onTell = vfsFileTell;
	vfs.onSeek = vfsFileSeek;

	ma_decoder decoder;
	ma_decoder_config config = ma_decoder_config_init_default();
	ma_result result = ma_decoder_init_vfs((ma_vfs *)&vfs, strToLoad.str(), &config, &decoder);
	if (result != MA_SUCCESS) return 0.0f;

	ma_uint64 lengthInFrames;
	result = ma_decoder_get_length_in_pcm_frames(&decoder, &lengthInFrames);
	if (result != MA_SUCCESS) {
		ma_decoder_uninit(&decoder);
		return 0.0f;
	}

	ma_uint32 sampleRate = decoder.outputSampleRate;
	if (sampleRate == 0) sampleRate = 44100;
	float lengthMS = (float)lengthInFrames / (float)sampleRate * 1000.0f;

	ma_decoder_uninit(&decoder);
	return lengthMS;
}

//-------------------------------------------------------------------------------------------------
void MiniAudioManager::closeAnySamplesUsingFile(const void *fileToClose)
{
	// GeneralsX @bugfix meerzulee 19/07/2026 FP env guard for audio entry point (see #215)
	ScopedFPUGuard fpuGuard;
	// miniaudio manages file lifecycle internally after decoding
}


//-------------------------------------------------------------------------------------------------
void MiniAudioManager::playStream(AudioEventRTS *event, ma_sound *sound)
{
	ma_sound_start(sound);
}

//-------------------------------------------------------------------------------------------------
void MiniAudioManager::setDeviceListenerPosition(void)
{
	if (!m_deviceOpened)
		return;
	ma_engine_listener_set_position(&m_engine, 0,
		m_listenerPosition.x, m_listenerPosition.y, m_listenerPosition.z);
	ma_engine_listener_set_direction(&m_engine, 0,
		m_listenerOrientation.x, m_listenerOrientation.y, m_listenerOrientation.z);
}

//-------------------------------------------------------------------------------------------------
const Coord3D *MiniAudioManager::getCurrentPositionFromEvent(AudioEventRTS *event)
{
	if (!event->isPositionalAudio()) return NULL;
	return event->getCurrentPosition();
}

//-------------------------------------------------------------------------------------------------
Bool MiniAudioManager::isOnScreen(const Coord3D *pos) const
{
	static ICoord2D dummy;
	return TheTacticalView->worldToScreen(pos, &dummy);
}

//-------------------------------------------------------------------------------------------------
Real MiniAudioManager::getEffectiveVolume(AudioEventRTS *event) const
{
	if (!event || m_muteReasonBits)
		return 0.0f;
	const AudioEventInfo *info = event->getAudioEventInfo();
	const Real gain = event->getVolume() * event->getVolumeShift() *
		getScriptAudio().fadeMultiplier(event->getEventName());
	if (info && info->m_soundType == AT_Music)
		return gain * m_musicVolume;
	if (info && (info->m_soundType == AT_Streaming || (info->m_type & ST_VOICE)))
		return gain * m_speechVolume;
	const Bool positional = event->isPositionalAudio();
	if ((event->isAmbient() || (info && info->m_soundType == AT_AmbientStream)) && m_ambientVolume >= 0.0f)
		return gain * m_ambientVolume * (positional ? m_scriptSound3DVolume : m_scriptSoundVolume);
	return gain * (positional ? m_sound3DVolume : m_soundVolume);
}

//-------------------------------------------------------------------------------------------------
void MiniAudioManager::buildProviderList(void) {}
void MiniAudioManager::createListener(void) {}
void MiniAudioManager::initDelayFilter(void) {}

//-------------------------------------------------------------------------------------------------
Bool MiniAudioManager::isValidProvider(void)
{
	return (m_selectedPlaybackDevice < m_playbackDeviceCount);
}

//-------------------------------------------------------------------------------------------------
void MiniAudioManager::initSamplePools(void) {}

//-------------------------------------------------------------------------------------------------
void MiniAudioManager::processRequest(AudioRequest *req)
{
	switch (req->m_request)
	{
	case AR_Play:   playAudioEvent(req); break;
	case AR_Pause:  pauseAudioEvent(req->m_handleToInteractOn); break;
	case AR_Stop:   stopAudioEvent(req->m_handleToInteractOn); break;
	}
}

//-------------------------------------------------------------------------------------------------
void MiniAudioManager::friend_forcePlayAudioEventRTS(const AudioEventRTS *eventToPlay)
{
	if (!eventToPlay->getAudioEventInfo()) {
		getInfoForAudioEvent(eventToPlay);
		if (!eventToPlay->getAudioEventInfo()) {
			DEBUG_CRASH(("MiniAudio: No info for forced audio event '%s'\n", eventToPlay->getEventName().str()));
			return;
		}
	}

	switch (eventToPlay->getAudioEventInfo()->m_soundType)
	{
	case AT_Music:     if (!isOn(AudioAffect_Music))  return; break;
	case AT_AmbientStream:
	case AT_SoundEffect:
		if (!isOn(AudioAffect_Sound) || !isOn(AudioAffect_Sound3D)) return;
		break;
	case AT_Streaming: if (!isOn(AudioAffect_Speech)) return; break;
	}

	AudioRequest *req = allocateAudioRequest();
	req->m_pendingEvent.Assign_No_Add_Ref(newInstance(DynamicAudioEventRTS)(*eventToPlay));
	req->m_pendingEvent->generateFilename();
	req->m_pendingEvent->generatePlayInfo();

	for (auto it = m_adjustedVolumes.begin(); it != m_adjustedVolumes.end(); ++it) {
		if (it->first == req->m_pendingEvent->getEventName()) {
			req->m_pendingEvent->setVolume(it->second);
			break;
		}
	}

	playAudioEvent(req);
	releaseAudioRequest(req);
}

#if defined(_DEBUG) || defined(_INTERNAL)
//-------------------------------------------------------------------------------------------------
void MiniAudioManager::dumpAllAssetsUsed()
{
	if (!TheGlobalData->m_preloadReport) return;

	FILE *logfile = fopen("PreloadedAssets.txt", "a+");
	if (!logfile) return;

	fprintf(logfile, "\nAudio Asset Report - BEGIN\n");
	{
		std::list<AsciiString> missingEvents;
		std::list<AsciiString> usedFiles;

		for (auto it = m_allEventsLoaded.begin(); it != m_allEventsLoaded.end(); ++it) {
			AsciiString astr = *it;
			AudioEventInfo *aei = findAudioEventInfo(astr);
			if (!aei) { missingEvents.push_back(astr); continue; }

			for (auto asIt = aei->m_attackSounds.begin(); asIt != aei->m_attackSounds.end(); ++asIt)
				usedFiles.push_back(asIt->name);
			for (auto asIt = aei->m_sounds.begin(); asIt != aei->m_sounds.end(); ++asIt)
				usedFiles.push_back(asIt->name);
			for (auto asIt = aei->m_decaySounds.begin(); asIt != aei->m_decaySounds.end(); ++asIt)
				usedFiles.push_back(asIt->name);
			if (!aei->m_filename.isEmpty())
				usedFiles.push_back(aei->m_filename);
		}

		fprintf(logfile, "\nMissing events - BEGIN\n");
		for (auto &s : missingEvents) fprintf(logfile, "%s\n", s.str());
		fprintf(logfile, "\nMissing events - END\n");

		fprintf(logfile, "\nFiles Used - BEGIN\n");
		for (auto &s : usedFiles) fprintf(logfile, "%s\n", s.str());
		fprintf(logfile, "\nFiles Used - END\n");
	}
	fprintf(logfile, "\nAudio Asset Report - END\n");
	fclose(logfile);
}
#endif

//-------------------------------------------------------------------------------------------------
// VFS callbacks — allow miniaudio to load files from the game's virtual file system
//-------------------------------------------------------------------------------------------------

struct VFSFileHandle { File *file; };

static ma_result vfsFileOpen(ma_vfs *pVFS, const char *filename, ma_uint32 mode, ma_vfs_file *pFile)
{
	VFSFileHandle *handle = new VFSFileHandle;
	handle->file = TheFileSystem->openFile(filename);
	if (!handle->file) {
		delete handle;
		return MA_ERROR;
	}
	*pFile = (ma_vfs_file)handle;
	return MA_SUCCESS;
}

static ma_result vfsFileClose(ma_vfs *pVFS, ma_vfs_file file)
{
	VFSFileHandle *handle = (VFSFileHandle *)file;
	if (handle) {
		if (handle->file) handle->file->close();
		delete handle;
	}
	return MA_SUCCESS;
}

static ma_result vfsFileInfo(ma_vfs *pVFS, ma_vfs_file file, ma_file_info *pInfo)
{
	VFSFileHandle *handle = (VFSFileHandle *)file;
	if (!handle || !handle->file) return MA_ERROR;
	pInfo->sizeInBytes = handle->file->size();
	return MA_SUCCESS;
}

static ma_result vfsFileRead(ma_vfs *pVFS, ma_vfs_file file, void *pBuffer, size_t bytesToRead, size_t *pBytesRead)
{
	VFSFileHandle *handle = (VFSFileHandle *)file;
	if (!handle || !handle->file) return MA_ERROR;
	Int bytesRead = handle->file->read(pBuffer, (Int)bytesToRead);
	if (pBytesRead) *pBytesRead = (size_t)(bytesRead < 0 ? 0 : bytesRead);
	return MA_SUCCESS;
}

static ma_result vfsFileTell(ma_vfs *pVFS, ma_vfs_file file, ma_int64 *pCursor)
{
	VFSFileHandle *handle = (VFSFileHandle *)file;
	if (!handle || !handle->file) return MA_ERROR;
	*pCursor = (ma_int64)handle->file->position();
	return MA_SUCCESS;
}

static ma_result vfsFileSeek(ma_vfs *pVFS, ma_vfs_file file, ma_int64 offset, ma_seek_origin origin)
{
	VFSFileHandle *handle = (VFSFileHandle *)file;
	if (!handle || !handle->file) return MA_ERROR;

	File::seekMode seekType;
	switch (origin) {
	case ma_seek_origin_start:   seekType = File::START;   break;
	case ma_seek_origin_current: seekType = File::CURRENT; break;
	case ma_seek_origin_end:     seekType = File::END;     break;
	default: return MA_ERROR;
	}

	handle->file->seek((Int)offset, seekType);
	return MA_SUCCESS;
}

//-------------------------------------------------------------------------------------------------
UnsignedInt MiniAudioManager::getNumAvailable2DSamples() const
{
	// GeneralsX @bugfix Mr. Meeseeks 27/06/2026 Fix available samples calculation to prevent voicelines culling
	UnsignedInt max2D = getAudioSettings()->m_sampleCount2D;
	UnsignedInt playing = 0;
	for (auto it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it) {
		if ((*it) && (*it)->m_type == PAT_Sample) {
			playing++;
		}
	}
	return (max2D > playing) ? (max2D - playing) : 0;
}

//-------------------------------------------------------------------------------------------------
UnsignedInt MiniAudioManager::getNumAvailable3DSamples() const
{
	// GeneralsX @bugfix Mr. Meeseeks 01/07/2026 Fix available samples calculation to only count 3D samples
	UnsignedInt max3D = getAudioSettings()->m_sampleCount3D;
	UnsignedInt playing = 0;
	for (auto it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it) {
		if ((*it) && (*it)->m_type == PAT_3DSample) {
			playing++;
		}
	}
	return (max3D > playing) ? (max3D - playing) : 0;
}
