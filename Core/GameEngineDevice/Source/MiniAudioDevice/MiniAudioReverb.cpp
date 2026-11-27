#include "Lib/BaseType.h"

#include "MiniAudioDevice/MiniAudioReverb.h"

#include <extras/nodes/ma_reverb_node/ma_reverb_node.h>
#include <algorithm>
#include <array>
#include <cmath>

namespace
{
struct RoomPreset
{
	Real decaySeconds;
	Real highFrequencyGain;
	Real lateGain;
};

constexpr std::array<RoomPreset, RetailScriptAudio::RoomCount> presets = {{
	{0.0f, 0.0f, 0.0f}, {0.17f, 0.0010f, 1.2691f}, {0.40f, 0.5929f, 1.0629f},
	{1.49f, 0.2512f, 3.2734f}, {0.50f, 0.0010f, 0.2805f}, {2.31f, 0.7079f, 1.1003f},
	{4.32f, 0.5781f, 0.7170f}, {3.92f, 0.5623f, 0.9977f}, {2.91f, 1.0000f, 0.7063f},
	{7.24f, 0.4477f, 1.0186f}, {10.05f, 0.3162f, 1.2560f}, {0.30f, 0.0100f, 0.1531f},
	{1.49f, 0.7079f, 1.6615f}, {2.70f, 0.7612f, 1.5758f}, {1.49f, 0.7328f, 0.9954f},
	{1.49f, 0.0224f, 0.7682f}, {1.49f, 0.3981f, 0.1427f}, {1.49f, 0.0562f, 0.1919f},
	{1.49f, 0.3162f, 1.7783f}, {1.49f, 0.1000f, 0.1089f}, {1.65f, 1.0000f, 0.2652f},
	{2.81f, 0.3162f, 3.2471f}, {1.49f, 0.0100f, 7.0795f}, {8.39f, 1.0000f, 3.1081f},
	{17.23f, 0.6310f, 0.4937f}, {7.56f, 0.8404f, 2.4378f}
}};
}

struct MiniAudioReverb::Effect
{
	ma_reverb_node node{};
	Bool ready = FALSE;

	~Effect()
	{
		if (ready)
			ma_reverb_node_uninit(&node, nullptr);
	}
};

MiniAudioReverb::MiniAudioReverb(ma_engine& engine) : m_engine(engine)
{
	const auto channels = ma_engine_get_channels(&engine);
	if (channels == 1 || channels == 2)
		m_wetGroupReady = ma_sound_group_init(&engine, MA_SOUND_FLAG_NO_DEFAULT_ATTACHMENT,
			nullptr, &m_wetGroup) == MA_SUCCESS;
}

MiniAudioReverb::~MiniAudioReverb()
{
	m_effect.reset();
	if (m_wetGroupReady)
		ma_sound_group_uninit(&m_wetGroup);
}

Bool MiniAudioReverb::setRoom(RetailScriptAudio::Room room, Real gain)
{
	if (m_room == room && m_gain == gain)
		return FALSE;
	m_effect.reset();
	m_room = room;
	m_gain = gain;
	if (!m_wetGroupReady || room == RetailScriptAudio::None || gain <= 0.0f)
		return TRUE;

	const auto channels = ma_engine_get_channels(&m_engine);
	auto config = ma_reverb_node_config_init(channels, ma_engine_get_sample_rate(&m_engine));
	auto effect = std::make_unique<Effect>();
	effect->ready = ma_reverb_node_init(ma_engine_get_node_graph(&m_engine), &config, nullptr,
		&effect->node) == MA_SUCCESS;
	if (!effect->ready)
		return TRUE;

	const auto& preset = presets[room];
	constexpr Real referenceSampleRate = 44100.0f;
	constexpr Real sixtyDecibelAmplitude = 0.001f;
	constexpr Real efxRoomGain = 0.3162f;
	const Real delay = verblib_combtuningL8 / referenceSampleRate;
	const Real feedback = std::pow(sixtyDecibelAmplitude, delay / preset.decaySeconds);
	verblib_set_room_size(&effect->node.reverb,
		std::clamp((feedback - verblib_offsetroom) / verblib_scaleroom, 0.0f, 1.0f));
	verblib_set_damping(&effect->node.reverb, 1.0f - preset.highFrequencyGain);
	verblib_set_dry(&effect->node.reverb, 0.0f);
	verblib_set_wet(&effect->node.reverb, gain * efxRoomGain * preset.lateGain / verblib_scalewet);
	if (ma_node_attach_output_bus(&effect->node, 0, ma_engine_get_endpoint(&m_engine), 0) != MA_SUCCESS ||
		ma_node_attach_output_bus(&m_wetGroup, 0, &effect->node, 0) != MA_SUCCESS)
		return TRUE;
	m_effect = std::move(effect);
	return TRUE;
}

void MiniAudioReverb::setSends(ma_sound* sound, ma_sound_group* dryGroup,
	std::unique_ptr<ma_splitter_node>& sends, Real dry, Real wet)
{
	if (m_effect && !sends)
	{
		auto splitter = std::make_unique<ma_splitter_node>();
		auto config = ma_splitter_node_config_init(ma_engine_get_channels(&m_engine));
		if (ma_splitter_node_init(ma_engine_get_node_graph(&m_engine), &config, nullptr, splitter.get()) != MA_SUCCESS)
			return;
		if (ma_node_attach_output_bus(splitter.get(), 0, dryGroup, 0) != MA_SUCCESS ||
			ma_node_attach_output_bus(splitter.get(), 1, &m_wetGroup, 0) != MA_SUCCESS ||
			ma_node_attach_output_bus(sound, 0, splitter.get(), 0) != MA_SUCCESS)
		{
			ma_splitter_node_uninit(splitter.get(), nullptr);
			return;
		}
		sends = std::move(splitter);
	}
	if (!sends)
		return;
	ma_node_set_output_bus_volume(sends.get(), 0, m_effect ? dry : 1.0f);
	ma_node_set_output_bus_volume(sends.get(), 1, m_effect ? wet : 0.0f);
}
