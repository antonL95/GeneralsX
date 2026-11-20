#include "PreRTS.h"

#include "GameLogic/Module/RotWKBodyModules.h"

#include "Common/Xfer.h"
#include "GameClient/FXList.h"
#include "GameLogic/Weapon.h"

void DelayedDeathBodyModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	RespawnBodyModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "DelayedDeathTime", INI::parseDurationUnsignedInt, nullptr, offsetof(DelayedDeathBodyModuleData, m_delayedDeathTime) },
		{ "ImmortalUntilDeathTime", INI::parseBool, nullptr, offsetof(DelayedDeathBodyModuleData, m_immortalUntilDeathTime) },
		{ "DoHealthCheck", INI::parseBool, nullptr, offsetof(DelayedDeathBodyModuleData, m_doHealthCheck) },
		{ "InvulnerableFX", INI::parseFXList, nullptr, offsetof(DelayedDeathBodyModuleData, m_invulnerableFX) },
		{ "DelayedDeathPrerequisiteUpgrade", INI::parseAsciiString, nullptr, offsetof(DelayedDeathBodyModuleData, m_delayedDeathPrerequisiteUpgrade) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

DelayedDeathBody::DelayedDeathBody(Thing* thing, const ModuleData* moduleData) : RespawnBody(thing, moduleData)
{
}

DelayedDeathBody::~DelayedDeathBody() = default;

void DelayedDeathBody::crc(Xfer* xfer)
{
	RespawnBody::crc(xfer);
}

void DelayedDeathBody::xfer(Xfer* xfer)
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion(&version, currentVersion);
	RespawnBody::xfer(xfer);
}

void DelayedDeathBody::loadPostProcess()
{
	RespawnBody::loadPostProcess();
}

void PorcupineFormationBodyModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	ActiveBodyModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "DamageWeaponTemplate", INI::parseWeaponTemplate, nullptr, offsetof(PorcupineFormationBodyModuleData, m_damageWeaponTemplate) },
		{ "CrushDamageWeaponTemplate", INI::parseWeaponTemplate, nullptr, offsetof(PorcupineFormationBodyModuleData, m_crushDamageWeaponTemplate) },
		{ "CrusherLevelResisted", INI::parseInt, nullptr, offsetof(PorcupineFormationBodyModuleData, m_crusherLevelResisted) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

PorcupineFormationBodyModule::PorcupineFormationBodyModule(Thing* thing, const ModuleData* moduleData) : ActiveBody(thing, moduleData)
{
}

PorcupineFormationBodyModule::~PorcupineFormationBodyModule() = default;

void PorcupineFormationBodyModule::crc(Xfer* xfer)
{
	ActiveBody::crc(xfer);
}

void PorcupineFormationBodyModule::xfer(Xfer* xfer)
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion(&version, currentVersion);
	ActiveBody::xfer(xfer);
}

void PorcupineFormationBodyModule::loadPostProcess()
{
	ActiveBody::loadPostProcess();
}

void SymbioticStructuresBodyModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	StructureBodyModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "Symbiote", INI::parseAsciiString, nullptr, offsetof(SymbioticStructuresBodyModuleData, m_symbiote) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

SymbioticStructuresBody::SymbioticStructuresBody(Thing* thing, const ModuleData* moduleData) : StructureBody(thing, moduleData)
{
}

SymbioticStructuresBody::~SymbioticStructuresBody() = default;

void SymbioticStructuresBody::crc(Xfer* xfer)
{
	StructureBody::crc(xfer);
}

void SymbioticStructuresBody::xfer(Xfer* xfer)
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion(&version, currentVersion);
	StructureBody::xfer(xfer);
}

void SymbioticStructuresBody::loadPostProcess()
{
	StructureBody::loadPostProcess();
}

void DetachableRiderBodyModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	ActiveBodyModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "HealthPercentageWhenRiderDies", INI::parsePercentToReal, nullptr, offsetof(DetachableRiderBodyModuleData, m_healthPercentageWhenRiderDies) },
		{ "StartsActive", INI::parseBool, nullptr, offsetof(DetachableRiderBodyModuleData, m_startsActive) },
		{ "RiderlessDeathChance", INI::parsePercentToReal, nullptr, offsetof(DetachableRiderBodyModuleData, m_riderlessDeathChance) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
	p.add(UpgradeMuxData::getFieldParse(), offsetof(DetachableRiderBodyModuleData, m_upgradeMuxData));
}

DetachableRiderBody::DetachableRiderBody(Thing* thing, const ModuleData* moduleData) : ActiveBody(thing, moduleData)
{
}

DetachableRiderBody::~DetachableRiderBody() = default;

void DetachableRiderBody::crc(Xfer* xfer)
{
	ActiveBody::crc(xfer);
}

void DetachableRiderBody::xfer(Xfer* xfer)
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion(&version, currentVersion);
	ActiveBody::xfer(xfer);
}

void DetachableRiderBody::loadPostProcess()
{
	ActiveBody::loadPostProcess();
}
