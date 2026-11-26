#include "PreRTS.h"

#include "Common/RetailFileLayers.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/ScriptEngine.h"

#include <cstdio>
#include <cstdlib>
#include <set>
#include <tuple>

namespace
{
struct RotwkTemplate
{
	Int type;
	Int retailIndex;
	const char *internalName;
	const char *uiName;
	const char *text;
	Int numParameters;
	Int wireTypes[MAX_PARMS];
	Bool implemented;
};

const RotwkTemplate s_actions[] = {
	{ ScriptAction::NAMED_RECEIVE_UPGRADE, 256, "NAMED_RECEIVE_UPGRADE", "RotWK/NAMED_RECEIVE_UPGRADE", "<14> <33>", 2, { 14, 33 }, TRUE },
	{ ScriptAction::SET_CAMERA_CLIP_DEPTH_MULTIPLIER, 334, "SET_CAMERA_CLIP_DEPTH_MULTIPLIER", "RotWK/SET_CAMERA_CLIP_DEPTH_MULTIPLIER", "<1>", 1, { 1 }, TRUE },
	{ ScriptAction::TEAM_GIVE_EXPERIENCE_POINTS, 356, "TEAM_GIVE_EXPERIENCE_POINTS", "RotWK/TEAM_GIVE_EXPERIENCE_POINTS", "<3> <0>", 2, { 3, 0 }, TRUE },
	{ ScriptAction::UNIT_GIVE_EXPERIENCE_POINTS, 359, "UNIT_GIVE_EXPERIENCE_POINTS", "RotWK/UNIT_GIVE_EXPERIENCE_POINTS", "<14> <0>", 2, { 14, 0 }, TRUE },
	{ ScriptAction::SET_RANDOM_COUNTER, 373, "SET_RANDOM_COUNTER", "RotWK/SET_RANDOM_COUNTER", "<4> <0> <0>", 3, { 4, 0, 0 }, TRUE },
	{ ScriptAction::SET_UNIT_REFERENCE, 376, "SET_UNIT_REFERENCE", "RotWK/SET_UNIT_REFERENCE", "<54> <14>", 2, { 54, 14 }, FALSE },
	{ ScriptAction::BUILD_BUILDING_ON_FOUNDATION, 386, "BUILD_BUILDING_ON_FOUNDATION", "RotWK/BUILD_BUILDING_ON_FOUNDATION", "<15> <14>", 2, { 15, 14 }, FALSE },
	{ ScriptAction::ALLOW_DISALLOW_ONE_BUILDING, 387, "ALLOW_DISALLOW_ONE_BUILDING", "RotWK/ALLOW_DISALLOW_ONE_BUILDING", "<11> <15> <8>", 3, { 11, 15, 8 }, FALSE },
	{ ScriptAction::TEAM_MOVE_TO_NEAREST_OBJECT_OF_KINDOF_OWNED_BY_PLAYER, 392, "TEAM_MOVE_TO_NEAREST_OBJECT_OF_KINDOF_OWNED_BY_PLAYER", "RotWK/TEAM_MOVE_TO_NEAREST_OBJECT_OF_KINDOF_OWNED_BY_PLAYER", "<3> <27> <11>", 3, { 3, 27, 11 }, FALSE },
	{ ScriptAction::TEAM_MOVE_TO_NEAREST_OBJECT_OF_TYPE_OWNED_BY_PLAYER, 394, "TEAM_MOVE_TO_NEAREST_OBJECT_OF_TYPE_OWNED_BY_PLAYER", "RotWK/TEAM_MOVE_TO_NEAREST_OBJECT_OF_TYPE_OWNED_BY_PLAYER", "<3> <61> <11>", 3, { 3, 61, 11 }, FALSE },
	{ ScriptAction::TEAM_RECRUIT_UNITS, 397, "TEAM_RECRUIT_UNITS", "RotWK/TEAM_RECRUIT_UNITS", "<3> <0> <61>", 3, { 3, 0, 61 }, FALSE },
	{ ScriptAction::TEAM_RECRUIT_UNITS_FROM_TEAM, 398, "TEAM_RECRUIT_UNITS_FROM_TEAM", "RotWK/TEAM_RECRUIT_UNITS_FROM_TEAM", "<3> <0> <61> <3>", 4, { 3, 0, 61, 3 }, FALSE },
	{ ScriptAction::TEAM_UPGRADE, 403, "TEAM_UPGRADE", "RotWK/TEAM_UPGRADE", "<3> <33>", 2, { 3, 33 }, FALSE },
	{ ScriptAction::SET_PLAYER_COMMAND_POINTS_AVAILABLE_TO_COUNTER, 408, "SET_PLAYER_COMMAND_POINTS_AVAILABLE_TO_COUNTER", "RotWK/SET_PLAYER_COMMAND_POINTS_AVAILABLE_TO_COUNTER", "<11> <4>", 2, { 11, 4 }, TRUE },
	{ ScriptAction::SET_PLAYER_COMMAND_POINTS_TOTAL_TO_COUNTER, 409, "SET_PLAYER_COMMAND_POINTS_TOTAL_TO_COUNTER", "RotWK/SET_PLAYER_COMMAND_POINTS_TOTAL_TO_COUNTER", "<11> <4>", 2, { 11, 4 }, TRUE },
	{ ScriptAction::TEAM_GUARD_FOR_SECONDS, 413, "TEAM_GUARD_FOR_SECONDS", "RotWK/TEAM_GUARD_FOR_SECONDS", "<3> <0>", 2, { 3, 0 }, TRUE },
	{ ScriptAction::TEAM_IDLE_FOR_SECONDS, 414, "TEAM_IDLE_FOR_SECONDS", "RotWK/TEAM_IDLE_FOR_SECONDS", "<3> <0>", 2, { 3, 0 }, TRUE },
	{ ScriptAction::SET_COUNTER_IN_SECONDS, 415, "SET_COUNTER_IN_SECONDS", "RotWK/SET_COUNTER_IN_SECONDS", "<4> <1>", 2, { 4, 1 }, TRUE },
	{ ScriptAction::GIVE_PLAYER_UPGRADE, 419, "GIVE_PLAYER_UPGRADE", "RotWK/GIVE_PLAYER_UPGRADE", "<11> <33>", 2, { 11, 33 }, FALSE },
	{ ScriptAction::UNIT_SET_MODELCONDITION_FOR_DURATION, 434, "UNIT_SET_MODELCONDITION_FOR_DURATION", "RotWK/UNIT_SET_MODELCONDITION_FOR_DURATION", "<14> <58> <1> <52>", 4, { 14, 58, 1, 52 }, FALSE },
	{ ScriptAction::TEAM_SET_MODELCONDITION_FOR_DURATION, 435, "TEAM_SET_MODELCONDITION_FOR_DURATION", "RotWK/TEAM_SET_MODELCONDITION_FOR_DURATION", "<3> <58> <1> <52>", 4, { 3, 58, 1, 52 }, TRUE },
	{ ScriptAction::UNIT_GUARD_AREA, 454, "UNIT_GUARD_AREA", "RotWK/UNIT_GUARD_AREA", "<14> <9>", 2, { 14, 9 }, TRUE },
	{ ScriptAction::SET_COUNTER_TO_TEAM_THREAT, 440, "SET_COUNTER_TO_TEAM_THREAT", "RotWK/SET_COUNTER_TO_TEAM_THREAT", "<4> <3> <1>", 3, { 4, 3, 1 }, FALSE },
	{ ScriptAction::CREATE_REINFORCEMENT_TEAM_AT_UNIT_POSITION, 444, "CREATE_REINFORCEMENT_TEAM_AT_UNIT_POSITION", "RotWK/CREATE_REINFORCEMENT_TEAM_AT_UNIT_POSITION", "<3> <14>", 2, { 3, 14 }, FALSE },
	{ ScriptAction::AUDIO_POP_MUSIC, 451, "AUDIO_POP_MUSIC", "RotWK/AUDIO_POP_MUSIC", "<8> <8>", 2, { 8, 8 }, TRUE },
	{ ScriptAction::MOVE_TEAM_HOME, 455, "MOVE_TEAM_HOME", "RotWK/MOVE_TEAM_HOME", "<3>", 1, { 3 }, FALSE },
	{ ScriptAction::MUSIC_SCRIPT_POP_MUSIC, 458, "MUSIC_SCRIPT_POP_MUSIC", "RotWK/MUSIC_SCRIPT_POP_MUSIC", "<8> <8>", 2, { 8, 8 }, TRUE },
	{ ScriptAction::MUSIC_RETURN_TO_MUSIC_SCRIPTING, 460, "MUSIC_RETURN_TO_MUSIC_SCRIPTING", "RotWK/MUSIC_RETURN_TO_MUSIC_SCRIPTING", "<8> <8>", 2, { 8, 8 }, TRUE },
	{ ScriptAction::TEAM_SET_PLAYERS_NEAREST_UNIT_OF_TYPE_TO_REFERENCE, 462, "TEAM_SET_PLAYERS_NEAREST_UNIT_OF_TYPE_TO_REFERENCE", "RotWK/TEAM_SET_PLAYERS_NEAREST_UNIT_OF_TYPE_TO_REFERENCE", "<61> <3> <54>", 3, { 61, 3, 54 }, TRUE },
	{ ScriptAction::GATE_OPEN, 463, "GATE_OPEN", "RotWK/GATE_OPEN", "<14>", 1, { 14 }, FALSE },
	{ ScriptAction::GATE_CLOSE, 464, "GATE_CLOSE", "RotWK/GATE_CLOSE", "<14>", 1, { 14 }, FALSE },
	{ ScriptAction::MUSIC_PLAY_TRACK_FINITE_TIMES, 471, "MUSIC_PLAY_TRACK_FINITE_TIMES", "RotWK/MUSIC_PLAY_TRACK_FINITE_TIMES", "<22> <0> <8> <8>", 4, { 22, 0, 8, 8 }, TRUE },
	{ ScriptAction::MUSIC_PLAY_TRACK_FINITE_TIMES_AND_NOTIFY, 472, "MUSIC_PLAY_TRACK_FINITE_TIMES_AND_NOTIFY", "RotWK/MUSIC_PLAY_TRACK_FINITE_TIMES_AND_NOTIFY", "<22> <0> <8> <8> <5>", 5, { 22, 0, 8, 8, 5 }, TRUE },
	{ ScriptAction::MUSIC_PUSH_TRACK_FINITE_TIMES_AND_NOTIFY, 474, "MUSIC_PUSH_TRACK_FINITE_TIMES_AND_NOTIFY", "RotWK/MUSIC_PUSH_TRACK_FINITE_TIMES_AND_NOTIFY", "<22> <0> <8> <8> <5>", 5, { 22, 0, 8, 8, 5 }, TRUE },
	{ ScriptAction::MUSIC_SCRIPT_PLAY_TRACK_FINITE_TIMES_AND_NOTIFY, 476, "MUSIC_SCRIPT_PLAY_TRACK_FINITE_TIMES_AND_NOTIFY", "RotWK/MUSIC_SCRIPT_PLAY_TRACK_FINITE_TIMES_AND_NOTIFY", "<22> <0> <8> <8> <5>", 5, { 22, 0, 8, 8, 5 }, TRUE },
	{ ScriptAction::MUSIC_SCRIPT_PUSH_TRACK_FINITE_TIMES_AND_NOTIFY, 478, "MUSIC_SCRIPT_PUSH_TRACK_FINITE_TIMES_AND_NOTIFY", "RotWK/MUSIC_SCRIPT_PUSH_TRACK_FINITE_TIMES_AND_NOTIFY", "<22> <0> <8> <8> <5>", 5, { 22, 0, 8, 8, 5 }, TRUE },
	{ ScriptAction::TEAM_GUARD_AREA_FROM_POSITION, 487, "TEAM_GUARD_AREA_FROM_POSITION", "RotWK/TEAM_GUARD_AREA_FROM_POSITION", "<3> <9> <7>", 3, { 3, 9, 7 }, TRUE },
	{ ScriptAction::FIND_HOME_BASE_OF_PLAYER, 488, "FIND_HOME_BASE_OF_PLAYER", "RotWK/FIND_HOME_BASE_OF_PLAYER", "<11> <54> <8>", 3, { 11, 54, 8 }, TRUE },
	{ ScriptAction::TEAM_SET_CUSTOM_STATE, 490, "TEAM_SET_CUSTOM_STATE", "RotWK/TEAM_SET_CUSTOM_STATE", "<3> <18> <8>", 3, { 3, 18, 8 }, FALSE },
	{ ScriptAction::UNIT_GUARD_AREA_FROM_POSITION, 486, "UNIT_GUARD_AREA_FROM_POSITION", "RotWK/UNIT_GUARD_AREA_FROM_POSITION", "<14> <9> <7>", 3, { 14, 9, 7 }, TRUE },
	{ ScriptAction::SET_REF_TO_NEREST_TEAM_OF_TYPE_OWNED_BY_PLAYER, 491, "SET_REF_TO_NEREST_TEAM_OF_TYPE_OWNED_BY_PLAYER", "RotWK/SET_REF_TO_NEREST_TEAM_OF_TYPE_OWNED_BY_PLAYER", "<61> <11> <3> <54>", 4, { 61, 11, 3, 54 }, FALSE },
	{ ScriptAction::SET_REF_TO_NEREST_TEAM_OF_UNNAMED_TYPE_OWNED_BY_PLAYER, 492, "SET_REF_TO_NEREST_TEAM_OF_UNNAMED_TYPE_OWNED_BY_PLAYER", "RotWK/SET_REF_TO_NEREST_TEAM_OF_UNNAMED_TYPE_OWNED_BY_PLAYER", "<61> <11> <3> <54>", 4, { 61, 11, 3, 54 }, FALSE },
	{ ScriptAction::NAMED_BASE_UNPACK_FREE, 493, "NAMED_BASE_UNPACK_FREE", "RotWK/NAMED_BASE_UNPACK_FREE", "<14> <54>", 2, { 14, 54 }, FALSE },
	{ ScriptAction::UNIT_SET_TEAM, 499, "UNIT_SET_TEAM", "RotWK/UNIT_SET_TEAM", "<14> <3>", 2, { 14, 3 }, FALSE },
	{ ScriptAction::TEAM_STAND_GROUND, 501, "TEAM_STAND_GROUND", "RotWK/TEAM_STAND_GROUND", "<3> <8>", 2, { 3, 8 }, FALSE },
	{ ScriptAction::UNIT_TELEPORT_TO_WAYPOINT, 502, "UNIT_TELEPORT_TO_WAYPOINT", "RotWK/UNIT_TELEPORT_TO_WAYPOINT", "<14> <7>", 2, { 14, 7 }, TRUE },
	{ ScriptAction::OVERRIDE_PLAYER_COMMAND_POINTS, 504, "OVERRIDE_PLAYER_COMMAND_POINTS", "RotWK/OVERRIDE_PLAYER_COMMAND_POINTS", "<11> <0> <0>", 3, { 11, 0, 0 }, TRUE },
	{ ScriptAction::SET_COUNTER_TO_CLIENT_RANDOM_VALUE, 508, "SET_COUNTER_TO_CLIENT_RANDOM_VALUE", "RotWK/SET_COUNTER_TO_CLIENT_RANDOM_VALUE", "<4> <0> <0>", 3, { 4, 0, 0 }, TRUE },
	{ ScriptAction::SHOW_MISSION_OBJECTIVE, 531, "SHOW_MISSION_OBJECTIVE", "RotWK/SHOW_MISSION_OBJECTIVE", "<0>", 1, { 0 }, TRUE },
	{ ScriptAction::HIDE_MISSION_OBJECTIVE, 532, "HIDE_MISSION_OBJECTIVE", "RotWK/HIDE_MISSION_OBJECTIVE", "<0>", 1, { 0 }, TRUE },
	{ ScriptAction::MARK_MISSION_OBJECTIVE_COMPLETED, 533, "MARK_MISSION_OBJECTIVE_COMPLETED", "RotWK/MARK_MISSION_OBJECTIVE_COMPLETED", "<0>", 1, { 0 }, TRUE },
	{ ScriptAction::MARK_MISSION_OBJECTIVE_NOT_COMPLETED, 534, "MARK_MISSION_OBJECTIVE_NOT_COMPLETED", "RotWK/MARK_MISSION_OBJECTIVE_NOT_COMPLETED", "<0>", 1, { 0 }, TRUE },
	{ ScriptAction::TEAM_FORCE_EMOTION, 520, "TEAM_FORCE_EMOTION", "RotWK/TEAM_FORCE_EMOTION", "<3> <63> <1>", 3, { 3, 63, 1 }, TRUE },
	{ ScriptAction::PLAYER_FORCE_EMOTION, 521, "PLAYER_FORCE_EMOTION", "RotWK/PLAYER_FORCE_EMOTION", "<11> <63> <1>", 3, { 11, 63, 1 }, TRUE },
	{ ScriptAction::TEAM_CHANGE_OBJECT_STATUS, 537, "TEAM_CHANGE_OBJECT_STATUS", "RotWK/TEAM_CHANGE_OBJECT_STATUS", "<3> <41> <8>", 3, { 3, 41, 8 }, FALSE },
	{ ScriptAction::ATTACK_MOVE_TEAM_TO, 544, "ATTACK_MOVE_TEAM_TO", "RotWK/ATTACK_MOVE_TEAM_TO", "<3> <7>", 2, { 3, 7 }, TRUE },
	{ ScriptAction::TEAM_ATTACK_MOVE_FOLLOW_WAYPOINTS, 545, "TEAM_ATTACK_MOVE_FOLLOW_WAYPOINTS", "RotWK/TEAM_ATTACK_MOVE_FOLLOW_WAYPOINTS", "<3> <24> <8> <8>", 4, { 3, 24, 8, 8 }, TRUE },
	{ ScriptAction::NAMED_ATTACK_FOLLOW_WAYPOINTS, 547, "NAMED_ATTACK_FOLLOW_WAYPOINTS", "RotWK/NAMED_ATTACK_FOLLOW_WAYPOINTS", "<14> <24>", 2, { 14, 24 }, TRUE },
	{ ScriptAction::ATTACK_MOVE_TEAM_TO_NAMED_OBJECT, 564, "ATTACK_MOVE_TEAM_TO_NAMED_OBJECT", "RotWK/ATTACK_MOVE_TEAM_TO_NAMED_OBJECT", "<3> <14>", 2, { 3, 14 }, FALSE },
	{ ScriptAction::ATTACK_MOVE_TEAM_TO_NEAREST_OBJECT_OF_TYPE_OF_PLAYER, 565, "ATTACK_MOVE_TEAM_TO_NEAREST_OBJECT_OF_TYPE_OF_PLAYER", "RotWK/ATTACK_MOVE_TEAM_TO_NEAREST_OBJECT_OF_TYPE_OF_PLAYER", "<3> <61> <11>", 3, { 3, 61, 11 }, FALSE },
	{ ScriptAction::ENABLE_OBJECTIVES_SCREEN, 582, "ENABLE_OBJECTIVES_SCREEN", "RotWK/ENABLE_OBJECTIVES_SCREEN", "", 0, {}, TRUE },
	{ ScriptAction::DISABLE_OBJECTIVES_SCREEN, 583, "DISABLE_OBJECTIVES_SCREEN", "RotWK/DISABLE_OBJECTIVES_SCREEN", "", 0, {}, TRUE },
	{ ScriptAction::DISPLAY_NOTIFICATION_BOX, 589, "DISPLAY_NOTIFICATION_BOX", "RotWK/DISPLAY_NOTIFICATION_BOX", "<77> <25> <0>", 3, { 77, 25, 0 }, TRUE },
	{ ScriptAction::DISPLAY_NOTIFICATION_BOX_WITH_OBJECT_TYPE_IMAGE_OVERRIDE, 593, "DISPLAY_NOTIFICATION_BOX_WITH_OBJECT_TYPE_IMAGE_OVERRIDE", "RotWK/DISPLAY_NOTIFICATION_BOX_WITH_OBJECT_TYPE_IMAGE_OVERRIDE", "<77> <25> <0> <15>", 4, { 77, 25, 0, 15 }, TRUE },
	{ ScriptAction::CLOSE_OBJECTIVES_SCREEN, 598, "CLOSE_OBJECTIVES_SCREEN", "RotWK/CLOSE_OBJECTIVES_SCREEN", "", 0, {}, TRUE },
};

const RotwkTemplate s_conditions[] = {
	{ Condition::PLAYER_HAS_COMPARISON_UNIT_TYPE_IN_TRIGGER_AREA_WITH_UPGRADE, 110, "PLAYER_HAS_COMPARISON_UNIT_TYPE_IN_TRIGGER_AREA_WITH_UPGRADE", "RotWK/PLAYER_HAS_COMPARISON_UNIT_TYPE_IN_TRIGGER_AREA_WITH_UPGRADE", "<11> <6> <0> <61> <9> <33>", 6, { 11, 6, 0, 61, 9, 33 }, TRUE },
	{ Condition::DISTANCE_BETWEEN_OBJ, 113, "DISTANCE_BETWEEN_OBJ", "RotWK/DISTANCE_BETWEEN_OBJ", "<14> <14> <6> <1>", 4, { 14, 14, 6, 1 }, TRUE },
	{ Condition::DISTANCE_BETWEEN_TEAM, 114, "DISTANCE_BETWEEN_TEAM", "RotWK/DISTANCE_BETWEEN_TEAM", "<3> <3> <6> <1>", 4, { 3, 3, 6, 1 }, TRUE },
	{ Condition::UNIT_THREAT_LEVEL, 115, "UNIT_THREAT_LEVEL", "RotWK/UNIT_THREAT_LEVEL", "<14> <6> <1> <1>", 4, { 14, 6, 1, 1 }, TRUE },
	{ Condition::TEAM_THREAT_LEVEL, 116, "TEAM_THREAT_LEVEL", "RotWK/TEAM_THREAT_LEVEL", "<3> <6> <1> <1>", 4, { 3, 6, 1, 1 }, FALSE },
	{ Condition::CAN_BUILD_AT_BASE, 127, "CAN_BUILD_AT_BASE", "RotWK/CAN_BUILD_AT_BASE", "<11> <14>", 2, { 11, 14 }, FALSE },
	{ Condition::PLAYER_HAS_KILLED_TYPE_UNITS, 129, "PLAYER_HAS_KILLED_TYPE_UNITS", "RotWK/PLAYER_HAS_KILLED_TYPE_UNITS", "<11> <0> <61>", 3, { 11, 0, 61 }, TRUE },
	{ Condition::PLAYER_HAS_N_OR_FEWER_BASES, 131, "PLAYER_HAS_N_OR_FEWER_BASES", "RotWK/PLAYER_HAS_N_OR_FEWER_BASES", "<11> <0>", 2, { 11, 0 }, FALSE },
	{ Condition::ENEMY_SIGHTED_BY_TEAM, 133, "ENEMY_SIGHTED_BY_TEAM", "RotWK/ENEMY_SIGHTED_BY_TEAM", "<3> <19> <11>", 3, { 3, 19, 11 }, FALSE },
	{ Condition::TEAM_CAN_PATH_TO_WAYPOINT, 139, "TEAM_CAN_PATH_TO_WAYPOINT", "RotWK/TEAM_CAN_PATH_TO_WAYPOINT", "<3> <7>", 2, { 3, 7 }, TRUE },
	{ Condition::TEAM_HAS_CUSTOM_STATE, 143, "TEAM_HAS_CUSTOM_STATE", "RotWK/TEAM_HAS_CUSTOM_STATE", "<3> <18>", 2, { 3, 18 }, FALSE },
	{ Condition::PLAYER_HAS_OBJECT_OF_VETERANCY, 146, "PLAYER_HAS_OBJECT_OF_VETERANCY", "RotWK/PLAYER_HAS_OBJECT_OF_VETERANCY", "<11> <61> <6> <0>", 4, { 11, 61, 6, 0 }, TRUE },
	{ Condition::PLAYER_HAS_NUMBER_OBJECTS_WITH_MODELCONDITION, 160, "PLAYER_HAS_NUMBER_OBJECTS_WITH_MODELCONDITION", "RotWK/PLAYER_HAS_NUMBER_OBJECTS_WITH_MODELCONDITION", "<11> <58> <6> <0>", 4, { 11, 58, 6, 0 }, TRUE },
	{ Condition::PLAYER_HAS_NUMBER_UNITS_DISTANCE_FROM_OBJECT, 161, "PLAYER_HAS_NUMBER_UNITS_DISTANCE_FROM_OBJECT", "RotWK/PLAYER_HAS_NUMBER_UNITS_DISTANCE_FROM_OBJECT", "<11> <6> <0> <1> <14>", 5, { 11, 6, 0, 1, 14 }, TRUE },
	{ Condition::UNIT_HAS_PASSENGER, 162, "UNIT_HAS_PASSENGER", "RotWK/UNIT_HAS_PASSENGER", "<14>", 1, { 14 }, FALSE },
	{ Condition::TEAM_IS_ATTACKED_AND_CANNOT_RETALIATE_ANY, 164, "TEAM_IS_ATTACKED_AND_CANNOT_RETALIATE_ANY", "RotWK/TEAM_IS_ATTACKED_AND_CANNOT_RETALIATE_ANY", "<3>", 1, { 3 }, FALSE },
	{ Condition::IS_GAME_IN_SKIRMISH_OR_MULTIPLAYER, 166, "IS_GAME_IN_SKIRMISH_OR_MULTIPLAYER", "RotWK/IS_GAME_IN_SKIRMISH_OR_MULTIPLAYER", "<8>", 1, { 8 }, TRUE },
	{ Condition::REGION_CAMPS_SHOULD_UNPACK, 167, "REGION_CAMPS_SHOULD_UNPACK", "RotWK/REGION_CAMPS_SHOULD_UNPACK", "", 0, {}, TRUE },
	{ Condition::COMPARE_NUM_PLAYERS_IN_GAME, 168, "COMPARE_NUM_PLAYERS_IN_GAME", "RotWK/COMPARE_NUM_PLAYERS_IN_GAME", "<6> <0>", 2, { 6, 0 }, TRUE },
	{ Condition::TEAM_HAS_FEWER_THAN_X_UNITS_WITH_KINDOF, 170, "TEAM_HAS_FEWER_THAN_X_UNITS_WITH_KINDOF", "RotWK/TEAM_HAS_FEWER_THAN_X_UNITS_WITH_KINDOF", "<3> <0> <27>", 3, { 3, 0, 27 }, FALSE },
	{ Condition::PLAYER_HAS_REACHED_LEVEL_CAP, 173, "PLAYER_HAS_REACHED_LEVEL_CAP", "RotWK/PLAYER_HAS_REACHED_LEVEL_CAP", "<11>", 1, { 11 }, TRUE },
	{ Condition::EVAL_TEAM_HEALTH, 174, "EVAL_TEAM_HEALTH", "RotWK/EVAL_TEAM_HEALTH", "<3> <6> <0>", 3, { 3, 6, 0 }, TRUE },
	{ Condition::PLAYER_HAS_NUM_UNITS_WITH_UPGRADE, 178, "PLAYER_HAS_NUM_UNITS_WITH_UPGRADE", "RotWK/PLAYER_HAS_NUM_UNITS_WITH_UPGRADE", "<11> <0> <33>", 3, { 11, 0, 33 }, FALSE },
	{ Condition::HAS_EVA_EVENT_PLAYED_IN_LAST_N_SECONDS, 186, "HAS_EVA_EVENT_PLAYED_IN_LAST_N_SECONDS", "RotWK/HAS_EVA_EVENT_PLAYED_IN_LAST_N_SECONDS", "<76> <1>", 2, { 76, 1 }, TRUE },
	{ Condition::IS_NUM_OF_UNITS_BELONGING_TO_PLAYER_NEAR_EVA_EVENT_LAST_PLAYED_LOCATION_COMPARISON_INT, 187, "IS_NUM_OF_UNITS_BELONGING_TO_PLAYER_NEAR_EVA_EVENT_LAST_PLAYED_LOCATION_COMPARISON_INT", "RotWK/IS_NUM_OF_UNITS_BELONGING_TO_PLAYER_NEAR_EVA_EVENT_LAST_PLAYED_LOCATION_COMPARISON_INT", "<11> <1> <76> <6> <0>", 5, { 11, 1, 76, 6, 0 }, TRUE },
	{ Condition::PLAYER_HAS_COMPARISON_UNIT_TYPE_IN_TRIGGER_AREA_COMPLETELY_BUILT, 188, "PLAYER_HAS_COMPARISON_UNIT_TYPE_IN_TRIGGER_AREA_COMPLETELY_BUILT", "RotWK/PLAYER_HAS_COMPARISON_UNIT_TYPE_IN_TRIGGER_AREA_COMPLETELY_BUILT", "<11> <6> <0> <61> <9>", 5, { 11, 6, 0, 61, 9 }, TRUE },
	{ Condition::IS_GAME_MODE_ACTIVE, 201, "IS_GAME_MODE_ACTIVE", "RotWK/IS_GAME_MODE_ACTIVE", "<10>", 1, { 10 }, TRUE },
};

const RotwkTemplate *findRotwk(Bool condition, Int type)
{
	if (!condition && type >= ScriptEngine::getActionTemplateCount())
		return nullptr;
	const RotwkTemplate *first = condition ? s_conditions : s_actions;
	const size_t count = condition ? ARRAY_SIZE(s_conditions) : ARRAY_SIZE(s_actions);
	for (size_t i = 0; i < count; ++i)
	{
		if (first[i].type == type)
			return &first[i];
	}
	return nullptr;
}

void fillTemplate(Template &target, const RotwkTemplate &source)
{
	target.m_internalName = source.internalName;
	target.m_uiName = source.uiName;
	target.m_numParameters = source.numParameters;
	for (Int i = 0; i < source.numParameters; ++i)
		target.m_parameters[i] = Parameter::fromBfmeWireType(source.wireTypes[i]);

	target.m_numUiStrings = 0;
	AsciiString segment;
	for (const char *ch = source.text; ; ++ch)
	{
		if (*ch == '<' || *ch == '\0')
		{
			if (*ch == '<' || segment.isNotEmpty())
				target.m_uiStrings[target.m_numUiStrings++] = segment;
			segment.clear();
			if (*ch == '\0')
				break;
			while (*ch && *ch != '>')
				++ch;
			if (*ch == '\0')
				break;
			continue;
		}
		segment.concat(*ch);
	}
	DEBUG_ASSERTCRASH(target.m_numUiStrings >= source.numParameters, ("RotWK template %s text and parameters differ", source.internalName));
}

void writeTemplates(FILE *file, Bool condition, const Template *templates, Int count)
{
	const char *kind = condition ? "condition" : "action";
	for (Int i = 0; i < count; ++i)
	{
		const Template &entry = templates[i];
		const RotwkTemplate *rotwk = findRotwk(condition, i);
		fprintf(file, "%s\t%d\t%s\t", kind, i, entry.m_internalName.str());
		for (Int p = 0; p < entry.getNumParameters(); ++p)
			fprintf(file, p ? ",%d" : "%d", static_cast<Int>(entry.getParameterType(p)));
		fprintf(file, "\t%d\t%d\n", entry.m_numUiStrings, rotwk ? rotwk->retailIndex : -1);
	}
}
}

void ScriptEngine::initRotwkTemplates()
{
	const Int actionCount = getActionTemplateCount();
	for (const RotwkTemplate &entry : s_actions)
		if (entry.type < actionCount)
			fillTemplate(m_actionTemplates[entry.type], entry);
	for (const RotwkTemplate &entry : s_conditions)
		fillTemplate(m_conditionTemplates[entry.type], entry);
}

Int ScriptEngine::getActionTemplateCount()
{
	return RetailFileLayers::enabled() ? ScriptAction::NUM_ITEMS : ScriptAction::SET_PLAYER_COMMAND_POINTS_AVAILABLE_TO_COUNTER;
}

Bool ScriptEngine::isRotwkImplemented(Bool condition, Int type)
{
	const RotwkTemplate *entry = findRotwk(condition, type);
	return entry && entry->implemented;
}

Int ScriptEngine::findRotwkActionByRetailIndex(Int retailIndex)
{
	if (!RetailFileLayers::enabled())
		return -1;
	for (const RotwkTemplate &entry : s_actions)
		if (entry.retailIndex == retailIndex)
			return entry.type;
	return -1;
}

Int ScriptEngine::findRotwkConditionByRetailIndex(Int retailIndex)
{
	for (const RotwkTemplate &entry : s_conditions)
		if (entry.retailIndex == retailIndex)
			return entry.type;
	return -1;
}

void ScriptEngine::reportRotwkUnimplemented(Bool condition, Int type)
{
	static Bool reportedActions[ScriptAction::NUM_ITEMS];
	static Bool reportedConditions[Condition::NUM_ITEMS];
	const Int limit = condition ? static_cast<Int>(Condition::NUM_ITEMS) : static_cast<Int>(ScriptAction::NUM_ITEMS);
	if (type < 0 || type >= limit)
		return;
	Bool &reported = condition ? reportedConditions[type] : reportedActions[type];
	if (reported)
		return;
	reported = TRUE;
	const RotwkTemplate *entry = findRotwk(condition, type);
	fprintf(stderr, "BFME script %s %s is not implemented\n", condition ? "condition" : "action",
		entry ? entry->internalName : "?");
}

void ScriptEngine::traceRotwk(Bool condition, Int type, Int value)
{
	static const Bool enabled = getenv("GENERALSX_SCRIPT_TRACE") != nullptr;
	if (!enabled)
		return;
	static std::set<std::tuple<Bool, Int, Int>> seen;
	if (!seen.emplace(condition, type, value).second)
		return;
	const RotwkTemplate *entry = findRotwk(condition, type);
	fprintf(stderr, "BFME_SCRIPT_TRACE\t%s\t%s\t%d\t%u\n", condition ? "condition" : "action",
		entry ? entry->internalName : "?", value, TheGameLogic ? TheGameLogic->getFrame() : 0u);
}

void ScriptEngine::dumpTemplates()
{
	const char *path = getenv("GENERALSX_SCRIPT_TEMPLATE_DUMP");
	if (!path || !*path)
		return;
	FILE *file = fopen(path, "w");
	if (!file)
		return;
	writeTemplates(file, FALSE, m_actionTemplates, getActionTemplateCount());
	writeTemplates(file, TRUE, m_conditionTemplates, Condition::NUM_ITEMS);
	fclose(file);
}
