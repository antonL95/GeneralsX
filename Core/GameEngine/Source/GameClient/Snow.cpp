/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// Snow.cpp ////////////////////////////////////////////////////////////////////////////////
// Snow Rendering implementation
// Author: Mark Wilczynski, July 2003
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the Game
#include "GameClient/Snow.h"
#include "GameClient/View.h"
#include "Common/AudioEventRTS.h"
#include "Common/AudioHandleSpecialValues.h"
#include "Common/GameAudio.h"
#include "Common/INIAudit.h"
#include "Common/RetailFileLayers.h"
#include "Common/RetailINIStartup.h"
#if !RTS_GENERALS
#include "GameLogic/GameLogic.h"
#endif
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <sstream>
#include <string>


SnowManager *TheSnowManager=nullptr;

SnowManager::SnowManager()
{
	m_retailWeather = FALSE;
	m_time = 0;
	m_velocity = 1;
	m_isVisible = TRUE;	//default to showing if it's enabled via INI.
}

void SnowManager::init()
{
	//starting heights of each particle
	//TODO: replace this lookup table with some kind of procedural method that takes x,y as input.
	m_startingHeights=NEW Real [ SNOW_NOISE_X * SNOW_NOISE_Y];
	m_time = 0;

	updateIniSettings();
}

static void loadRetailCloudEffect(WeatherSetting &weather)
{
	if (!weather.m_darkCloudTexture.isEmpty() || !weather.m_cloudEffectTexture.isEmpty())
		return;
	for (const std::string &content : RetailINIStartup::environment())
	{
		std::istringstream input(content);
		bool inBlock = false;
		for (std::string line; std::getline(input, line); )
		{
			const std::size_t semi = line.find(';');
			if (semi != std::string::npos)
				line.erase(semi);
			const std::size_t slash = line.find("//");
			if (slash != std::string::npos)
				line.erase(slash);
			std::istringstream row(line);
			std::string name;
			if (!(row >> name))
				continue;
			if (stricmp(name.c_str(), "CloudEffect") == 0)
			{
				inBlock = true;
				continue;
			}
			if (inBlock && stricmp(name.c_str(), "End") == 0)
			{
				inBlock = false;
				continue;
			}
			if (!inBlock)
				continue;
			char separator = 0;
			if (!(row >> separator) || separator != '=')
				continue;
			if (stricmp(name.c_str(), "CloudTexture") == 0)
			{
				std::string value;
				if (row >> value)
					weather.m_cloudEffectTexture = value.c_str();
			}
			else if (stricmp(name.c_str(), "DarkCloudTexture") == 0)
			{
				std::string value;
				if (row >> value)
					weather.m_darkCloudTexture = value.c_str();
			}
			else if (stricmp(name.c_str(), "DarkeningFactor") == 0)
			{
				RGBColor color = weather.m_cloudDarkeningFactor;
				std::string token;
				while (row >> token)
				{
					if (token.size() > 2 && token[1] == ':')
					{
						char *end = nullptr;
						const Real channel = std::strtof(token.c_str() + 2, &end);
						if (end != token.c_str() + 2 && std::isfinite(channel))
						{
							const Real scaled = std::clamp(channel / 255.0f, 0.0f, 1.0f);
							const int key = std::toupper(static_cast<unsigned char>(token[0]));
							if (key == 'R')
								color.red = scaled;
							else if (key == 'G')
								color.green = scaled;
							else if (key == 'B')
								color.blue = scaled;
						}
					}
				}
				weather.m_cloudDarkeningFactor = color;
			}
			else if (stricmp(name.c_str(), "DarkeningRate") == 0)
			{
				Real value = 0.0f;
				if ((row >> value) && std::isfinite(value) && value >= 1.0f)
					weather.m_cloudDarkeningRate = value;
			}
			else if (stricmp(name.c_str(), "LighteningRate") == 0)
			{
				Real value = 0.0f;
				if ((row >> value) && std::isfinite(value) && value >= 1.0f)
					weather.m_cloudLighteningRate = value;
			}
			else if (stricmp(name.c_str(), "LightningChance") == 0)
			{
				Real value = 0.0f;
				if ((row >> value) && std::isfinite(value))
					weather.m_cloudLightningChance = std::clamp(value, 0.0f, 1.0f);
			}
			else if (stricmp(name.c_str(), "LightningDuration") == 0 ||
				stricmp(name.c_str(), "LightningIntensity") == 0)
			{
				Real low = 0.0f;
				Real high = 0.0f;
				if ((row >> low >> high) && std::isfinite(low) && std::isfinite(high) && low >= 0.0f && high >= low)
				{
					auto& range = stricmp(name.c_str(), "LightningDuration") == 0 ?
						weather.m_cloudLightningDuration : weather.m_cloudLightningIntensity;
					range.setRange(low, high);
				}
			}
			else if (stricmp(name.c_str(), "JitterLightningLightIntensity") == 0)
			{
				std::string value;
				if (row >> value)
					weather.m_jitterCloudLightningIntensity = stricmp(value.c_str(), "Yes") == 0;
			}
		}
	}
}

void SnowManager::updateIniSettings()
{
	m_retailWeather = RetailFileLayers::enabled();
	if (m_retailWeather && TheWeatherSetting != nullptr)
	{
		WeatherSetting *weatherSet = (WeatherSetting *)(TheWeatherSetting.getNonOverloadedPointer());
		weatherSet = (WeatherSetting *)(weatherSet->friend_getFinalOverride());
		loadRetailCloudEffect(*weatherSet);
	}
	m_boxDimensions = m_retailWeather ? TheWeatherSetting->m_snowBoxHeight : TheWeatherSetting->m_snowBoxDimensions;

	Real *dst=m_startingHeights;
	//initialize a table of random starting positions for each particle.
	Int boxDimensions = (Int)m_boxDimensions;
	UnsignedInt noise = 1;
	for (Int y=0; y<SNOW_NOISE_Y; y++)
	{
		for (Int x=0; x<SNOW_NOISE_X; x++)
		{
			if (m_retailWeather)
			{
				noise = noise * 1664525u + 1013904223u;
				*dst = (Real)((noise >> 8) % boxDimensions);
			}
			else
			{
				*dst = (Real)(rand() % boxDimensions);
			}
			dst++;
		}
	}

	m_frequencyScaleX = TheWeatherSetting->m_snowFrequencyScaleX;
	m_frequencyScaleY = TheWeatherSetting->m_snowFrequencyScaleY;
	m_amplitude	= TheWeatherSetting->m_snowAmplitude;
	m_pointSize = TheWeatherSetting->m_snowPointSize;
	m_quadSize	= TheWeatherSetting->m_snowQuadSize;
	m_maxPointSize = TheWeatherSetting->m_snowMaxPointSize;
	m_minPointSize = TheWeatherSetting->m_snowMinPointSize;

	//Time for snow flake to make it from top to bottom of rendered cube around camera.
	if (m_retailWeather)
	{
		m_time = 0.0f;
		resetRetailSpellRain();
	}
	else
	{
		m_velocity = TheWeatherSetting->m_snowVelocity;
		m_emitterSpacing = 1.0f / TheWeatherSetting->m_snowBoxDensity;
		m_fullTimePeriod = m_boxDimensions / m_velocity;
	}
}

Bool SnowManager::isPrecipitationEnabled() const
{
#if !RTS_GENERALS
	if (m_retailWeather && TheGameLogic && TheGameLogic->getRetailWeather() == GameLogic::WEATHER_SUNNY)
		return FALSE;
#endif
	return TheWeatherSetting->m_snowEnabled &&
		(!m_retailWeather || TheWeatherSetting->m_isSnowing || isRetailSpellRainActive());
}

void SnowManager::setRetailFallParameters(Real spacing, Real velocity)
{
	m_emitterSpacing = spacing;
	m_velocity = velocity;
	m_fullTimePeriod = m_velocity != 0.0f ? m_boxDimensions / std::abs(m_velocity) : 0.0f;
}

void SnowManager::resetRetailSpellRain()
{
	m_spellRainActive = FALSE;
	m_spellRainTicks = 0;
	m_cloudyDarkness = 0.0f;
	setRetailFallParameters(TheWeatherSetting->m_snowSpacing, TheWeatherSetting->m_snowSpeed);
}

void SnowManager::stopRetailWeatherSound()
{
	if (TheAudio && m_weatherSoundHandle >= AHSV_FirstHandle)
		TheAudio->removeAudioEvent(m_weatherSoundHandle);
	m_weatherSoundHandle = AHSV_NoSound;
}

void SnowManager::updateRetailWeatherPresentation(Int state, UnsignedInt frame)
{
	const WeatherSetting& weather = **TheWeatherSetting;
	if (state < 0 || static_cast<std::size_t>(state) >= weather.m_retailPresets.size())
		return;
	const auto& preset = weather.m_retailPresets[state];
	static const Bool traceWeather = std::getenv("GENERALSX_TEST_WEATHER_DRAW") != nullptr;
	if (m_weatherPreset != state)
	{
		stopRetailWeatherSound();
		if (TheAudio && preset.sound.isNotEmpty() && preset.sound.isNotNone())
		{
			AudioEventRTS start(preset.sound);
			m_weatherSoundHandle = TheAudio->addAudioEvent(&start);
		}
		m_weatherPreset = state;
		if (traceWeather)
			std::fprintf(stderr, "TEST weather-preset state=%d lightning=%d sound=%s\n",
				state, preset.hasLightning, preset.sound.isEmpty() ? "-" : preset.sound.str());
	}

	if (!preset.hasLightning)
	{
		m_lightningTicks = 0;
		m_lightningRolls = 0;
		m_lightningIntensity = 0.0f;
		return;
	}

	if (frame == m_lastWeatherPresentationFrame)
		return;
	m_lastWeatherPresentationFrame = frame;
	ScopedVisualRandom random(TRUE);
	if (m_lightningTicks > 0)
	{
		--m_lightningTicks;
		if (m_lightningTicks == 0)
		{
			m_lightningRolls = 0;
			m_lightningIntensity = 0.0f;
		}
		else if (weather.m_jitterCloudLightningIntensity)
			m_lightningIntensity = weather.m_cloudLightningIntensity.getValue();
		return;
	}

	const UnsignedInt retailLightningTriggerRolls = 4;
	if (!(weather.m_cloudLightningChance > 0.0f) ||
		GameClientRandomValueReal(0.0f, 1.0f) >= weather.m_cloudLightningChance ||
		++m_lightningRolls < retailLightningTriggerRolls)
		return;
	m_lightningRolls = 0;
	const Real ticks = weather.m_cloudLightningDuration.getValue();
	if (!std::isfinite(ticks) || ticks < 1.0f || ticks >= static_cast<Real>(std::numeric_limits<UnsignedInt>::max()))
		return;
	m_lightningTicks = static_cast<UnsignedInt>(ticks);
	m_lightningIntensity = weather.m_cloudLightningIntensity.getValue();
	if (traceWeather)
		std::fprintf(stderr, "TEST weather-lightning frame=%u duration=%u intensity=%.3f\n",
			frame, m_lightningTicks, m_lightningIntensity);
}

RGBColor SnowManager::getRetailCloudyLightScale() const
{
	RGBColor scale = { 1.0f, 1.0f, 1.0f };
	if (!m_retailWeather || !TheWeatherSetting)
		return scale;
	const RGBColor &factor = TheWeatherSetting->m_cloudDarkeningFactor;
	const Real lightning = std::isfinite(m_lightningIntensity) ? std::clamp(m_lightningIntensity, 0.0f, 1.0f) : 0.0f;
	scale.red = std::clamp(1.0f - m_cloudyDarkness * factor.red, 0.0f, 1.0f) + lightning;
	scale.green = std::clamp(1.0f - m_cloudyDarkness * factor.green, 0.0f, 1.0f) + lightning;
	scale.blue = std::clamp(1.0f - m_cloudyDarkness * factor.blue, 0.0f, 1.0f) + lightning;
	return scale;
}

void SnowManager::updateRetailCloudy()
{
	const WeatherSetting &weather = **TheWeatherSetting;
#if !RTS_GENERALS
	const Int state = TheGameLogic ? TheGameLogic->getRetailWeather() : GameLogic::WEATHER_NONE;
	if (TheGameLogic)
		updateRetailWeatherPresentation(state, TheGameLogic->getFrame());
	const Bool cloudy = state == GameLogic::WEATHER_CLOUDY || state == GameLogic::WEATHER_CLOUDYRAINY;
#else
	const Bool cloudy = FALSE;
#endif
	if (cloudy)
	{
		const Real rate = weather.m_cloudDarkeningRate >= 1.0f ? weather.m_cloudDarkeningRate : 255.0f;
		m_cloudyDarkness = std::min(1.0f, m_cloudyDarkness + 1.0f / rate);
	}
	else if (m_cloudyDarkness > 0.0f)
	{
		const Real rate = weather.m_cloudLighteningRate >= 1.0f ? weather.m_cloudLighteningRate : 255.0f;
		m_cloudyDarkness = std::max(0.0f, m_cloudyDarkness - 1.0f / rate);
	}
}

void SnowManager::updateRetailSpellRain()
{
	updateRetailCloudy();
	const WeatherSetting &weather = **TheWeatherSetting;
#if !RTS_GENERALS
	const Bool active = weather.m_spellEnabled && TheGameLogic && TheGameLogic->getRetailWeather() == GameLogic::WEATHER_RAINY;
#else
	const Bool active = FALSE;
#endif
	if (!active && !m_spellRainActive && m_spellRainTicks == 0)
		return;

	const Int duration = std::max(0, weather.m_spellDuration);
	const Real rampIn = duration * std::clamp(weather.m_rampControl.x, 0.0f, 1.0f);
	const Real fadeStart = std::clamp(weather.m_rampControl.y, 0.0f, 1.0f);
	const Int rampOut = static_cast<Int>(std::max(0.0, duration * (1.0 - fadeStart)));
	if (active != m_spellRainActive)
	{
		m_spellRainActive = active;
		m_spellRainTicks = active ? 0 : rampOut;
	}
	else if (active && m_spellRainTicks < duration)
	{
		++m_spellRainTicks;
	}
	else if (!active)
	{
		--m_spellRainTicks;
	}

	if (!active && m_spellRainTicks == 0)
	{
		resetRetailSpellRain();
		return;
	}

	const Real rampLength = active ? rampIn : static_cast<Real>(rampOut);
	const Real strength = rampLength > 0.0f ? std::min(1.0f, m_spellRainTicks / rampLength) : 1.0f;
	const Real spacing = std::lerp(weather.m_rampSpacing.x, weather.m_rampSpacing.y, strength);
	const Real velocity = std::lerp(weather.m_rampSpeed.x, weather.m_rampSpeed.y, strength);
	setRetailFallParameters(
		spacing >= 1.0f && spacing < static_cast<Real>(std::numeric_limits<Int>::max()) ? spacing : weather.m_snowSpacing,
		std::isfinite(velocity) ? velocity : weather.m_snowSpeed);
}

void SnowManager::setVisible(Bool showWeather)
{
	m_isVisible = showWeather;
}

void SnowManager::reset()
{
	stopRetailWeatherSound();
	m_weatherPreset = -1;
	m_lastWeatherPresentationFrame = 0;
	m_lightningTicks = 0;
	m_lightningRolls = 0;
	m_lightningIntensity = 0.0f;
	m_isVisible = TRUE;	//default to showing if it's enabled via INI.
	if (m_retailWeather)
		resetRetailSpellRain();
}

SnowManager::~SnowManager()
{
	stopRetailWeatherSound();
	delete [] m_startingHeights;
	m_startingHeights=nullptr;

	// TheSuperHackers @fix Mauller 13/04/2025 Delete the instance of the weather settings
	deleteInstance((WeatherSetting*)TheWeatherSetting.getNonOverloadedPointer());
	TheWeatherSetting=nullptr;
}

OVERRIDE<WeatherSetting> TheWeatherSetting = nullptr;

// PRIVATE DATA ///////////////////////////////////////////////////////////////////////////////////
const FieldParse WeatherSetting::m_weatherSettingFieldParseTable[] =
{
	{ "SnowTexture",							INI::parseAsciiString,nullptr,			offsetof( WeatherSetting, m_snowTexture ) },
	{ "SnowFrequencyScaleX",					INI::parseReal,nullptr,			offsetof( WeatherSetting, m_snowFrequencyScaleX ) },
	{ "SnowFrequencyScaleY",					INI::parseReal,nullptr,			offsetof( WeatherSetting, m_snowFrequencyScaleY ) },
	{ "SnowAmplitude",							INI::parseReal,nullptr,			offsetof( WeatherSetting, m_snowAmplitude ) },
	{ "SnowPointSize",							INI::parseReal,nullptr,			offsetof( WeatherSetting, m_snowPointSize ) },
	{ "SnowMaxPointSize",						INI::parseReal,nullptr,			offsetof( WeatherSetting, m_snowMaxPointSize ) },
	{ "SnowMinPointSize",						INI::parseReal,nullptr,			offsetof( WeatherSetting, m_snowMinPointSize ) },
	{ "SnowQuadSize",							INI::parseReal,nullptr,			offsetof( WeatherSetting, m_snowQuadSize ) },
	{ "SnowBoxDimensions",						INI::parseReal,nullptr,			offsetof( WeatherSetting, m_snowBoxDimensions ) },
	{ "SnowBoxDensity",							INI::parseReal,nullptr,			offsetof( WeatherSetting, m_snowBoxDensity ) },
	{ "SnowVelocity",							INI::parseReal,nullptr,			offsetof( WeatherSetting, m_snowVelocity ) },
	{ "SnowPointSprites",						INI::parseBool,nullptr,			offsetof( WeatherSetting, m_usePointSprites ) },
	{ "SnowEnabled",							INI::parseBool,nullptr,			offsetof( WeatherSetting, m_snowEnabled ) },
	{ nullptr, nullptr, nullptr, 0 },
};

static void parseWeatherLength(INI *ini, void *, void *store, const void *)
{
	const Real value = INI::scanReal(ini->getNextToken());
	if (!(value >= 1.0f && value < static_cast<Real>(std::numeric_limits<Int>::max())))
		throw INI_INVALID_DATA;
	*static_cast<Real *>(store) = value;
}

static void parseWeatherSpeed(INI *ini, void *, void *store, const void *)
{
	const Real value = INI::scanReal(ini->getNextToken());
	if (!std::isfinite(value))
		throw INI_INVALID_DATA;
	*static_cast<Real *>(store) = value;
}

static void parseLightningFactor(INI *ini, void *, void *store, const void *)
{
	const Real minimum = INI::scanReal(ini->getNextToken());
	const Real maximum = INI::scanReal(ini->getNextToken());
	Real *range = static_cast<Real *>(store);
	range[0] = minimum;
	range[1] = maximum;
}

static const FieldParse retailWeatherFields[] =
{
	{ "IsSnowing", INI::parseBool, nullptr, offsetof(WeatherSetting, m_isSnowing) },
	{ "SnowBoxHeight", parseWeatherLength, nullptr, offsetof(WeatherSetting, m_snowBoxHeight) },
	{ "SnowSpacing", parseWeatherLength, nullptr, offsetof(WeatherSetting, m_snowSpacing) },
	{ "NumberTiles", INI::parseInt, nullptr, offsetof(WeatherSetting, m_numberTiles) },
	{ "SnowSpeed", parseWeatherSpeed, nullptr, offsetof(WeatherSetting, m_snowSpeed) },
	{ "SnowXYSpeed", INI::parseCoord2D, nullptr, offsetof(WeatherSetting, m_snowXYSpeed) },
	{ "LightningEnabled", INI::parseBool, nullptr, offsetof(WeatherSetting, m_lightningEnabled) },
	{ "LightningFactor", parseLightningFactor, nullptr, offsetof(WeatherSetting, m_lightningFactor) },
	{ "LightningDuration", INI::parseInt, nullptr, offsetof(WeatherSetting, m_lightningDuration) },
	{ "LightningChance", INI::parseReal, nullptr, offsetof(WeatherSetting, m_lightningChance) },
	{ "SpellEnabled", INI::parseBool, nullptr, offsetof(WeatherSetting, m_spellEnabled) },
	{ "SpellDuration", INI::parseInt, nullptr, offsetof(WeatherSetting, m_spellDuration) },
	{ "RampControl", INI::parseCoord2D, nullptr, offsetof(WeatherSetting, m_rampControl) },
	{ "RampSpeed", INI::parseCoord2D, nullptr, offsetof(WeatherSetting, m_rampSpeed) },
	{ "RampSpacing", INI::parseCoord2D, nullptr, offsetof(WeatherSetting, m_rampSpacing) },
	{ "HardwareFogEnable", INI::parseBool, nullptr, offsetof(WeatherSetting, m_hardwareFogEnable) },
	{ "HardwareFogColor", INI::parseRGBColor, nullptr, offsetof(WeatherSetting, m_hardwareFogColor) },
	{ "HardwareFogStart", INI::parseReal, nullptr, offsetof(WeatherSetting, m_hardwareFogStart) },
	{ "HardwareFogEnd", INI::parseReal, nullptr, offsetof(WeatherSetting, m_hardwareFogEnd) },
	{ "CloudTextureSize", INI::parseCoord2D, nullptr, offsetof(WeatherSetting, m_cloudTextureSize) },
	{ "CloudOffsetPerSecond", INI::parseCoord2D, nullptr, offsetof(WeatherSetting, m_cloudOffsetPerSecond) },
	{ nullptr, nullptr, nullptr, 0 },
};

//-------------------------------------------------------------------------------------------------
void INI::parseWeatherDefinition( INI *ini )
{
	const Bool retail = RetailFileLayers::enabled() || INIAudit::isEnabled();
	if (TheWeatherSetting == nullptr) {
		TheWeatherSetting = newInstance(WeatherSetting);
	} else if (ini->getLoadType() == INI_LOAD_CREATE_OVERRIDES) {
		WeatherSetting* ws = (WeatherSetting*) (TheWeatherSetting.getNonOverloadedPointer());
		WeatherSetting* wsOverride = newInstance(WeatherSetting);
		*wsOverride = retail ? *static_cast<WeatherSetting *>(ws->friend_getFinalOverride()) : *ws;
		wsOverride->setNextOverride(nullptr);

		// Mark that it is an override.
		wsOverride->markAsOverride();

		ws->friend_getFinalOverride()->setNextOverride(wsOverride);
	} else {
		throw INI_INVALID_DATA;
	}

	WeatherSetting* weatherSet = (WeatherSetting*) (TheWeatherSetting.getNonOverloadedPointer());
	weatherSet = (WeatherSetting*) (weatherSet->friend_getFinalOverride());
	MultiIniFieldParse fields;
	fields.add(weatherSet->getFieldParse());
	if (retail)
		fields.add(retailWeatherFields);
	ini->initFromINIMulti(weatherSet, fields);

	if (TheSnowManager)
		TheSnowManager->updateIniSettings();
}

template<auto Member, INIFieldParseProc Parse>
static void parseCloudEffectField(INI* ini, void* instance, void*, const void*)
{
	auto* weather = static_cast<WeatherSetting*>(instance);
	Parse(ini, instance, &(weather->*Member), nullptr);
}

static const FieldParse retailCloudEffectFields[] =
{
	{ "CloudTexture", parseCloudEffectField<&WeatherSetting::m_cloudEffectTexture, INI::parseAsciiString>, nullptr, 0 },
	{ "DarkCloudTexture", parseCloudEffectField<&WeatherSetting::m_darkCloudTexture, INI::parseAsciiString>, nullptr, 0 },
	{ "DarkeningFactor", parseCloudEffectField<&WeatherSetting::m_cloudDarkeningFactor, INI::parseRGBColor>, nullptr, 0 },
	{ "DarkeningRate", parseCloudEffectField<&WeatherSetting::m_cloudDarkeningRate, INI::parseReal>, nullptr, 0 },
	{ "LighteningRate", parseCloudEffectField<&WeatherSetting::m_cloudLighteningRate, INI::parseReal>, nullptr, 0 },
	{ "LightningChance", parseCloudEffectField<&WeatherSetting::m_cloudLightningChance, INI::parseReal>, nullptr, 0 },
	{ "LightningDuration", parseCloudEffectField<&WeatherSetting::m_cloudLightningDuration, INI::parseGameClientRandomVariable>, nullptr, 0 },
	{ "LightningIntensity", parseCloudEffectField<&WeatherSetting::m_cloudLightningIntensity, INI::parseGameClientRandomVariable>, nullptr, 0 },
	{ "JitterLightningLightIntensity", parseCloudEffectField<&WeatherSetting::m_jitterCloudLightningIntensity, INI::parseBool>, nullptr, 0 },
	{ nullptr, nullptr, nullptr, 0 },
};

void INI::parseCloudEffectDefinition( INI *ini )
{
	if (TheWeatherSetting == nullptr)
		TheWeatherSetting = newInstance(WeatherSetting);

	WeatherSetting *weatherSet = (WeatherSetting *)(TheWeatherSetting.getNonOverloadedPointer());
	weatherSet = (WeatherSetting *)(weatherSet->friend_getFinalOverride());
	MultiIniFieldParse fields;
	fields.add(retailCloudEffectFields);
	ini->initFromINIMulti(weatherSet, fields);
}

void WeatherSetting::parseRetailPreset(INI* ini)
{
	static const char* const names[] = {"NONE", "CLOUDY", "RAINY", "CLOUDYRAINY", "SUNNY", nullptr};
	static const FieldParse fields[] = {
		{ "WeatherSound", INI::parseAsciiString, nullptr, offsetof(RetailWeatherPreset, sound) },
		{ "HasLightning", INI::parseBool, nullptr, offsetof(RetailWeatherPreset, hasLightning) },
		{ nullptr, nullptr, nullptr, 0 },
	};
	if (ini->getLoadType() != INI_LOAD_OVERWRITE)
		throw INI_INVALID_DATA;
	const Int state = INI::scanIndexList(ini->getNextToken(), names);
	if (TheWeatherSetting == nullptr)
		TheWeatherSetting = newInstance(WeatherSetting);
	auto* weather = const_cast<WeatherSetting*>(TheWeatherSetting.getNonOverloadedPointer());
	ini->initFromINI(&weather->m_retailPresets[state], fields);
}
