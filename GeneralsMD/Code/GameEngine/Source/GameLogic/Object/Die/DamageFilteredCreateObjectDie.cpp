#include "PreRTS.h"

#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/DamageFilteredCreateObjectDie.h"
#include "GameLogic/Object.h"
#include "GameLogic/ObjectCreationList.h"
#include "GameLogic/RetailTestCommands.h"

#include <cstdio>

void DamageFilteredCreateObjectDieModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	DieModuleData::buildFieldParse(p);

	static const FieldParse fields[] =
	{
		{ "CreationList", INI::parseObjectCreationList, nullptr, offsetof(DamageFilteredCreateObjectDieModuleData, m_ocl) },
		{ "DamageTypeTriggersInstantly", INI::parseAsciiString, nullptr, offsetof(DamageFilteredCreateObjectDieModuleData, m_instantSubType) },
		{ "DamageTypeTriggersForDuration", INI::parseAsciiString, nullptr, offsetof(DamageFilteredCreateObjectDieModuleData, m_durationSubType) },
		{ "PostFilterTriggeredDuration", INI::parseDurationUnsignedInt, nullptr, offsetof(DamageFilteredCreateObjectDieModuleData, m_triggeredDuration) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

DamageFilteredCreateObjectDie::DamageFilteredCreateObjectDie(Thing* thing, const ModuleData* moduleData)
	: DieModule(thing, moduleData)
{
}

DamageFilteredCreateObjectDie::~DamageFilteredCreateObjectDie()
{
}

void DamageFilteredCreateObjectDie::onDamage(DamageInfo* damageInfo)
{
	const auto* data = getDamageFilteredCreateObjectDieModuleData();
	if (!data->m_durationSubType.isEmpty() && damageInfo->retailDamageSubType.compareNoCase(data->m_durationSubType) == 0)
		m_triggeredUntil = TheGameLogic->getFrame() + data->m_triggeredDuration;
}

void DamageFilteredCreateObjectDie::onDie(const DamageInfo* damageInfo)
{
	const auto* data = getDamageFilteredCreateObjectDieModuleData();
	if (!isDieApplicable(damageInfo))
		return;
	const Bool instant = !data->m_instantSubType.isEmpty() &&
		damageInfo->retailDamageSubType.compareNoCase(data->m_instantSubType) == 0;
	if (!instant && !(m_triggeredUntil && TheGameLogic->getFrame() <= m_triggeredUntil))
		return;

	const Object* victim = getObject();
	const Object* killer = TheGameLogic->findObjectByID(damageInfo->in.m_sourceID);
	if (!killer)
		return;
	const Object* created = ObjectCreationList::create(data->m_ocl, killer, victim->getPosition(), victim->getPosition(),
		victim->getOrientation());
	if (RetailTestCommands::tracesCombat())
		std::fprintf(stderr, "TEST death-conversion frame=%u victim=%u(%s) created=%u(%s)\n", TheGameLogic->getFrame(),
			static_cast<unsigned>(victim->getID()), victim->getTemplate()->getName().str(),
			created ? static_cast<unsigned>(created->getID()) : 0u, created ? created->getTemplate()->getName().str() : "-");
}

void DamageFilteredCreateObjectDie::crc(Xfer* xfer)
{
	DieModule::crc(xfer);
}

void DamageFilteredCreateObjectDie::xfer(Xfer* xfer)
{
	XferVersion version = 1;
	xfer->xferVersion(&version, 1);
	DieModule::xfer(xfer);
	xfer->xferUnsignedInt(&m_triggeredUntil);
}

void DamageFilteredCreateObjectDie::loadPostProcess()
{
	DieModule::loadPostProcess();
}
