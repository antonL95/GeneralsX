#include "PreRTS.h"

#include "GameLogic/Module/WeaponChangeSpecialPowerModule.h"

#include "Common/INI.h"
#include "Common/INIException.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Object.h"

#if !RTS_GENERALS
#include "GameLogic/Module/AttributeModifierHelper.h"
#endif

void WeaponChangeSpecialPowerModuleData::buildFieldParse(MultiIniFieldParse& fields)
{
	SpecialPowerModuleData::buildFieldParse(fields);
	static const FieldParse table[] =
	{
		{ "FlagsUsedForToggle", WeaponSetFlags::parseFromINI, nullptr, offsetof(WeaponChangeSpecialPowerModuleData, m_toggleFlags) },
		{ "ToggleOnSleepFrames", INI::parseUnsignedInt, nullptr, offsetof(WeaponChangeSpecialPowerModuleData, m_onSleepFrames) },
		{ "ToggleOffSleepFrames", INI::parseUnsignedInt, nullptr, offsetof(WeaponChangeSpecialPowerModuleData, m_offSleepFrames) },
		{ "ToggleOnAttributeModifier", INI::parseAsciiString, nullptr, offsetof(WeaponChangeSpecialPowerModuleData, m_onModifier) },
		{ "ToggleOffAttributeModifier", INI::parseAsciiString, nullptr, offsetof(WeaponChangeSpecialPowerModuleData, m_offModifier) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	fields.add(table);
}

ModuleData* WeaponChangeSpecialPowerModule::friend_newModuleData(INI* ini)
{
	auto data = std::unique_ptr<WeaponChangeSpecialPowerModuleData>(MSGNEW("AllModuleData") WeaponChangeSpecialPowerModuleData);
	if (ini)
	{
		ini->initFromINIMultiProc(data.get(), WeaponChangeSpecialPowerModuleData::buildFieldParse);
		if (!data->m_specialPowerTemplate)
			throw INIException("WeaponChangeSpecialPowerModule requires a defined SpecialPowerTemplate");
	}
	return data.release();
}

WeaponChangeSpecialPowerModule::~WeaponChangeSpecialPowerModule() = default;

void WeaponChangeSpecialPowerModule::doSpecialPowerAtObject(Object*, UnsignedInt commandOptions)
{
	doSpecialPower(commandOptions);
}

void WeaponChangeSpecialPowerModule::doSpecialPowerAtLocation(const Coord3D*, Real, UnsignedInt commandOptions)
{
	doSpecialPower(commandOptions);
}

void WeaponChangeSpecialPowerModule::applyPowerEffect(const Coord3D*)
{
	const auto* data = static_cast<const WeaponChangeSpecialPowerModuleData*>(getModuleData());
	Object* caster = getObject();
	WeaponSetFlags turningOn = data->m_toggleFlags;
	turningOn.clear(caster->getWeaponSetFlags());
	WeaponSetFlags turningOff = data->m_toggleFlags;
	turningOff.clear(turningOn);
	const Bool enabling = turningOn.any();
	const UnsignedInt sleep = enabling ? data->m_onSleepFrames : data->m_offSleepFrames;
	const UnsignedInt stopUntil = TheGameLogic->getFrame() + sleep;
#if !RTS_GENERALS
	const auto modifierOwner = AttributeModifiers::ownerOf(caster, getModuleTagNameKey());
	const AsciiString& previousModifier = enabling ? data->m_offModifier : data->m_onModifier;
	const AsciiString& nextModifier = enabling ? data->m_onModifier : data->m_offModifier;
#endif

	const auto change = [&](Object* unit)
	{
		if (unit->isEffectivelyDead())
			return;
		unit->setWeaponSetFlags(turningOff, FALSE);
		unit->setWeaponSetFlags(turningOn, TRUE);
#if !RTS_GENERALS
		if (!previousModifier.isEmpty())
			AttributeModifiers::remove(unit, previousModifier, modifierOwner);
		if (sleep != 0 && !nextModifier.isEmpty())
			AttributeModifiers::apply(unit, nextModifier, sleep, 0, modifierOwner);
#endif
		if (enabling)
			unit->setPowerModelCondition(MODELCONDITION_WEAPON_TOGGLING, sleep);
		if (sleep != 0 && !unit->isDisabledByType(DISABLED_PARALYZED))
			unit->setDisabledUntil(DISABLED_PARALYZED, stopUntil);
	};

	change(caster);
	const ContainModuleInterface* contain = caster->getContain();
	if (!caster->isKindOf(KINDOF_HORDE) || !contain)
		return;
	if (const ContainedItemsList* members = contain->getContainedItemsList())
	{
		for (Object* member : *members)
			change(member);
	}
}
