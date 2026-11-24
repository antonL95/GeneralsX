#pragma once

#include "Common/GameLOD.h"

#include <array>

class OptionPreferences;

namespace RetailGraphicsOptions
{
enum Control { Model, Animation, Effects, Shadows, Terrain, Water, Texture, Shader, Decals, Count };

struct Range
{
	Int minimum;
	Int maximum;
};

struct Draft
{
	StaticGameLODLevel preset;
	std::array<Int, Count> values;
	Bool operator==(const Draft &) const = default;
};

Range range(Control control);
Draft fromPreset(StaticGameLODLevel preset);
Bool load(const OptionPreferences &preferences, Draft &draft);
void save(OptionPreferences &preferences, const Draft &draft);
StaticGameLODInfo apply(const Draft &draft);
}
