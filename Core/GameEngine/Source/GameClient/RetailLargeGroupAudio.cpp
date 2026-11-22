#include "PreRTS.h"

#include "GameClient/RetailLargeGroupAudio.h"

#include <algorithm>
#include <map>

#include "Common/FileSystem.h"
#include "Common/GameUtility.h"
#include "Common/INI.h"
#include "Common/Player.h"
#include "Common/RetailFileLayers.h"
#include "GameClient/Drawable.h"
#include "GameClient/GameClient.h"
#include "GameClient/RetailClientAudio.h"
#include "GameLogic/Module/RotwkParseOnlyModules.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"

namespace
{
struct Sound
{
	AsciiString name;
	std::vector<AsciiString> keys;
};

struct AudioMap
{
	RetailClientAudio::Condition condition;
	ObjectStatusMaskType excludedStatus;
	std::vector<Sound> sounds;
	Real size = 0;
	Int start = 0;
	Int stop = 0;
	Bool ignoreStealthed = TRUE;
};

struct Unit
{
	const Object* object;
	const LargeGroupAudioUpdateModuleData* data;
	const ModelConditionFlags* flags;
};

struct Loop
{
	ObjectID owner;
	Coord3D position;
	AudioHandle handle;
};

using LoopKey = std::pair<AsciiString, AsciiString>;
std::map<AsciiString, AudioMap> maps;
std::map<LoopKey, std::vector<Loop>> active;

void parseSound(INI* ini, void* instance, void*, const void*)
{
	static const FieldParse fields[] = {
		{ "Sound", INI::parseAsciiString, nullptr, offsetof(Sound, name) },
		{ "Key", INI::parseAsciiStringVectorAppend, nullptr, offsetof(Sound, keys) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	Sound sound;
	ini->initFromINI(&sound, fields);
	auto& sounds = static_cast<AudioMap*>(instance)->sounds;
	const auto existing = std::find_if(sounds.begin(), sounds.end(),
		[&](const Sound& other) { return other.name.compareNoCase(sound.name) == 0; });
	if (existing == sounds.end())
		sounds.push_back(std::move(sound));
	else
		existing->keys.insert(existing->keys.end(), sound.keys.begin(), sound.keys.end());
}

Real distanceSquared(const Coord3D& a, const Coord3D& b)
{
	const Real x = a.x - b.x;
	const Real y = a.y - b.y;
	return x * x + y * y;
}

Bool matches(const Unit& unit, const AudioMap& map, const Sound& sound)
{
	return map.condition.matches(*unit.flags) && unit.object->getStatusBits().testForNone(map.excludedStatus) &&
		(!map.ignoreStealthed || !unit.object->testStatus(OBJECT_STATUS_STEALTHED)) &&
		std::any_of(unit.data->m_keys.begin(), unit.data->m_keys.end(), [&](const auto& key)
		{
			return std::any_of(sound.keys.begin(), sound.keys.end(),
				[&](const auto& pattern) { return key.compareNoCase(pattern) == 0; });
		});
}

Int64 weightWithin(const std::vector<Unit>& units, const Coord3D& position, Real radiusSquared)
{
	Int64 weight = 0;
	for (const auto& unit : units)
	{
		if (distanceSquared(position, *unit.object->getPosition()) <= radiusSquared)
			weight += std::max(unit.data->m_unitWeight, 1);
	}
	return weight;
}

void dispatch(const AudioMap& map, const Sound& sound, std::vector<Unit> units, std::vector<Loop>& loops)
{
	const Real radiusSquared = map.size * map.size;
	const auto removeNearby = [&](const Coord3D& center)
	{
		std::erase_if(units, [&](const Unit& unit)
			{ return distanceSquared(*unit.object->getPosition(), center) <= radiusSquared; });
	};
	const auto findOwner = [&](ObjectID id)
	{
		return std::find_if(units.begin(), units.end(), [&](const Unit& unit) { return unit.object->getID() == id; });
	};
	for (auto it = loops.begin(); it != loops.end();)
	{
		const auto owner = findOwner(it->owner);
		if (owner != units.end() &&
			weightWithin(units, *owner->object->getPosition(), radiusSquared) >= std::max(map.stop, 1))
		{
			it->position = *owner->object->getPosition();
			removeNearby(it->position);
			++it;
		}
		else
		{
			RetailClientAudio::stop(it->owner, sound.name, it->handle);
			it = loops.erase(it);
		}
	}
	while (!units.empty())
	{
		const Object* anchor = nullptr;
		Int64 weight = 0;
		for (const auto& unit : units)
		{
			const Int64 nearby = weightWithin(units, *unit.object->getPosition(), radiusSquared);
			if (nearby > weight)
			{
				weight = nearby;
				anchor = unit.object;
			}
		}
		if (!anchor || weight < std::max(map.start, 1))
			break;
		const Coord3D center = *anchor->getPosition();
		loops.push_back({anchor->getID(), center, RetailClientAudio::start(*anchor, sound.name)});
		removeNearby(center);
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
	for (const auto& [key, loops] : active)
		for (const Loop& loop : loops)
			RetailClientAudio::stop(loop.owner, key.second, loop.handle);
	active.clear();
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
		{ "StartThreshold", INI::parseInt, nullptr, offsetof(AudioMap, start) },
		{ "StopThreshold", INI::parseInt, nullptr, offsetof(AudioMap, stop) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	AsciiString name = ini->getNextAsciiString();
	name.toLower();
	AudioMap map;
	ini->initFromINI(&map, fields);
	if (const auto it = maps.find(name); it != maps.end())
	{
		for (const Sound& sound : it->second.sounds)
		{
			const auto loops = active.find({name, sound.name});
			if (loops == active.end())
				continue;
			for (const Loop& loop : loops->second)
				RetailClientAudio::stop(loop.owner, sound.name, loop.handle);
			active.erase(loops);
		}
	}
	maps.insert_or_assign(name, std::move(map));
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
	if (maps.empty() || !TheGameClient)
		return;
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
	std::stable_sort(candidates.begin(), candidates.end(), [](const Unit& a, const Unit& b)
		{ return a.object->getID() < b.object->getID(); });
	for (const auto& [name, map] : maps)
	{
		if (map.size <= 0.0f)
			continue;
		for (const Sound& sound : map.sounds)
		{
			std::vector<Unit> units;
			std::copy_if(candidates.begin(), candidates.end(), std::back_inserter(units),
				[&](const Unit& unit) { return matches(unit, map, sound); });
			dispatch(map, sound, std::move(units), active[{name, sound.name}]);
		}
	}
}
}
