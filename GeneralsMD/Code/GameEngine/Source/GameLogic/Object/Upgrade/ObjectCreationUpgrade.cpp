/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// FILE: ObjectCreationUpgrade.cpp /////////////////////////////////////////////////////////////////////////////
// Author: Matthew D. Campbell, April 2002
// Desc:	 upgrades that spawn OCLs
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/ModelState.h"
#include "Common/Player.h"
#include "Common/Xfer.h"
#include "GameClient/Drawable.h"
#include "GameLogic/Module/ObjectCreationUpgrade.h"
#include "Common/RetailFileLayers.h"
#include "Common/RetailSaveBlocks.h"
#include "Common/RotwkDuration.h"
#include "Common/ThingFactory.h"
#include "Common/Upgrade.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/SlavedUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/ObjectCreationList.h"

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
ObjectCreationUpgradeModuleData::ObjectCreationUpgradeModuleData()
{

	m_ocl = nullptr;

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
/* static */ void ObjectCreationUpgradeModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpgradeModuleData::buildFieldParse( p );

	static const FieldParse dataFieldParse[] =
	{
		{ "UpgradeObject", INI::parseObjectCreationList, nullptr, offsetof( ObjectCreationUpgradeModuleData, m_ocl ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(dataFieldParse);
	if (RetailFileLayers::enabled())
	{
		static const FieldParse retailFields[] =
		{
			{ "RemoveUpgrade", INI::parseAsciiString, nullptr, offsetof(ObjectCreationUpgradeModuleData, m_removeUpgrade) },
			{ "GrantUpgrade", INI::parseAsciiString, nullptr, offsetof(ObjectCreationUpgradeModuleData, m_grantUpgrade) },
			{ "Delay", INI::parseDurationUnsignedInt, nullptr, offsetof(ObjectCreationUpgradeModuleData, m_delay) },
			{ "FadeInTime", INI::parseUnsignedInt, nullptr, offsetof(ObjectCreationUpgradeModuleData, m_fadeInMsecs) },
			{ "ThingToSpawn", INI::parseAsciiString, nullptr, offsetof(ObjectCreationUpgradeModuleData, m_spawnTemplate) },
			{ "Offset", INI::parseCoord3D, nullptr, offsetof(ObjectCreationUpgradeModuleData, m_spawnOffset) },
			{ "Angle", INI::parseAngleReal, nullptr, offsetof(ObjectCreationUpgradeModuleData, m_spawnAngle) },
			{ "DestroyWhenSold", INI::parseBool, nullptr, offsetof(ObjectCreationUpgradeModuleData, m_destroyWhenSold) },
			{ "DeathAnimAndDuration", SpecialPowerAnimation::parse, nullptr, offsetof(ObjectCreationUpgradeModuleData, m_deathAnimation) },
			{ nullptr, nullptr, nullptr, 0 }
		};
		p.add(retailFields);
	}

}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
ObjectCreationUpgrade::ObjectCreationUpgrade( Thing *thing, const ModuleData* moduleData ) :
							UpgradeModule( thing, moduleData )
{

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
ObjectCreationUpgrade::~ObjectCreationUpgrade()
{

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void ObjectCreationUpgrade::onDelete()
{
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void ObjectCreationUpgrade::upgradeImplementation()
{
	// spawn everything in the OCL
	if (getObjectCreationUpgradeModuleData() && getObjectCreationUpgradeModuleData()->m_ocl)
	{
		ObjectCreationList::create((getObjectCreationUpgradeModuleData()->m_ocl), getObject(), nullptr);
	}
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void ObjectCreationUpgrade::crc( Xfer *xfer )
{

	// extend base class
	UpgradeModule::crc( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void ObjectCreationUpgrade::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	UpgradeModule::xfer( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void ObjectCreationUpgrade::loadPostProcess()
{

	// extend base class
	UpgradeModule::loadPostProcess();

}

namespace
{
SlavedUpdateInterface* directSpawnSlavedUpdate(Object* object)
{
	for (BehaviorModule** module = object->getBehaviorModules(); *module; ++module)
		if (auto* slaved = (*module)->getSlavedUpdateInterface())
			return slaved;
	return nullptr;
}

SlaveWatcherBehavior* directSpawnWatcher(Object* producer)
{
	static const NameKeyType name = NAMEKEY("SlaveWatcherBehavior");
	for (BehaviorModule** module = producer->getBehaviorModules(); *module; ++module)
		if ((*module)->getModuleNameKey() == name)
			return static_cast<SlaveWatcherBehavior*>(*module);
	return nullptr;
}

RetailObjectCreationUpgrade* findCreationUpgrade(BehaviorModule* module)
{
	static const NameKeyType name = NAMEKEY("RetailObjectCreationUpgrade");
	return module->getModuleNameKey() == name ? static_cast<RetailObjectCreationUpgrade*>(module) : nullptr;
}

UpgradeMaskType creationUpgrades(const Object& producer)
{
	UpgradeMaskType upgrades = producer.getObjectCompletedUpgradeMask();
	if (const Player* player = producer.getControllingPlayer())
		upgrades.set(player->getCompletedUpgradeMask());
	return upgrades;
}
}

RetailObjectCreationUpgrade::RetailObjectCreationUpgrade(Thing* thing, const ModuleData* data)
	: UpdateModule(thing, data)
{
}

RetailObjectCreationUpgrade::~RetailObjectCreationUpgrade() = default;

void RetailObjectCreationUpgrade::getUpgradeActivationMasks(UpgradeMaskType& activation, UpgradeMaskType& conflicting) const
{
	getObjectCreationUpgradeModuleData()->m_upgradeMuxData.getUpgradeActivationMasks(activation, conflicting);
}

void RetailObjectCreationUpgrade::performUpgradeFX()
{
	getObjectCreationUpgradeModuleData()->m_upgradeMuxData.performUpgradeFX(getObject());
}

void RetailObjectCreationUpgrade::processUpgradeRemoval()
{
	getObjectCreationUpgradeModuleData()->m_upgradeMuxData.muxDataProcessUpgradeRemoval(getObject());
}

Bool RetailObjectCreationUpgrade::requiresAllActivationUpgrades() const
{
	return getObjectCreationUpgradeModuleData()->m_upgradeMuxData.m_requiresAllTriggers;
}

void RetailObjectCreationUpgrade::upgradeImplementation()
{
	const auto* data = getObjectCreationUpgradeModuleData();
	if (!data->m_ocl && data->m_grantUpgrade.isEmpty() && data->m_removeUpgrade.isEmpty() && data->m_spawnTemplate.isEmpty())
		return;
	setUpgradeExecuted(TRUE);
	m_pending = TRUE;
	if (data->m_delay || data->m_spawnTemplate.isNotEmpty() || data->m_grantUpgrade.isNotEmpty() || data->m_removeUpgrade.isNotEmpty())
		setWakeFrame(getObject(), UPDATE_SLEEP(data->m_delay + RotwkDuration::engineFrames(1, LOGICFRAMES_PER_SECOND)));
	else
		update();
}

Bool RetailObjectCreationUpgrade::resetUpgrade(const UpgradeMaskType& mask)
{
	if (testUpgradeConditions(creationUpgrades(*getObject())) || !UpgradeMux::resetUpgrade(mask))
		return FALSE;
	m_pending = FALSE;
	destroySpawn(FALSE);
	return TRUE;
}

UpdateSleepTime RetailObjectCreationUpgrade::update()
{
	Object* object = getObject();
	if (object->isEffectivelyDead() || object->testStatus(OBJECT_STATUS_SOLD))
	{
		onDelete();
		return UPDATE_SLEEP_FOREVER;
	}
	if (m_spawned != INVALID_ID)
	{
		Object* spawned = TheGameLogic->findObjectByID(m_spawned);
		if (!spawned || spawned->isDestroyed() || spawned->isEffectivelyDead())
		{
			const ObjectID gone = m_spawned;
			m_spawned = INVALID_ID;
			if (auto* watcher = directSpawnWatcher(object))
				watcher->onSpawnGone(gone);
		}
		else if (auto* watcher = directSpawnWatcher(object))
			watcher->shareUpgrades(*spawned);
	}
	if (!m_pending)
		return m_spawned != INVALID_ID ? UPDATE_SLEEP(15) : UPDATE_SLEEP_FOREVER;
	m_pending = FALSE;
	if (!object->getControllingPlayer() || !testUpgradeConditions(creationUpgrades(*object)))
	{
		setUpgradeExecuted(FALSE);
		return UPDATE_SLEEP_FOREVER;
	}

	const auto* data = getObjectCreationUpgradeModuleData();
	if (data->m_ocl)
		ObjectCreationList::create(data->m_ocl, object, nullptr);
	if (m_spawned == INVALID_ID && data->m_spawnTemplate.isNotEmpty())
	{
		const auto* type = TheThingFactory->findTemplate(data->m_spawnTemplate, FALSE);
		if (!type)
			return UPDATE_SLEEP_FOREVER;
		Object* spawned = TheThingFactory->newObject(type, object->getTeam());
		m_spawned = spawned->getID();
		spawned->setProducer(object);
		const auto& offset = data->m_spawnOffset;
		Vector3 world;
		Matrix3D::Transform_Vector(*object->getTransformMatrix(), Vector3(offset.x, offset.y, offset.z), &world);
		const Coord3D position{world.X, world.Y, world.Z};
		spawned->setPosition(&position);
		spawned->setOrientation(object->getOrientation() + data->m_spawnAngle);
		const auto fadeFrames = static_cast<UnsignedInt>(
			static_cast<UnsignedInt64>(data->m_fadeInMsecs) * LOGICFRAMES_PER_SECOND / 1000);
		if (fadeFrames)
			if (Drawable* drawable = spawned->getDrawable())
				drawable->startRetailOclFade(fadeFrames, TRUE, FALSE, FALSE);
		if (auto* watcher = directSpawnWatcher(object))
			watcher->onSpawnCreated(*spawned);
		if (auto* slaved = directSpawnSlavedUpdate(spawned))
		{
			slaved->onEnslave(object);
			if (object->isKindOf(KINDOF_IMMOBILE) && offset.z > 0.0f)
				spawned->setStatus(MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_DECK_HEIGHT_OFFSET));
		}
	}
	if (data->m_grantUpgrade.isNotEmpty())
	{
		const UpgradeTemplate* upgrade = TheUpgradeCenter->findUpgrade(data->m_grantUpgrade);
		if (upgrade && !object->hasUpgrade(upgrade))
			object->giveUpgrade(upgrade);
	}
	if (const auto* upgrade = TheUpgradeCenter->findUpgrade(data->m_removeUpgrade); upgrade && object->hasUpgrade(upgrade))
		object->removeUpgrade(upgrade);
	return m_spawned != INVALID_ID ? UPDATE_SLEEP(15) : UPDATE_SLEEP_FOREVER;
}

void RetailObjectCreationUpgrade::destroySpawn(Bool selling)
{
	const auto* data = getObjectCreationUpgradeModuleData();
	if (!data->m_destroyWhenSold || !TheGameLogic)
		return;
	const auto* watcher = directSpawnWatcher(getObject());
	const ObjectID id = watcher ? watcher->getWatchedSpawn() : (selling ? m_spawned : INVALID_ID);
	Object* child = TheGameLogic->findObjectByID(id);
	if (!child || child->isDestroyed() || child->isEffectivelyDead())
		return;

	child->kill();
	const auto& animation = data->m_deathAnimation;
	if (watcher && !child->isDestroyed() && animation.m_condition >= 0)
	{
		const auto duration = RotwkDuration::engineFrames(RotwkDuration::retailFrames(animation.m_duration), LOGICFRAMES_PER_SECOND);
		child->setTimedModelCondition(static_cast<ModelConditionFlagType>(animation.m_condition), duration);
	}
}

void RetailObjectCreationUpgrade::onProducerSold(Object* producer)
{
	for (BehaviorModule** module = producer->getBehaviorModules(); *module; ++module)
		if (auto* creation = findCreationUpgrade(*module); creation && creation->isAlreadyUpgraded())
			creation->destroySpawn(TRUE);
}

void RetailObjectCreationUpgrade::onDelete()
{
	if (m_spawned == INVALID_ID || !TheGameLogic)
		return;
	Object* spawned = TheGameLogic->findObjectByID(m_spawned);
	m_spawned = INVALID_ID;
	if (!spawned || spawned->isDestroyed() || spawned->isEffectivelyDead())
		return;
	if (getObjectCreationUpgradeModuleData()->m_destroyWhenSold && getObject()->testStatus(OBJECT_STATUS_SOLD))
		spawned->kill();
	else if (auto* slaved = directSpawnSlavedUpdate(spawned))
		slaved->onSlaverDie(nullptr);
	spawned->clearStatus(MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_DECK_HEIGHT_OFFSET));
}

Bool RetailObjectCreationUpgrade::getSpawnDeckHeight(ObjectID child, Real& height) const
{
	if (m_spawned != child)
		return FALSE;
	height = getObjectCreationUpgradeModuleData()->m_spawnOffset.z;
	return TRUE;
}

Object* RetailObjectCreationUpgrade::getSlavedSpawn() const
{
	Object* child = TheGameLogic->findObjectByID(m_spawned);
	if (!child || child->isDestroyed() || child->isEffectivelyDead() ||
		child->getControllingPlayer() != getObject()->getControllingPlayer())
		return nullptr;
	const auto* slaved = directSpawnSlavedUpdate(child);
	return slaved && slaved->getSlaverID() == getObject()->getID() ? child : nullptr;
}

void RetailObjectCreationUpgrade::onCapture(Player*, Player* newOwner)
{
	if (newOwner && m_spawned != INVALID_ID)
		if (Object* spawned = TheGameLogic->findObjectByID(m_spawned); spawned && directSpawnSlavedUpdate(spawned))
			spawned->setTeam(newOwner->getDefaultTeam());
}

void RetailObjectCreationUpgrade::crc(Xfer* xfer)
{
	UpdateModule::crc(xfer);
	upgradeMuxCRC(xfer);
	xfer->xferBool(&m_pending);
	if (getObjectCreationUpgradeModuleData()->m_spawnTemplate.isNotEmpty())
		xfer->xferObjectID(&m_spawned);
}

void RetailObjectCreationUpgrade::xfer(Xfer* xfer)
{
	const XferVersion currentVersion = getObjectCreationUpgradeModuleData()->m_spawnTemplate.isNotEmpty() ? 3 : 2;
	XferVersion version = currentVersion;
	xfer->xferVersion(&version, currentVersion);
	if (version >= 2)
	{
		UpdateModule::xfer(xfer);
		xfer->xferBool(&m_pending);
	}
	else
	{
		XferVersion upgradeVersion = 1;
		xfer->xferVersion(&upgradeVersion, 1);
		BehaviorModule::xfer(xfer);
	}
	upgradeMuxXfer(xfer);
	if (version >= 3)
		xfer->xferObjectID(&m_spawned);
}

void RetailObjectCreationUpgrade::loadPostProcess()
{
	UpdateModule::loadPostProcess();
	upgradeMuxLoadPostProcess();
}

void SlaveWatcherBehaviorModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	BehaviorModuleData::buildFieldParse(p);
	static const FieldParse fields[] = {
		{ "RemoveUpgrade", INI::parseAsciiString, nullptr, offsetof(SlaveWatcherBehaviorModuleData, m_removeUpgrade) },
		{ "GrantUpgrade", INI::parseAsciiString, nullptr, offsetof(SlaveWatcherBehaviorModuleData, m_grantUpgrade) },
		{ "ShareUpgrades", INI::parseBool, nullptr, offsetof(SlaveWatcherBehaviorModuleData, m_shareUpgrades) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

SlaveWatcherBehavior::SlaveWatcherBehavior(Thing* thing, const ModuleData* data) : BehaviorModule(thing, data)
{
}

SlaveWatcherBehavior::~SlaveWatcherBehavior() = default;

void SlaveWatcherBehavior::onSpawnCreated(Object& child)
{
	m_watchedSpawn = child.getID();
	shareUpgrades(child);
}

void SlaveWatcherBehavior::onSpawnGone(ObjectID child)
{
	if (child != m_watchedSpawn)
		return;
	m_watchedSpawn = INVALID_ID;
	Object* producer = getObject();
	const auto* data = getSlaveWatcherBehaviorModuleData();
	if (const auto* upgrade = TheUpgradeCenter->findUpgrade(data->m_removeUpgrade); upgrade && producer->hasUpgrade(upgrade))
		producer->removeUpgrade(upgrade);
	if (const auto* upgrade = TheUpgradeCenter->findUpgrade(data->m_grantUpgrade); upgrade && !producer->hasUpgrade(upgrade))
		producer->giveUpgrade(upgrade);
	else
		producer->updateUpgradeModules();
}

void SlaveWatcherBehavior::shareUpgrades(Object& child) const
{
	if (!getSlaveWatcherBehaviorModuleData()->m_shareUpgrades)
		return;
	const auto& upgrades = getObject()->getObjectCompletedUpgradeMask();
	if (child.getObjectCompletedUpgradeMask().testForAll(upgrades))
		return;
	for (auto* upgrade = TheUpgradeCenter->firstUpgradeTemplate(); upgrade; upgrade = upgrade->friend_getNext())
		if (upgrades.testForAny(upgrade->getUpgradeMask()) && !child.hasUpgrade(upgrade))
			child.giveUpgrade(upgrade);
}

void SlaveWatcherBehavior::crc(Xfer* xfer)
{
	BehaviorModule::crc(xfer);
	xferRetailBlocks(xfer, TRUE);
}

void SlaveWatcherBehavior::xfer(Xfer* xfer)
{
	const XferVersion currentVersion = 2;
	XferVersion version = currentVersion;
	xfer->xferVersion(&version, currentVersion);
	if (version >= 2)
		BehaviorModule::xfer(xfer);
	else
		ObjectModule::xfer(xfer);
	xferRetailBlocks(xfer, version >= 2);
}

void SlaveWatcherBehavior::xferRetailBlocks(Xfer* xfer, Bool present)
{
	RetailSaveBlocks::transferSection(xfer, xfer->getXferMode() == XFER_LOAD, present, {
		{ "watched-creation-spawn", [this](Xfer* block) { block->xferObjectID(&m_watchedSpawn); },
			[this] { m_restoreLegacySpawn = TRUE; } },
	});
}

void SlaveWatcherBehavior::loadPostProcess()
{
	BehaviorModule::loadPostProcess();
	if (!m_restoreLegacySpawn)
		return;
	m_restoreLegacySpawn = FALSE;
	for (BehaviorModule** module = getObject()->getBehaviorModules(); *module; ++module)
		if (const auto* creation = findCreationUpgrade(*module); creation && creation->m_spawned > m_watchedSpawn)
			m_watchedSpawn = creation->m_spawned;
}
