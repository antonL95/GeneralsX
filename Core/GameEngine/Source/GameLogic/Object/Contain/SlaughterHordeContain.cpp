#include "PreRTS.h"

#include <algorithm>
#include <vector>

#include "Common/Player.h"
#include "Common/GameAudio.h"
#include "Common/RetailFileLayers.h"
#include "Common/ThingTemplate.h"
#include "GameClient/RetailWorldUI.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/CostModifierUpgrade.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Module/SlaughterHordeContain.h"
#include "GameLogic/Object.h"
#include "WWMath/wwmath.h"

void SlaughterHordeContainModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	OpenContainModuleData::buildFieldParse(p);
	static const FieldParse fields[] = {
		{ "PassengerFilter", SpecialPowerObjectFilter::parse, nullptr, offsetof(SlaughterHordeContainModuleData, m_passengers) },
		{ "ObjectStatusOfContained", ObjectStatusMaskType::parseFromINI, nullptr, offsetof(SlaughterHordeContainModuleData, m_passengerStatus) },
		{ "CashBackPercent", INI::parsePercentToReal, nullptr, offsetof(SlaughterHordeContainModuleData, m_cashBack) },
		{ "MaxHordeCapacity", INI::parseInt, nullptr, offsetof(SlaughterHordeContainModuleData, m_maxHordeCapacity) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

SlaughterHordeContain::SlaughterHordeContain(Thing* thing, const ModuleData* data) : OpenContain(thing, data)
{
}

SlaughterHordeContain::~SlaughterHordeContain() = default;

Bool SlaughterHordeContain::canSlaughter(const Object* passenger) const
{
	return passenger && !passenger->isEffectivelyDead() && !passenger->isDestroyed() &&
		OpenContain::isValidContainerFor(passenger, FALSE) &&
		getSlaughterHordeContainModuleData()->m_passengers.matchesObject(getObject(), passenger);
}

Bool SlaughterHordeContain::isValidContainerFor(const Object* passenger, Bool checkCapacity) const
{
	if (!RetailFileLayers::enabled() || !passenger || passenger == getObject() ||
		passenger->isEffectivelyDead() || passenger->isDestroyed() ||
		getObject()->isEffectivelyDead() || getObject()->isDestroyed() ||
		getObject()->testStatus(OBJECT_STATUS_SOLD) || getObject()->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION))
		return FALSE;

	const auto* data = getSlaughterHordeContainModuleData();
	const auto* horde = HordeContain::get(passenger);
	const Int count = horde ? static_cast<Int>(horde->getContainCount()) : 1;
	if (checkCapacity && ((data->m_containMax >= 0 && count > data->m_containMax) ||
		(horde && data->m_maxHordeCapacity > 0 && count > data->m_maxHordeCapacity)))
		return FALSE;

	if (!horde)
		return canSlaughter(passenger);
	const auto* members = horde->getContainedItemsList();
	return std::any_of(members->begin(), members->end(), [this](const Object* member) { return canSlaughter(member); });
}

void SlaughterHordeContain::slaughter(Object* passenger)
{
	const auto* data = getSlaughterHordeContainModuleData();
	const Int bounty = passenger->getTemplate()->getRotWKObjectData().bountyValue;
	Player* player = passenger->getControllingPlayer();
	const Real baseRefund = WWMath::Ceil(static_cast<Real>(bounty) * data->m_cashBack);
	const Real refund = baseRefund > 0.0f ?
		WWMath::Ceil(baseRefund * CostModifierUpgrade::retailSlaughterFactor(*player)) : 0.0f;
	if (refund > 0.0f)
	{
		const auto amount = static_cast<UnsignedInt>(refund);
		player->getMoney()->deposit(amount);
		player->getScoreKeeper()->addMoneyEarned(amount);
		RetailWorldUI::showPayout(*passenger, amount, FALSE);
		getObject()->getExperienceTracker()->addRetailExperience(refund, FALSE);
	}
	passenger->setStatus(data->m_passengerStatus);
	DamageInfo damage;
	damage.in.m_sourceID = INVALID_ID;
	damage.in.m_damageType = DAMAGE_UNRESISTABLE;
	damage.in.m_deathType = DEATH_SLAUGHTERED;
	damage.in.m_amount = passenger->getBodyModule()->getMaxHealth();
	damage.in.m_kill = TRUE;
	passenger->attemptDamage(&damage);
}

void SlaughterHordeContain::addToContain(Object* passenger)
{
	if (!isValidContainerFor(passenger, TRUE))
		return;

	AudioEventRTS sound = getSlaughterHordeContainModuleData()->m_enterSound;
	if (!sound.getEventName().isEmpty())
	{
		sound.setObjectID(getObject()->getID());
		TheAudio->addAudioEvent(&sound);
	}

	if (const auto* horde = HordeContain::get(passenger))
	{
		const auto* contents = horde->getContainedItemsList();
		std::vector<Object*> members(contents->begin(), contents->end());
		for (Object* member : members)
			if (canSlaughter(member))
				slaughter(member);
	}
	else
		slaughter(passenger);
}

void SlaughterHordeContain::crc(Xfer* xfer)
{
	OpenContain::crc(xfer);
}

void SlaughterHordeContain::xfer(Xfer* xfer)
{
	OpenContain::xfer(xfer);
}

void SlaughterHordeContain::loadPostProcess()
{
	OpenContain::loadPostProcess();
}
