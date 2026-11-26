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
#include "Common/INIAudit.h"
#include "Common/RetailFileLayers.h"
#if !RTS_GENERALS
#include "GameLogic/GameLogic.h"
#endif
#include <algorithm>
#include <cmath>
#include <limits>


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

void SnowManager::updateIniSettings()
{
	m_retailWeather = RetailFileLayers::enabled();
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

RGBColor SnowManager::getRetailCloudyLightScale() const
{
	RGBColor scale = { 1.0f, 1.0f, 1.0f };
	if (!m_retailWeather || !(m_cloudyDarkness > 0.0f) || !TheWeatherSetting)
		return scale;
	const RGBColor &factor = TheWeatherSetting->m_cloudDarkeningFactor;
	scale.red = std::clamp(1.0f - m_cloudyDarkness * factor.red, 0.0f, 1.0f);
	scale.green = std::clamp(1.0f - m_cloudyDarkness * factor.green, 0.0f, 1.0f);
	scale.blue = std::clamp(1.0f - m_cloudyDarkness * factor.blue, 0.0f, 1.0f);
	return scale;
}

void SnowManager::updateRetailCloudy()
{
	const WeatherSetting &weather = **TheWeatherSetting;
#if !RTS_GENERALS
	const Int state = TheGameLogic ? TheGameLogic->getRetailWeather() : GameLogic::WEATHER_NONE;
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
	m_isVisible = TRUE;	//default to showing if it's enabled via INI.
	if (m_retailWeather)
		resetRetailSpellRain();
}

SnowManager::~SnowManager()
{
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

static const FieldParse retailCloudEffectFields[] =
{
	{ "CloudTexture", INI::parseAsciiString, nullptr, offsetof(WeatherSetting, m_cloudEffectTexture) },
	{ "DarkCloudTexture", INI::parseAsciiString, nullptr, offsetof(WeatherSetting, m_darkCloudTexture) },
	{ "DarkeningFactor", INI::parseRGBColor, nullptr, offsetof(WeatherSetting, m_cloudDarkeningFactor) },
	{ "DarkeningRate", INI::parseReal, nullptr, offsetof(WeatherSetting, m_cloudDarkeningRate) },
	{ "LighteningRate", INI::parseReal, nullptr, offsetof(WeatherSetting, m_cloudLighteningRate) },
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
