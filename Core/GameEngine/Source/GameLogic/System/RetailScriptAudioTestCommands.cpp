#include "PreRTS.h"

#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include "Common/AudioHandleSpecialValues.h"
#include "Common/DataChunk.h"
#include "Common/GameAudio.h"
#include "Common/NameKeyGenerator.h"
#include "Common/RetailScriptAudio.h"
#include "Common/Team.h"
#include "GameLogic/BfmeMapScripts.h"
#include "GameLogic/Object.h"
#include "GameLogic/PolygonTrigger.h"
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
constexpr char fixtureArea[] = "AudioScriptFixtureArea";
constexpr Int fixtureAreaHalfWidth = 16;

void ensureArea(const Object& unit)
{
	if (PolygonTrigger::findByName(fixtureArea))
		return;
	const Int x = static_cast<Int>(unit.getPosition()->x);
	const Int y = static_cast<Int>(unit.getPosition()->y);
	auto* polygon = newInstance(PolygonTrigger)(4);
	polygon->setTriggerName(fixtureArea);
	polygon->setCastleAreaOwner(unit.getID());
	polygon->addPoint({x - fixtureAreaHalfWidth, y - fixtureAreaHalfWidth, 0});
	polygon->addPoint({x + fixtureAreaHalfWidth, y - fixtureAreaHalfWidth, 0});
	polygon->addPoint({x + fixtureAreaHalfWidth, y + fixtureAreaHalfWidth, 0});
	polygon->addPoint({x - fixtureAreaHalfWidth, y + fixtureAreaHalfWidth, 0});
	PolygonTrigger::addPolygonTrigger(polygon);
}

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
		ensureArea(*unit);
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

void playTeam(const RetailTestCommands::Context& ctx)
{
	Object* unit = ctx.requireObject();
	if (!unit || !unit->getTeam())
		return;
	Team* team = unit->getTeam();
	const Object* firstMember = team->getFirstItemIn_TeamMemberList();
	if (!firstMember)
		return;
	TheAudio->setTraceEnabled(TRUE);
	TheAudio->setHeadlessVirtualPlayback(TRUE);
	ScriptAction* action = parseAction("PLAY_SOUND_EFFECT_AT_TEAM", 510, {{12, fixtureSound}, {3, team->getName().str()}});
	const Bool mapped = action->getActionType() != ScriptAction::NO_OP;
	std::fprintf(stderr, "TEST script audio team frame=%u mapped=%d owner=%u\n", ctx.frame, mapped,
		static_cast<UnsignedInt>(firstMember->getID()));
	TheScriptEngine->friend_executeAction(action, team);
	deleteInstance(action);
}

void runMixAction(const RetailTestCommands::Context& ctx)
{
	ScriptAction* action = nullptr;
	const std::string_view kind(ctx.argument);
	if (kind == "room")
		action = parseAction("AUDIO_SET_REVERB_ROOM_TYPE", 448, {{60, "Cave"}});
	else if (kind == "polygon")
		action = parseAction("AUDIO_SET_REVERB_SUPPRESSION_POLYGON", 525, {{9, fixtureArea}, {52, "", 0.25f}});
	else if (kind == "immune")
		action = parseAction("AUDIO_MAKE_SOUND_IMMUNE_TO_FADE", 441, {{59, fixtureSound}});
	else if (kind == "fade")
		action = parseAction("AUDIO_FADE_VOLUME", 417,
			{{1, "", 1.0f}, {1, "", 0.25f}, {1, "", 0.1f}, {1, "", 0.25f}, {1, "", 0.1f}});
	else
		return;
	const Bool mapped = action->getActionType() != ScriptAction::NO_OP;
	std::fprintf(stderr, "TEST script audio %s frame=%u mapped=%d\n", ctx.argument, ctx.frame, mapped);
	TheScriptEngine->friend_executeAction(action);
	deleteInstance(action);
}

void probeMix(const RetailTestCommands::Context& ctx)
{
	const Object* unit = ctx.requireObject();
	if (!unit)
		return;
	Coord3D outside = *unit->getPosition();
	outside.x += 2 * fixtureAreaHalfWidth;
	const auto& mix = TheAudio->getScriptAudio();
	const Real gain = mix.fadeMultiplier("AudioScriptFixtureOther");
	if (std::string_view(ctx.argument) == "transition")
	{
		std::fprintf(stderr, "TEST script audio transition frame=%u active=%d\n", ctx.frame,
			gain > 0.25f && gain < 1.0f);
		return;
	}
	std::fprintf(stderr,
		"TEST script audio mix-state frame=%u room=%d inside=%.2f outside=%.2f immune=%.2f fade=%.2f\n",
		ctx.frame, mix.getRoom() == RetailScriptAudio::Cave, mix.suppressionAt(unit->getPosition()),
		mix.suppressionAt(&outside), mix.fadeMultiplier(fixtureSound), gain);
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
	registerAction("script-audio-team", playTeam);
	registerAction("script-audio-mix", runMixAction);
	registerAction("script-audio-mix-state", probeMix);
#endif
}
}
