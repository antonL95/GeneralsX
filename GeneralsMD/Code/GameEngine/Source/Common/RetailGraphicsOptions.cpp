#include "PreRTS.h"

#include "Common/RetailGraphicsOptions.h"
#include "Common/OptionPreferences.h"

#include <algorithm>

namespace RetailGraphicsOptions
{
namespace
{
struct Preference
{
	const char *key;
	Range range;
};

constexpr Preference detail[] = {
	{"RetailModelDetail", {1, 4}},
	{"RetailAnimationDetail", {0, 4}},
	{"RetailEffectsDetail", {0, 4}},
	{"RetailShadowDetail", {0, 4}},
	{"RetailTerrainDetail", {0, 2}},
	{"RetailWaterDetail", {1, 4}},
	{"RetailTextureDetail", {0, 2}},
	{"RetailShaderDetail", {1, 4}},
	{"RetailDecalDetail", {0, 2}}
};
static_assert(std::size(detail) == Count);

const StaticGameLODInfo &presetInfo(StaticGameLODLevel preset)
{
	return TheGameLODManager->m_staticGameLODInfo[preset];
}
}

Range range(Control control)
{
	return detail[control].range;
}

Draft fromPreset(StaticGameLODLevel preset)
{
	const StaticGameLODInfo &info = presetInfo(preset);
	Draft draft{preset, {info.m_modelQuality, info.m_animationDetail, info.m_effectQuality,
		info.m_shadowQuality, info.m_useTerrainNormalMap ? 2 : info.m_useDistanceDependantTerrainTextures ? 1 : 0,
		info.m_waterQuality, 2 - std::max(0, info.m_textureReduction), info.m_shaderQuality,
		info.m_decalQuality >= 3 ? 2 : info.m_decalQuality > 0 ? 1 : 0}};
	for (Int i = 0; i < Count; ++i)
		draft.values[i] = std::clamp(draft.values[i], detail[i].range.minimum, detail[i].range.maximum);
	return draft;
}

Bool load(const OptionPreferences &preferences, Draft &draft)
{
	const AsciiString name = preferences.getAsciiString("RetailCustomGraphicsPreset", "");
	if (name.isEmpty())
		return FALSE;
	for (Int i = STATIC_GAME_LOD_VERY_LOW; i <= STATIC_GAME_LOD_LAST; ++i)
	{
		const auto preset = StaticGameLODLevel(i);
		if (name.compareNoCase(TheGameLODManager->getStaticGameLODLevelName(preset)) != 0)
			continue;
		draft = fromPreset(preset);
		for (Int j = 0; j < Count; ++j)
			draft.values[j] = std::clamp(preferences.getInt(detail[j].key, draft.values[j]),
				detail[j].range.minimum, detail[j].range.maximum);
		return TRUE;
	}
	return FALSE;
}

void save(OptionPreferences &preferences, const Draft &draft)
{
	preferences.setAsciiString("RetailCustomGraphicsPreset", TheGameLODManager->getStaticGameLODLevelName(draft.preset));
	for (Int i = 0; i < Count; ++i)
		preferences.setInt(detail[i].key, draft.values[i]);
}

StaticGameLODInfo apply(const Draft &draft)
{
	StaticGameLODInfo info = presetInfo(draft.preset);
	const auto original = fromPreset(draft.preset);
	const auto &value = draft.values;
	const auto changed = [&](Control control) { return value[control] != original.values[control]; };
	if (changed(Model))
	{
		info.m_modelQuality = value[Model];
		info.m_useTrees = presetInfo(StaticGameLODLevel(value[Model])).m_useTrees;
	}
	if (changed(Animation))
		info.m_animationDetail = value[Animation];
	if (changed(Effects))
	{
		const auto &effect = presetInfo(StaticGameLODLevel(value[Effects]));
		info.m_effectQuality = value[Effects];
		info.m_maxParticleCount = effect.m_maxParticleCount;
		info.m_minParticlePriority = effect.m_minParticlePriority;
		info.m_minParticleSkipPriority = effect.m_minParticleSkipPriority;
	}
	if (changed(Shadows))
	{
		const auto &shadow = presetInfo(StaticGameLODLevel(value[Shadows]));
		info.m_shadowQuality = value[Shadows];
		info.m_useShadowMapping = shadow.m_useShadowMapping;
		info.m_useShadowVolumes = shadow.m_useShadowVolumes;
	}
	if (changed(Terrain))
	{
		info.m_useTerrainNormalMap = value[Terrain] == 2;
		info.m_useDistanceDependantTerrainTextures = value[Terrain] >= 1;
	}
	if (changed(Water))
	{
		info.m_waterQuality = value[Water];
		info.m_showSoftWaterEdge = presetInfo(StaticGameLODLevel(value[Water])).m_showSoftWaterEdge;
	}
	if (changed(Texture))
		info.m_textureReduction = 2 - value[Texture];
	if (changed(Shader))
	{
		const auto &shader = presetInfo(StaticGameLODLevel(value[Shader]));
		info.m_shaderQuality = value[Shader];
		info.m_shaderMaterialReplacement = shader.m_shaderMaterialReplacement;
		info.m_useHeatEffects = shader.m_useHeatEffects;
	}
	if (changed(Decals))
	{
		info.m_decalQuality = value[Decals] == 2 ? 3 : value[Decals];
		info.m_useShadowDecals = value[Decals] > 0;
	}
	return info;
}
}
