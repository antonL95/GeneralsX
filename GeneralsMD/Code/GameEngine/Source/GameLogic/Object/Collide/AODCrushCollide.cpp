#include "PreRTS.h"

#define DEFINE_DEATH_NAMES
#include "GameLogic/Module/AODCrushCollide.h"

#include "Common/Player.h"
#include "Common/RetailFileLayers.h"
#include "Common/ThingTemplate.h"
#include "GameClient/FXList.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Object.h"
#include "GameLogic/ObjectCreationList.h"
#include "GameLogic/RetailTestCommands.h"

void AODCrushCollideModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	CollideModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "SmallFXList", INI::parseFXList, nullptr, offsetof(AODCrushCollideModuleData, m_smallFX) },
		{ "MediumFXList", INI::parseFXList, nullptr, offsetof(AODCrushCollideModuleData, m_mediumFX) },
		{ "LargeFXList", INI::parseFXList, nullptr, offsetof(AODCrushCollideModuleData, m_largeFX) },
		{ "MediumObjectCreationList", INI::parseObjectCreationList, nullptr, offsetof(AODCrushCollideModuleData, m_mediumOCL) },
		{ "SpecialObject", SpecialPowerObjectFilter::parse, nullptr, offsetof(AODCrushCollideModuleData, m_specialObject) },
		{ "Damage", INI::parseReal, nullptr, offsetof(AODCrushCollideModuleData, m_damage) },
		{ "SpecialDamage", INI::parseReal, nullptr, offsetof(AODCrushCollideModuleData, m_specialDamage) },
		{ "SelfDamage", INI::parseReal, nullptr, offsetof(AODCrushCollideModuleData, m_selfDamage) },
		{ "DamageType", DamageTypeFlags::parseSingleBitFromINI, nullptr, offsetof(AODCrushCollideModuleData, m_damageType) },
		{ "SpecialDamageType", DamageTypeFlags::parseSingleBitFromINI, nullptr, offsetof(AODCrushCollideModuleData, m_specialDamageType) },
		{ "SelfDamageType", DamageTypeFlags::parseSingleBitFromINI, nullptr, offsetof(AODCrushCollideModuleData, m_selfDamageType) },
		{ "DeathType", INI::parseIndexList, TheDeathNames, offsetof(AODCrushCollideModuleData, m_deathType) },
		{ "SpecialDeathType", INI::parseIndexList, TheDeathNames, offsetof(AODCrushCollideModuleData, m_specialDeathType) },
		{ "SelfDeathType", INI::parseIndexList, TheDeathNames, offsetof(AODCrushCollideModuleData, m_selfDeathType) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

namespace
{
void dealContactDamage(Object* source, Object* victim, Real amount, DamageType type, DeathType death)
{
	DamageInfo info;
	info.in.m_sourceID = source->getID();
	if (const Player* owner = source->getControllingPlayer())
		info.in.m_sourcePlayerMask = owner->getPlayerMask();
	info.in.m_amount = amount;
	info.in.m_damageType = type;
	info.in.m_deathType = death;
	victim->attemptDamage(&info);
	if (RetailTestCommands::tracesCombat())
		std::fprintf(stderr, "TEST aod contact frame=%u source=%u(%s) victim=%u(%s) amount=%.2f dealt=%.2f\n",
			TheGameLogic->getFrame(), unsigned(source->getID()), source->getTemplate()->getName().str(),
			unsigned(victim->getID()), victim->getTemplate()->getName().str(), amount, info.out.m_actualDamageDealt);
}
}

void AODCrushCollide::onCollide(Object* other, const Coord3D*, const Coord3D*)
{
	Object* self = getObject();
	if (!RetailFileLayers::enabled() || !other || other == self || other->isEffectivelyDead() ||
		self->isEffectivelyDead() || self->getRelationship(other) == ALLIES || other->isKindOf(KINDOF_ARMY_OF_DEAD) ||
		other->isKindOf(KINDOF_HORDE) || other->isKindOf(KINDOF_CRATE) ||
		!other->getBodyModule() || other->getBodyModule()->isIndestructible())
		return;

	const auto* data = getAODCrushCollideModuleData();
	const Bool special = data->m_specialObject.matchesObject(self, other);
	dealContactDamage(self, other, special ? data->m_specialDamage : data->m_damage,
		special ? data->m_specialDamageType : data->m_damageType,
		special ? data->m_specialDeathType : data->m_deathType);
	if (special && data->m_selfDamage > 0.0f)
		dealContactDamage(other, self, data->m_selfDamage, data->m_selfDamageType, data->m_selfDeathType);

	const Bool large = other->isKindOf(KINDOF_STRUCTURE);
	const Bool small = other->getGeometryInfo().getIsSmall();
	FXList::doFXObj(large ? data->m_largeFX : small ? data->m_smallFX : data->m_mediumFX, other, self);
	if (!large && !small)
		ObjectCreationList::create(data->m_mediumOCL, other, self);
}

AODCrushCollide::~AODCrushCollide() = default;
void AODCrushCollide::crc(Xfer* xfer) { CollideModule::crc(xfer); }
void AODCrushCollide::xfer(Xfer* xfer) { CollideModule::xfer(xfer); }
void AODCrushCollide::loadPostProcess() { CollideModule::loadPostProcess(); }
