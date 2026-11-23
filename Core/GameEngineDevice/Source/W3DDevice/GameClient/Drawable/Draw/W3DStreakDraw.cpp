#include "W3DDevice/GameClient/Module/W3DStreakDraw.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include "Common/GlobalData.h"
#include "Common/INI.h"
#include "Common/RetailFileLayers.h"
#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"
#include "GameClient/Drawable.h"
#include "GameLogic/GameLogic.h"
#include "W3DDevice/GameClient/W3DDisplay.h"
#include "W3DDevice/GameClient/W3DScene.h"
#include "WW3D2/assetmgr.h"
#include "WW3D2/segline.h"

void W3DStreakDrawModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	ModuleData::buildFieldParse(p);

	static const FieldParse fields[] =
	{
		{ "Length", INI::parseReal, nullptr, offsetof(W3DStreakDrawModuleData, m_length) },
		{ "Width", INI::parseReal, nullptr, offsetof(W3DStreakDrawModuleData, m_width) },
		{ "Additive", INI::parseBool, nullptr, offsetof(W3DStreakDrawModuleData, m_additive) },
		{ "Color", INI::parseRGBColor, nullptr, offsetof(W3DStreakDrawModuleData, m_color) },
		{ "NumSegments", INI::parseInt, nullptr, offsetof(W3DStreakDrawModuleData, m_numSegments) },
		{ "Texture", INI::parseAsciiString, nullptr, offsetof(W3DStreakDrawModuleData, m_texture) },
		{ "WeatherTexture", parseWeatherTexture, nullptr, offsetof(W3DStreakDrawModuleData, m_weatherTextures) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

void W3DStreakDrawModuleData::parseWeatherTexture(INI* ini, void*, void* store, const void*)
{
	const Int weather = INI::scanIndexList(ini->getNextToken(), WeatherNames);
	AsciiString texture = ini->getNextAsciiString();
	static_cast<std::vector<std::pair<Int, AsciiString>>*>(store)->emplace_back(weather, texture);
}

const AsciiString& W3DStreakDrawModuleData::textureFor(Weather weather) const
{
	for (const auto& entry : m_weatherTextures)
	{
		if (entry.first == weather)
			return entry.second;
	}
	return m_texture;
}

W3DStreakDraw::W3DStreakDraw(Thing* thing, const ModuleData* moduleData)
	: DrawModule(thing, moduleData)
{
	const W3DStreakDrawModuleData* data = getW3DStreakDrawModuleData();
	const Weather weather = TheGlobalData ? TheGlobalData->m_weather : WEATHER_NORMAL;
	m_streakTexture = WW3DAssetManager::Get_Instance()->Get_Texture(data->textureFor(weather).str());

	m_line = NEW SegmentedLineClass;
	m_line->Set_Texture(m_streakTexture);
	m_line->Set_Shader(data->m_additive ? ShaderClass::_PresetAdditiveSpriteShader : ShaderClass::_PresetAlphaSpriteShader);
	m_line->Set_Width(data->m_width);
	m_line->Set_Texture_Mapping_Mode(SegLineRendererClass::UNIFORM_LENGTH_TEXTURE_MAP);
	m_line->Set_Color(Vector3(data->m_color.red, data->m_color.green, data->m_color.blue));
}

W3DStreakDraw::~W3DStreakDraw()
{
	if (m_line)
	{
		if (m_line->Peek_Scene())
			W3DDisplay::m_3DScene->Remove_Render_Object(m_line);
		REF_PTR_RELEASE(m_line);
	}
	REF_PTR_RELEASE(m_streakTexture);
}

void W3DStreakDraw::setFullyObscuredByShroud(Bool fullyObscured)
{
	m_obscured = fullyObscured;
	updateLine();
}

void W3DStreakDraw::addPoint(const Vector3& location)
{
	const Real distance = m_points.empty() ? 0.0f : m_distances.back() + (location - m_points.back()).Length();
	m_points.push_back(location);
	m_distances.push_back(distance);
}

void W3DStreakDraw::updateTrail(const Vector3& head)
{
	if (m_points.empty())
	{
		addPoint(head);
		return;
	}
	if (m_points.back() == head)
		return;

	if (m_points.size() == 1)
	{
		addPoint(head);
	}
	else
	{
		const std::size_t last = m_points.size() - 1;
		const Real step = (head - m_points[last - 1]).Length();
		const Real total = m_distances[last - 1] + step;
		m_points[last] = head;
		m_distances[last] = total;

		const W3DStreakDrawModuleData* data = getW3DStreakDrawModuleData();
		const Real segmentLength = data->m_length / static_cast<Real>(std::max(1, data->m_numSegments));
		if (step > segmentLength)
		{
			m_points.push_back(head);
			m_distances.push_back(total);
		}

		while (m_points.size() > 2 && m_distances[1] < total - data->m_length)
		{
			m_points.erase(m_points.begin());
			m_distances.erase(m_distances.begin());
		}
	}
	m_line->Set_Points(static_cast<unsigned int>(m_points.size()), m_points.data());
}

void W3DStreakDraw::updateLine()
{
	if (!m_line || !W3DDisplay::m_3DScene)
		return;

	const Bool visible = !m_obscured && m_points.size() >= 2;
	if (!visible)
	{
		if (m_line->Peek_Scene())
			W3DDisplay::m_3DScene->Remove_Render_Object(m_line);
		return;
	}

	const W3DStreakDrawModuleData* data = getW3DStreakDrawModuleData();
	const Real opacity = getDrawable()->getEffectiveOpacity();
	const Vector3 color(data->m_color.red, data->m_color.green, data->m_color.blue);
	if (data->m_additive)
	{
		m_line->Set_Color(color * opacity);
		m_line->Set_Opacity(1.0f);
	}
	else
	{
		m_line->Set_Color(color);
		m_line->Set_Opacity(opacity);
	}
	if (!m_line->Peek_Scene())
		W3DDisplay::m_3DScene->Add_Render_Object(m_line);
}

void W3DStreakDraw::reactToTransformChange(const Matrix3D*, const Coord3D*, Real)
{
	if (m_points.empty() || !RetailFileLayers::enabled())
		return;

	const Drawable* draw = getDrawable();
	Matrix3D transform = *draw->getTransformMatrix();
	if (!draw->isInstanceIdentity())
		transform.postMul(*draw->getInstanceMatrix());
	updateTrail(transform.Get_Translation());
}

void W3DStreakDraw::doDrawModule(const Matrix3D* transformMtx)
{
	updateTrail(transformMtx->Get_Translation());
	updateLine();

	static const char* const traced = std::getenv("GENERALSX_TEST_STREAKTRACE");
	const Drawable* draw = getDrawable();
	if (traced && TheGameLogic && draw->getTemplate()->getName().compareNoCase(traced) == 0)
	{
		Real longest = 0.0f;
		for (std::size_t i = 1; i < m_distances.size(); ++i)
			longest = std::max(longest, m_distances[i] - m_distances[i - 1]);
		std::fprintf(stderr, "TEST streak frame %u id %u %s longest %.3f\n", TheGameLogic->getFrame(),
			static_cast<UnsignedInt>(draw->getID()), draw->getTemplate()->getName().str(), longest);
	}
}

void W3DStreakDraw::crc(Xfer* xfer)
{
	DrawModule::crc(xfer);
}

void W3DStreakDraw::xfer(Xfer* xfer)
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion(&version, currentVersion);
	DrawModule::xfer(xfer);
}

void W3DStreakDraw::loadPostProcess()
{
	DrawModule::loadPostProcess();
}
