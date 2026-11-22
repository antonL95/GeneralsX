#include "PreRTS.h"

#include "GameLogic/Module/RubbleRiseUpdate.h"

#include <algorithm>

#include "GameClient/Drawable.h"
#include "GameClient/FXList.h"
#include "Common/GlobalData.h"
#include "Common/INI.h"
#include "Common/RandomValue.h"
#include "Common/RetailFileLayers.h"
#include "Common/RetailSaveBlocks.h"
#include "Common/Xfer.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"

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
	const auto phase = INI::scanIndexList(ini->getNextToken(), TheRubbleRisePhaseNames);
	for (const char* token = ini->getNextToken(); token; token = ini->getNextTokenOrNull())
		self->m_fx[phase].push_back(TheFXListStore->findFXList(token));
}

UnsignedInt randomDelay(UnsignedInt first, UnsignedInt last)
{
	if (first >= last)
		return first;
	return GameLogicRandomValue(first, last);
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

void RubbleRiseUpdate::onObjectCreated()
{
	if (!RetailFileLayers::enabled() || !getObject()->testStatus(OBJECT_STATUS_RECONSTRUCTING))
		return;
	const auto* data = getRubbleRiseUpdateModuleData();
	m_riseFrame = TheGameLogic->getFrame() + randomDelay(data->m_minRubbleRiseDelay, data->m_maxRubbleRiseDelay);
	m_height = -std::max(0.0f, getObject()->getGeometryInfo().getMaxHeightAbovePosition() - data->m_rubbleHeight);
	m_speed = 0.0f;
	m_rising = TRUE;
	setWakeFrame(getObject(), UPDATE_SLEEP_NONE);
}

void RubbleRiseUpdate::playPhase(StructureCollapsePhaseType phase) const
{
	for (const FXList* effect : getRubbleRiseUpdateModuleData()->m_fx[phase])
		if (effect)
			FXList::doFXPos(effect, getObject()->getPosition());
}

void RubbleRiseUpdate::setVisualOffset(Real x, Real y) const
{
	if (Drawable* drawable = getObject()->getDrawable())
	{
		Matrix3D transform = *drawable->getInstanceMatrix();
		transform.Set_Translation(Vector3(x, y, m_height));
		drawable->setInstanceMatrix(&transform);
	}
}

UpdateSleepTime RubbleRiseUpdate::update()
{
	if (!RetailFileLayers::enabled() || !m_rising)
		return UPDATE_SLEEP_FOREVER;

	const auto* data = getRubbleRiseUpdateModuleData();
	const auto frame = TheGameLogic->getFrame();
	if (!getObject()->testStatus(OBJECT_STATUS_RECONSTRUCTING) || getObject()->isEffectivelyDead())
		m_height = 0.0f;
	else if (frame >= m_riseFrame)
	{
		if (!m_burstFrame)
		{
			playPhase(SCPHASE_INITIAL);
			playPhase(SCPHASE_BURST);
			m_burstFrame = frame + std::max(1u, randomDelay(data->m_minBurstDelay, data->m_maxBurstDelay));
		}
		else if (frame >= m_burstFrame)
		{
			const Bool large = data->m_bigBurstFrequency > 0 && GameLogicRandomValue(1, data->m_bigBurstFrequency) == 1;
			playPhase(large ? SCPHASE_BURST : SCPHASE_DELAY);
			m_burstFrame = frame + std::max(1u, randomDelay(data->m_minBurstDelay, data->m_maxBurstDelay));
		}
		const Real acceleration = -TheGlobalData->m_gravity * (1.0f - std::clamp(data->m_rubbleRiseDamping, 0.0f, 1.0f));
		m_speed += acceleration;
		m_height = m_speed > 0.0f ? std::min(0.0f, m_height + m_speed) : 0.0f;
	}

	if (m_height >= 0.0f)
	{
		playPhase(SCPHASE_FINAL);
		setVisualOffset(0.0f, 0.0f);
		m_rising = FALSE;
		return UPDATE_SLEEP_FOREVER;
	}

	const Real shudder = data->m_maxShudder;
	const Real x = GameClientRandomValueReal(-shudder, shudder);
	const Real y = GameClientRandomValueReal(-shudder, shudder);
	setVisualOffset(x, y);
	return UPDATE_SLEEP_NONE;
}

void RubbleRiseUpdate::crc(Xfer* xfer)
{
	UpdateModule::crc(xfer);
	if (RetailFileLayers::enabled())
		xferRetailBlocks(xfer, TRUE);
}

void RubbleRiseUpdate::xfer(Xfer* xfer)
{
	const XferVersion currentVersion = RetailFileLayers::enabled() ? 2 : 1;
	XferVersion version = currentVersion;
	xfer->xferVersion(&version, currentVersion);
	UpdateModule::xfer(xfer);
	if (RetailFileLayers::enabled())
		xferRetailBlocks(xfer, version >= 2);
}

void RubbleRiseUpdate::xferRetailBlocks(Xfer* xfer, Bool present)
{
	RetailSaveBlocks::transferSection<Xfer>(xfer, xfer->getXferMode() == XFER_LOAD, present, {
		{"rubble-rise", [this](Xfer* block) {
			block->xferUnsignedInt(&m_riseFrame);
			block->xferUnsignedInt(&m_burstFrame);
			block->xferReal(&m_height);
			block->xferReal(&m_speed);
			block->xferBool(&m_rising);
		}, [this] {
			m_riseFrame = 0;
			m_burstFrame = 0;
			m_height = 0.0f;
			m_speed = 0.0f;
			m_rising = FALSE;
		}}
	});
}

void RubbleRiseUpdate::loadPostProcess()
{
	UpdateModule::loadPostProcess();
	if (RetailFileLayers::enabled() && m_rising)
		setVisualOffset(0.0f, 0.0f);
}
