#include "PreRTS.h"

#include "GameLogic/Module/HordeWorkerAIUpdate.h"

#include "Common/ActionManager.h"
#include "Common/INI.h"
#include "Common/RetailFileLayers.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/Module/DozerAIUpdate.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Object.h"

void HordeWorkerAIUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	HordeAIUpdateModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "SpecialContactPoints", ThingTemplate::parseContactPointFlags, nullptr, offsetof(HordeWorkerAIUpdateModuleData, m_specialContactPoints) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

HordeWorkerAIUpdate::HordeWorkerAIUpdate(Thing* thing, const ModuleData* data) : HordeAIUpdate(thing, data)
{
}

HordeWorkerAIUpdate::~HordeWorkerAIUpdate() = default;

const HordeWorkerAIUpdate* HordeWorkerAIUpdate::find(const Object* object)
{
	return RetailFileLayers::enabled() && object
		? static_cast<const HordeWorkerAIUpdate*>(object->findUpdateModule(NAMEKEY("HordeWorkerAIUpdate")))
		: nullptr;
}

void HordeWorkerAIUpdate::aiDoCommand(const AICommandParms* parms)
{
	if (!RetailFileLayers::enabled() || parms->m_cmd != AICMD_REPAIR)
	{
		HordeAIUpdate::aiDoCommand(parms);
		return;
	}
	if (!isAllowedToRespondToAiCommands(parms) ||
		!TheActionManager->canRepairObject(getObject(), parms->m_obj, parms->m_cmdSource))
		return;
	const HordeContain* contain = HordeContain::get(getObject());
	const auto* members = contain ? contain->getContainedItemsList() : nullptr;
	if (!members)
		return;
	for (Object* member : *members)
	{
		auto* ai = member->getAIUpdateInterface();
		auto* builder = ai ? ai->getDozerAIInterface() : nullptr;
		if (builder && !member->isEffectivelyDead())
			builder->newTask(DOZER_TASK_REPAIR, parms->m_obj);
	}
}

void HordeWorkerAIUpdate::crc(Xfer* xfer)
{
	HordeAIUpdate::crc(xfer);
}

void HordeWorkerAIUpdate::xfer(Xfer* xfer)
{
	HordeAIUpdate::xfer(xfer);
}

void HordeWorkerAIUpdate::loadPostProcess()
{
	HordeAIUpdate::loadPostProcess();
}

RepairSpecialPower::RepairSpecialPower(Thing* thing, const ModuleData* data) : SpecialPowerModule(thing, data)
{
}

RepairSpecialPower::~RepairSpecialPower() = default;

ModuleData* RepairSpecialPower::friend_newModuleData(INI* ini)
{
	return SpecialPowerModule::friend_newModuleData(ini);
}

void RepairSpecialPower::doSpecialPowerAtObject(Object* target, UnsignedInt)
{
	Object* source = getObject();
	if (RetailFileLayers::enabled() && TheActionManager->canRepairObject(source, target, CMD_FROM_PLAYER))
		if (auto* ai = source->getAIUpdateInterface())
			ai->aiRepair(target, CMD_FROM_PLAYER);
}

void RepairSpecialPower::crc(Xfer* xfer)
{
	SpecialPowerModule::crc(xfer);
}

void RepairSpecialPower::xfer(Xfer* xfer)
{
	SpecialPowerModule::xfer(xfer);
}

void RepairSpecialPower::loadPostProcess()
{
	SpecialPowerModule::loadPostProcess();
}
