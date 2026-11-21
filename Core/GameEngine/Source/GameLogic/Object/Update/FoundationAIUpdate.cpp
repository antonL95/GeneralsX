#include "PreRTS.h"

#include "GameLogic/Module/FoundationAIUpdate.h"

#include <algorithm>

#include "Common/BuildAssistant.h"
#include "Common/INI.h"
#include "Common/Money.h"
#include "Common/Player.h"
#include "Common/RetailFileLayers.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"
#include "GameClient/ControlBar.h"
#include "GameClient/Drawable.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/CastleRules.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/CastleBehavior.h"
#include "GameLogic/Module/CreateModule.h"

namespace
{
constexpr UnsignedInt STRUCTURE_CHECK_FRAMES = LOGICFRAMES_PER_SECOND / 2;

Bool commandSetOffers(const Object& foundation, const ThingTemplate& what)
{
	CommandCatalog* catalog = TheRetailCommandCatalog;
	if (!catalog)
		return FALSE;
	const CommandSet* commands = catalog->findCommandSet(foundation.getCommandSetString());
	if (!commands)
		return FALSE;

	for (Int i = 0; i < CommandButtonRules::RETAIL_COMMANDS_PER_SET; ++i)
	{
		const CommandButton* button = commands->getCommandButton(i);
		if (button && button->getCommandType() == GUI_COMMAND_FOUNDATION_CONSTRUCT &&
			button->getThingTemplate() && button->getThingTemplate()->isEquivalentTo(&what))
			return TRUE;
	}
	return FALSE;
}
}

void FoundationAIUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpdateModuleData::buildFieldParse(p);

	static const FieldParse fields[] =
	{
		{ "BuildVariation", INI::parseInt, nullptr, offsetof(FoundationAIUpdateModuleData, m_buildVariation) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields, 0);
}

FoundationAIUpdate::FoundationAIUpdate(Thing* thing, const ModuleData* data) : UpdateModule(thing, data)
{
	setWakeFrame(getObject(), UPDATE_SLEEP_FOREVER);
}

FoundationAIUpdate::~FoundationAIUpdate() = default;

FoundationAIUpdate* FoundationAIUpdate::find(const Object& object)
{
	return static_cast<FoundationAIUpdate*>(object.findUpdateModule(NAMEKEY("FoundationAIUpdate")));
}

Object* FoundationAIUpdate::getStructure() const
{
	return m_structure == INVALID_ID ? nullptr : TheGameLogic->findObjectByID(m_structure);
}

Bool FoundationAIUpdate::canBuildOn(const Player* player) const
{
	const Object* foundation = getObject();
	return player && foundation->getControllingPlayer() == player && !foundation->isEffectivelyDead() && !getStructure();
}

Object* FoundationAIUpdate::construct(const ThingTemplate* what, Player* player, Bool prebuilt)
{
	Object* foundation = getObject();
	if (!what || !canBuildOn(player) || !what->isKindOf(KINDOF_STRUCTURE) ||
		(!prebuilt && !player->canBuild(what)) || !commandSetOffers(*foundation, *what))
		return nullptr;
#if RTS_ZEROHOUR
	if (!prebuilt && !player->canBuildMoreOfType(what))
		return nullptr;
#endif

	const UnsignedInt cost = prebuilt ? 0 : static_cast<UnsignedInt>(std::max(0, what->calcCostToBuild(player)));
	Money* money = player->getMoney();
	if (money->countMoney() < cost)
		return nullptr;
	money->withdraw(cost);

	Object* structure = TheThingFactory->newObject(what, player->getDefaultTeam(),
		MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_UNDER_CONSTRUCTION));
	structure->setProducer(foundation);
	structure->setBuilder(foundation);

	const Coord3D* at = foundation->getPosition();
	const Coord3D ground = {at->x, at->y, TheTerrainLogic->getGroundHeight(at->x, at->y)};
	structure->setOrientation(normalizeAngle(foundation->getOrientation() + what->getPlacementViewAngle()));
	structure->setPosition(&ground);
	TheAI->pathfinder()->addObjectToPathfindMap(structure);
	player->onStructureCreated(foundation, structure);

	structure->setConstructionPercent(0.0f);
	BodyModuleInterface* body = structure->getBodyModule();
	body->internalChangeHealth(-body->getHealth() + 1.0f);
	structure->clearAndSetModelConditionFlags(
		MAKE_MODELCONDITION_MASK(MODELCONDITION_AWAITING_CONSTRUCTION),
		MAKE_MODELCONDITION_MASK2(MODELCONDITION_PARTIALLY_CONSTRUCTED, MODELCONDITION_ACTIVELY_BEING_CONSTRUCTED));

	const Int variation = getFoundationAIUpdateModuleData()->m_buildVariation;
	if (variation == 1)
		structure->setModelConditionState(MODELCONDITION_BUILD_VARIATION_ONE);
	else if (variation == 2)
		structure->setModelConditionState(MODELCONDITION_BUILD_VARIATION_TWO);

	m_structure = structure->getID();
	if (const CastleBehavior* castle = CastleBehavior::findForMember(*foundation))
		castle->applyKeepUpgrades(*structure);
	showFoundation(FALSE);
	if (prebuilt)
	{
		structure->setConstructionPercent(100.0f);
		body->internalChangeHealth(body->getMaxHealth() - body->getHealth());
		advanceConstruction(*structure, foundation);
	}
	setWakeFrame(foundation, UPDATE_SLEEP_NONE);
	return structure;
}

void FoundationAIUpdate::showFoundation(Bool show)
{
	Object* foundation = getObject();
	if (show)
		foundation->clearStatus(MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_UNSELECTABLE));
	else
		foundation->setStatus(MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_UNSELECTABLE));
	if (Drawable* drawable = foundation->getDrawable())
		drawable->setDrawableHidden(!show);
}

void FoundationAIUpdate::advanceConstruction(Object& structure, Object* builder)
{
	Player* player = structure.getControllingPlayer();
	const Int frames = structure.getTemplate()->calcTimeToBuild(player);
	structure.setConstructionPercent(structure.getConstructionPercent() + CastleRules::constructionStep(frames));
	BodyModuleInterface* body = structure.getBodyModule();
	body->internalChangeHealth(body->getMaxHealth() / INT_TO_REAL(frames > 0 ? frames : 1));
	if (structure.getConstructionPercent() < 100.0f)
		return;

	structure.clearStatus(MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_UNDER_CONSTRUCTION));
	structure.clearModelConditionFlags(MAKE_MODELCONDITION_MASK3(MODELCONDITION_AWAITING_CONSTRUCTION,
		MODELCONDITION_PARTIALLY_CONSTRUCTED, MODELCONDITION_ACTIVELY_BEING_CONSTRUCTED));
	structure.setConstructionPercent(CONSTRUCTION_COMPLETE);
	if (!RetailFileLayers::enabled())
		body->evaluateVisualCondition();

	if (player)
	{
		player->onStructureConstructionComplete(builder, &structure, FALSE);
		for (BehaviorModule** module = structure.getBehaviorModules(); *module; ++module)
		{
			CreateModuleInterface* create = (*module)->getCreate();
			if (create)
				create->onBuildComplete();
		}
	}

	structure.handlePartitionCellMaintenance();
	structure.updateUpgradeModules();
}

UpdateSleepTime FoundationAIUpdate::update()
{
	if (m_structure == INVALID_ID)
		return UPDATE_SLEEP_FOREVER;

	Object* structure = getStructure();
	if (!structure)
	{
		m_structure = INVALID_ID;
		showFoundation(TRUE);
		return UPDATE_SLEEP_FOREVER;
	}

	if (structure->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION) && !structure->isEffectivelyDead())
	{
		advanceConstruction(*structure, getObject());
		return UPDATE_SLEEP_NONE;
	}
	return UPDATE_SLEEP(STRUCTURE_CHECK_FRAMES);
}

void FoundationAIUpdate::crc(Xfer* xfer)
{
	UpdateModule::crc(xfer);
}

void FoundationAIUpdate::xfer(Xfer* xfer)
{
	XferVersion version = 1;
	xfer->xferVersion(&version, 1);
	UpdateModule::xfer(xfer);
	xfer->xferObjectID(&m_structure);
}

void FoundationAIUpdate::loadPostProcess()
{
	UpdateModule::loadPostProcess();
	if (m_structure != INVALID_ID)
	{
		if (Drawable* drawable = getObject()->getDrawable())
			drawable->setDrawableHidden(TRUE);
	}
}
