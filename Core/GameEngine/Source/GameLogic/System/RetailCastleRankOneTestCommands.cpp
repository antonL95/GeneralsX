#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/DrawModule.h"
#include "Common/Player.h"
#include "Common/PlayerTemplate.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "GameClient/Drawable.h"
#include "../../../../GameEngineDevice/Include/W3DDevice/GameClient/Module/W3DModelDraw.h"
#include "GameLogic/Module/BehaviorModule.h"
#include "GameLogic/Module/CastleBehavior.h"
#include "GameLogic/Module/FoundationAIUpdate.h"
#include "GameLogic/Module/UpgradeModule.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"

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
	Bool rendered = FALSE;
	Bool foundMesh = FALSE;
	if (Drawable* drawable = object->getDrawable())
	{
		for (DrawModule** module = drawable->getDrawModules(); module && *module; ++module)
		{
			auto* model = dynamic_cast<W3DModelDraw*>(*module);
			Bool hidden;
			if (model && model->getSubObjectHidden("WILD", hidden))
				wild = !hidden;
			RenderObjClass* render = model ? model->getRenderObject() : nullptr;
			if (render)
			{
				rendered = TRUE;
				Int index;
				RenderObjClass* child = render->Get_Sub_Object_By_Name("WILD", &index);
				if (child)
				{
					foundMesh = TRUE;
					wild = !child->Is_Hidden();
					child->Release_Ref();
				}
			}
		}
	}
	if (rendered && !foundMesh)
		std::fprintf(stderr, "TEST castle-rank-one missing-mesh %u %s\n", ctx.frame, ctx.targetName);
	std::fprintf(stderr, "TEST castle-rank-one %u %s construction=%d rank=%d hide=%d banner=%d show=%d wild=%d\n",
		ctx.frame, ctx.targetName, object->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION), rank && object->hasUpgrade(rank), hide,
		banners && object->hasUpgrade(banners), show, wild);
}

void createConstructionControl(const RetailTestCommands::Context& ctx)
{
	Player* player = ctx.findPlayer();
	const ThingTemplate* type = TheThingFactory->findTemplate(ctx.targetName, FALSE);
	Real x, y;
	if (!player || !type || std::sscanf(ctx.argument, "%f,%f", &x, &y) != 2)
		return;
	Object* object = TheThingFactory->newObjectFromExactTemplate(type, player->getDefaultTeam(),
		MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_UNDER_CONSTRUCTION));
	Coord3D position = {x, y, TheTerrainLogic->getGroundHeight(x, y)};
	object->setPosition(&position);
	object->giveUpgrade(TheUpgradeCenter->findUpgrade("Upgrade_StructureLevel1"));
}

void completeConstructionControl(const RetailTestCommands::Context& ctx)
{
	if (Object* object = ctx.requireObject())
	{
		object->setConstructionPercent(100.0f);
		FoundationAIUpdate::advanceConstruction(*object, nullptr);
	}
}

void sampleNativeCastle(const RetailTestCommands::Context& ctx)
{
	Object* object = ctx.requireObject();
	const CastleBehavior* castle = CastleBehavior::find(object);
	const Player* player = ctx.findPlayer();
	std::fprintf(stderr, "TEST castle-rank-one native %u faction=%s unpacked=%d\n", ctx.frame,
		player && player->getPlayerTemplate() ? player->getPlayerTemplate()->getName().str() : "-",
		castle && castle->isUnpacked());
}
}
#endif

namespace RetailTestCommands
{
void registerRetailCastleRankOneTestCommands()
{
#if !RTS_GENERALS
	registerAction("castle-rank-one-state", sampleCastleRankOne);
	registerAction("castle-rank-one-create-control", createConstructionControl);
	registerAction("castle-rank-one-finish", completeConstructionControl);
	registerAction("castle-rank-one-native", sampleNativeCastle);
#endif
}
}
