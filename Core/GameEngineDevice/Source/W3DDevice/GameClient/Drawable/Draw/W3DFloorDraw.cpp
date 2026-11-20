#include "Common/GameLOD.h"
#include "Common/GlobalData.h"
#include "Common/NameKeyGenerator.h"
#include "Common/Xfer.h"
#include "Common/ThingTemplate.h"
#include "GameClient/Drawable.h"
#include "GameClient/View.h"
#include "W3DDevice/GameClient/Module/W3DFloorDraw.h"
#include "W3DDevice/GameClient/W3DAssetManager.h"
#include "W3DDevice/GameClient/W3DDisplay.h"
#include "W3DDevice/GameClient/W3DScene.h"
#include "WW3D2/matinfo.h"
#include "WW3D2/rendobj.h"

namespace
{
StaticGameLODLevel floorLOD()
{
	return TheGameLODManager ? TheGameLODManager->getStaticLODLevel() : STATIC_GAME_LOD_UNKNOWN;
}

void parseHideCondition(INI* ini, void*, void* store, const void*)
{
	const Int condition = INI::scanIndexList(ini->getNextToken(), ModelConditionFlags::getBitNames());
	static_cast<ModelConditionFlags*>(store)->set(condition);
}

void parseWeatherTexture(INI* ini, void*, void* store, const void*)
{
	const Int weather = INI::scanIndexList(ini->getNextToken(), WeatherNames);
	static_cast<AsciiString*>(store)[weather] = ini->getNextToken();
}

AsciiString firstTexture(RenderObjClass& render)
{
	MaterialInfoClass* material = render.Get_Material_Info();
	if (material)
	{
		const TextureClass* texture = material->Texture_Count() ? material->Peek_Texture(0) : nullptr;
		const AsciiString name = texture ? texture->Get_Texture_Name().str() : "";
		material->Release_Ref();
		if (name.isNotEmpty())
			return name;
	}

	for (Int i = 0; i < render.Get_Num_Sub_Objects(); ++i)
	{
		RenderObjClass* child = render.Get_Sub_Object(i);
		const AsciiString name = firstTexture(*child);
		child->Release_Ref();
		if (name.isNotEmpty())
			return name;
	}
	return {};
}
}

void W3DFloorDrawModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	ModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "ModelName", INI::parseAsciiString, nullptr, offsetof(W3DFloorDrawModuleData, m_modelName) },
		{ "HideIfModelConditions", parseHideCondition, nullptr, offsetof(W3DFloorDrawModuleData, m_hideConditions) },
		{ "StaticModelLODMode", INI::parseBool, nullptr, offsetof(W3DFloorDrawModuleData, m_staticModelLODMode) },
		{ "ForceToBack", INI::parseBool, nullptr, offsetof(W3DFloorDrawModuleData, m_forceToBack) },
		{ "StartHidden", INI::parseBool, nullptr, offsetof(W3DFloorDrawModuleData, m_startHidden) },
		{ "WeatherTexture", parseWeatherTexture, nullptr, offsetof(W3DFloorDrawModuleData, m_weatherTextures) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

Bool W3DFloorDrawModuleData::isHidden(const ModelConditionFlags& conditions) const
{
	return m_hideConditions.testForAny(conditions);
}

RenderObjClass* W3DFloorDrawModuleData::createModel(W3DAssetManager& assets, Real scale, Weather weather) const
{
	if (m_modelName.isEmpty() || m_modelName.isNone())
		return nullptr;

	AsciiString modelName = m_modelName;
	if (m_staticModelLODMode)
	{
		const StaticGameLODLevel lod = floorLOD();
		if (lod == STATIC_GAME_LOD_MEDIUM)
			modelName.concat('M');
		else if (lod == STATIC_GAME_LOD_LOW)
			modelName.concat('L');
	}

	RenderObjClass* model = assets.Create_Render_Obj(modelName.str(), scale, 0);
	if (!model && modelName != m_modelName)
	{
		modelName = m_modelName;
		model = assets.Create_Render_Obj(modelName.str(), scale, 0);
	}
	if (!model)
		return nullptr;

	const AsciiString& replacement = weather >= WEATHER_NORMAL && weather < WEATHER_COUNT
		? m_weatherTextures[weather] : AsciiString::TheEmptyString;
	if (replacement.isNotEmpty() && !replacement.isNone())
	{
		const AsciiString original = firstTexture(*model);
		if (original.isNotEmpty())
		{
			RenderObjClass* variant = assets.Create_Render_Obj(modelName.str(), scale, 0, original.str(), replacement.str());
			if (variant)
			{
				model->Release_Ref();
				model = variant;
			}
		}
	}

	return model;
}

W3DFloorDraw::W3DFloorDraw(Thing* thing, const ModuleData* data)
	: DrawModule(thing, data),
	  m_lod(floorLOD()),
	  m_hiddenBySubObject(getW3DFloorDrawModuleData()->m_startHidden)
{
	m_weather = TheGlobalData->m_weather;
	if (W3DDisplay::m_assetManager && W3DDisplay::m_3DScene)
		doDrawModule(getDrawable()->getTransformMatrix());
}

W3DFloorDraw::~W3DFloorDraw()
{
	removeModel();
}

void W3DFloorDraw::removeModel()
{
	if (m_model)
	{
		if (W3DDisplay::m_3DScene)
			W3DDisplay::m_3DScene->Remove_Render_Object(m_model);
		m_model->Release_Ref();
		m_model = nullptr;
	}
}

static Int floorPickType(const ThingTemplate* tmplate)
{
	if (tmplate == nullptr || tmplate->isKindOf(KINDOF_CLICK_THROUGH))
		return 0;
	Int type = 0;
	if (tmplate->isKindOf(KINDOF_SELECTABLE))
		type |= PICK_TYPE_SELECTABLE;
	if (tmplate->isKindOf(KINDOF_FORCEATTACKABLE))
		type |= PICK_TYPE_FORCEATTACKABLE;
	return type;
}

void W3DFloorDraw::updateModel()
{
	removeModel();
	m_initialized = TRUE;
	const auto* data = getW3DFloorDrawModuleData();
	m_weather = TheGlobalData->m_weather;
	m_lod = floorLOD();
	m_model = data->createModel(*W3DDisplay::m_assetManager, getDrawable()->getScale(), m_weather);
	if (!m_model)
		return;

	m_model->Set_Collision_Type(floorPickType(getDrawable()->getTemplate()));
	m_model->Set_User_Data(getDrawable()->getDrawableInfo());
	W3DDisplay::m_3DScene->addFloorRenderObject(m_model, data->m_forceToBack);
}

void W3DFloorDraw::doDrawModule(const Matrix3D* transform)
{
	if (W3DDisplay::m_assetManager && W3DDisplay::m_3DScene &&
		(!m_initialized || m_weather != TheGlobalData->m_weather ||
		(getW3DFloorDrawModuleData()->m_staticModelLODMode && m_lod != floorLOD())))
		updateModel();
	if (!m_model)
		return;

	const Real scale = getDrawable()->getInstanceScale();
	Matrix3D scaled = *transform;
	if (scale != 1.0f)
		scaled.Scale(scale);
	m_model->Set_ObjectScale(scale);
	m_model->Set_Transform(scaled);
	updateVisibility(getDrawable()->isDrawableEffectivelyHidden(), getDrawable()->getFullyObscuredByShroud());
}

void W3DFloorDraw::reactToTransformChange(const Matrix3D*, const Coord3D*, Real)
{
	doDrawModule(getDrawable()->getTransformMatrix());
}

void W3DFloorDraw::setFullyObscuredByShroud(Bool obscured)
{
	updateVisibility(getDrawable()->isDrawableEffectivelyHidden(), obscured);
}

void W3DFloorDraw::setDrawableHidden(Bool hidden)
{
	updateVisibility(hidden, getDrawable()->getFullyObscuredByShroud());
}

void W3DFloorDraw::showDrawableSubObject(const AsciiString& name, Bool show)
{
	if (name.isNotEmpty() && name.compareNoCase(KEYNAME(getModuleTagNameKey()).str()) == 0)
	{
		m_hiddenBySubObject = !show;
		updateVisibility(getDrawable()->isDrawableEffectivelyHidden(), getDrawable()->getFullyObscuredByShroud());
	}
}

void W3DFloorDraw::updateVisibility(Bool hidden, Bool obscured)
{
	if (m_model)
		m_model->Set_Hidden(hidden || obscured || m_hiddenBySubObject ||
			getW3DFloorDrawModuleData()->isHidden(getDrawable()->getModelConditionFlags()));
}

void W3DFloorDraw::crc(Xfer* xfer)
{
	DrawModule::crc(xfer);
}

void W3DFloorDraw::xfer(Xfer* xfer)
{
	const XferVersion currentVersion = 2;
	XferVersion version = currentVersion;
	xfer->xferVersion(&version, currentVersion);
	if (version == 1)
	{
		DrawableModule::xfer(xfer);
		return;
	}

	DrawModule::xfer(xfer);
	xfer->xferBool(&m_hiddenBySubObject);
}

void W3DFloorDraw::loadPostProcess()
{
	DrawModule::loadPostProcess();
	updateVisibility(getDrawable()->isDrawableEffectivelyHidden(), getDrawable()->getFullyObscuredByShroud());
}
