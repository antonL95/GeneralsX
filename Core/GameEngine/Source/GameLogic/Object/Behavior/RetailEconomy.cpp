#include "PreRTS.h"

#include <limits>

#include "Common/BuildAssistant.h"
#include "Common/INI.h"
#include "Common/Player.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "Common/Xfer.h"
#include "GameClient/RetailWorldUI.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/RetailEconomy.h"
#include "GameLogic/Object.h"
#include "WWMath/wwmath.h"

Int RetailEconomy::applyDepositUpgrade(const Player& player, const AsciiString& name, Real multiplier, Int amount)
{
	const UpgradeTemplate* upgrade = name.isEmpty() ? nullptr : TheUpgradeCenter->findUpgrade(name);
	if (!upgrade || !player.hasUpgradeComplete(upgrade))
		return amount;

	const Real scaled = amount * multiplier;
	if (!(scaled > 0.0f))
		return 0;
	if (scaled >= static_cast<Real>(std::numeric_limits<Int>::max()))
		return std::numeric_limits<Int>::max();
	return static_cast<Int>(scaled);
}

void PillageModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	BehaviorModuleData::buildFieldParse(p);
	static const FieldParse fields[] = {
		{ "PillageAmount", INI::parseUnsignedInt, nullptr, offsetof(PillageModuleData, m_amount) },
		{ "NumDamageEventsPerPillage", INI::parseUnsignedInt, nullptr, offsetof(PillageModuleData, m_eventsPerPillage) },
		{ "PillageFilter", SpecialPowerObjectFilter::parse, nullptr, offsetof(PillageModuleData, m_filter) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

PillageModule::~PillageModule() = default;

void PillageModule::onDamageDealt(const Object& source, const Object& victim)
{
	static const NameKeyType key = NAMEKEY("PillageModule");
	for (BehaviorModule** module = source.getBehaviorModules(); *module; ++module)
		if ((*module)->getModuleNameKey() == key)
			static_cast<PillageModule*>(*module)->pillage(victim);
}

void PillageModule::pillage(const Object& victim)
{
	const auto* data = getPillageModuleData();
	const Object* source = getObject();
	Player* attacker = source->getControllingPlayer();
	Player* defender = victim.getControllingPlayer();
	if (!attacker || !defender || attacker == defender || !data->m_filter.matchesObject(source, &victim))
		return;
	if (++m_damageEvents < data->m_eventsPerPillage)
		return;
	m_damageEvents = 0;

	const UnsignedInt amount = defender->getMoney()->withdraw(data->m_amount, FALSE);
	attacker->getMoney()->deposit(amount, FALSE);
	RetailWorldUI::showPayout(*source, amount, FALSE);
}

void PillageModule::crc(Xfer* xfer)
{
	BehaviorModule::crc(xfer);
	xfer->xferUnsignedInt(&m_damageEvents);
}

void PillageModule::xfer(Xfer* xfer)
{
	XferVersion version = 1;
	xfer->xferVersion(&version, 1);
	BehaviorModule::xfer(xfer);
	xfer->xferUnsignedInt(&m_damageEvents);
}

void PillageModule::loadPostProcess()
{
	BehaviorModule::loadPostProcess();
}

void RefundDieModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	DieModuleData::buildFieldParse(p);
	static const FieldParse fields[] = {
		{ "UpgradeRequired", INI::parseAsciiString, nullptr, offsetof(RefundDieModuleData, m_upgrade) },
		{ "BuildingRequired", [](INI* ini, void* instance, void* store, const void* data) {
			auto& building = static_cast<std::optional<SpecialPowerObjectFilter>*>(store)->emplace();
			SpecialPowerObjectFilter::parse(ini, instance, &building, data);
		}, nullptr, offsetof(RefundDieModuleData, m_building) },
		{ "RefundPercent", INI::parsePercentToReal, nullptr, offsetof(RefundDieModuleData, m_percent) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

RefundDie::~RefundDie() = default;

void RefundDie::onDie(const DamageInfo* damageInfo)
{
	const Object* structure = getObject();
	const auto* data = getRefundDieModuleData();
	Player* owner = structure->getControllingPlayer();
	const Int paid = structure->getConstructionCostPaid();
	if (!owner || paid <= 0 || !(data->m_percent > 0.0f) || !isDieApplicable(damageInfo) ||
		structure->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION) || structure->testStatus(OBJECT_STATUS_SOLD))
		return;

	if (data->m_upgrade.isNotEmpty())
	{
		const auto* upgrade = TheUpgradeCenter->findUpgrade(data->m_upgrade);
		if (!upgrade || !owner->hasUpgradeComplete(upgrade))
			return;
	}

	if (data->m_building)
	{
		const Object* candidate = TheGameLogic->getFirstObject();
		for (; candidate; candidate = candidate->getNextObject())
			if (candidate != structure && candidate->getControllingPlayer() == owner && !candidate->isDestroyed() &&
				!candidate->isEffectivelyDead() && candidate->getConstructionPercent() == CONSTRUCTION_COMPLETE &&
				data->m_building->matchesObject(structure, candidate))
				break;
		if (!candidate)
			return;
	}

	const Real refund = paid * data->m_percent;
	const Int amount = refund >= static_cast<Real>(std::numeric_limits<Int>::max())
		? std::numeric_limits<Int>::max() : static_cast<Int>(WWMath::Ceil(refund));
	owner->getMoney()->deposit(amount, FALSE, FALSE);
	RetailWorldUI::showPayout(*structure, amount, FALSE);
}

void RefundDie::crc(Xfer* xfer)
{
	DieModule::crc(xfer);
}

void RefundDie::xfer(Xfer* xfer)
{
	XferVersion version = 1;
	xfer->xferVersion(&version, 1);
	DieModule::xfer(xfer);
}

void RefundDie::loadPostProcess()
{
	DieModule::loadPostProcess();
}
