#include "PreRTS.h"

#include "GameClient/RetailEva.h"

#include <algorithm>
#include <cstdlib>
#include <map>
#include <set>

#include "Common/AudioEventRTS.h"
#include "Common/GameAudio.h"
#include "Common/GameUtility.h"
#include "GameClient/ClientRandomValue.h"
#include "Common/INI.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/PlayerTemplate.h"
#include "Common/SpecialPower.h"
#include "Common/Radar.h"
#include "Common/RetailFileLayers.h"
#include "Common/ThingTemplate.h"
#include "GameClient/Drawable.h"
#include "GameLogic/Damage.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#if !RTS_GENERALS
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Module/SlavedUpdate.h"
#endif

namespace
{
struct SideSound
{
	AsciiString side;
	std::vector<AsciiString> sounds;
};

struct Event
{
	UnsignedInt priority = 5;
	UnsignedInt repeat = 0;
	UnsignedInt expiration = 0;
	UnsignedInt delay = 0;
	UnsignedInt quiet = 0;
	Bool fromBase = FALSE;
	Bool jumpTo = TRUE;
	std::vector<AsciiString> blocks;
	std::vector<SideSound> sides;
};

struct Pending
{
	const Event *event;
	AsciiString name;
	UnsignedInt frame;
	Coord3D position;
	Bool positioned;
};

struct Settings
{
	Real sightRange = 0;
	UnsignedInt enemyCampTimeout = 0;
	UnsignedInt friendlyCampTimeout = 0;
	UnsignedInt jumpKeep = 0;
	UnsignedInt jumpReset = 0;
	Real jumpDistance = 0;
};

struct PlayedEvent
{
	UnsignedInt frame;
	Coord3D position;
	Bool positioned;
};

Event s_defaults;
std::map<AsciiString, Event> s_events;
std::vector<Pending> s_pending;
std::map<AsciiString, UnsignedInt> s_lastPlayed;
std::map<AsciiString, PlayedEvent> s_playedEvents;
AudioEventRTS s_speech;
UnsignedInt s_quietUntil = 0;
std::map<std::pair<ObjectID, UnsignedInt>, const SpecialPowerTemplate *> s_recharging;
std::set<ObjectID> s_seenRings;
std::set<ObjectID> s_seenEnemies;
const Player *s_viewer = nullptr;

void parseSideSound(INI *ini, void *, void *store, const void *)
{
	static const FieldParse fields[] = {
		{ "Side", INI::parseAsciiString, nullptr, offsetof(SideSound, side) },
		{ "Sound", INI::parseSoundsList, nullptr, offsetof(SideSound, sounds) },
		{ "Sounds", INI::parseSoundsList, nullptr, offsetof(SideSound, sounds) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	auto &sides = *static_cast<std::vector<SideSound> *>(store);
	SideSound sound;
	ini->initFromINI(&sound, fields);
	sides.push_back(sound);
}

const SideSound *findSound(const Event &event, const Player &player)
{
	AsciiString templateSide = "Player";
	templateSide.concat(player.getSide());
	const PlayerTemplate *playerTemplate = player.getPlayerTemplate();
	const AsciiString baseSide = playerTemplate ? playerTemplate->getBaseSide() : AsciiString();
	for (const AsciiString &side : {player.getSide(), baseSide, templateSide, AsciiString("Default")})
	{
		if (side.isEmpty())
			continue;
		for (auto it = event.sides.rbegin(); it != event.sides.rend(); ++it)
		{
			if (it->side.compareNoCase(side) == 0)
				return &*it;
		}
	}
	return nullptr;
}

RadarEventType radarType(RetailEva::Ping ping)
{
	switch (ping)
	{
		case RetailEva::Ping::Attack: return RADAR_EVENT_UNDER_ATTACK;
		case RetailEva::Ping::Generic: return RADAR_EVENT_CONSTRUCTION;
		case RetailEva::Ping::JumpOnly: return RADAR_EVENT_FAKE;
		case RetailEva::Ping::Upgrade: return RADAR_EVENT_UPGRADE;
		case RetailEva::Ping::OneRing: return RADAR_EVENT_ONE_RING;
		default: return RADAR_EVENT_INVALID;
	}
}

AsciiString eventKey(AsciiString name)
{
	name.toLower();
	return name;
}
}

namespace RetailEva
{
void load()
{
	INI ini;
	ini.loadFileDirectory("Data\\INI\\Default\\Eva", INI_LOAD_OVERWRITE, nullptr, 0);
	ini.loadFileDirectory("Data\\INI\\Eva", INI_LOAD_OVERWRITE, nullptr, 0);
}

void parseEvent(INI *ini)
{
	static const FieldParse fields[] = {
		{ "Priority", INI::parseUnsignedInt, nullptr, offsetof(Event, priority) },
		{ "TimeBetweenEventsMS", INI::parseDurationUnsignedInt, nullptr, offsetof(Event, repeat) },
		{ "TimeBetweenChecksMS", INI::parseDurationUnsignedInt, nullptr, offsetof(Event, repeat) },
		{ "ExpirationTimeMS", INI::parseDurationUnsignedInt, nullptr, offsetof(Event, expiration) },
		{ "MillisecondsToWaitBeforePlaying", INI::parseDurationUnsignedInt, nullptr, offsetof(Event, delay) },
		{ "QuietTimeMS", INI::parseDurationUnsignedInt, nullptr, offsetof(Event, quiet) },
		{ "AlwaysPlayFromHomeBase", INI::parseBool, nullptr, offsetof(Event, fromBase) },
		{ "CountAsJumpToLocation", INI::parseBool, nullptr, offsetof(Event, jumpTo) },
		{ "OtherEvaEventsToBlock", INI::parseSoundsList, nullptr, offsetof(Event, blocks) },
		{ "SideSound", parseSideSound, nullptr, offsetof(Event, sides) },
		{ "SideSounds", parseSideSound, nullptr, offsetof(Event, sides) },
		{ nullptr, nullptr, nullptr, 0 }
	};

	const AsciiString name = ini->getNextToken();
	Event event = s_defaults;
	ini->initFromINI(&event, fields);
	if (name.compareNoCase("DefaultEvaEvent") == 0)
		s_defaults = event;
	else
		s_events[eventKey(name)] = event;
}

void parseSettings(INI *ini)
{
	static const FieldParse fields[] = {
		{ "EnemySightedMaxVoicePositionScanRange", INI::parseReal, nullptr, offsetof(Settings, sightRange) },
		{ "EnemyCampDestroyedDamageTimeoutMS", INI::parseUnsignedInt, nullptr, offsetof(Settings, enemyCampTimeout) },
		{ "FriendlyCampDestroyedDamageTimeoutMS", INI::parseUnsignedInt, nullptr, offsetof(Settings, friendlyCampTimeout) },
		{ "MaxMillisecondsToKeepJumpToEvents", INI::parseUnsignedInt, nullptr, offsetof(Settings, jumpKeep) },
		{ "MaxMillisecondsBeforeResettingLastJumpTo", INI::parseUnsignedInt, nullptr, offsetof(Settings, jumpReset) },
		{ "MinDistanceBetweenJumpToEvents", INI::parseReal, nullptr, offsetof(Settings, jumpDistance) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	Settings settings;
	ini->initFromINI(&settings, fields);
}

void reset()
{
	s_pending.clear();
	s_lastPlayed.clear();
	s_playedEvents.clear();
	s_recharging.clear();
	s_seenRings.clear();
	s_seenEnemies.clear();
	s_viewer = nullptr;
	s_speech.setPlayingHandle(0);
	s_quietUntil = 0;
}

void queue(const Player *recipient, const AsciiString &name, const Coord3D *position, Ping ping,
	const Coord3D *radarPosition)
{
	if (!RetailFileLayers::enabled() || !recipient || !ThePlayerList || recipient != rts::getObservedOrLocalPlayer_Safe() || !TheGameLogic)
		return;
	const auto key = eventKey(name);
	const auto it = s_events.find(key);
	const Coord3D* location = radarPosition ? radarPosition : position;
	if (location && TheRadar && ping != Ping::None)
	{
		const Bool jumpTo = ping == Ping::Attack || it == s_events.end() || it->second.jumpTo;
		const RadarEventType type = radarType(ping);
		if (ping == Ping::Generic || ping == Ping::Upgrade)
			TheRadar->createEvent(location, type, 4.0f, jumpTo);
		else if (ping == Ping::JumpOnly)
		{
			if (it != s_events.end() && jumpTo)
				TheRadar->tryEvent(type, location);
		}
		else if (ping == Ping::Attack || it != s_events.end() || name.isEmpty())
			TheRadar->tryEvent(type, location, jumpTo);
	}
	if (it == s_events.end())
		return;

	const auto frame = TheGameLogic->getFrame();
	const Event &event = it->second;
	const auto last = s_lastPlayed.find(key);
	if (last != s_lastPlayed.end() && frame - last->second < event.repeat)
		return;
	if (std::any_of(s_pending.begin(), s_pending.end(), [&event](const Pending &pending) { return pending.event == &event; }))
		return;
	s_pending.push_back({&event, name, frame, position ? *position : Coord3D{}, position != nullptr});
}

Bool getLastPlayed(const AsciiString &name, UnsignedInt &frame, Coord3D *position)
{
	const auto found = s_playedEvents.find(eventKey(name));
	if (found == s_playedEvents.end() || (position && !found->second.positioned))
		return FALSE;
	frame = found->second.frame;
	if (position)
		*position = found->second.position;
	return TRUE;
}

void markFirstSighted(const Object& object)
{
#if !RTS_GENERALS
	const Object* control = SlavedUpdate::getEvaSightedControl(&object);
#else
	const Object* control = &object;
#endif
	s_seenEnemies.insert(control->getID());
}

void watchRecharge(const Object &object, const SpecialPowerTemplate &power)
{
	if (RetailFileLayers::enabled() && object.getControllingPlayer() == rts::getObservedOrLocalPlayer_Safe())
		s_recharging[{object.getID(), power.getID()}] = &power;
}

static void updateWorldEvents(Player &player, UnsignedInt frame)
{
	for (auto it = s_recharging.begin(); it != s_recharging.end(); )
	{
		Object *object = TheGameLogic->findObjectByID(it->first.first);
		const SpecialPowerModuleInterface *module = object ? object->getSpecialPowerModule(it->second) : nullptr;
		if (!module || object->isEffectivelyDead() || object->getControllingPlayer() != &player)
			it = s_recharging.erase(it);
		else if (module->getReadyFrame() <= frame && module->isReady())
		{
			queue(&player, it->second->getName(), object->getPosition(), Ping::JumpOnly);
			it = s_recharging.erase(it);
		}
		else
			++it;
	}

	if (frame % LOGICFRAMES_PER_SECOND != 0)
		return;
	for (const Object *object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
	{
		if (object->isKindOf(KINDOF_ONE_RING) && !object->isEffectivelyDead() &&
			object->getShroudedStatus(player.getPlayerIndex()) <= OBJECTSHROUD_PARTIAL_CLEAR &&
			s_seenRings.insert(object->getID()).second)
			queue(&player, "DiscoveredRing", object->getPosition(), Ping::OneRing);
#if !RTS_GENERALS
		if (!RetailFileLayers::enabled() || object->isEffectivelyDead() || object->getEnclosingContainedBy() ||
			(object->testStatus(OBJECT_STATUS_STEALTHED) && !object->testStatus(OBJECT_STATUS_DETECTED)))
			continue;
		const Object* control = SlavedUpdate::getEvaSightedControl(object);
		const AsciiString& event = control->getTemplate()->getRotWKObjectData().evaEnemyObjectSightedEvent;
		if (!event.isEmpty() && event.compareNoCase("None") != 0 &&
			player.getRelationship(control->getTeam()) == ENEMIES &&
			ThePartitionManager->getShroudStatusForPlayer(player.getPlayerIndex(), object->getPosition()) == CELLSHROUD_CLEAR &&
			s_seenEnemies.insert(control->getID()).second)
			queue(&player, event, control->getPosition(), Ping::JumpOnly);
#endif
	}
}

void update()
{
	if (!TheGameLogic || !ThePlayerList || !TheAudio)
		return;

	Player *player = rts::getObservedOrLocalPlayer_Safe();
	if (!player || !TheGameLogic->isInGame())
		return;
	if (s_viewer && s_viewer != player)
		reset();
	s_viewer = player;

	const auto frame = TheGameLogic->getFrame();
	updateWorldEvents(*player, frame);
	std::erase_if(s_pending, [frame](const Pending &pending)
	{
		return frame - pending.frame > std::max(pending.event->delay, pending.event->expiration);
	});
	if (s_pending.empty() || frame < s_quietUntil || s_speech.isCurrentlyPlaying())
		return;

	auto selected = s_pending.end();
	for (auto it = s_pending.begin(); it != s_pending.end(); ++it)
	{
		if (frame - it->frame >= it->event->delay &&
			(selected == s_pending.end() || it->event->priority > selected->event->priority))
			selected = it;
	}
	if (selected == s_pending.end())
		return;

	const Pending pending = *selected;
	s_pending.erase(selected);
	const Event &event = *pending.event;

	s_lastPlayed[eventKey(pending.name)] = frame;
	s_quietUntil = frame + event.quiet;
	for (const AsciiString &name : event.blocks)
	{
		const auto blocked = s_events.find(eventKey(name));
		if (blocked == s_events.end())
			continue;
		s_lastPlayed[blocked->first] = frame;
		std::erase_if(s_pending, [&blocked](const Pending &other) { return other.event == &blocked->second; });
	}

	Coord3D position = pending.position;
	const Bool positioned = event.fromBase ? player->getAiBaseCenter(&position) : pending.positioned;
	s_playedEvents.insert_or_assign(eventKey(pending.name), PlayedEvent{frame, position, positioned});

	const SideSound *side = findSound(event, *player);
	if (!side || side->sounds.empty())
		return;
	const AsciiString &sound = side->sounds[GameClientRandomValue(0, static_cast<Int>(side->sounds.size() - 1))];
	if (sound.compareNoCase("None") == 0)
		return;

	AudioEventRTS speech(sound);
	speech.setPlayerIndex(player->getPlayerIndex());
	if (positioned)
		speech.setPosition(&position);
	const Bool valid = TheAudio->isValidAudioEvent(&speech);
	if (valid)
	{
		s_speech = speech;
		s_speech.setPlayingHandle(TheAudio->addAudioEvent(&s_speech));
	}
	if (std::getenv("GENERALSX_RETAIL_EVA_LOG"))
	{
		std::fprintf(stderr, "EVA frame=%u event=%s sound=%s valid=%d\n", frame, pending.name.str(), sound.str(), valid);
		std::fflush(stderr);
	}
}

#if !RTS_GENERALS
void underAttack(Object &object, const DamageInfo &damage)
{
	const Player *owner = object.getControllingPlayer();
	if (!owner || !TheGameLogic || owner != rts::getObservedOrLocalPlayer_Safe())
		return;

	const auto &events = object.getTemplate()->getRotWKObjectData();
	const Object *source = TheGameLogic->findObjectByID(damage.in.m_sourceID);
	const Object *horde = HordeContain::getAnchor(&object);
	const auto combat = MAKE_MODELCONDITION_MASK2(MODELCONDITION_EMOTION_ALERT, MODELCONDITION_ATTACKING);
	const Bool canBeAmbushed = events.evaEventAmbushed.isNotEmpty() &&
		!object.getLogicModelConditions().test(MODELCONDITION_ENGAGED) &&
		!object.getLogicModelConditions().testForAny(combat) &&
		(!horde || !horde->getLogicModelConditions().testForAny(combat));

	const AsciiString *name = &events.evaEventDamagedOwner;
	if (canBeAmbushed && source && object.getRelationship(source) == ENEMIES && !source->isLogicallyVisible())
		name = &events.evaEventAmbushed;
	else if (damage.in.m_damageType == DAMAGE_LOGICAL_FIRE)
	{
		if (events.evaEventDamagedByFireOwner.isNotEmpty())
			name = &events.evaEventDamagedByFireOwner;
	}
	else if (events.evaEventDamagedFromShroudedSourceOwner.isNotEmpty() && source && source->getDrawable() &&
		source->getDrawable()->getFullyObscuredByShroud())
		name = &events.evaEventDamagedFromShroudedSourceOwner;

	queue(owner, name->isEmpty() ? AsciiString(object.isKindOf(KINDOF_STRUCTURE) ? "StructureUnderAttack" : "UnitUnderAttack") : *name,
		object.getPosition(), object.friend_getRadarData() ? Ping::Attack : Ping::None);
}
#endif

void destroyed(const Object &object)
{
	Player *owner = object.getControllingPlayer();
	Player *viewer = rts::getObservedOrLocalPlayer_Safe();
	if (!owner || !viewer)
		return;

	const auto &data = object.getTemplate()->getRotWKObjectData();
	if (owner == viewer)
		queue(viewer, data.evaEventDieOwner, object.getPosition(), Ping::JumpOnly);
	else
	{
		const Relationship relationship = viewer->getRelationship(object.getTeam());
		const Int playerIndex = viewer->getPlayerIndex();
		if (object.getShroudedStatus(playerIndex) > OBJECTSHROUD_PARTIAL_CLEAR ||
			ThePartitionManager->getShroudStatusForPlayer(playerIndex, object.getPosition()) != CELLSHROUD_CLEAR ||
			(relationship == ENEMIES && object.testStatus(OBJECT_STATUS_STEALTHED) &&
			 !object.testStatus(OBJECT_STATUS_DETECTED) && !object.testStatus(OBJECT_STATUS_DISGUISED)))
			return;

		if (relationship == ALLIES)
			queue(viewer, data.evaEventDieAlly, object.getPosition(), Ping::JumpOnly);
		else if (relationship == ENEMIES)
			queue(viewer, data.evaEventDieEnemy, object.getPosition(), Ping::JumpOnly);
	}
}
}
