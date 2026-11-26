#include "PreRTS.h"

#include "GameLogic/Module/FoundationAIUpdate.h"
#include "GameLogic/Module/WallHubBehavior.h"

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
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/HeroRoster.h"
#include "GameLogic/Object.h"
#if !RTS_GENERALS
#include "GameLogic/LuaScriptEngine.h"
#endif
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/CastleBehavior.h"
#include "GameLogic/Module/CreateModule.h"
#include "GameLogic/Module/RotwkParseOnlyModules.h"

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
		if (!button || button->getCommandType() != GUI_COMMAND_FOUNDATION_CONSTRUCT ||
			!button->getThingTemplate() || !button->getThingTemplate()->isEquivalentTo(&what))
			continue;
#if RTS_ZEROHOUR
		if (button->getRequiredRank() > foundation.getExperienceTracker()->getRank() ||
			!HeroRoster::buttonUpgradesMet(button, foundation.getControllingPlayer(), &foundation))
			continue;
#endif
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

Object* FoundationAIUpdate::findPlotFor(const Object& structure)
{
	if (structure.getProducerID() == INVALID_ID)
		return nullptr;
	Object* plot = TheGameLogic->findObjectByID(structure.getProducerID());
	const auto* foundation = plot && !plot->isDestroyed() ? find(*plot) : nullptr;
	return foundation && foundation->m_structure == structure.getID() ? plot : nullptr;
}

Object* FoundationAIUpdate::getStructure() const
{
	return m_structure == INVALID_ID ? nullptr : TheGameLogic->findObjectByID(m_structure);
}

void FoundationAIUpdate::replaceStructure(Object& replacement)
{
	m_structure = replacement.getID();
	replacement.setProducer(getObject());
	linkStructureToCastle(replacement);
}

void FoundationAIUpdate::linkStructureToCastle(Object& structure) const
{
	const Object* foundation = getObject();
	if (const CastleBehavior* castle = CastleBehavior::findForMember(*foundation))
	{
		const Object* keep = castle->getKeep();
		CastleMemberBehavior::linkToCastle(structure, castle->getCastleObjectID(),
			keep ? keep->getID() : INVALID_ID, FALSE, castle->isOwnedMemberTemplate(*foundation->getTemplate()));
		castle->applyKeepUpgrades(structure);
	}
}

Bool FoundationAIUpdate::canBuildOn(const Player* player) const
{
	const Object* foundation = getObject();
	if (!player || foundation->getControllingPlayer() != player || foundation->isEffectivelyDead() || getStructure())
		return FALSE;
	if (const CastleMemberBehavior* cm = CastleMemberBehavior::find(foundation))
	{
		if (cm->getKeepID() != INVALID_ID)
		{
			const Object* keep = TheGameLogic->findObjectByID(cm->getKeepID());
			if (!keep || keep->isDestroyed() || keep->isEffectivelyDead())
				return FALSE;
		}
	}
	return TRUE;
}

Object* FoundationAIUpdate::construct(const ThingTemplate* what, Player* player, Bool prebuilt)
{
	Object* foundation = getObject();
	if (!what || !canBuildOn(player) || !what->isKindOf(KINDOF_STRUCTURE) ||
		(!prebuilt && (!player->canBuild(what) || !commandSetOffers(*foundation, *what))))
		return nullptr;
#if RTS_ZEROHOUR
	if (!prebuilt && !player->canBuildMoreOfType(what))
		return nullptr;
#endif

	const UnsignedInt cost = prebuilt ? 0 : static_cast<UnsignedInt>(std::max(0, what->calcCostToBuild(player)));
	Money* money = player->getMoney();
	if (money->countMoney() < cost)
		return nullptr;
	const UnsignedInt paid = money->withdraw(cost);

	Object* structure = TheThingFactory->newObject(what, player->getDefaultTeam(),
		MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_UNDER_CONSTRUCTION));
	structure->setProducer(foundation);
	structure->setBuilder(foundation);
	structure->setConstructionCostPaid(paid);

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
#if !RTS_GENERALS
	if (variation == 1 || variation == 2)
		if (auto* lua = LuaScriptEngine::active())
			lua->dispatch(*structure, "OnBuildVariation", static_cast<Real>(variation));
#endif

	m_structure = structure->getID();
	linkStructureToCastle(*structure);
	showFoundation(FALSE);
	if (prebuilt)
	{
		structure->setConstructionPercent(100.0f);
		body->internalChangeHealth(body->getMaxHealth() - body->getHealth());
		advanceConstruction(*structure, foundation);
	}
	else if (RetailFileLayers::enabled())
		GettingBuiltBehavior::beginConstruction(*structure);
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

void FoundationAIUpdate::advanceConstruction(Object& structure, Object* builder, Int overrideFrames, Bool isRebuild)
{
	Player* player = structure.getControllingPlayer();
	const Int frames = overrideFrames > 0 ? overrideFrames :
		GettingBuiltBehavior::getConstructionFrames(structure, structure.getTemplate()->calcTimeToBuild(player));
	structure.setConstructionPercent(structure.getConstructionPercent() + CastleRules::constructionStep(frames));
	BodyModuleInterface* body = structure.getBodyModule();
	body->internalChangeHealth(body->getMaxHealth() / INT_TO_REAL(frames > 0 ? frames : 1));
	if (structure.getConstructionPercent() < 100.0f)
		return;

	const Bool reconstructing = RetailFileLayers::enabled() && structure.testStatus(OBJECT_STATUS_RECONSTRUCTING);
	structure.clearStatus(MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_UNDER_CONSTRUCTION));
	if (reconstructing)
		structure.clearStatus(MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_RECONSTRUCTING));
	structure.clearModelConditionFlags(MAKE_MODELCONDITION_MASK3(MODELCONDITION_AWAITING_CONSTRUCTION,
		MODELCONDITION_PARTIALLY_CONSTRUCTED, MODELCONDITION_ACTIVELY_BEING_CONSTRUCTED));
	structure.setConstructionPercent(CONSTRUCTION_COMPLETE);
	if (!RetailFileLayers::enabled())
		body->evaluateVisualCondition();

	if (player)
	{
		player->onStructureConstructionComplete(builder, &structure, isRebuild || reconstructing);
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
		if (RetailFileLayers::enabled())
		{
			const auto* behavior = GettingBuiltBehavior::find(*structure);
			if (behavior && behavior->managesConstruction())
				return UPDATE_SLEEP(STRUCTURE_CHECK_FRAMES);
		}
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
