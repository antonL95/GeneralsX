#include "PreRTS.h"

#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/RetailFileLayers.h"
#include "Common/RetailSaveBlocks.h"
#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/DamageFilteredCreateObjectDie.h"
#include "GameLogic/Object.h"
#include "GameLogic/ObjectCreationList.h"
#include "GameLogic/RetailTestCommands.h"

#include <cstdio>

namespace
{
const Object* findSourceObjectForPlayer(Player* player)
{
	if (!player)
		return nullptr;
	if (Object* commandCenter = player->findNaturalCommandCenter())
		return commandCenter;
	const Object* found = nullptr;
	player->iterateObjects([](Object* object, void* userData)
	{
		auto** out = static_cast<const Object**>(userData);
		if (!*out && object && !object->isDestroyed() && !object->isEffectivelyDead())
			*out = object;
	}, &found);
	return found;
}
}

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
	{
		const Object* source = TheGameLogic->findObjectByID(damageInfo->in.m_sourceID);
		const Player* player = source ? source->getControllingPlayer() : nullptr;
		if (!player && damageInfo->in.m_sourcePlayerMask != 0 && ThePlayerList)
			player = ThePlayerList->getPlayerFromMask(damageInfo->in.m_sourcePlayerMask);
		if (player)
		{
			m_sourcePlayerIndex = player->getPlayerIndex();
			m_triggeredFrame = TheGameLogic->getFrame();
		}
	}
}

void DamageFilteredCreateObjectDie::onDie(const DamageInfo* damageInfo)
{
	const auto* data = getDamageFilteredCreateObjectDieModuleData();
	if (!isDieApplicable(damageInfo))
		return;
	const Bool instant = !data->m_instantSubType.isEmpty() &&
		damageInfo->retailDamageSubType.compareNoCase(data->m_instantSubType) == 0;

	const Object* sourceObject = nullptr;
	if (instant)
	{
		sourceObject = TheGameLogic->findObjectByID(damageInfo->in.m_sourceID);
		if (!sourceObject)
			return;
	}
	else
	{
		if (m_sourcePlayerIndex < 0)
			return;
		const UnsignedInt now = TheGameLogic->getFrame();
		if (now - m_triggeredFrame >= data->m_triggeredDuration)
		{
			m_sourcePlayerIndex = -1;
			return;
		}
		Player* sourcePlayer = ThePlayerList ? ThePlayerList->getNthPlayer(m_sourcePlayerIndex) : nullptr;
		if (!sourcePlayer || !sourcePlayer->isPlayerActive())
			return;
		sourceObject = findSourceObjectForPlayer(sourcePlayer);
		if (!sourceObject)
			return;
	}

	const Object* victim = getObject();
	const Object* created = ObjectCreationList::create(data->m_ocl, sourceObject, victim->getPosition(), victim->getPosition(),
		victim->getOrientation());
	if (RetailTestCommands::tracesCombat())
		std::fprintf(stderr, "TEST death-conversion frame=%u victim=%u(%s) created=%u(%s) owner=%d\n", TheGameLogic->getFrame(),
			static_cast<unsigned>(victim->getID()), victim->getTemplate()->getName().str(),
			created ? static_cast<unsigned>(created->getID()) : 0u, created ? created->getTemplate()->getName().str() : "-",
			created && created->getControllingPlayer() ? static_cast<int>(created->getControllingPlayer()->getPlayerIndex()) : -1);
}

void DamageFilteredCreateObjectDie::xferRetailBlocks(Xfer* xfer, Bool present)
{
	RetailSaveBlocks::transferSection(xfer, xfer->getXferMode() == XFER_LOAD, present, {
		{"duration-source", [this](Xfer* block)
		{
			block->xferUnsignedInt(&m_triggeredFrame);
			block->xferInt(&m_sourcePlayerIndex);
		}, [this]
		{
			m_triggeredFrame = 0;
			m_sourcePlayerIndex = -1;
		}}
	});
}

void DamageFilteredCreateObjectDie::crc(Xfer* xfer)
{
	DieModule::crc(xfer);
	if (RetailFileLayers::enabled())
		xferRetailBlocks(xfer, TRUE);
}

void DamageFilteredCreateObjectDie::xfer(Xfer* xfer)
{
	XferVersion currentVersion = RetailFileLayers::enabled() ? 2 : 1;
	XferVersion version = currentVersion;
	xfer->xferVersion(&version, currentVersion);
	DieModule::xfer(xfer);
	UnsignedInt legacyTriggeredUntil = 0;
	xfer->xferUnsignedInt(&legacyTriggeredUntil);
	if (RetailFileLayers::enabled())
		xferRetailBlocks(xfer, version >= 2);
}

void DamageFilteredCreateObjectDie::loadPostProcess()
{
	DieModule::loadPostProcess();
}
