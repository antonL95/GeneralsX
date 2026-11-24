#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/DrawModule.h"
#include "Common/Upgrade.h"
#include "GameClient/Drawable.h"
#include "../../../../GameEngineDevice/Include/W3DDevice/GameClient/Module/W3DModelDraw.h"
#include "GameLogic/Module/BehaviorModule.h"
#include "GameLogic/Module/UpgradeModule.h"
#include "GameLogic/Object.h"

namespace
{
void sampleCastleRankOne(const RetailTestCommands::Context& ctx)
{
	Object* object = ctx.requireObject();
	if (!object)
		return;
	const auto* rank = TheUpgradeCenter->findUpgrade("Upgrade_StructureLevel1");
	const auto* banners = TheUpgradeCenter->findUpgrade("Upgrade_WildFortressGoblinTownTrigger");
	Int hide = -1;
	Int show = -1;
	for (BehaviorModule** module = object->getBehaviorModules(); *module; ++module)
	{
		const UpgradeModuleInterface* upgrade = (*module)->getUpgrade();
		if (!upgrade)
			continue;
		if ((*module)->getModuleTagNameKey() == NAMEKEY("ModuleTag_HideAll"))
			hide = upgrade->isAlreadyUpgraded();
		if ((*module)->getModuleTagNameKey() == NAMEKEY("ModuleTag_ShowBanners"))
			show = upgrade->isAlreadyUpgraded();
	}
	Int wild = -1;
	if (Drawable* drawable = object->getDrawable())
	{
		for (DrawModule** module = drawable->getDrawModules(); module && *module; ++module)
		{
			auto* model = dynamic_cast<W3DModelDraw*>(*module);
			Bool hidden;
			if (model && model->getSubObjectHidden("WILD", hidden))
				wild = !hidden;
			RenderObjClass* render = model ? model->getRenderObject() : nullptr;
			for (Int i = 0; render && i < render->Get_Num_Sub_Objects(); ++i)
			{
				RenderObjClass* child = render->Get_Sub_Object(i);
				if (!child)
					continue;
				if (stricmp(child->Get_Name(), "WILD") == 0)
					wild = !child->Is_Hidden();
				child->Release_Ref();
			}
		}
	}
	std::fprintf(stderr, "TEST castle-rank-one %u construction=%d rank=%d hide=%d banner=%d show=%d wild=%d\n",
		ctx.frame, object->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION), rank && object->hasUpgrade(rank), hide,
		banners && object->hasUpgrade(banners), show, wild);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailCastleRankOneTestCommands()
{
#if !RTS_GENERALS
	registerAction("castle-rank-one-state", sampleCastleRankOne);
#endif
}
}
