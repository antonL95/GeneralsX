#include "PreRTS.h"

#include "GameLogic/Module/RubbleRiseUpdate.h"

#include "GameClient/FXList.h"
#include "Common/INI.h"
#include "Common/Xfer.h"

namespace
{
const char* TheRubbleRisePhaseNames[] =
{
	"INITIAL",
	"DELAY",
	"BURST",
	"FINAL",
	"ALMOST_FINAL",
	nullptr
};

void parseRubbleFX(INI* ini, void* instance, void*, const void*)
{
	RubbleRiseUpdateModuleData* self = static_cast<RubbleRiseUpdateModuleData*>(instance);
	INI::scanIndexList(ini->getNextToken(), TheRubbleRisePhaseNames);
	for (const char* token = ini->getNextToken(); token; token = ini->getNextTokenOrNull())
		self->m_fx.push_back(TheFXListStore->findFXList(token));
}
}

void RubbleRiseUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpdateModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "MinRubbleRiseDelay", INI::parseDurationUnsignedInt, nullptr, offsetof(RubbleRiseUpdateModuleData, m_minRubbleRiseDelay) },
		{ "MaxRubbleRiseDelay", INI::parseDurationUnsignedInt, nullptr, offsetof(RubbleRiseUpdateModuleData, m_maxRubbleRiseDelay) },
		{ "RubbleRiseDamping", INI::parseReal, nullptr, offsetof(RubbleRiseUpdateModuleData, m_rubbleRiseDamping) },
		{ "RubbleHeight", INI::parseReal, nullptr, offsetof(RubbleRiseUpdateModuleData, m_rubbleHeight) },
		{ "MaxShudder", INI::parseReal, nullptr, offsetof(RubbleRiseUpdateModuleData, m_maxShudder) },
		{ "MinBurstDelay", INI::parseDurationUnsignedInt, nullptr, offsetof(RubbleRiseUpdateModuleData, m_minBurstDelay) },
		{ "MaxBurstDelay", INI::parseDurationUnsignedInt, nullptr, offsetof(RubbleRiseUpdateModuleData, m_maxBurstDelay) },
		{ "BigBurstFrequency", INI::parseInt, nullptr, offsetof(RubbleRiseUpdateModuleData, m_bigBurstFrequency) },
		{ "FXList", parseRubbleFX, nullptr, 0 },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

RubbleRiseUpdate::RubbleRiseUpdate(Thing* thing, const ModuleData* data) : UpdateModule(thing, data)
{
	setWakeFrame(getObject(), UPDATE_SLEEP_FOREVER);
}

RubbleRiseUpdate::~RubbleRiseUpdate() = default;

UpdateSleepTime RubbleRiseUpdate::update()
{
	return UPDATE_SLEEP_FOREVER;
}

void RubbleRiseUpdate::crc(Xfer* xfer)
{
	UpdateModule::crc(xfer);
}

void RubbleRiseUpdate::xfer(Xfer* xfer)
{
	XferVersion version = 1;
	xfer->xferVersion(&version, 1);
	UpdateModule::xfer(xfer);
}

void RubbleRiseUpdate::loadPostProcess()
{
	UpdateModule::loadPostProcess();
}
