#include "PreRTS.h"

#include <algorithm>

#include "Common/INIAudit.h"
#include "Common/Player.h"
#include "Common/RetailFileLayers.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "GameLogic/Module/CostModifierUpgrade.h"
#include "GameLogic/Object.h"

namespace
{
const CostModifierUpgradeModuleData* activeRetailModifier(const BehaviorModule* module)
{
	static const NameKeyType key = NAMEKEY("CostModifierUpgrade");
	if (module->getModuleNameKey() != key)
		return nullptr;

	return static_cast<const CostModifierUpgrade*>(module)->activeRetailData();
}

struct Discount
{
	const CostModifierUpgradeModuleData* data;
	std::size_t count;

	Real change() const
	{
		const auto& table = data->m_retailPercentages;
		return table[std::min(count, table.size()) - 1];
	}
};

template<typename Matches>
std::vector<Discount> collectDiscounts(const Player& player, Matches matches)
{
	struct Query
	{
		Matches matches;
		std::vector<Discount> discounts;
	} query{matches, {}};

	player.iterateObjects([](Object* object, void* context)
	{
		if (object->isDestroyed() || object->isEffectivelyDead() ||
			object->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION) || object->testStatus(OBJECT_STATUS_SOLD))
			return;
		auto& query = *static_cast<Query*>(context);
		for (BehaviorModule** module = object->getBehaviorModules(); *module; ++module)
		{
			const auto* data = activeRetailModifier(*module);
			if (!data || !query.matches(*data))
				continue;
			const auto found = std::find_if(query.discounts.begin(), query.discounts.end(),
				[data](const Discount& entry) { return entry.data == data; });
			if (found == query.discounts.end())
				query.discounts.push_back({data, 1});
			else
				++found->count;
		}
	}, &query);
	return query.discounts;
}

Real costFactor(const std::vector<Discount>& discounts)
{
	Real factor = 1.0f;
	for (const Discount& discount : discounts)
		factor *= std::max(0.0f, 1.0f + discount.change());
	return factor;
}
}

void CostModifierUpgradeModuleData::addRetailFields(MultiIniFieldParse& p)
{
	if (!RetailFileLayers::enabled() && !INIAudit::isEnabled())
		return;

	static const FieldParse fields[] = {
		{ "StartsActive", INI::parseBool, nullptr, offsetof(CostModifierUpgradeModuleData, m_retailStartsActive) },
		{ "ObjectFilter", SpecialPowerObjectFilter::parse, nullptr, offsetof(CostModifierUpgradeModuleData, m_retailFilter) },
		{ "LabelForPalantirString", INI::parseAsciiString, nullptr, offsetof(CostModifierUpgradeModuleData, m_retailLabel) },
		{ "UpgradeDiscount", INI::parseBool, nullptr, offsetof(CostModifierUpgradeModuleData, m_retailUpgradeDiscount) },
		{ "ApplyToTheseUpgrades", INI::parseAsciiStringVector, nullptr, offsetof(CostModifierUpgradeModuleData, m_retailUpgrades) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

void CostModifierUpgradeModuleData::parsePercentage(INI* ini, void* instance, void*, const void*)
{
	auto& data = *static_cast<CostModifierUpgradeModuleData*>(instance);
	INI::parsePercentToReal(ini, nullptr, &data.m_percentage, nullptr);
	if (RetailFileLayers::enabled() || INIAudit::isEnabled())
		data.m_retailPercentages.push_back(data.m_percentage);
}

Bool CostModifierUpgradeModuleData::usesRetailDiscount() const
{
	return RetailFileLayers::enabled() && !m_retailPercentages.empty() &&
		(m_retailStartsActive || m_retailUpgradeDiscount || m_retailFilter.m_rules ||
		m_retailFilter.m_includeKinds.any() || m_retailFilter.m_excludeKinds.any() ||
		!m_retailFilter.m_includeObjects.empty() || !m_retailFilter.m_excludeObjects.empty());
}

const CostModifierUpgradeModuleData* CostModifierUpgrade::activeRetailData() const
{
	const auto* data = getCostModifierUpgradeModuleData();
	return data->usesRetailDiscount() && (data->m_retailStartsActive || isAlreadyUpgraded()) ? data : nullptr;
}

Real CostModifierUpgrade::retailBuildFactor(const Player& player, const ThingTemplate& type)
{
	return costFactor(collectDiscounts(player, [&](const CostModifierUpgradeModuleData& data)
	{
		return !data.m_retailUpgradeDiscount && data.m_retailFilter.matchesTemplate(type);
	}));
}

Real CostModifierUpgrade::retailUpgradeFactor(const Player& player, const UpgradeTemplate& upgrade)
{
	return costFactor(collectDiscounts(player, [&](const CostModifierUpgradeModuleData& data)
	{
		return data.m_retailUpgradeDiscount && (data.m_retailUpgrades.empty() ||
			std::any_of(data.m_retailUpgrades.begin(), data.m_retailUpgrades.end(), [&](const AsciiString& name)
			{
				return name.compareNoCase(upgrade.getUpgradeName()) == 0;
			}));
	}));
}

Bool CostModifierUpgrade::retailLabel(const Object& object, AsciiString& label, Real& discount)
{
	const Player* player = object.getControllingPlayer();
	if (!player)
		return FALSE;
	for (BehaviorModule** module = object.getBehaviorModules(); *module; ++module)
	{
		const auto* data = activeRetailModifier(*module);
		if (!data || data->m_retailLabel.isEmpty() || data->m_retailLabel.compareNoCase("GUI:Blank") == 0)
			continue;
		const auto discounts = collectDiscounts(*player, [data](const CostModifierUpgradeModuleData& other)
		{
			return &other == data;
		});
		if (discounts.empty())
			continue;
		label = data->m_retailLabel;
		discount = -discounts.front().change();
		return TRUE;
	}
	return FALSE;
}
