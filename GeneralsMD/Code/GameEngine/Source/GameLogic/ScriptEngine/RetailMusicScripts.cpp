#include "PreRTS.h"
#include "GameClient/RetailMusicScripts.h"

#include "Common/AudioAffect.h"
#include "Common/AudioEventInfo.h"
#include "Common/AudioEventRTS.h"
#include "Common/AudioHandleSpecialValues.h"
#include "Common/GameAudio.h"
#include "Common/LatchRestore.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/RetailFileLayers.h"
#include "GameClient/ClientRandomValue.h"
#include "GameClient/RetailEva.h"
#include "GameClient/RetailLargeGroupAudio.h"
#include "GameLogic/BfmeScriptRules.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/ScriptActions.h"
#include "GameLogic/ScriptConditions.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Scripts.h"
#include "GameLogic/SidesList.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <map>
#include <utility>
#include <vector>

namespace
{
struct Track
{
	AsciiString name;
	AsciiString flag;
	Int remaining;
};

struct MusicCommand
{
	ScriptAction::ScriptActionType type;
	Track track;
	Bool fadeOut;
	Bool fadeIn;
};

struct Counter
{
	Int value = 0;
	Bool timer = FALSE;
};

std::vector<Track> s_scripted;
std::vector<Track> s_general;
std::vector<MusicCommand> s_pending;
std::vector<std::pair<Int, AsciiString>> s_specialPowers;
std::map<AsciiString, Bool> s_flags;
std::map<AsciiString, Counter> s_counters;
std::map<AsciiString, ObjectID> s_references;
std::vector<const Script*> s_calls;
AudioHandle s_handle = AHSV_NoSound;
UnsignedInt s_frame = -1u;
bool s_evaluating = false;

Bool activeForClient()
{
	return RetailFileLayers::enabled() && !TheGlobalData->m_headless && TheAudio && TheAudio->isOn(AudioAffect_Music);
}

Track* currentTrack()
{
	if (!s_general.empty())
		return &s_general.back();
	if (!s_scripted.empty())
		return &s_scripted.back();
	return nullptr;
}

void notify(Track& track)
{
	track.remaining = 0;
	if (track.flag.isNotEmpty())
		s_flags[track.flag] = TRUE;
}

void play(Bool fadeIn)
{
	Track* track = currentTrack();
	if (!track)
		return;
	if (track->remaining <= 0)
	{
		notify(*track);
		return;
	}
	AudioEventRTS event(RetailLargeGroupAudio::chooseMultiSoundTrack(track->name));
	if (!TheAudio->isValidAudioEvent(&event) || event.getAudioEventInfo()->m_soundType != AT_Music)
	{
		notify(*track);
		return;
	}
	event.setPlayMusicOnce();
	event.setShouldFade(fadeIn);
	if (const Player* local = ThePlayerList->getLocalPlayer())
		event.setPlayerIndex(local->getPlayerIndex());
	s_handle = TheAudio->addAudioEvent(&event);
	if (s_handle < AHSV_FirstHandle)
		notify(*track);
}

void apply(MusicCommand command)
{
	const auto type = command.type;
	ScriptEngine::traceRotwk(FALSE, type, command.track.remaining);
	const Bool scripted = type == ScriptAction::MUSIC_SCRIPT_PLAY_TRACK_FINITE_TIMES_AND_NOTIFY ||
		type == ScriptAction::MUSIC_SCRIPT_PUSH_TRACK_FINITE_TIMES_AND_NOTIFY || type == ScriptAction::MUSIC_SCRIPT_POP_MUSIC;
	auto& stack = scripted ? s_scripted : s_general;
	if (type == ScriptAction::MUSIC_RETURN_TO_MUSIC_SCRIPTING)
	{
		if (s_general.empty())
			return;
		s_general.clear();
	}
	else if (type == ScriptAction::AUDIO_POP_MUSIC || type == ScriptAction::MUSIC_SCRIPT_POP_MUSIC)
	{
		if (stack.empty())
			return;
		stack.pop_back();
	}
	else
	{
		if (command.track.flag.isNotEmpty())
			s_flags[command.track.flag] = FALSE;
		const Bool push = type == ScriptAction::MUSIC_PUSH_TRACK_FINITE_TIMES_AND_NOTIFY ||
			type == ScriptAction::MUSIC_SCRIPT_PUSH_TRACK_FINITE_TIMES_AND_NOTIFY;
		if (!push && !stack.empty())
			stack.pop_back();
		stack.push_back(std::move(command.track));
	}
	if (scripted && !s_general.empty())
		return;
	TheAudio->removeAudioEvent(command.fadeOut ? AHSV_StopTheMusicFade : AHSV_StopTheMusic);
	s_handle = AHSV_NoSound;
	play(command.fadeIn);
}

MusicCommand commandFor(ScriptAction& action)
{
	const auto type = action.getActionType();
	if (type == ScriptAction::AUDIO_POP_MUSIC || type == ScriptAction::MUSIC_SCRIPT_POP_MUSIC ||
		type == ScriptAction::MUSIC_RETURN_TO_MUSIC_SCRIPTING)
		return {type, {}, action.getParameter(0)->getInt() != 0, action.getParameter(1)->getInt() != 0};
	const Parameter* flag = action.getParameter(4);
	return {type, {action.getParameter(0)->getString(), flag ? flag->getString() : AsciiString(),
		action.getParameter(1)->getInt()}, action.getParameter(2)->getInt() != 0, action.getParameter(3)->getInt() != 0};
}

Bool evaluate(Condition& condition)
{
	const auto parameter = [&](Int i) { return condition.getParameter(i); };
	switch (condition.getConditionType())
	{
		case Condition::CONDITION_TRUE:
			return TRUE;
		case Condition::CONDITION_FALSE:
			return FALSE;
		case Condition::FLAG:
			return s_flags[parameter(0)->getString()] == (parameter(1)->getInt() != 0);
		case Condition::COUNTER:
			return BfmeScriptRules::compare(parameter(1)->getInt(), s_counters[parameter(0)->getString()].value, parameter(2)->getInt());
		case Condition::TIMER_EXPIRED:
		{
			const Counter& timer = s_counters[parameter(0)->getString()];
			return timer.timer && timer.value <= 0;
		}
		case Condition::PLAYER_HAS_NUMBER_UNITS_DISTANCE_FROM_OBJECT:
		{
			const auto reference = s_references.find(parameter(4)->getString());
			const Object* anchor = reference == s_references.end() ? TheScriptEngine->getUnitNamed(parameter(4)->getString()) :
				TheGameLogic->findObjectByID(reference->second);
			return evaluateRetailUnitsFarFromObject(condition, anchor);
		}
		case Condition::HAS_EVA_EVENT_PLAYED_IN_LAST_N_SECONDS:
		{
			UnsignedInt frame;
			const Real seconds = parameter(1)->getReal();
			return seconds >= 0 && RetailEva::getLastPlayed(parameter(0)->getString(), frame) &&
				TheGameLogic->getFrame() - frame <= seconds * LOGICFRAMES_PER_SECOND;
		}
		case Condition::IS_NUM_OF_UNITS_BELONGING_TO_PLAYER_NEAR_EVA_EVENT_LAST_PLAYED_LOCATION_COMPARISON_INT:
		{
			UnsignedInt frame;
			Coord3D position;
			return RetailEva::getLastPlayed(parameter(2)->getString(), frame, &position) &&
				evaluateRetailUnitsNearPosition(condition, position);
		}
		case Condition::BUILT_BY_PLAYER:
		case Condition::PLAYER_ACQUIRED_SCIENCE:
		case Condition::PLAYER_HAS_REACHED_LEVEL_CAP:
		case Condition::PLAYER_HAS_NUMBER_OBJECTS_WITH_MODELCONDITION:
		case Condition::SKIRMISH_PLAYER_FACTION:
			return TheScriptConditions->evaluateCondition(&condition);
		case Condition::PLAYER_TRIGGERED_SPECIAL_POWER:
		{
			const PlayerMaskType players = TheScriptEngine->getRetailScriptPlayers(parameter(0)->getString());
			const AsciiString& power = parameter(1)->getString();
			return std::any_of(s_specialPowers.begin(), s_specialPowers.end(), [&](const auto& event) {
				return (players & (1u << event.first)) && event.second == power;
			});
		}
		default:
			return FALSE;
	}
}

Bool matches(Script& script)
{
	for (OrCondition* clause = script.getOrCondition(); clause; clause = clause->getNextOrCondition())
	{
		Condition* condition = clause->getFirstAndCondition();
		if (!condition)
			continue;
		while (condition)
		{
			const auto& opcode = condition->getBfmeOpcode();
			if (opcode.enabled && (!opcode.supported || evaluate(*condition) == opcode.inverted))
				break;
			condition = condition->getNext();
		}
		if (!condition)
			return TRUE;
	}
	return FALSE;
}

void run(Script& script);
void walk(Script* script, ScriptGroup*& group, Int depth, Bool active);

void changeScript(const AsciiString& name, ScriptAction::ScriptActionType type)
{
	ScriptList* list = TheSidesList->getRetailMusicScripts();
	const Bool call = type == ScriptAction::CALL_SUBROUTINE;
	const Bool enable = type == ScriptAction::ENABLE_SCRIPT;
	for (ScriptGroup* group = list->getScriptGroup(); group; group = group->getNext())
	{
		if (group->getName() != name)
			continue;
		if (!call)
			group->setActive(enable);
		else if (group->isActive() && group->isSubroutine())
		{
			ScriptGroup* child = group->getNext();
			walk(group->getScript(), child, group->getBfmeDepth() + 1, TRUE);
		}
		return;
	}
	const auto find = [&](Script* script) {
		for (; script; script = script->getNext())
		{
			if (script->getName() != name)
				continue;
			if (!call)
				script->setActive(enable);
			else if (script->isSubroutine())
				run(*script);
			return TRUE;
		}
		return FALSE;
	};
	if (find(list->getScript()))
		return;
	for (ScriptGroup* group = list->getScriptGroup(); group; group = group->getNext())
		if (find(group->getScript()))
			return;
}

void runActions(ScriptAction* action)
{
	for (; action; action = action->getNext())
	{
		const auto& opcode = action->getBfmeOpcode();
		if (!opcode.enabled || !opcode.supported)
			continue;
		const auto parameter = [&](Int i) { return action->getParameter(i); };
		switch (const auto type = action->getActionType())
		{
			case ScriptAction::ENABLE_SCRIPT:
			case ScriptAction::DISABLE_SCRIPT:
			case ScriptAction::CALL_SUBROUTINE:
				changeScript(parameter(0)->getString(), type);
				break;
			case ScriptAction::SET_FLAG:
				s_flags[parameter(0)->getString()] = parameter(1)->getInt() != 0;
				break;
			case ScriptAction::SET_COUNTER:
				s_counters[parameter(0)->getString()] = {parameter(1)->getInt(), FALSE};
				break;
			case ScriptAction::INCREMENT_COUNTER:
			{
				Int& value = s_counters[parameter(0)->getString()].value;
				const auto sum = static_cast<std::int64_t>(value) + parameter(1)->getInt();
				value = static_cast<Int>(std::clamp<std::int64_t>(sum, std::numeric_limits<Int>::min(), std::numeric_limits<Int>::max()));
				break;
			}
			case ScriptAction::SET_COUNTER_TO_CLIENT_RANDOM_VALUE:
			{
				const Int lo = parameter(1)->getInt();
				const Int hi = parameter(2)->getInt();
				s_counters[parameter(0)->getString()] = {GameClientRandomValue(std::min(lo, hi), std::max(lo, hi)), FALSE};
				break;
			}
			case ScriptAction::SET_MILLISECOND_TIMER:
			case ScriptAction::SET_RANDOM_MSEC_TIMER:
			{
				Real seconds = parameter(1)->getReal();
				if (type == ScriptAction::SET_RANDOM_MSEC_TIMER)
				{
					const Real end = parameter(2)->getReal();
					seconds = GameClientRandomValueReal(std::min(seconds, end), std::max(seconds, end));
				}
				const Real frames = seconds * LOGICFRAMES_PER_SECOND;
				if (frames >= 0 && frames < static_cast<Real>(std::numeric_limits<Int>::max()))
					s_counters[parameter(0)->getString()] = {static_cast<Int>(REAL_TO_INT_CEIL(frames)), TRUE};
				break;
			}
			case ScriptAction::FIND_HOME_BASE_OF_PLAYER:
			{
				const PlayerMaskType players = TheScriptEngine->getRetailScriptPlayers(parameter(0)->getString());
				const Object* home = findRetailScriptHome(players, parameter(2)->getInt() != 0);
				s_references[parameter(1)->getString()] = home ? home->getID() : INVALID_ID;
				break;
			}
			case ScriptAction::SHOW_MILITARY_CAPTION:
				TheScriptActions->executeAction(action);
				break;
			case ScriptAction::AUDIO_POP_MUSIC:
			case ScriptAction::MUSIC_SCRIPT_POP_MUSIC:
			case ScriptAction::MUSIC_RETURN_TO_MUSIC_SCRIPTING:
			case ScriptAction::MUSIC_PLAY_TRACK_FINITE_TIMES:
			case ScriptAction::MUSIC_PLAY_TRACK_FINITE_TIMES_AND_NOTIFY:
			case ScriptAction::MUSIC_PUSH_TRACK_FINITE_TIMES_AND_NOTIFY:
			case ScriptAction::MUSIC_SCRIPT_PLAY_TRACK_FINITE_TIMES_AND_NOTIFY:
			case ScriptAction::MUSIC_SCRIPT_PUSH_TRACK_FINITE_TIMES_AND_NOTIFY:
				apply(commandFor(*action));
				break;
			default:
				break;
		}
	}
}

void run(Script& script)
{
	if (!script.isActive() || s_frame < script.getFrameToEvaluate() ||
		std::find(s_calls.begin(), s_calls.end(), &script) != s_calls.end())
		return;
	const Player* player = ThePlayerList->getLocalPlayer();
	const GameDifficulty difficulty = player ? player->getPlayerDifficulty() : TheScriptEngine->getGlobalDifficulty();
	if ((difficulty == DIFFICULTY_EASY && !script.isEasy()) ||
		(difficulty == DIFFICULTY_NORMAL && !script.isNormal()) ||
		(difficulty >= DIFFICULTY_HARD && !script.isHard()))
		return;
	if (script.getDelayEvalSeconds() > 0)
		script.setFrameToEvaluate(s_frame + script.getDelayEvalSeconds() * LOGICFRAMES_PER_SECOND);
	s_calls.push_back(&script);
	const Bool result = matches(script);
	runActions(result ? script.getAction() : script.getFalseAction());
	if (result && script.isOneShot())
		script.setActive(FALSE);
	s_calls.pop_back();
}

void walk(Script* script, ScriptGroup*& group, Int depth, Bool active)
{
	while (script || (group && group->getBfmeDepth() == depth))
	{
		if (group && group->getBfmeDepth() == depth && (!script || group->getBfmeOrder() < script->getBfmeOrder()))
		{
			ScriptGroup* current = group;
			group = group->getNext();
			walk(current->getScript(), group, depth + 1, active && current->isActive() && !current->isSubroutine());
		}
		else
		{
			if (active && !script->isSubroutine())
				run(*script);
			script = script->getNext();
		}
	}
}
}

namespace RetailMusicScripts
{
void reset()
{
	if (TheAudio && s_handle >= AHSV_FirstHandle)
		TheAudio->removeAudioEvent(s_handle);
	s_scripted.clear();
	s_general.clear();
	s_pending.clear();
	s_specialPowers.clear();
	s_flags.clear();
	s_counters.clear();
	s_references.clear();
	s_handle = AHSV_NoSound;
	s_frame = -1u;
}

void enqueue(ScriptAction& action)
{
	if (activeForClient())
		s_pending.push_back(commandFor(action));
}

void recordSpecialPower(int playerIndex, const AsciiString& power)
{
	if (playerIndex >= 0 && playerIndex < MAX_PLAYER_COUNT && activeForClient())
		s_specialPowers.emplace_back(playerIndex, power);
}

bool isEvaluating()
{
	return s_evaluating;
}

void update()
{
	if (TheGameLogic->isLoadingMap() || TheGameLogic->isLoadingSave())
		return;
	if (!activeForClient() || !TheGameLogic->isInGame() || TheGameLogic->isInShellGame())
	{
		s_pending.clear();
		s_specialPowers.clear();
		return;
	}
	if (s_handle >= AHSV_FirstHandle && !TheAudio->isCurrentlyPlaying(s_handle))
	{
		s_handle = AHSV_NoSound;
		if (Track* track = currentTrack())
		{
			if (--track->remaining > 0)
				play(FALSE);
			else
				notify(*track);
		}
	}
	for (auto& event : s_pending)
		apply(std::move(event));
	s_pending.clear();
	ScriptList* list = TheSidesList->getRetailMusicScripts();
	const UnsignedInt frame = TheGameLogic->getFrame();
	if (!list)
	{
		s_specialPowers.clear();
		return;
	}
	if (frame == s_frame && s_specialPowers.empty())
		return;
	const UnsignedInt elapsed = s_frame == -1u ? 0 : frame - s_frame;
	s_frame = frame;
	for (auto& [name, counter] : s_counters)
		if (counter.timer && counter.value >= 0)
			counter.value = elapsed > static_cast<UnsignedInt>(counter.value) ? -1 : counter.value - static_cast<Int>(elapsed);
	ScriptGroup* group = list->getScriptGroup();
	const LatchRestore<bool> evaluating(s_evaluating, true);
	walk(list->getScript(), group, 1, TRUE);
	s_specialPowers.clear();
}
}
