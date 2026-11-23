#pragma once

#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "Common/AudioEventRTS.h"
#include "Common/ModelState.h"

class INI;
class Object;
class Drawable;

namespace RetailClientAudio
{
struct Condition
{
	ModelConditionFlags required;
	ModelConditionFlags excluded;

	Bool matches(const ModelConditionFlags& flags) const
	{
		return flags.testForAll(required) && flags.testForNone(excluded);
	}
};

struct Loop
{
	Condition condition;
	AsciiString sound;
};

struct AnimationCue
{
	Condition condition;
	AsciiString sound;
	std::vector<AsciiString> animations;
	std::vector<std::vector<Int>> frames;
};

struct VoiceRule
{
	Condition condition;
	std::vector<AsciiString> upgrades;
	std::vector<AsciiString> excludedUpgrades;
	std::map<AsciiString, AudioEventRTS> sounds;
	std::optional<Int> priority;
};

struct RandomPriority
{
	Real chance = 0;
	Int priority = 0;
	Bool reroll = FALSE;
};

struct Data
{
	std::vector<Loop> loops;
	std::vector<AnimationCue> cues;
	Real animationRange = 0;
	std::vector<VoiceRule> voices;
	std::optional<RandomPriority> randomPriority;
};

std::shared_ptr<const Data> parse(INI* ini, const AsciiString& name);
const AudioEventRTS* select(const Object& owner, AsciiString response, const AudioEventRTS* fallback = nullptr);
Int priority(const Object& owner);
AudioHandle start(const Object& owner, const AsciiString& sound);
void stop(ObjectID owner, const AsciiString& sound, AudioHandle handle);
void trace(const char* action, ObjectID owner, const AsciiString& sound);

class Loops
{
public:
	static Bool hasLoops(const Drawable& drawable);
	~Loops();
	void update(const Drawable& drawable);

private:
	ObjectID m_owner = INVALID_ID;
	std::map<AsciiString, AudioHandle> m_active;
};

void animate(const Drawable& drawable, const char* clip, Real before, Real current,
	Int frameCount, Bool first, Bool backwards, Bool loops);
}
