#include "Common/GlobalData.h"
#include "Common/INI.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "GameClient/Drawable.h"
#include "GameClient/ClientRandomValue.h"
#include "GameClient/RetailBuffVisuals.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "W3DDevice/GameClient/Module/W3DBuffDraw.h"
#include "W3DDevice/GameClient/W3DDisplay.h"
#include "W3DDevice/GameClient/W3DAssetManager.h"
#include "WW3D2/rendobj.h"
#include "WW3D2/rinfo.h"
#include "WW3D2/camera.h"

#include <map>
#include <string>

namespace
{
struct ActiveBuff
{
	~ActiveBuff()
	{
		if (model)
			model->Release_Ref();
	}

	const W3DBuffDrawModuleData* art = nullptr;
	RenderObjClass* model = nullptr;
	UnsignedInt start = 0;
	UnsignedInt duration = 0;
	Bool attempted = FALSE;
};

std::map<std::pair<ObjectID, std::string>, ActiveBuff> activeBuffs;
constexpr size_t MAX_ACTIVE_BUFFS = 4096;

void revealBuffArt(RenderObjClass &model)
{
	model.Set_Hidden(FALSE);
	for (Int i = 0; i < model.Get_Num_Sub_Objects(); ++i)
		if (auto* child = model.Get_Sub_Object(i))
		{
			revealBuffArt(*child);
			child->Release_Ref();
		}
}
}

void W3DBuffDrawModuleData::buildFieldParse(MultiIniFieldParse& parse)
{
	W3DFloorDrawModuleData::buildFieldParse(parse);
	static const FieldParse fields[] =
	{
		{ "PreDraw", INI::parseBool, nullptr, offsetof(W3DBuffDrawModuleData, m_preDraw) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	parse.add(fields);
}

W3DBuffDraw::~W3DBuffDraw() = default;

void W3DBuffDraw::crc(Xfer* xfer) { W3DFloorDraw::crc(xfer); }
void W3DBuffDraw::xfer(Xfer* xfer) { W3DFloorDraw::xfer(xfer); }
void W3DBuffDraw::loadPostProcess() { W3DFloorDraw::loadPostProcess(); }

void setRetailBuffVisual(const Object& owner, const AsciiString& type, const AsciiString& visual, UnsignedInt duration)
{
	if (TheGlobalData->m_headless || type.isEmpty())
		return;
	const auto key = std::make_pair(owner.getID(), std::string(type.str()));
	if (!duration)
	{
		activeBuffs.erase(key);
		return;
	}

	const ThingTemplate* buff = visual.isNotEmpty() && !visual.isNone() ? TheThingFactory->findTemplate(visual, FALSE) : nullptr;
	if (!buff)
		return;
	const ModuleInfo& modules = buff->getDrawModuleInfo();
	for (Int i = 0; i < modules.getCount(); ++i)
	{
		if (modules.getNthName(i) != "W3DBuffDraw")
			continue;
		const auto* art = static_cast<const W3DBuffDrawModuleData*>(modules.getNthData(i));
		auto found = activeBuffs.find(key);
		if (found != activeBuffs.end() && found->second.art != art)
		{
			activeBuffs.erase(found);
			found = activeBuffs.end();
		}
		if (found == activeBuffs.end())
		{
			if (activeBuffs.size() >= MAX_ACTIVE_BUFFS)
				return;
			found = activeBuffs.try_emplace(key).first;
		}
		ActiveBuff& active = found->second;
		if (active.model && std::getenv("GENERALSX_TRACE_RETAIL_FX_LISTS"))
			std::fprintf(stderr, "[Retail Buff] refresh model=%s owner=%s frame=%u\n",
				art->m_modelName.str(), owner.getTemplate()->getName().str(), TheGameLogic->getFrame());
		active.art = art;
		active.start = TheGameLogic->getFrame();
		active.duration = duration;
		return;
	}
}

void W3DBuffDraw::reset()
{
	activeBuffs.clear();
}

void W3DBuffDraw::render(RenderInfoClass& info, Int viewer, Bool preDraw)
{
	if (activeBuffs.empty() || !W3DDisplay::m_assetManager)
		return;
	const ScopedVisualRandom random(TRUE);
	const UnsignedInt now = TheGameLogic->getFrame();
	auto* lights = info.light_environment;
	info.light_environment = nullptr;
	for (auto it = activeBuffs.begin(); it != activeBuffs.end();)
	{
		ActiveBuff& active = it->second;
		const Object* owner = TheGameLogic->findObjectByID(it->first.first);
		if (!owner || owner->isEffectivelyDead() || now - active.start >= active.duration)
		{
			if (owner && active.model && std::getenv("GENERALSX_TRACE_RETAIL_FX_LISTS"))
				std::fprintf(stderr, "[Retail Buff] expired model=%s owner=%s frame=%u\n",
					active.art->m_modelName.str(), owner->getTemplate()->getName().str(), now);
			it = activeBuffs.erase(it);
			continue;
		}
		++it;
		Drawable* draw = owner->getDrawable();
		if (active.art->m_preDraw != preDraw || !draw || draw->isDrawableEffectivelyHidden() ||
			draw->getFullyObscuredByShroud() || viewer < 0 ||
			ThePartitionManager->getShroudStatusForPlayer(viewer, owner->getPosition()) != CELLSHROUD_CLEAR)
			continue;

		if (!active.attempted)
		{
			active.attempted = TRUE;
			active.model = active.art->createModel(*W3DDisplay::m_assetManager, draw->getScale(), TheGlobalData->m_weather);
			if (active.model)
				revealBuffArt(*active.model);
			if (std::getenv("GENERALSX_TRACE_RETAIL_FX_LISTS"))
			{
				Int visibleMeshes = 0;
				if (active.model)
				{
					if (active.model->Class_ID() == RenderObjClass::CLASSID_MESH)
						visibleMeshes = !active.model->Is_Hidden();
					for (Int i = 0; i < active.model->Get_Num_Sub_Objects(); ++i)
						if (auto* subObject = active.model->Get_Sub_Object(i))
						{
							visibleMeshes += subObject->Class_ID() == RenderObjClass::CLASSID_MESH && !subObject->Is_Hidden();
							subObject->Release_Ref();
						}
				}
				std::fprintf(stderr, "[Retail Buff] model=%s owner=%s id=%u frame=%u duration=%u loaded=%d preDraw=%d visibleMeshes=%d\n",
					active.art->m_modelName.str(), owner->getTemplate()->getName().str(), static_cast<UnsignedInt>(owner->getID()),
					now, active.duration, active.model != nullptr, preDraw, visibleMeshes);
			}
		}
		if (active.model)
		{
			active.model->Set_Transform(*draw->getTransformMatrix());
			if (!info.Camera.Cull_Sphere(active.model->Get_Bounding_Sphere()))
				active.model->Render(info);
		}
	}
	info.light_environment = lights;
}
