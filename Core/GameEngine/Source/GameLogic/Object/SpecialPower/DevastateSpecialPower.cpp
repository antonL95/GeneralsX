#include "PreRTS.h"

#include "GameLogic/Module/DevastateSpecialPower.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "Common/GlobalData.h"
#include "Common/INI.h"
#include "Common/INIException.h"
#include "Common/Player.h"
#include "GameClient/FXList.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Weapon.h"

void DevastateSpecialPowerModuleData::buildFieldParse(MultiIniFieldParse& fields)
{
	SpecialPowerModuleData::buildFieldParse(fields);
	static const FieldParse table[] =
	{
		{ "Radius", INI::parseReal, nullptr, offsetof(DevastateSpecialPowerModuleData, m_radius) },
		{ "TreeValueMultiplier", INI::parsePercentToReal, nullptr, offsetof(DevastateSpecialPowerModuleData, m_treeValueMultiplier) },
		{ "TreeValueTotalCap", INI::parseReal, nullptr, offsetof(DevastateSpecialPowerModuleData, m_treeValueTotalCap) },
		{ "FX", INI::parseAsciiString, nullptr, offsetof(DevastateSpecialPowerModuleData, m_fx) },
		{ "FireWeapon", INI::parseAsciiString, nullptr, offsetof(DevastateSpecialPowerModuleData, m_fireWeapon) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	fields.add(table);
}

ModuleData* DevastateSpecialPower::friend_newModuleData(INI* ini)
{
	auto data = std::unique_ptr<DevastateSpecialPowerModuleData>(MSGNEW("AllModuleData") DevastateSpecialPowerModuleData);
	if (ini)
	{
		ini->initFromINIMultiProc(data.get(), DevastateSpecialPowerModuleData::buildFieldParse);
		if (!data->m_specialPowerTemplate)
			throw INIException("DevastateSpecialPower requires a defined SpecialPowerTemplate");
	}
	return data.release();
}

DevastateSpecialPower::~DevastateSpecialPower() = default;

void DevastateSpecialPower::applyPowerEffect(const Coord3D* location)
{
	Object* source = getObject();
	Player* player = source->getControllingPlayer();
	if (!player || !location)
		return;

	const auto* data = static_cast<const DevastateSpecialPowerModuleData*>(getModuleData());
	if (TheTerrainLogic && data->m_radius > 0.0f && std::isfinite(data->m_radius))
	{
		const FXList* fx = TheFXListStore && !data->m_fx.isEmpty() ? TheFXListStore->findFXList(data->m_fx.str()) : nullptr;
		const auto boxes = TheTerrainLogic->harvestMapTrees(*location, data->m_radius, fx);
		if (boxes > 0 && TheGlobalData->m_baseValuePerSupplyBox > 0 &&
			data->m_treeValueMultiplier > 0.0f && data->m_treeValueTotalCap > 0.0f)
		{
			const Real value = static_cast<Real>(boxes) * TheGlobalData->m_baseValuePerSupplyBox * data->m_treeValueMultiplier;
			const Real cappedValue = std::min(value, data->m_treeValueTotalCap);
			constexpr UnsignedInt maxPayout = std::numeric_limits<UnsignedInt>::max();
			const UnsignedInt payout = cappedValue >= static_cast<Real>(maxPayout) ? maxPayout : static_cast<UnsignedInt>(cappedValue);
			if (payout > 0)
				player->getMoney()->deposit(payout, TRUE, FALSE);
		}
	}

	if (TheWeaponStore && !data->m_fireWeapon.isEmpty())
	{
		if (const WeaponTemplate* weaponTemplate = TheWeaponStore->findWeaponTemplate(data->m_fireWeapon))
		{
			Weapon* weapon = TheWeaponStore->allocateNewWeapon(weaponTemplate, PRIMARY_WEAPON);
			weapon->loadAmmoNow(source);
			weapon->forceFireWeapon(source, location);
			deleteInstance(weapon);
		}
	}
}
