#include "PreRTS.h"

#include "GameClient/RetailLargeGroupAudio.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <map>
#include <string_view>

#include "Common/AudioEventRTS.h"
#include "Common/AudioHandleSpecialValues.h"
#include "Common/AudioRandomValue.h"
#include "Common/FileSystem.h"
#include "Common/GameUtility.h"
#include "Common/INI.h"
#include "Common/Player.h"
#include "Common/RetailFileLayers.h"
#include "GameClient/Drawable.h"
#include "GameClient/GameClient.h"
#include "GameClient/RetailClientAudio.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/RotwkParseOnlyModules.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/TerrainLogic.h"
#include "WWMath/wwmath.h"

namespace
{
struct Duck
{
	AsciiString map;
	AsciiString bucket;
	Real gain;
};

struct Sound
{
	AsciiString bucket;
	AsciiString event;
	std::vector<AsciiString> keys;
	std::vector<Duck> ducks;
};

struct MultiSound
{
	std::vector<AsciiString> sounds;
	Bool playOne = FALSE;
	Bool loop = FALSE;
};

struct AudioMap
{
	RetailClientAudio::Condition condition;
	ObjectStatusMaskType excludedStatus;
	std::vector<Sound> sounds;
	Real size = 10000.0f;
	Real speed = ConvertVelocityInSecsToFrames(20.0f);
	UnsignedShort start = 65535;
	UnsignedShort stop = 65535;
	UnsignedInt handoff = LOGICFRAMES_PER_SECOND * 2;
	Bool ignoreStealthed = TRUE;
};

struct Unit
{
	const Object* object;
	const LargeGroupAudioUpdateModuleData* data;
	const ModelConditionFlags* flags;
};

struct CellID
{
	Int x;
	Int y;

	bool operator==(const CellID&) const = default;
	bool operator<(const CellID& other) const { return std::tie(y, x) < std::tie(other.y, other.x); }
};

constexpr std::array<CellID, 8> directions = {{
	{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}
}};

struct Cell
{
	UnsignedInt weight = 0;
	const Object* owner = nullptr;
	AsciiString key;
};

struct Loop
{
	ObjectID owner;
	std::shared_ptr<AudioEventRTS::RetailEmitter> emitter;
	std::vector<AudioHandle> handles;
	AsciiString key;
	UnsignedInt handoff = 0;
};

using Cells = std::map<CellID, Cell>;
using Loops = std::map<CellID, Loop>;
using LoopKey = std::pair<AsciiString, AsciiString>;
std::map<AsciiString, AudioMap> maps;
std::map<LoopKey, Loops> active;
std::map<AsciiString, MultiSound> multiSounds;
AsciiString traceMap;
UnsignedInt lastFrame = -1u;

AsciiString lower(AsciiString text)
{
	text.toLower();
	return text;
}

AsciiString keySetName(const std::vector<AsciiString>& keys)
{
	std::vector<AsciiString> names;
	for (const AsciiString& key : keys)
		names.push_back(lower(key));
	std::sort(names.begin(), names.end());
	names.erase(std::unique(names.begin(), names.end()), names.end());
	AsciiString name;
	for (const AsciiString& key : names)
	{
		if (name.isNotEmpty())
			name.concat(" ");
		name.concat(key);
	}
	return name;
}

void parseDuck(INI* ini, void* instance, void*, const void*)
{
	Duck duck;
	duck.map = lower(ini->getNextSubToken("AudioMap"));
	duck.bucket = lower(ini->getNextSubToken("Sound"));
	duck.gain = INI::scanPercentToReal(ini->getNextSubToken("Multiplier"));
	static_cast<Sound*>(instance)->ducks.push_back(std::move(duck));
}

void parseSound(INI* ini, void* instance, void*, const void*)
{
	static const FieldParse fields[] = {
		{ "Sound", INI::parseAsciiString, nullptr, offsetof(Sound, event) },
		{ "Key", INI::parseAsciiStringVectorAppend, nullptr, offsetof(Sound, keys) },
		{ "Duck", parseDuck, nullptr, 0 },
		{ nullptr, nullptr, nullptr, 0 }
	};
	Sound sound;
	sound.bucket = ini->getNextAsciiString();
	ini->initFromINI(&sound, fields);
	if (sound.event.isEmpty() || sound.event.isNone() || sound.event.compareNoCase("NoSound") == 0)
		return;
	sound.bucket = sound.bucket.isEmpty() ? keySetName(sound.keys) : lower(sound.bucket);
	auto& sounds = static_cast<AudioMap*>(instance)->sounds;
	const auto existing = std::find_if(sounds.begin(), sounds.end(),
		[&](const Sound& other) { return other.bucket == sound.bucket; });
	if (existing == sounds.end())
		sounds.push_back(std::move(sound));
	else
		*existing = std::move(sound);
}

Bool matches(const Unit& unit, const AudioMap& map)
{
	return map.condition.matches(*unit.flags) && unit.object->getStatusBits().testForNone(map.excludedStatus) &&
		(!map.ignoreStealthed || !unit.object->testStatus(OBJECT_STATUS_STEALTHED));
}

CellID neighbor(CellID cell, CellID direction)
{
	return {cell.x + direction.x, cell.y + direction.y};
}

Int lookupAxis(Real position, Real size, Int offset)
{
	const Real origin = offset ? -size * 0.5f : 0.0f;
	return static_cast<Int>((position - origin) / size) * 2 + 1 - offset;
}

Coord3D center(CellID id, Real size)
{
	return {id.x * (size * 0.5f), id.y * (size * 0.5f), 0.0f};
}

Bool inGrid(CellID id, Real size, const Region3D& extent)
{
	const Coord3D c = center(id, size);
	const Real half = size * 0.5f;
	return id.x >= 0 && id.y >= 0 && c.x - half < extent.hi.x && c.y - half < extent.hi.y;
}

Cells tally(const AudioMap& map, const Sound& sound, const std::vector<Unit>& units, const Region3D& extent)
{
	Cells cells;
	for (const Unit& unit : units)
	{
		const auto key = std::find_if(unit.data->m_keys.begin(), unit.data->m_keys.end(),
			[&](const AsciiString& key)
			{
				return std::any_of(sound.keys.begin(), sound.keys.end(),
					[&](const AsciiString& pattern) { return key.compareNoCase(pattern) == 0; });
			});
		if (key == unit.data->m_keys.end() || unit.data->m_unitWeight == 0)
			continue;
		const Coord3D& pos = *unit.object->getPosition();
		for (Int y = 0; y < 2; ++y)
		{
			for (Int x = 0; x < 2; ++x)
			{
				const CellID id = {lookupAxis(pos.x, map.size, x), lookupAxis(pos.y, map.size, y)};
				if (!inGrid(id, map.size, extent))
					continue;
				Cell& cell = cells[id];
				cell.weight = static_cast<UnsignedShort>(cell.weight + unit.data->m_unitWeight);
				if (!cell.owner || unit.object->getID() < cell.owner->getID())
				{
					cell.owner = unit.object;
					cell.key = *key;
				}
			}
		}
	}
	return cells;
}

UnsignedInt weightAt(const Cells& cells, CellID id)
{
	const auto it = cells.find(id);
	return it == cells.end() ? 0 : it->second.weight;
}

Coord3D emitterTarget(CellID id, const AudioMap& map, const Cells& cells)
{
	Coord3D position = center(id, map.size);
	Real xWeight = 0.0f;
	Real yWeight = 0.0f;
	Real xOffset = 0.0f;
	Real yOffset = 0.0f;
	constexpr Real diagonalWeight = 0.7071067811865476f;
	for (const CellID direction : directions)
	{
		const Real weight = weightAt(cells, neighbor(id, direction)) *
			(direction.x && direction.y ? diagonalWeight : 1.0f);
		if (direction.x)
		{
			xWeight += weight;
			xOffset += weight * direction.x;
		}
		if (direction.y)
		{
			yWeight += weight;
			yOffset += weight * direction.y;
		}
	}
	const Real scale = map.size / 3.0f;
	if (xWeight > 0.0f)
		position.x += xOffset / xWeight * scale;
	if (yWeight > 0.0f)
		position.y += yOffset / yWeight * scale;
	position.z = TheTerrainLogic->getGroundHeight(position.x, position.y);
	return position;
}

void trace(const AsciiString& group, const Sound& sound, const char* kind, CellID id,
	const AsciiString& key, UnsignedInt weight, const Loop* loop = nullptr)
{
	if (traceMap != group)
		return;
	std::fprintf(stderr, "LARGE GROUP AUDIO frame=%u kind=%s group=%s key=%s weight=%u event=%s cell=%d,%d",
		lastFrame, kind, group.str(), key.str(), weight, sound.event.str(), id.x, id.y);
	if (loop)
	{
		const auto& emitter = *loop->emitter;
		std::fprintf(stderr, " owner=%u remaining=%u gain=%g pos=%.3f,%.3f,%.3f",
			static_cast<UnsignedInt>(loop->owner), loop->handoff, emitter.gain,
			emitter.position.x, emitter.position.y, emitter.position.z);
	}
	std::fputc('\n', stderr);
	std::fflush(stderr);
}

void queueEvent(Loop& loop, const AsciiString& event, Int playerIndex)
{
	std::vector<AsciiString> events;
	if (!RetailLargeGroupAudio::expandMultiSound(event, events))
		events.push_back(event);
	for (const AsciiString& child : events) {
		AudioEventRTS audio(child, &loop.emitter->position);
		audio.setPlayerIndex(playerIndex);
		audio.setRetailEmitter(loop.emitter);
		const AudioHandle handle = TheAudio->addAudioEvent(&audio);
		if (handle >= AHSV_FirstHandle)
			loop.handles.push_back(handle);
	}
}

Loop start(const Sound& sound, const Cell& cell, const Coord3D& position)
{
	Loop loop{cell.owner->getID(), std::make_shared<AudioEventRTS::RetailEmitter>(), {}, cell.key};
	loop.emitter->position = position;
	RetailClientAudio::trace("start", loop.owner, sound.event);
	if (TheAudio)
	{
		const Player* player = cell.owner->getControllingPlayer();
		queueEvent(loop, sound.event, player ? player->getPlayerIndex() : -1);
	}
	return loop;
}

void stop(const Sound& sound, const Loop& loop)
{
	RetailClientAudio::trace("stop", loop.owner, sound.event);
	if (TheAudio)
		for (AudioHandle handle : loop.handles)
			TheAudio->removeAudioEvent(handle);
}

Bool moveEmitter(Loop& loop, const Coord3D& target, Real speed, UnsignedInt elapsed)
{
	Coord3D& pos = loop.emitter->position;
	Coord3D delta = target;
	delta.sub(pos);
	Real fraction = 1.0f;
	Bool limited = FALSE;
	if (loop.handoff)
	{
		const UnsignedInt step = std::min(elapsed, loop.handoff);
		fraction = Real(step) / Real(loop.handoff);
		loop.handoff -= step;
	}
	else
	{
		const Real lengthSquared = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
		const Real maxDistance = speed * elapsed;
		if (lengthSquared > maxDistance * maxDistance)
		{
			fraction = maxDistance / WWMath::Sqrt(lengthSquared);
			limited = TRUE;
		}
	}
	pos.x += delta.x * fraction;
	pos.y += delta.y * fraction;
	pos.z += delta.z * fraction;
	return limited;
}

void dispatch(const AsciiString& name, const AudioMap& map, const Sound& sound,
	const Cells& cells, const Region3D& extent, UnsignedInt elapsed)
{
	Loops& loops = active[{name, sound.bucket}];
	const auto strongerNeighbor = [&](CellID id, UnsignedInt weight)
	{
		return std::any_of(directions.begin(), directions.end(),
			[&](CellID direction) { return weightAt(cells, neighbor(id, direction)) > weight; });
	};
	const auto reserved = [&](CellID id)
	{
		return loops.contains(id) || std::any_of(directions.begin(), directions.end(),
			[&](CellID direction) { return loops.contains(neighbor(id, direction)); });
	};
	const auto neighborsInGrid = [&](CellID id)
	{
		return std::count_if(directions.begin(), directions.end(),
			[&](CellID direction) { return inGrid(neighbor(id, direction), map.size, extent); });
	};
	const auto better = [&](CellID a, CellID b)
	{
		const UnsignedInt aWeight = weightAt(cells, a);
		const UnsignedInt bWeight = weightAt(cells, b);
		return aWeight != bWeight ? aWeight > bWeight : neighborsInGrid(a) > neighborsInGrid(b);
	};

	Loops displaced;
	for (auto it = loops.begin(); it != loops.end();)
	{
		const CellID id = it->first;
		const UnsignedInt weight = weightAt(cells, id);
		if (cells.empty() || weight < map.stop || strongerNeighbor(id, weight))
			displaced.insert(loops.extract(it++));
		else
			++it;
	}

	for (auto& [old, loop] : displaced)
	{
		CellID next = old;
		Bool found = FALSE;
		const auto consider = [&](CellID id)
		{
			if (cells.contains(id) && weightAt(cells, id) >= map.start && !reserved(id) && (!found || better(id, next)))
			{
				next = id;
				found = TRUE;
			}
		};
		for (CellID direction : directions)
			consider(neighbor(old, direction));
		consider(old);
		if (!found)
		{
			stop(sound, loop);
			trace(name, sound, "stop", old, loop.key, weightAt(cells, old), &loop);
			continue;
		}
		if (next != old)
		{
			loop.handoff = map.handoff;
			loop.key = cells.at(next).key;
			trace(name, sound, "handoff", next, loop.key, weightAt(cells, next), &loop);
		}
		loops.emplace(next, std::move(loop));
	}

	for (auto& [id, loop] : loops)
	{
		if (moveEmitter(loop, emitterTarget(id, map, cells), map.speed, elapsed))
			trace(name, sound, "speed-limit", id, loop.key, weightAt(cells, id), &loop);
	}

	std::vector<CellID> eligible;
	for (const auto& [id, cell] : cells)
	{
		if (cell.weight >= map.start)
			eligible.push_back(id);
		else if (traceMap == name && !reserved(id))
			trace(name, sound, "below", id, cell.key, cell.weight);
	}
	std::stable_sort(eligible.begin(), eligible.end(), better);
	for (CellID id : eligible)
	{
		if (reserved(id))
			continue;
		const Cell& cell = cells.at(id);
		const auto it = loops.emplace(id, start(sound, cell, emitterTarget(id, map, cells))).first;
		trace(name, sound, "start", id, cell.key, cell.weight, &it->second);
	}
	if (loops.empty())
		active.erase({name, sound.bucket});
}

Bool overlaps(CellID source, Real sourceSize, CellID target, Real targetSize)
{
	const Coord3D a = center(source, sourceSize);
	const Coord3D b = center(target, targetSize);
	const Real aHalf = sourceSize * 0.5f;
	const Real bHalf = targetSize * 0.5f;
	return a.x - aHalf < b.x + bHalf && a.x + aHalf >= b.x - bHalf &&
		a.y - aHalf < b.y + bHalf && a.y + aHalf >= b.y - bHalf;
}

void updateDucking(const std::map<LoopKey, Cells>& tracedCells)
{
	std::map<std::pair<LoopKey, CellID>, Real> gains;
	for (const auto& [name, map] : maps)
	{
		for (const Sound& sound : map.sounds)
		{
			if (sound.ducks.empty())
				continue;
			const auto source = active.find({name, sound.bucket});
			if (source == active.end())
				continue;
			for (const Duck& duck : sound.ducks)
			{
				const LoopKey targetKey{duck.map, duck.bucket};
				const auto target = active.find(targetKey);
				if (target == active.end())
					continue;
				const Real targetSize = maps.at(duck.map).size;
				for (const auto& [targetCell, loop] : target->second)
				{
					const Bool affected = std::any_of(source->second.begin(), source->second.end(),
						[&](const auto& entry) { return overlaps(entry.first, map.size, targetCell, targetSize); });
					if (affected)
					{
						auto [gain, inserted] = gains.try_emplace({targetKey, targetCell}, duck.gain);
						if (!inserted)
							gain->second = std::min(gain->second, duck.gain);
					}
				}
			}
		}
	}
	for (const auto& [name, map] : maps)
	{
		for (const Sound& sound : map.sounds)
		{
			const LoopKey key{name, sound.bucket};
			const auto source = active.find(key);
			if (source == active.end())
				continue;
			for (auto& [id, loop] : source->second)
			{
				const auto it = gains.find({key, id});
				const Real gain = it == gains.end() ? 1.0f : it->second;
				if (gain != loop.emitter->gain)
				{
					loop.emitter->gain = gain;
					if (const auto traced = tracedCells.find(key); traced != tracedCells.end())
						trace(name, sound, "duck", id, loop.key, weightAt(traced->second, id), &loop);
				}
			}
		}
	}
}

void clearLoops(const AsciiString& name, const AudioMap& map)
{
	for (const Sound& sound : map.sounds)
	{
		const auto it = active.find({name, sound.bucket});
		if (it == active.end())
			continue;
		for (const auto& [id, loop] : it->second)
			stop(sound, loop);
		active.erase(it);
	}
}
}

namespace RetailLargeGroupAudio
{
void load()
{
	INI ini;
	for (const char* path : {"Data\\INI\\Default\\LargeGroupAudio.ini", "Data\\INI\\LargeGroupAudio.ini"})
	{
		if (TheFileSystem->doesFileExist(path))
			ini.load(path, INI_LOAD_OVERWRITE, nullptr);
	}
}

void reset()
{
	for (const auto& [name, map] : maps)
		clearLoops(name, map);
	traceMap.clear();
	lastFrame = -1u;
}

void traceGroup(const char* name)
{
	traceMap = std::string_view(name) == "off" ? AsciiString() : lower(name);
}

void parseMap(INI* ini)
{
	static const FieldParse fields[] = {
		{ "Sound", parseSound, nullptr, 0 },
		{ "RequiredModelConditionFlags", ModelConditionFlags::parseFromINI, nullptr,
			offsetof(AudioMap, condition) + offsetof(RetailClientAudio::Condition, required) },
		{ "ExcludedModelConditionFlags", ModelConditionFlags::parseFromINI, nullptr,
			offsetof(AudioMap, condition) + offsetof(RetailClientAudio::Condition, excluded) },
		{ "ExcludedObjectStatusBits", ObjectStatusMaskType::parseFromINI, nullptr, offsetof(AudioMap, excludedStatus) },
		{ "IgnoreStealthedUnits", INI::parseBool, nullptr, offsetof(AudioMap, ignoreStealthed) },
		{ "Size", INI::parseReal, nullptr, offsetof(AudioMap, size) },
		{ "StartThreshold", INI::parseUnsignedShort, nullptr, offsetof(AudioMap, start) },
		{ "StopThreshold", INI::parseUnsignedShort, nullptr, offsetof(AudioMap, stop) },
		{ "HandOffModeDuration", INI::parseDurationUnsignedInt, nullptr, offsetof(AudioMap, handoff) },
		{ "MaximumAudioSpeed", INI::parseVelocityReal, nullptr, offsetof(AudioMap, speed) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	const AsciiString name = lower(ini->getNextAsciiString());
	AudioMap map;
	ini->initFromINI(&map, fields);
	if (const auto it = maps.find(name); it != maps.end())
		clearLoops(name, it->second);
	maps.insert_or_assign(name, std::move(map));
}

void parseMultiSound(INI* ini)
{
	const AsciiString name = lower(ini->getNextAsciiString());
	static const FieldParse fields[] = {
		{ "Subsounds", INI::parseAsciiStringVectorAppend, nullptr, offsetof(MultiSound, sounds) },
		{ "Control", [](INI* ini, void* instance, void*, const void*)
			{
				for (const char* flag = ini->getNextToken(); flag; flag = ini->getNextTokenOrNull())
				{
					if (stricmp(flag, "PLAY_ONE") == 0)
						static_cast<MultiSound*>(instance)->playOne = TRUE;
					else if (stricmp(flag, "LOOP") == 0)
						static_cast<MultiSound*>(instance)->loop = TRUE;
					else
						throw INI_INVALID_DATA;
				}
			}, nullptr, 0 },
		{ nullptr, nullptr, nullptr, 0 }
	};
	MultiSound multi;
	ini->initFromINI(&multi, fields);
	multiSounds.insert_or_assign(name, std::move(multi));
}

AsciiString chooseMultiSoundTrack(const AsciiString& event)
{
	AsciiString track = event;
	std::vector<AsciiString> parents;
	while (true)
	{
		const AsciiString key = lower(track);
		const auto multi = multiSounds.find(key);
		if (multi == multiSounds.end())
			return track;
		const MultiSound& found = multi->second;
		const auto& children = found.sounds;
		if (children.empty() || (!found.playOne && children.size() != 1) ||
			std::find(parents.begin(), parents.end(), key) != parents.end())
			return AsciiString();
		parents.push_back(key);
		track = children[GameAudioRandomValue(0, static_cast<Int>(children.size() - 1))];
	}
}

Bool expandMultiSound(const AsciiString& name, std::vector<AsciiString>& events)
{
	if (multiSounds.find(lower(name)) == multiSounds.end())
		return FALSE;
	std::vector<AsciiString> parents;
	const auto expand = [&](const auto& self, const AsciiString& event) -> void {
		const AsciiString key = lower(event);
		const auto found = multiSounds.find(key);
		if (found == multiSounds.end()) {
			events.push_back(event);
			return;
		}
		const MultiSound& multi = found->second;
		if (multi.sounds.empty() || std::find(parents.begin(), parents.end(), key) != parents.end())
			return;
		parents.push_back(key);
		if (multi.playOne)
			self(self, multi.sounds[GameAudioRandomValue(0, static_cast<Int>(multi.sounds.size() - 1))]);
		else
			for (const AsciiString& child : multi.sounds)
				self(self, child);
		parents.pop_back();
	};
	expand(expand, name);
	return TRUE;
}

Bool isMultiSound(const AsciiString& name)
{
	const auto found = multiSounds.find(lower(name));
	return found != multiSounds.end() && !found->second.sounds.empty();
}

void parseUnusedKeys(INI* ini)
{
	static const FieldParse fields[] = {
		{ "Key", INI::parseAsciiStringVectorAppend, nullptr, 0 },
		{ nullptr, nullptr, nullptr, 0 }
	};
	std::vector<AsciiString> keys;
	ini->initFromINI(&keys, fields);
}

void update()
{
	if (!RetailFileLayers::enabled() || maps.empty() || !TheGameClient || !TheGameLogic || !TheTerrainLogic)
		return;
	const UnsignedInt frame = TheGameLogic->getFrame();
	if (frame == lastFrame)
		return;
	const UnsignedInt elapsed = frame > lastFrame ? frame - lastFrame : 1;
	lastFrame = frame;
	Region3D extent{};
	TheTerrainLogic->getExtent(&extent);

	std::vector<Unit> candidates;
	const Player* viewer = rts::getObservedOrLocalPlayer_Safe();
	static const NameKeyType groupKey = NAMEKEY("LargeGroupAudioUpdate");
	for (const Drawable* drawable = TheGameClient->firstDrawable(); drawable; drawable = drawable->getNextDrawable())
	{
		const Object* object = drawable->getObject();
		if (!object || object->isEffectivelyDead() || !object->isLogicallyVisible())
			continue;
		const PartitionData* partition = object->friend_getPartitionData();
		if (viewer && partition && !object->isKindOf(KINDOF_ALWAYS_VISIBLE) &&
			!partition->hasClearCell(viewer->getPlayerIndex()))
			continue;
		for (BehaviorModule** module = object->getBehaviorModules(); *module; ++module)
		{
			if ((*module)->getModuleNameKey() == groupKey)
			{
				const auto* data = static_cast<const LargeGroupAudioUpdate*>(*module)->audioData();
				candidates.push_back({object, data, &drawable->getModelConditionFlags()});
			}
		}
	}

	std::map<LoopKey, Cells> tracedCells;
	for (const auto& [name, map] : maps)
	{
		if (map.size <= 0.0f)
			continue;
		std::vector<Unit> units;
		std::copy_if(candidates.begin(), candidates.end(), std::back_inserter(units),
			[&](const Unit& unit) { return matches(unit, map); });
		for (const Sound& sound : map.sounds)
		{
			Cells cells = tally(map, sound, units, extent);
			const LoopKey key{name, sound.bucket};
			if (!cells.empty() || active.contains(key))
				dispatch(name, map, sound, cells, extent, elapsed);
			if (traceMap == name)
				tracedCells.emplace(key, std::move(cells));
		}
	}
	updateDucking(tracedCells);
}
}
