#include "PreRTS.h"

#include <algorithm>

#include "Common/INIAudit.h"
#include "Common/Player.h"
#include "Common/RetailFileLayers.h"
#include "Common/Xfer.h"
#include "GameClient/Drawable.h"
#include "GameClient/RetailWorldUI.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/SupplyCenterProductionExitUpdate.h"
#include "GameLogic/Module/WorkerAIUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"

namespace
{
Real distanceSquared(const Coord3D& first, const Coord3D& second)
{
	const Real x = first.x - second.x;
	const Real y = first.y - second.y;
	return x * x + y * y;
}

const SupplyCenterProductionExitUpdate* harvestDepot(const Object* depot, const Player* player)
{
	if (!depot || !depot->isKindOf(KINDOF_SUPPLY_GATHERING_CENTER) || depot->getControllingPlayer() != player ||
		depot->isDestroyed() || depot->isEffectivelyDead() || depot->testStatus(OBJECT_STATUS_SOLD) ||
		depot->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION))
		return nullptr;
	return static_cast<const SupplyCenterProductionExitUpdate*>(
		depot->findUpdateModule(NAMEKEY("SupplyCenterProductionExitUpdate")));
}
}

void WorkerAIUpdateModuleData::addRetailHarvestFields(MultiIniFieldParse& p)
{
	if (!RetailFileLayers::enabled() && !INIAudit::isEnabled())
		return;
	static const FieldParse fields[] = {
		{ "HarvestTrees", INI::parseBool, nullptr, offsetof(WorkerAIUpdateModuleData, m_harvestTrees) },
		{ "HarvestActivationRange", INI::parseReal, nullptr, offsetof(WorkerAIUpdateModuleData, m_harvestRange) },
		{ "HarvestPreparationTime", INI::parseDurationUnsignedInt, nullptr, offsetof(WorkerAIUpdateModuleData, m_harvestPreparation) },
		{ "HarvestActionTime", INI::parseDurationUnsignedInt, nullptr, offsetof(WorkerAIUpdateModuleData, m_harvestAction) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

void WorkerAIUpdate::startRetailHarvest(const Coord3D* location)
{
	if (!getWorkerAIUpdateModuleData()->m_harvestTrees)
		return;
	stopRetailHarvest();
	if (getCurrentTask() != DOZER_TASK_INVALID)
		cancelTask(getCurrentTask());
	aiIdle(CMD_FROM_AI);
	m_harvestActive = TRUE;
	m_harvestHasOrigin = location != nullptr;
	if (location)
		m_harvestOrigin = *location;
}

void WorkerAIUpdate::stopRetailHarvest()
{
	if (m_harvestState == HARVEST_PREPARE)
		getObject()->clearModelConditionState(MODELCONDITION_HARVEST_PREPARATION);
	if (m_harvestState == HARVEST_GATHER)
		getObject()->clearModelConditionState(MODELCONDITION_HARVEST_ACTION);
	m_forcePending = FALSE;
	m_harvestActive = FALSE;
	m_harvestState = HARVEST_IDLE;
	m_harvestDeadline = 0;
}

Bool WorkerAIUpdate::updateRetailHarvest()
{
	Object* worker = getObject();
	if (worker->isEffectivelyDead())
		return FALSE;
	if (m_forcePending)
	{
		m_forcePending = FALSE;
		m_harvestActive = TRUE;
		m_harvestHasOrigin = FALSE;
	}
	if (!m_harvestActive || getCurrentTask() != DOZER_TASK_INVALID)
		return FALSE;

	const auto* data = getWorkerAIUpdateModuleData();
	Player* player = worker->getControllingPlayer();
	const auto frame = TheGameLogic->getFrame();
	const Coord3D& position = *worker->getPosition();
	const Real range = std::max(10.0f, data->m_harvestRange);

	switch (m_harvestState)
	{
		case HARVEST_IDLE:
			if (!isIdle() || frame < m_harvestDeadline)
				break;
			if (m_numberBoxes > 0)
			{
				const Object* depot = TheGameLogic->findObjectByID(worker->getProducerID());
				const auto* exit = harvestDepot(depot, player);
				if (!exit)
				{
					Real best = 0.0f;
					for (const Object* candidate = TheGameLogic->getFirstObject(); candidate; candidate = candidate->getNextObject())
					{
						const auto* candidateExit = harvestDepot(candidate, player);
						if (!candidateExit)
							continue;
						const Real distance = distanceSquared(position, *candidate->getPosition());
						if (!exit || distance < best || (distance == best && candidate->getID() < depot->getID()))
						{
							depot = candidate;
							exit = candidateExit;
							best = distance;
						}
					}
				}
				if (exit && exit->getNaturalRallyPoint(m_harvestDropPoint))
				{
					m_harvestDepot = depot->getID();
					m_harvestState = HARVEST_RETURN;
					aiMoveToPosition(&m_harvestDropPoint, CMD_FROM_AI);
					break;
				}
			}
			else if (data->m_maxBoxesData > 0 && data->m_warehouseScanDistance > 0 &&
				TheTerrainLogic->findNearestHarvestTree(m_harvestHasOrigin ? m_harvestOrigin : position,
					data->m_warehouseScanDistance, m_harvestTree))
			{
				m_harvestState = HARVEST_APPROACH;
				aiMoveToPosition(&m_harvestTree, CMD_FROM_AI);
				break;
			}
			m_harvestDeadline = frame + LOGICFRAMES_PER_SECOND;
			break;

		case HARVEST_APPROACH:
			if (distanceSquared(position, m_harvestTree) <= range * range)
			{
				aiIdle(CMD_FROM_AI);
				worker->setModelConditionState(MODELCONDITION_HARVEST_PREPARATION);
				m_harvestState = HARVEST_PREPARE;
				m_harvestDeadline = frame + data->m_harvestPreparation;
			}
			else if (isIdle())
			{
				m_harvestState = HARVEST_IDLE;
				m_harvestDeadline = frame + LOGICFRAMES_PER_SECOND;
			}
			break;

		case HARVEST_PREPARE:
			if (frame >= m_harvestDeadline)
			{
				worker->clearModelConditionState(MODELCONDITION_HARVEST_PREPARATION);
				worker->setModelConditionState(MODELCONDITION_HARVEST_ACTION);
				m_harvestState = HARVEST_GATHER;
				m_harvestDeadline = frame + data->m_harvestAction;
			}
			break;

		case HARVEST_GATHER:
			if (frame >= m_harvestDeadline)
			{
				worker->clearModelConditionState(MODELCONDITION_HARVEST_ACTION);
				if (distanceSquared(position, m_harvestTree) <= range * range)
					m_numberBoxes = TheTerrainLogic->takeMapTreeBoxes(m_harvestTree, data->m_maxBoxesData);
				if (m_numberBoxes)
					worker->setModelConditionState(MODELCONDITION_CARRYING);
				m_harvestState = HARVEST_IDLE;
			}
			break;

		case HARVEST_RETURN:
			if (!isIdle())
				break;
			if (distanceSquared(position, m_harvestDropPoint) <= range * range)
			{
				m_harvestState = HARVEST_DEPOSIT;
				m_harvestDeadline = frame + data->m_centerDelay;
			}
			else
			{
				m_harvestState = HARVEST_IDLE;
				m_harvestDeadline = frame + LOGICFRAMES_PER_SECOND;
			}
			break;

		case HARVEST_DEPOSIT:
			if (frame >= m_harvestDeadline)
			{
				if (Object* depot = TheGameLogic->findObjectByID(m_harvestDepot);
					harvestDepot(depot, player) && distanceSquared(position, m_harvestDropPoint) <= range * range)
				{
					const UnsignedInt amount = m_numberBoxes * player->getSupplyBoxValue();
					player->getMoney()->deposit(amount);
					player->getScoreKeeper()->addMoneyEarned(amount);
					RetailWorldUI::showPayout(*depot, amount, FALSE);
					m_numberBoxes = 0;
					worker->clearModelConditionState(MODELCONDITION_CARRYING);
				}
				m_harvestState = HARVEST_IDLE;
			}
			break;
	}
	return TRUE;
}

void WorkerAIUpdate::xferRetailHarvest(Xfer* xfer)
{
	xfer->xferBool(&m_harvestActive);
	xfer->xferBool(&m_harvestHasOrigin);
	xfer->xferInt(&m_harvestState);
	xfer->xferUnsignedInt(&m_harvestDeadline);
	xfer->xferObjectID(&m_harvestDepot);
	xfer->xferCoord3D(&m_harvestOrigin);
	xfer->xferCoord3D(&m_harvestTree);
	xfer->xferCoord3D(&m_harvestDropPoint);
}
