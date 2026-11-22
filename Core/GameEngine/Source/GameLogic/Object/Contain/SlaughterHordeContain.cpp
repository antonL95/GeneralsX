#include "PreRTS.h"

#include <algorithm>
#include <vector>

#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/GameAudio.h"
#include "Common/RetailFileLayers.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "Common/Xfer.h"
#include "GameClient/ClientRandomValue.h"
#include "GameClient/FXList.h"
#include "GameClient/RetailEva.h"
#include "GameClient/RetailWorldUI.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/CostModifierUpgrade.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Module/SlaughterHordeContain.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/TerrainLogic.h"
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
	if (!passenger || passenger->isEffectivelyDead() || passenger->isDestroyed())
		return FALSE;
	const auto* data = getSlaughterHordeContainModuleData();
	const Bool ownPlayer = data->m_allowOwnPlayer && passenger->getControllingPlayer() == getObject()->getControllingPlayer();
	const Bool allowed = ownPlayer ?
		passenger->isAnyKindOf(data->m_allowInsideKindOf) && !passenger->isAnyKindOf(data->m_forbidInsideKindOf) :
		OpenContain::isValidContainerFor(passenger, FALSE);
	return allowed && data->m_passengers.matchesObject(getObject(), passenger);
}

Bool SlaughterHordeContain::canAcceptEntry(const Object* passenger) const
{
	return RetailFileLayers::enabled() && passenger && passenger != getObject() &&
		!passenger->isEffectivelyDead() && !passenger->isDestroyed() &&
		!getObject()->isEffectivelyDead() && !getObject()->isDestroyed() &&
		!getObject()->testStatus(OBJECT_STATUS_SOLD) && !getObject()->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION);
}

Bool SlaughterHordeContain::isValidContainerFor(const Object* passenger, Bool checkCapacity) const
{
	if (!canAcceptEntry(passenger))
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

void SlaughterHordeContain::playEntrySound()
{
	AudioEventRTS sound = getSlaughterHordeContainModuleData()->m_enterSound;
	if (TheAudio && !sound.getEventName().isEmpty())
	{
		sound.setObjectID(getObject()->getID());
		TheAudio->addAudioEvent(&sound);
	}
}

void SlaughterHordeContain::addToContain(Object* passenger)
{
	if (!SlaughterHordeContain::isValidContainerFor(passenger, TRUE))
		return;
	playEntrySound();
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

void CitadelSlaughterHordeContainModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	SlaughterHordeContainModuleData::buildFieldParse(p);
	static const FieldParse fields[] = {
		{ "AllowOwnPlayerInsideOverride", INI::parseBool, nullptr, offsetof(CitadelSlaughterHordeContainModuleData, m_allowOwnPlayer) },
		{ "StatusForRingEntry", ObjectStatusMaskType::parseFromINI, nullptr, offsetof(CitadelSlaughterHordeContainModuleData, m_ringStatus) },
		{ "UpgradeForRingEntry", INI::parseAsciiStringVectorAppend, nullptr, offsetof(CitadelSlaughterHordeContainModuleData, m_ringUpgrades) },
		{ "ObjectToDestroyForRingEntry", SpecialPowerObjectFilter::parse, nullptr, offsetof(CitadelSlaughterHordeContainModuleData, m_ringFilter) },
		{ "FXForRingEntry", INI::parseFXList, nullptr, offsetof(CitadelSlaughterHordeContainModuleData, m_ringFX) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

CitadelSlaughterHordeContain::~CitadelSlaughterHordeContain() = default;

Object* CitadelSlaughterHordeContain::findRing(const Object* passenger) const
{
	const auto* data = getCitadelSlaughterHordeContainModuleData();
	const auto* horde = HordeContain::get(passenger);
	if (!data->m_ringStatus.any() || passenger->getControllingPlayer() != getObject()->getControllingPlayer())
		return nullptr;
	const auto hasRingStatus = [&](const Object* candidate) { return candidate->getStatusBits().testForAll(data->m_ringStatus); };
	if (!hasRingStatus(passenger))
	{
		const auto* members = horde ? horde->getContainedItemsList() : nullptr;
		if (!members || std::none_of(members->begin(), members->end(), hasRingStatus))
			return nullptr;
	}

	Object* result = nullptr;
	for (Object* candidate = TheGameLogic->getFirstObject(); candidate; candidate = candidate->getNextObject())
	{
		if (!candidate->isKindOf(KINDOF_ONE_RING) || candidate->isDestroyed() || candidate->isEffectivelyDead() ||
			!data->m_ringFilter.matchesObject(getObject(), candidate))
			continue;
		const auto* attachment = static_cast<const AttachUpdate*>(candidate->findUpdateModule(NAMEKEY("AttachUpdate")));
		const Object* carrier = attachment ? TheGameLogic->findObjectByID(attachment->getParentID()) : nullptr;
		if (carrier && (carrier == passenger || (horde && carrier->getContainedBy() == passenger)) &&
			hasRingStatus(carrier) && (!result || candidate->getID() < result->getID()))
			result = candidate;
	}
	return result;
}

Bool CitadelSlaughterHordeContain::isValidContainerFor(const Object* passenger, Bool checkCapacity) const
{
	if (!canAcceptEntry(passenger))
		return FALSE;
	if (findRing(passenger))
		return TRUE;
	return SlaughterHordeContain::isValidContainerFor(passenger, checkCapacity);
}

void CitadelSlaughterHordeContain::addToContain(Object* passenger)
{
	if (!canAcceptEntry(passenger))
		return;
	Object* ring = findRing(passenger);
	if (!ring)
	{
		SlaughterHordeContain::addToContain(passenger);
		return;
	}

	Object* self = getObject();
	Player* player = self->getControllingPlayer();
	const auto* data = getCitadelSlaughterHordeContainModuleData();
	std::vector<const UpgradeTemplate*> upgrades;
	for (const AsciiString& name : data->m_ringUpgrades)
	{
		const UpgradeTemplate* upgrade = TheUpgradeCenter->findUpgrade(name);
		if (!upgrade)
			return;
		upgrades.push_back(upgrade);
	}
	playEntrySound();
	if (auto* ai = passenger->getAI(); ai && ai->getEnterTarget() == self)
		ai->aiIdle(ai->getLastCommandSource());
	TheGameLogic->destroyObject(ring);
	for (const UpgradeTemplate* upgrade : upgrades)
	{
		if (upgrade->getUpgradeType() == UPGRADE_TYPE_PLAYER)
			player->addUpgrade(upgrade, UPGRADE_STATUS_COMPLETE);
		else
			self->giveUpgrade(upgrade);
	}
	if (data->m_ringFX)
		FXList::doFXObj(data->m_ringFX, self);
}

void CitadelSlaughterHordeContain::crc(Xfer* xfer)
{
	SlaughterHordeContain::crc(xfer);
}

void CitadelSlaughterHordeContain::xfer(Xfer* xfer)
{
	SlaughterHordeContain::xfer(xfer);
}

void CitadelSlaughterHordeContain::loadPostProcess()
{
	SlaughterHordeContain::loadPostProcess();
}

void AttachUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	static const FieldParse fields[] = {
		{ "ObjectFilter", SpecialPowerObjectFilter::parse, nullptr, offsetof(AttachUpdateModuleData, m_filter) },
		{ "ParentStatus", ObjectStatusMaskType::parseFromINI, nullptr, offsetof(AttachUpdateModuleData, m_parentStatus) },
		{ "ScanRange", INI::parseReal, nullptr, offsetof(AttachUpdateModuleData, m_scanRange) },
		{ "AlwaysTeleport", INI::parseBool, nullptr, offsetof(AttachUpdateModuleData, m_alwaysTeleport) },
		{ "AnchorToTopOfGeometry", INI::parseBool, nullptr, offsetof(AttachUpdateModuleData, m_anchorToTop) },
		{ "AttachFX", INI::parseFXList, nullptr, offsetof(AttachUpdateModuleData, m_attachFX) },
		{ "ParentOwnerAttachmentEvaEvent", INI::parseAsciiString, nullptr, offsetof(AttachUpdateModuleData, m_parentOwnerAttachmentEva) },
		{ "ParentEnemyAttachmentEvaEvent", INI::parseAsciiString, nullptr, offsetof(AttachUpdateModuleData, m_parentEnemyAttachmentEva) },
		{ "ParentOwnerDiedEvaEvent", INI::parseAsciiString, nullptr, offsetof(AttachUpdateModuleData, m_parentOwnerDiedEva) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	UpdateModuleData::buildFieldParse(p);
	p.add(fields);
}

AttachUpdate::~AttachUpdate() = default;

void AttachUpdate::followParent(const Object* parent)
{
	Object* self = getObject();
	const auto* data = getAttachUpdateModuleData();
	Coord3D position = *parent->getPosition();
	if (data->m_anchorToTop)
		position.z += parent->getGeometryInfo().getMaxHeightAbovePosition();
	if (position == *self->getPosition())
		return;

	self->setPosition(&position);
	if (data->m_alwaysTeleport)
	{
		self->setLayer(parent->getLayer());
		if (auto* physics = self->getPhysics())
			physics->resetDynamicPhysics();
	}
}

Object* AttachUpdate::reconcileParent()
{
	if (m_parent == INVALID_ID)
		return nullptr;

	Object* self = getObject();
	Object* parent = TheGameLogic->findObjectByID(m_parent);
	const auto* data = getAttachUpdateModuleData();
	const Bool parentAlive = parent && !parent->isEffectivelyDead() && !parent->isDestroyed();
	if (parentAlive && data->m_filter.matchesObject(self, parent))
		return parent;

	const Bool ring = self->isKindOf(KINDOF_ONE_RING);
	if (parent)
	{
		parent->clearStatus(data->m_parentStatus);
		if (!parentAlive)
			RetailEva::queue(parent->getControllingPlayer(), data->m_parentOwnerDiedEva,
				self->getPosition(), ring ? RetailEva::Ping::OneRing : RetailEva::Ping::None);
	}

	m_parent = INVALID_ID;
	self->clearStatus(MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_ATTACHED));
	if (ring)
	{
		Coord3D position = *self->getPosition();
		position.z = TheTerrainLogic->getGroundHeight(position.x, position.y);
		self->setPosition(&position);
	}
	else
		self->kill();
	return nullptr;
}

void AttachUpdate::reconcileOwnershipChange(const Object* changed)
{
	static const NameKeyType attachKey = NAMEKEY("AttachUpdate");
	for (Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
	{
		if (object->isEffectivelyDead() || object->isDestroyed())
			continue;
		for (BehaviorModule** module = object->getBehaviorModules(); *module; ++module)
		{
			if ((*module)->getModuleNameKey() != attachKey)
				continue;
			auto* attach = static_cast<AttachUpdate*>(*module);
			if (object == changed || attach->m_parent == changed->getID())
				attach->reconcileParent();
			if (object->isEffectivelyDead() || object->isDestroyed())
				break;
		}
	}
}

UpdateSleepTime AttachUpdate::update()
{
	Object* self = getObject();
	const auto* data = getAttachUpdateModuleData();
	if (!RetailFileLayers::enabled() || self->isEffectivelyDead() || self->isDestroyed() || data->m_scanRange < 0.0f)
		return UPDATE_SLEEP_FOREVER;

	const Bool ring = self->isKindOf(KINDOF_ONE_RING);
	const auto ping = ring ? RetailEva::Ping::OneRing : RetailEva::Ping::None;
	if (const Object* parent = reconcileParent())
	{
		followParent(parent);
		return UPDATE_SLEEP_NONE;
	}
	if (self->isEffectivelyDead() || self->isDestroyed())
		return UPDATE_SLEEP_FOREVER;

	Object* parent = nullptr;
	SimpleObjectIterator* iter = ThePartitionManager->iterateObjectsInRange(self->getPosition(), data->m_scanRange, FROM_CENTER_3D);
	MemoryPoolObjectHolder holder(iter);
	Real bestDistanceSquared = data->m_scanRange * data->m_scanRange;
	for (Object* candidate = iter->first(); candidate; candidate = iter->next())
	{
		if (candidate == self || candidate->isEffectivelyDead() || candidate->isDestroyed() ||
			TEST_OBJECT_STATUS_MASK_ANY(candidate->getStatusBits(), data->m_parentStatus) ||
			!data->m_filter.matchesObject(self, candidate))
			continue;
		const Real distanceSquared = ThePartitionManager->getDistanceSquared(candidate, self, FROM_CENTER_3D);
		if (distanceSquared < bestDistanceSquared ||
			(distanceSquared == bestDistanceSquared && (!parent || candidate->getID() < parent->getID())))
		{
			parent = candidate;
			bestDistanceSquared = distanceSquared;
		}
	}
	if (parent && !parent->isDestroyed() && !parent->isEffectivelyDead())
	{
		m_parent = parent->getID();
		parent->setStatus(data->m_parentStatus);
		self->setStatus(MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_ATTACHED));
		if (data->m_attachFX)
		{
			const ScopedVisualRandom random(TRUE);
			FXList::doFXObj(data->m_attachFX, self, parent);
		}
		const Player* owner = parent->getControllingPlayer();
		RetailEva::queue(owner, data->m_parentOwnerAttachmentEva, parent->getPosition(), ping);
		const Player* local = ThePlayerList->getLocalPlayer();
		if (local && local != owner)
			RetailEva::queue(local, local->getRelationship(parent->getTeam()) == ENEMIES ?
				data->m_parentEnemyAttachmentEva : data->m_parentOwnerAttachmentEva, parent->getPosition(), ping);
		followParent(parent);
	}
	return UPDATE_SLEEP_NONE;
}

void AttachUpdate::onDelete()
{
	if (m_parent != INVALID_ID)
		if (Object* parent = TheGameLogic->findObjectByID(m_parent))
			parent->clearStatus(getAttachUpdateModuleData()->m_parentStatus);
}

void AttachUpdate::crc(Xfer* xfer)
{
	UpdateModule::crc(xfer);
	xfer->xferObjectID(&m_parent);
}

void AttachUpdate::xfer(Xfer* xfer)
{
	XferVersion version = 1;
	xfer->xferVersion(&version, 1);
	UpdateModule::xfer(xfer);
	xfer->xferObjectID(&m_parent);
}

void AttachUpdate::loadPostProcess()
{
	UpdateModule::loadPostProcess();
	if (RetailFileLayers::enabled() && !getObject()->isEffectivelyDead() && !getObject()->isDestroyed())
		reconcileParent();
}
