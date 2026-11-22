#define DEFINE_SHADOW_NAMES

#include "W3DDevice/GameClient/Module/W3DTornadoDraw.h"

#include "Common/GameState.h"
#include "Common/INI.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/Xfer.h"
#include "GameClient/Drawable.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "WWMath/wwmath.h"

#include <algorithm>

void W3DTornadoDrawModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	ModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "DecalTemplate", parseDecal, nullptr, offsetof(W3DTornadoDrawModuleData, m_decal) },
		{ "DecalCount", INI::parseUnsignedInt, nullptr, offsetof(W3DTornadoDrawModuleData, m_decalCount) },
		{ "DecalMaxRadius", INI::parseReal, nullptr, offsetof(W3DTornadoDrawModuleData, m_decalMaxRadius) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

void W3DTornadoDrawModuleData::parseDecal(INI* ini, void*, void* store, const void*)
{
	static const FieldParse fields[] =
	{
		{ "Texture", INI::parseAsciiString, nullptr, offsetof(Decal, texture) },
		{ "Style", INI::parseBitString32, TheShadowNames, offsetof(Decal, style) },
		{ "OpacityMin", INI::parsePercentToReal, nullptr, offsetof(Decal, opacityMin) },
		{ "OpacityMax", INI::parsePercentToReal, nullptr, offsetof(Decal, opacityMax) },
		{ "OpacityThrobTime", INI::parseEngineDurationUnsignedInt, nullptr, offsetof(Decal, opacityThrobTime) },
		{ "MaxRadius", INI::parseReal, nullptr, offsetof(Decal, maxRadius) },
		{ "RotationsPerMinute", INI::parseReal, nullptr, offsetof(Decal, rotationsPerMinute) },
		{ "SpiralAcceleration", INI::parseReal, nullptr, offsetof(Decal, spiralAcceleration) },
		{ "Color", INI::parseColorInt, nullptr, offsetof(Decal, color), true },
		{ "OnlyVisibleToOwningPlayer", INI::parseBool, nullptr, offsetof(Decal, ownerOnly) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	ini->initFromINI(store, fields);
}

W3DTornadoDraw::W3DTornadoDraw(Thing* thing, const ModuleData* moduleData)
	: DrawModule(thing, moduleData)
{
}

W3DTornadoDraw::~W3DTornadoDraw()
{
	releaseShadows();
}

void W3DTornadoDraw::releaseShadows()
{
	for (Layer& layer : m_layers)
	{
		if (layer.shadow)
		{
			layer.shadow->release();
			layer.shadow = nullptr;
		}
	}
}

void W3DTornadoDraw::setShadowsEnabled(Bool enabled)
{
	m_shadowsEnabled = enabled;
	for (const Layer& layer : m_layers)
		if (layer.shadow)
			layer.shadow->enableShadowRender(enabled);
}

void W3DTornadoDraw::setDrawableHidden(Bool hidden)
{
	DrawModule::setDrawableHidden(hidden);
	if (hidden)
		releaseShadows();
}

void W3DTornadoDraw::setFullyObscuredByShroud(Bool obscured)
{
	m_obscured = obscured;
	if (obscured)
		releaseShadows();
}

void W3DTornadoDraw::doDrawModule(const Matrix3D* transformMtx)
{
	const auto* data = getW3DTornadoDrawModuleData();
	const auto& decal = data->m_decal;
	const Object* object = getDrawable()->getObject();
	const Player* owner = object ? object->getControllingPlayer() : nullptr;
	if (m_obscured || getDrawable()->isDrawableEffectivelyHidden() || !owner ||
		(decal.ownerOnly && (!ThePlayerList || owner != ThePlayerList->getLocalPlayer())))
	{
		releaseShadows();
		return;
	}

	const UnsignedInt count = std::min(data->m_decalCount, 64u);
	const Real radius = WWMath::Min(decal.maxRadius, data->m_decalMaxRadius);
	if (!TheProjectedShadowManager || !TheGameLogic || decal.texture.isEmpty() || count < 2 || radius <= 0.0f)
		return;

	const UnsignedInt frame = TheGameLogic->getFrame();
	const Real maxDiameter = 2.0f * radius;
	if (m_layers.size() != count - 1)
	{
		releaseShadows();
		m_layers.assign(count - 1, Layer{});
		for (UnsignedInt i = 1; i < count; ++i)
			m_layers[i - 1].spiralPosition = maxDiameter * static_cast<Real>(i) / static_cast<Real>(count);
		m_lastFrame = frame;
	}

	const UnsignedInt elapsed = frame >= m_lastFrame ? frame - m_lastFrame : 0;
	m_lastFrame = frame;
	const UnsignedInt period = std::max(decal.opacityThrobTime, 1u);
	const Real pulse = 0.5f + 0.5f * WWMath::Sin(WWMATH_TWO_PI * static_cast<Real>(frame % period) / static_cast<Real>(period));
	const Real opacity = WWMath::Lerp(decal.opacityMin, decal.opacityMax, pulse);
	const Real spiralDiameter = 2.0f * decal.maxRadius;
	const Real transparency = getDrawable()->getEffectiveOpacity();
	const Vector3 position = transformMtx->Get_Translation();
	for (Layer& layer : m_layers)
	{
		if (!layer.shadow)
		{
			Shadow::ShadowTypeInfo info;
			info.m_type = decal.style;
			info.allowWorldAlign = TRUE;
			info.m_sizeX = layer.spiralPosition;
			info.m_sizeY = layer.spiralPosition;
			strlcpy(info.m_ShadowName, decal.texture.str(), ARRAY_SIZE(info.m_ShadowName));
			layer.shadow = TheProjectedShadowManager->addDecal(&info);
			if (!layer.shadow)
				continue;
			layer.shadow->setColor(decal.color ? decal.color : owner->getPlayerColor());
			layer.shadow->enableShadowRender(m_shadowsEnabled);
		}
		Real angle = DEG_TO_RADF(decal.rotationsPerMinute * 60.0f) * static_cast<Real>(frame) / LOGICFRAMES_PER_SECOND;
		Real alpha = opacity;
		if (decal.rotationsPerMinute > 0.0f && decal.spiralAcceleration > 0.0f)
		{
			for (UnsignedInt step = 0; step < std::min(elapsed, 120u); ++step)
			{
				const Real diameter = layer.spiralPosition - decal.spiralAcceleration;
				layer.spiralPosition = diameter - spiralDiameter * WWMath::Floor(diameter / spiralDiameter);
				const Real phase = 1.0f - layer.spiralPosition / spiralDiameter;
				layer.angle = WWMath::Normalize_Angle(layer.angle + decal.rotationsPerMinute * phase * phase);
			}
			const Real phase = 1.0f - layer.spiralPosition / spiralDiameter;
			alpha = phase * phase;
			const auto grey = static_cast<UnsignedInt>(alpha * 255.0f);
			layer.shadow->setColor(0xff000000u | (grey * 0x010101u));
			angle = layer.angle;
			layer.shadow->setSize(layer.spiralPosition, layer.spiralPosition);
		}
		layer.shadow->setAngle(WWMath::Normalize_Angle(angle));
		layer.shadow->setOpacity(static_cast<Int>(255.0f * WWMath::Clamp(alpha * transparency, 0.0f, 1.0f)));
		layer.shadow->setPosition(position.X, position.Y, position.Z);
	}
}

void W3DTornadoDraw::crc(Xfer* xfer)
{
	DrawModule::crc(xfer);
}

void W3DTornadoDraw::xfer(Xfer* xfer)
{
	XferVersion version = 2;
	xfer->xferVersion(&version, 2);
	DrawModule::xfer(xfer);
	if (version >= 2)
	{
		xfer->xferUnsignedInt(&m_lastFrame);
		UnsignedInt count = static_cast<UnsignedInt>(m_layers.size());
		xfer->xferUnsignedInt(&count);
		if (xfer->getXferMode() == XFER_LOAD)
		{
			if (count >= 64)
				throw SC_INVALID_DATA;
			releaseShadows();
			m_layers.assign(count, Layer{});
		}
		for (Layer& layer : m_layers)
		{
			xfer->xferReal(&layer.spiralPosition);
			xfer->xferReal(&layer.angle);
		}
	}
}

void W3DTornadoDraw::loadPostProcess()
{
	DrawModule::loadPostProcess();
}
