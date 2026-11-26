#include "W3DDevice/GameClient/Module/W3DLightDraw.h"

#include "Common/INI.h"
#include "Common/Xfer.h"
#include "GameClient/ClientRandomValue.h"
#include "GameClient/Drawable.h"
#include "GameLogic/GameLogic.h"
#include "W3DDevice/GameClient/W3DDisplay.h"
#include "W3DDevice/GameClient/W3DDynamicLight.h"
#include "W3DDevice/GameClient/W3DScene.h"
#include "WWMath/wwmath.h"

void W3DLightDrawModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	ModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "Ambient", INI::parseRGBColor, nullptr, offsetof(W3DLightDrawModuleData, m_ambient) },
		{ "Diffuse", INI::parseRGBColor, nullptr, offsetof(W3DLightDrawModuleData, m_diffuse) },
		{ "Specular", INI::parseRGBColor, nullptr, offsetof(W3DLightDrawModuleData, m_specular) },
		{ "Radius", INI::parseReal, nullptr, offsetof(W3DLightDrawModuleData, m_radius) },
		{ "Intensity", INI::parseReal, nullptr, offsetof(W3DLightDrawModuleData, m_intensity) },
		{ "FlickerAmplitude", INI::parseReal, nullptr, offsetof(W3DLightDrawModuleData, m_flickerAmplitude) },
		{ "FlickerFrequency", INI::parseReal, nullptr, offsetof(W3DLightDrawModuleData, m_flickerFrequency) },
		{ "PulseAmplitude", INI::parseReal, nullptr, offsetof(W3DLightDrawModuleData, m_pulseAmplitude) },
		{ "PulseFrequency", INI::parseReal, nullptr, offsetof(W3DLightDrawModuleData, m_pulseFrequency) },
		{ "AttachToBoneInAnotherModule", INI::parseAsciiString, nullptr, offsetof(W3DLightDrawModuleData, m_attachBone) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

W3DLightDraw::W3DLightDraw(Thing* thing, const ModuleData* moduleData)
	: DrawModule(thing, moduleData)
{
	const ScopedVisualRandom random(TRUE);
	m_pulsePhase = GameClientRandomValueReal(0.0f, 30.0f);
	m_flickerStart = getW3DLightDrawModuleData()->m_intensity;
	m_flickerTarget = m_flickerStart;
}

W3DLightDraw::~W3DLightDraw()
{
	releaseLight();
}

void W3DLightDraw::releaseLight()
{
	if (m_light)
	{
		m_light->setEnabled(false);
		m_light = nullptr;
	}
}

void W3DLightDraw::setDrawableHidden(Bool hidden)
{
	DrawModule::setDrawableHidden(hidden);
	if (hidden)
		releaseLight();
}

void W3DLightDraw::setFullyObscuredByShroud(Bool obscured)
{
	m_obscured = obscured;
	if (obscured)
		releaseLight();
}

void W3DLightDraw::doDrawModule(const Matrix3D* transformMtx)
{
	const auto* data = getW3DLightDrawModuleData();
	if (m_obscured || isHiddenByScript() || getDrawable()->isDrawableEffectivelyHidden() || data->m_radius <= 0.0f)
	{
		releaseLight();
		return;
	}

	if (!m_light && W3DDisplay::m_3DScene)
		m_light = W3DDisplay::m_3DScene->getADynamicLight();
	if (!m_light)
		return;

	Real intensity = data->m_intensity;
	const Real elapsed = TheGameLogic ? static_cast<Real>(TheGameLogic->getFrame()) / LOGICFRAMES_PER_SECOND : 0.0f;
	if (data->m_flickerAmplitude > 0.0f && data->m_flickerFrequency > 0.0f)
	{
		Real progress = (elapsed - m_flickerStartTime) * data->m_flickerFrequency;
		if (m_flickerStartTime < 0.0f || progress < 0.0f || progress >= 1.0f)
		{
			const ScopedVisualRandom random(TRUE);
			m_flickerStart = m_flickerTarget;
			m_flickerTarget = intensity + GameClientRandomValueReal(-data->m_flickerAmplitude, data->m_flickerAmplitude);
			m_flickerStartTime = elapsed;
			progress = 0.0f;
		}
		intensity = WWMath::Lerp(m_flickerStart, m_flickerTarget, progress);
	}
	if (data->m_pulseAmplitude > 0.0f)
		intensity += data->m_pulseAmplitude * WWMath::Sin((elapsed + m_pulsePhase) * data->m_pulseFrequency * WWMATH_TWO_PI);
	intensity = WWMath::Max(0.0f, intensity) * getDrawable()->getEffectiveOpacity();
	Vector3 position = transformMtx->Get_Translation();
	Matrix3D boneTransform;
	if (data->m_attachBone.isNotEmpty() && data->m_attachBone.compareNoCase("None") != 0 &&
		getDrawable()->getCurrentWorldspaceClientBonePositions(data->m_attachBone.str(), boneTransform))
		position = boneTransform.Get_Translation();

	const RGBColor& ambient = data->m_ambient;
	const RGBColor& diffuse = data->m_diffuse;
	const RGBColor& specular = data->m_specular;
	m_light->Set_Ambient(Vector3(ambient.red, ambient.green, ambient.blue) * intensity);
	m_light->Set_Diffuse(Vector3(diffuse.red, diffuse.green, diffuse.blue) * intensity);
	m_light->Set_Specular(Vector3(specular.red, specular.green, specular.blue) * intensity);
	m_light->Set_Far_Attenuation_Range(0.0f, data->m_radius);
	m_light->Set_Flag(LightClass::FAR_ATTENUATION, true);
	m_light->Set_Position(position);
}

void W3DLightDraw::crc(Xfer* xfer)
{
	DrawModule::crc(xfer);
}

void W3DLightDraw::xfer(Xfer* xfer)
{
	XferVersion version = 2;
	xfer->xferVersion(&version, 2);
	DrawModule::xfer(xfer);
	if (version >= 2)
	{
		xfer->xferReal(&m_pulsePhase);
		xfer->xferReal(&m_flickerStart);
		xfer->xferReal(&m_flickerTarget);
		xfer->xferReal(&m_flickerStartTime);
	}
}

void W3DLightDraw::loadPostProcess()
{
	DrawModule::loadPostProcess();
}
