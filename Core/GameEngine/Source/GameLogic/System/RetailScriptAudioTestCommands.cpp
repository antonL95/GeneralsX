#include "PreRTS.h"

#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include "Common/AudioHandleSpecialValues.h"
#include "Common/DataChunk.h"
#include "Common/GameAudio.h"
#include "Common/NameKeyGenerator.h"
#include "GameLogic/BfmeMapScripts.h"
#include "GameLogic/Object.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Scripts.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <vector>

namespace
{
class ScriptActionStream final : public OutputStream, public ChunkInputStream
{
public:
	Int write(const void* data, Int length) override
	{
		const auto* bytes = static_cast<const char*>(data);
		m_data.insert(m_data.end(), bytes, bytes + length);
		return length;
	}

	Int read(void* data, Int length) override
	{
		const auto count = std::min<std::size_t>(length, m_data.size() - m_position);
		std::memcpy(data, m_data.data() + m_position, count);
		m_position += count;
		return static_cast<Int>(count);
	}

	UnsignedInt tell() override { return static_cast<UnsignedInt>(m_position); }
	Bool eof() override { return m_position == m_data.size(); }

	Bool absoluteSeek(UnsignedInt position) override
	{
		if (position > m_data.size())
			return FALSE;
		m_position = position;
		return TRUE;
	}

private:
	std::vector<char> m_data;
	std::size_t m_position = 0;
};

struct ScriptWireValue
{
	Int type;
	const char* text;
	Real number = 0.0f;
};

constexpr char fixtureUnitName[] = "AudioScriptFixtureNamedUnit";
constexpr char fixtureSound[] = "AudioScriptFixtureSound";

ScriptAction* parseAction(const char* name, Int wireAction,
	std::initializer_list<ScriptWireValue> parameters)
{
	ScriptActionStream bytes;
	{
		DataChunkOutput output(&bytes);
		output.openDataChunk("Action", 3);
		output.writeInt(wireAction);
		output.writeNameKey(TheNameKeyGenerator->nameToKey(name));
		output.writeInt(static_cast<Int>(parameters.size()));
		for (const auto& parameter : parameters)
		{
			output.writeInt(parameter.type);
			output.writeInt(0);
			output.writeReal(parameter.number);
			output.writeAsciiString(parameter.text);
		}
		output.writeInt(1);
		output.closeDataChunk();
	}
	DataChunkInput input(&bytes);
	DataChunkVersionType version;
	input.openDataChunk(&version);
	ScriptAction* action = newInstance(ScriptAction)(ScriptAction::NO_OP);
	action->setNextAction(nullptr);
	action->setWarnings(FALSE);
	BfmeMapScripts::readAction(input, *action);
	input.closeDataChunk();
	return action;
}

void arm(const RetailTestCommands::Context& ctx)
{
	if (Object* unit = ctx.requireObject())
	{
		unit->setName(fixtureUnitName);
		TheScriptEngine->addObjectToCache(unit);
	}
}

void playNamed(const RetailTestCommands::Context& ctx)
{
	Object* unit = ctx.requireObject();
	if (!unit)
		return;
	TheAudio->setTraceEnabled(TRUE);
	TheAudio->setHeadlessVirtualPlayback(TRUE);
	ScriptAction* action = parseAction("SOUND_PLAY_NAMED", 84, {{12, fixtureSound}, {14, fixtureUnitName}});
	const Bool mapped = action->getActionType() == ScriptAction::SOUND_PLAY_NAMED;
	const Bool named = TheScriptEngine->getUnitNamed(fixtureUnitName) == unit;
	std::fprintf(stderr, "TEST script audio frame=%u named=%d mapped=%d\n", ctx.frame, named, mapped);
	TheScriptEngine->friend_executeAction(action, unit->getTeam());
	deleteInstance(action);
}

void overrideVolume(const RetailTestCommands::Context& ctx)
{
	TheAudio->setTraceEnabled(TRUE);
	TheAudio->setHeadlessVirtualPlayback(TRUE);
	ScriptAction* action = parseAction("AUDIO_OVERRIDE_VOLUME_TYPE", 224, {{59, fixtureSound}, {1, "", 0.0f}});
	const Bool mapped = action->getActionType() == ScriptAction::AUDIO_OVERRIDE_VOLUME_TYPE;
	TheScriptEngine->friend_executeAction(action);
	deleteInstance(action);

	AudioEventRTS sound(fixtureSound);
	const Bool muted = TheAudio->addAudioEvent(&sound) == AHSV_Muted;
	TheAudio->setAudioEventVolumeOverride(fixtureSound, -1.0f);
	std::fprintf(stderr, "TEST script audio volume frame=%u mapped=%d muted=%d\n", ctx.frame, mapped, muted);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailScriptAudioTestCommands()
{
#if !RTS_GENERALS
	registerAction("script-audio-arm", arm);
	registerAction("script-audio-named", playNamed);
	registerAction("script-audio-volume", overrideVolume);
#endif
}
}
