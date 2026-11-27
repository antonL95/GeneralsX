#pragma once

#include "Common/RetailScriptAudio.h"

#include <miniaudio.h>
#include <memory>

class MiniAudioReverb
{
public:
	explicit MiniAudioReverb(ma_engine& engine);
	~MiniAudioReverb();

	Bool setRoom(RetailScriptAudio::Room room, Real gain);
	Bool isActive() const { return m_effect != nullptr; }
	void setSends(ma_sound* sound, ma_sound_group* dryGroup, std::unique_ptr<ma_splitter_node>& sends,
		Real dry, Real wet);

private:
	struct Effect;

	ma_engine& m_engine;
	ma_sound_group m_wetGroup{};
	Bool m_wetGroupReady = FALSE;
	std::unique_ptr<Effect> m_effect;
	RetailScriptAudio::Room m_room = RetailScriptAudio::None;
	Real m_gain = 0.0f;
};
