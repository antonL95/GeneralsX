#include "PreRTS.h"

#include "GameLogic/Module/HordeWorkerAIUpdate.h"

#include "Common/INI.h"

void HordeWorkerAIUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	WorkerAIUpdateModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "ComboLocomotorSet", INI::parseAsciiString, nullptr, offsetof(HordeWorkerAIUpdateModuleData, m_comboLocomotorSet) },
		{ "ComboLocoAttackDistance", INI::parseInt, nullptr, offsetof(HordeWorkerAIUpdateModuleData, m_comboLocoAttackDistance) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

HordeWorkerAIUpdate::HordeWorkerAIUpdate(Thing* thing, const ModuleData* data) : WorkerAIUpdate(thing, data)
{
}

HordeWorkerAIUpdate::~HordeWorkerAIUpdate() = default;

void HordeWorkerAIUpdate::crc(Xfer* xfer)
{
	WorkerAIUpdate::crc(xfer);
}

void HordeWorkerAIUpdate::xfer(Xfer* xfer)
{
	WorkerAIUpdate::xfer(xfer);
}

void HordeWorkerAIUpdate::loadPostProcess()
{
	WorkerAIUpdate::loadPostProcess();
}
