#include "PreRTS.h"

#include "GameClient/RetailClientAudio.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <set>
#include <utility>

#include "Common/GameAudio.h"
#include "Common/AudioHandleSpecialValues.h"
#include "Common/INI.h"
#include "Common/Player.h"
#include "Common/RetailFileLayers.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "GameClient/Drawable.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"

#if !RTS_GENERALS
#include "GameLogic/Module/ProductionUpdate.h"
#endif

namespace
{
using namespace RetailClientAudio;

using Attribute = std::pair<AsciiString, std::vector<AsciiString>>;

std::vector<Attribute> attributes(INI* ini)
{
	std::string text;
	while (const char* token = ini->getNextTokenOrNull())
	{
		if (!text.empty())
			text += ' ';
		text += token;
	}
	std::vector<Attribute> result;
	std::size_t colon = text.find(':');
	while (colon != std::string::npos)
	{
		const std::size_t keyEnd = text.find_last_not_of(' ', colon - 1) + 1;
		const std::size_t keyStart = text.rfind(' ', keyEnd - 1) + 1;
		if (keyEnd <= keyStart || keyEnd >= colon + 1)
			throw INI_INVALID_DATA;
		result.emplace_back(text.substr(keyStart, keyEnd - keyStart).c_str(), std::vector<AsciiString>{});
		const std::size_t next = text.find(':', colon + 1);
		const std::size_t valueEnd = next == std::string::npos ? text.size() :
			text.rfind(' ', text.find_last_not_of(' ', next - 1));
		for (std::size_t pos = text.find_first_not_of(' ', colon + 1); pos < valueEnd;)
		{
			const std::size_t end = std::min(text.find(' ', pos), valueEnd);
			result.back().second.emplace_back(text.substr(pos, end - pos).c_str());
			pos = text.find_first_not_of(' ', end);
		}
		colon = next;
	}
	return result;
}

void applyConditions(ModelConditionFlags& flags, const std::vector<AsciiString>& names)
{
	for (const auto& name : names)
	{
		if (name.isNotNone())
			flags.set(INI::scanIndexList(name.str(), ModelConditionFlags::getBitNames()));
	}
}

void parseLoop(INI* ini, void* instance, void*, const void*)
{
	Loop rule;
	for (const auto& [key, values] : attributes(ini))
	{
		if (key.compareNoCase("Required") == 0)
			applyConditions(rule.condition.required, values);
		else if (key.compareNoCase("Excluded") == 0)
			applyConditions(rule.condition.excluded, values);
		else if (key.compareNoCase("Sound") == 0 && values.size() == 1)
			rule.sound = values.front();
		else
			throw INI_INVALID_DATA;
	}
	if (rule.sound.isEmpty())
		throw INI_INVALID_DATA;
	static_cast<Data*>(instance)->loops.push_back(std::move(rule));
}

void parseAnimation(INI* ini, void* instance, void*, const void*)
{
	AnimationCue cue;
	for (const auto& [key, values] : attributes(ini))
	{
		if (key.compareNoCase("RequiredMC") == 0)
			applyConditions(cue.condition.required, values);
		else if (key.compareNoCase("ExcludedMC") == 0)
			applyConditions(cue.condition.excluded, values);
		else if (key.compareNoCase("Sound") == 0 && values.size() == 1)
			cue.sound = values.front();
		else if (key.compareNoCase("Animation") == 0)
			cue.animations.insert(cue.animations.end(), values.begin(), values.end());
		else if (key.compareNoCase("Frames") == 0)
		{
			auto& frames = cue.frames.emplace_back();
			for (const auto& value : values)
				frames.push_back(INI::scanInt(value.str()));
		}
		else
			throw INI_INVALID_DATA;
	}
	if (cue.sound.isEmpty() || cue.animations.empty() ||
		(cue.frames.size() != 1 && cue.frames.size() != cue.animations.size()))
		throw INI_INVALID_DATA;
	static_cast<Data*>(instance)->cues.push_back(std::move(cue));
}

void parseSoundEntry(INI* ini, void*, void* store, const void* field)
{
	AsciiString name(static_cast<const char*>(field));
	name.toLower();
	auto& sounds = *static_cast<std::map<AsciiString, AudioEventRTS>*>(store);
	sounds.insert_or_assign(name, AudioEventRTS(ini->getNextAsciiString()));
}

void parseUnitSounds(INI* ini, void*, void* store, const void*)
{
	static const FieldParse fields[] = {
		{ nullptr, parseSoundEntry, nullptr, 0 }
	};
	ini->initFromINI(store, fields);
}

void parseVoicePriority(INI* ini, void* instance, void*, const void*)
{
	static_cast<VoiceRule*>(instance)->priority = INI::scanInt(ini->getNextToken());
}

void parseVoices(INI* ini, void* instance, const void* selector)
{
	static const FieldParse fields[] = {
		{ "RequiredModelConditions", ModelConditionFlags::parseFromINI, nullptr,
			offsetof(VoiceRule, condition) + offsetof(Condition, required) },
		{ "ExcludedUpgrades", INI::parseAsciiStringVector, nullptr, offsetof(VoiceRule, excludedUpgrades) },
		{ "VoicePriority", parseVoicePriority, nullptr, 0 },
		{ "UnitSpecificSounds", parseUnitSounds, nullptr, offsetof(VoiceRule, sounds) },
		{ nullptr, parseSoundEntry, nullptr, offsetof(VoiceRule, sounds) }
	};
	VoiceRule rule;
	if (selector)
		ModelConditionFlags::parseFromINI(ini, nullptr, &rule.condition.required, nullptr);
	else
		INI::parseAsciiStringVector(ini, nullptr, &rule.upgrades, nullptr);
	ini->initFromINI(&rule, fields);
	static_cast<Data*>(instance)->voices.push_back(std::move(rule));
}

void parseVoiceState(INI* ini, void* instance, void*, const void* selector)
{
	parseVoices(ini, instance, selector);
}

void parseRandomPriority(INI* ini, Data& data)
{
	static const FieldParse fields[] = {
		{ "Chance", INI::parsePercentToReal, nullptr, offsetof(RandomPriority, chance) },
		{ "VoicePriority", INI::parseInt, nullptr, offsetof(RandomPriority, priority) },
		{ "RerollOnEveryFrame", INI::parseBool, nullptr, offsetof(RandomPriority, reroll) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	ini->initFromINI(&data.randomPriority.emplace(), fields);
}

Bool hasUpgrade(const Object& object, const AsciiString& name)
{
	const UpgradeTemplate* upgrade = TheUpgradeCenter ? TheUpgradeCenter->findUpgrade(name) : nullptr;
	const Player* player = object.getControllingPlayer();
	return upgrade && (object.getObjectCompletedUpgradeMask().testForAny(upgrade->getUpgradeMask()) ||
		(player && player->getCompletedUpgradeMask().testForAny(upgrade->getUpgradeMask())));
}

Bool matches(const VoiceRule& rule, const Object& object)
{
	const Drawable* drawable = object.getDrawable();
	const ModelConditionFlags flags = drawable ? drawable->getModelConditionFlags() : ModelConditionFlags{};
	return rule.condition.matches(flags) &&
		std::all_of(rule.upgrades.begin(), rule.upgrades.end(), [&](const auto& name) { return hasUpgrade(object, name); }) &&
		std::none_of(rule.excludedUpgrades.begin(), rule.excludedUpgrades.end(), [&](const auto& name) { return hasUpgrade(object, name); });
}
}

namespace RetailClientAudio
{
std::shared_ptr<const Data> parse(INI* ini, const AsciiString& name)
{
	static const FieldParse loopFields[] = {
		{ "ModelCondition", parseLoop, nullptr, 0 },
		{ nullptr, nullptr, nullptr, 0 }
	};
	static const FieldParse animationFields[] = {
		{ "AnimationSound", parseAnimation, nullptr, 0 },
		{ "MaxUpdateRangeCap", INI::parseReal, nullptr, offsetof(Data, animationRange) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	static const FieldParse voiceFields[] = {
		{ "SoundState", parseVoiceState, "SoundState", 0 },
		{ "SoundUpgrade", parseVoiceState, nullptr, 0 },
		{ nullptr, nullptr, nullptr, 0 }
	};

	auto data = std::make_shared<Data>();
	if (name.compareNoCase("ModelConditionAudioLoopClientBehavior") == 0)
		ini->initFromINI(data.get(), loopFields);
	else if (name.compareNoCase("AnimationSoundClientBehavior") == 0)
		ini->initFromINI(data.get(), animationFields);
	else if (name.compareNoCase("ModelConditionSoundSelectorClientBehavior") == 0 ||
		name.compareNoCase("UpgradeSoundSelectorClientBehavior") == 0)
		ini->initFromINI(data.get(), voiceFields);
	else if (name.compareNoCase("RandomSoundSelectorClientBehavior") == 0)
		parseRandomPriority(ini, *data);
	else
		return {};
	return data;
}

const AudioEventRTS* select(const Object& owner, AsciiString response, const AudioEventRTS* fallback)
{
	if (!RetailFileLayers::enabled())
		return fallback;
	response.toLower();
	const auto& entries = owner.getTemplate()->getRotWKObjectData().clientBehaviors;
	for (const auto& entry : entries)
	{
		if (!entry.audio)
			continue;
		for (const auto& rule : entry.audio->voices)
		{
			const auto sound = rule.sounds.find(response);
			if (sound != rule.sounds.end() && matches(rule, owner))
				return &sound->second;
		}
	}
	return fallback;
}

Int priority(const Object& owner)
{
	const auto& config = owner.getTemplate()->getRotWKObjectData();
	Int priority = config.voicePriority;
	for (const auto& entry : config.clientBehaviors)
	{
		if (!entry.audio)
			continue;
		for (const auto& rule : entry.audio->voices)
		{
			if (rule.priority && matches(rule, owner))
				priority = *rule.priority;
		}
		if (const auto& random = entry.audio->randomPriority; random && random->chance > 0.0f)
		{
			std::seed_seq seed{static_cast<UnsignedInt>(owner.getID()),
				random->reroll && TheGameLogic ? TheGameLogic->getFrame() : 0u};
			std::minstd_rand generator(seed);
			if (std::generate_canonical<Real, std::numeric_limits<Real>::digits>(generator) < random->chance)
				priority = random->priority;
		}
	}
	return priority;
}

void trace(const char* action, ObjectID owner, const AsciiString& sound)
{
	static const Bool enabled = std::getenv("GENERALSX_TRACE_RETAIL_AUDIO") != nullptr;
	if (enabled && TheGameLogic && sound.isNotEmpty() && sound.isNotNone() && sound.compareNoCase("NoSound") != 0)
		std::fprintf(stderr, "RETAIL AUDIO %s frame=%u owner=%u sound=%s\n", action,
			TheGameLogic->getFrame(), static_cast<UnsignedInt>(owner), sound.str());
}

AudioHandle start(const Object& owner, const AsciiString& sound)
{
	if (!TheAudio || sound.isEmpty() || sound.isNone() ||
		sound.compareNoCase("NoSound") == 0)
		return AHSV_NoSound;
	AudioEventRTS event(sound);
	event.setObjectID(owner.getID());
	if (const Player* player = owner.getControllingPlayer())
		event.setPlayerIndex(player->getPlayerIndex());
	trace("start", owner.getID(), sound);
	return TheAudio->addAudioEvent(&event);
}

void stop(ObjectID owner, const AsciiString& sound, AudioHandle handle)
{
	trace("stop", owner, sound);
	if (TheAudio && handle >= AHSV_FirstHandle)
		TheAudio->removeAudioEvent(handle);
}

Loops::~Loops()
{
	for (const auto& [sound, handle] : m_active)
		stop(m_owner, sound, handle);
}

Bool Loops::hasLoops(const Drawable& drawable)
{
#if !RTS_GENERALS
	const Object* object = drawable.getObject();
	if (object && ProductionUpdate::getRetailSpeedBonusLoop(*object, FALSE))
		return TRUE;
#endif
	const auto& entries = drawable.getTemplate()->getRotWKObjectData().clientBehaviors;
	return std::any_of(entries.begin(), entries.end(), [](const auto& entry)
		{ return entry.audio && !entry.audio->loops.empty(); });
}

void Loops::update(const Drawable& drawable)
{
	const Object* object = drawable.getObject();
	std::set<AsciiString> desired;
	if (object && !object->isEffectivelyDead())
	{
		m_owner = object->getID();
#if !RTS_GENERALS
		if (const auto* sound = ProductionUpdate::getRetailSpeedBonusLoop(*object, TRUE))
			desired.insert(*sound);
#endif
		const auto& flags = drawable.getModelConditionFlags();
		for (const auto& entry : drawable.getTemplate()->getRotWKObjectData().clientBehaviors)
		{
			if (!entry.audio)
				continue;
			for (const auto& loop : entry.audio->loops)
			{
				if (loop.condition.matches(flags))
					desired.insert(loop.sound);
			}
		}
	}

	for (auto it = m_active.begin(); it != m_active.end();)
	{
		if (desired.erase(it->first) != 0)
			++it;
		else
		{
			stop(m_owner, it->first, it->second);
			it = m_active.erase(it);
		}
	}
	for (const auto& sound : desired)
		m_active.emplace(sound, start(*object, sound));
}

void animate(const Drawable& drawable, const char* clip, Real before, Real current,
	Int frameCount, Bool first, Bool backwards, Bool loops)
{
	const Object* object = drawable.getObject();
	if (!object || !TheAudio || (!first && before == current))
		return;
	const Coord3D* listener = TheAudio->getListenerPosition();
	const Coord3D* position = drawable.getPosition();
	const auto& flags = drawable.getModelConditionFlags();
	for (const auto& entry : drawable.getTemplate()->getRotWKObjectData().clientBehaviors)
	{
		if (!entry.audio || entry.audio->cues.empty())
			continue;
		const Real range = entry.audio->animationRange;
		if (range > 0.0f && listener)
		{
			const Real x = position->x - listener->x;
			const Real y = position->y - listener->y;
			if (x * x + y * y > range * range)
				continue;
		}
		for (const auto& cue : entry.audio->cues)
		{
			if (!cue.condition.matches(flags))
				continue;
			for (std::size_t i = 0; i < cue.animations.size(); ++i)
			{
				if (stricmp(clip, cue.animations[i].str()) != 0)
					continue;
				const auto& frames = cue.frames[cue.frames.size() == 1 ? 0 : i];
				for (Int frame : frames)
				{
					if (frame < 0 || frame >= frameCount)
						continue;
					const Bool crossed = backwards ?
						((loops && current > before) ? frame < before || frame >= current :
							(frame < before || (first && frame == before)) && frame >= current) :
						((loops && current < before) ? frame > before || frame <= current :
							(frame > before || (first && frame == before)) && frame <= current);
					if (crossed)
						start(*object, cue.sound);
				}
			}
		}
	}
}
}
