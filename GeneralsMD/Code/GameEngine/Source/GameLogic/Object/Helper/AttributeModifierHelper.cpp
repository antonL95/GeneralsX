#include "PreRTS.h"

#include "GameLogic/Module/AttributeModifierHelper.h"

#include <algorithm>
#include <cstdio>

#include "Common/GlobalData.h"
#include "Common/RetailFileLayers.h"
#include "Common/RetailSaveBlocks.h"
#include "Common/NameKeyGenerator.h"
#include "Common/SpecialPowerINI.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "Common/Xfer.h"
#include "GameClient/FXList.h"
#include "GameLogic/AttributeModifierStore.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/RetailBuildingEvents.h"
#include "GameLogic/WeaponNugget.h"

using namespace AttributeModifierRules;

namespace
{
const ListDef& defOf(int list)
{
	return TheAttributeModifierStore->def(list);
}

struct DefLookup
{
	const ListDef& operator()(int list) const { return defOf(list); }
};

AttributeModifierHelper* helperOf(const Object* obj)
{
	return obj ? obj->getAttributeModifierHelper() : nullptr;
}

const UpgradeTemplate* upgradeNamed(const AsciiString& name)
{
	return TheUpgradeCenter ? TheUpgradeCenter->findUpgrade(name) : nullptr;
}

Owner makeOwner(UnsignedInt objectID, NameKeyType moduleKey)
{
	return (static_cast<Owner>(objectID) << 32) | static_cast<Owner>(static_cast<UnsignedInt>(moduleKey));
}

UnsignedInt ownerObjectOf(Owner owner)
{
	return static_cast<UnsignedInt>(owner >> 32);
}

NameKeyType ownerModuleOf(Owner owner)
{
	return static_cast<NameKeyType>(static_cast<UnsignedInt>(owner & 0xFFFFFFFFu));
}

void xferHolds(Xfer* xfer, std::vector<Hold>& holds)
{
	UnsignedShort count = static_cast<UnsignedShort>(holds.size());
	xfer->xferUnsignedShort( &count );
	if (xfer->getXferMode() == XFER_LOAD)
		holds.assign(count, Hold());
	for (Hold& hold : holds)
	{
		UnsignedInt objectID = ownerObjectOf(hold.owner);
		const NameKeyType moduleKey = ownerModuleOf(hold.owner);
		AsciiString moduleName = hold.owner != ANONYMOUS_OWNER ? TheNameKeyGenerator->keyToName(moduleKey) : AsciiString::TheEmptyString;
		xfer->xferUnsignedInt( &objectID );
		xfer->xferAsciiString( &moduleName );
		xfer->xferUnsignedInt( &hold.endFrame );
		if (xfer->getXferMode() == XFER_LOAD)
			hold.owner = moduleName.isEmpty() ? ANONYMOUS_OWNER : makeOwner(objectID, TheNameKeyGenerator->nameToKey(moduleName));
	}
}
}

AttributeModifierHelper::AttributeModifierHelper( Thing *thing, const ModuleData *modData ) :
	ObjectHelper( thing, modData ),
	m_appliedHealthAdd( 0.0f ),
	m_appliedHealthMult( 1.0f ),
	m_appliedShroudFactor( 1.0f ),
	m_version( 0 ),
	m_cachedVersion( 0 ),
	m_cachedHorde( nullptr ),
	m_cachedHordeVersion( 0 ),
	m_cacheValid( FALSE )
{
}

AttributeModifierHelper::~AttributeModifierHelper()
{
}

AttributeModifierHelper::TransferState AttributeModifierHelper::getTransferState() const
{
	return {m_stack, m_pendingUpgrades};
}

void AttributeModifierHelper::inheritTransferState(TransferState state)
{
	m_stack = std::move(state.stack);
	m_pendingUpgrades = std::move(state.pendingUpgrades);
	++m_version;
	refreshDerived();
	scheduleWake();
}

const AttributeModifierHelper* AttributeModifierHelper::hordeHelper() const
{
	const Object* container = getObject()->getContainedBy();
	return container && HordeContain::get(container) ? container->getAttributeModifierHelper() : nullptr;
}

const Totals& AttributeModifierHelper::getTotals() const
{
	const AttributeModifierHelper* horde = hordeHelper();
	const UnsignedInt hordeVersion = horde ? horde->m_version : 0;
	if (!m_cacheValid || m_cachedVersion != m_version || m_cachedHorde != horde || m_cachedHordeVersion != hordeVersion)
	{
		m_cachedTotals = TheAttributeModifierStore ? totalsOf({&m_stack, horde ? &horde->m_stack : nullptr}, DefLookup()) : Totals();
		m_cachedVersion = m_version;
		m_cachedHorde = horde;
		m_cachedHordeVersion = hordeVersion;
		m_cacheValid = TRUE;
	}
	return m_cachedTotals;
}

Bool AttributeModifierHelper::hasAnyModifier() const
{
	if (!m_stack.active().empty())
		return TRUE;
	const AttributeModifierHelper* horde = hordeHelper();
	return horde && !horde->m_stack.active().empty();
}

ApplyResult AttributeModifierHelper::apply( Int list, UnsignedInt durationFrames, UnsignedInt antiMask, Owner owner )
{
	if (!TheAttributeModifierStore || !TheAttributeModifierStore->get(list) || getObject()->isEffectivelyDead())
		return IGNORED;
	const ListDef& def = defOf(list);
	if (durationFrames == USE_LIST_DURATION)
		durationFrames = def.durationFrames;
	const UnsignedInt now = TheGameLogic->getFrame();
	std::vector<int> removed;
	const ApplyResult result = m_stack.apply(list, endFrameFor(now, durationFrames), antiMask, DefLookup(), &removed, owner);
	std::vector<Int> added;
	if (result == APPLIED)
		added.push_back(list);
	if (!added.empty() || !removed.empty())
		changed(added, std::vector<Int>(removed.begin(), removed.end()));
	if (result == REFRESHED && RetailFileLayers::enabled() && !TheGlobalData->m_headless)
		TheAttributeModifierStore->get(list)->playFX(getObject(), FALSE, TRUE);
	scheduleWake();
	return result;
}

Bool AttributeModifierHelper::remove( Int list, Owner owner )
{
	const Bool removed = m_stack.remove(list, owner, TheGameLogic->getFrame());
	if (removed)
		changed({}, {list});
	scheduleWake();
	return removed;
}

void AttributeModifierHelper::removeCategories( UnsignedInt mask )
{
	std::vector<int> removed;
	m_stack.removeCategories(mask, DefLookup(), &removed);
	if (!removed.empty())
		changed({}, std::vector<Int>(removed.begin(), removed.end()));
}

void AttributeModifierHelper::removeTimed()
{
	const std::vector<int> removed = m_stack.removeTimed();
	if (!removed.empty())
		changed({}, std::vector<Int>(removed.begin(), removed.end()));
	scheduleWake();
}

void AttributeModifierHelper::clearAll()
{
	std::vector<Int> removed;
	for (const Active& active : m_stack.active())
		removed.push_back(active.list);
	m_stack.clear();
	m_pendingUpgrades.clear();
	if (!removed.empty())
		changed({}, removed);
}

void AttributeModifierHelper::setWeatherAntiMask( UnsignedInt mask )
{
	if (mask != m_stack.weatherAntiMask())
	{
		m_stack.setWeatherAntiMask(mask);
		changed({}, {});
	}
}

void AttributeModifierHelper::grantUpgrades( Int list )
{
	const ModifierListTemplate* tmpl = TheAttributeModifierStore->get(list);
	if (!tmpl || tmpl->m_upgradeNames.empty())
		return;
	if (tmpl->m_upgradeDelayFrames > 0)
	{
		m_pendingUpgrades.push_back(PendingUpgrade{list, TheGameLogic->getFrame() + tmpl->m_upgradeDelayFrames});
		return;
	}
	for (const AsciiString& name : tmpl->m_upgradeNames)
	{
		const UpgradeTemplate* upgrade = upgradeNamed(name);
		if (upgrade && upgrade->getUpgradeType() == UPGRADE_TYPE_OBJECT && !getObject()->hasUpgrade(upgrade))
			getObject()->giveUpgrade(upgrade);
	}
}

void AttributeModifierHelper::revokeUpgrades( Int list )
{
	m_pendingUpgrades.erase(std::remove_if(m_pendingUpgrades.begin(), m_pendingUpgrades.end(),
		[list](const PendingUpgrade& pending) { return pending.list == list; }), m_pendingUpgrades.end());
	const ModifierListTemplate* tmpl = TheAttributeModifierStore->get(list);
	if (!tmpl)
		return;
	for (const AsciiString& name : tmpl->m_upgradeNames)
	{
		const UpgradeTemplate* upgrade = upgradeNamed(name);
		if (upgrade && upgrade->getUpgradeType() == UPGRADE_TYPE_OBJECT && getObject()->hasUpgrade(upgrade))
			getObject()->removeUpgrade(upgrade);
	}
}

void AttributeModifierHelper::changed( const std::vector<Int>& added, const std::vector<Int>& removed )
{
	++m_version;
	Object* obj = getObject();
	for (Int list : removed)
	{
		if (const ModifierListTemplate* tmpl = TheAttributeModifierStore->get(list))
		{
			if (!obj->isEffectivelyDead())
				tmpl->playFX(obj, TRUE);
		}
		revokeUpgrades(list);
	}
	for (Int list : added)
	{
		const ModifierListTemplate* tmpl = TheAttributeModifierStore->get(list);
		if (!tmpl)
			continue;
		tmpl->playFX(obj, FALSE);
		if (tmpl->m_clearModelCondition >= 0)
			obj->clearModelConditionState((ModelConditionFlagType)tmpl->m_clearModelCondition);
		grantUpgrades(list);
	}
	refreshDerived();
	if (HordeContain::get(obj))
		refreshMembers();
}

void AttributeModifierHelper::refreshMembers()
{
	ContainModuleInterface* contain = getObject()->getContain();
	const ContainedItemsList* members = contain ? contain->getContainedItemsList() : nullptr;
	if (!members)
		return;
	for (Object* member : *members)
	{
		if (AttributeModifierHelper* helper = helperOf(member))
			helper->refreshDerived();
	}
}

void AttributeModifierHelper::refreshDerived()
{
	Object* obj = getObject();
	const Totals& totals = getTotals();

	const Real add = totals.value[HEALTH];
	const Real mult = totals.value[HEALTH_MULT];
	BodyModuleInterface* body = obj->getBodyModule();
	if (body && !obj->isEffectivelyDead() && (add != m_appliedHealthAdd || mult != m_appliedHealthMult) && body->getMaxHealth() > 0.0f)
	{
		const Real newMax = maxHealthAfter(body->getMaxHealth(), m_appliedHealthAdd, m_appliedHealthMult, add, mult);
		MaxHealthChangeType type = SAME_CURRENTHEALTH;
		if (mult != m_appliedHealthMult)
			type = PRESERVE_RATIO;
		else if (add > m_appliedHealthAdd)
			type = ADD_CURRENT_HEALTH_TOO;
		body->setMaxHealth(newMax, type);
		m_appliedHealthAdd = add;
		m_appliedHealthMult = mult;
	}

	const Real shroud = totals.shroudFactor();
	if (shroud != m_appliedShroudFactor)
	{
		m_appliedShroudFactor = shroud;
		obj->refreshModifiedShroudClearing();
	}

	std::vector<Int> conditions;
	const AttributeModifierHelper* horde = hordeHelper();
	const Stack* const stacks[] = { &m_stack, horde ? &horde->m_stack : nullptr };
	for (const Stack* stack : stacks)
	{
		if (!stack)
			continue;
		for (const Active& active : stack->active())
		{
			const ModifierListTemplate* tmpl = TheAttributeModifierStore->get(active.list);
			if (tmpl && tmpl->m_modelCondition >= 0 &&
				std::find(conditions.begin(), conditions.end(), tmpl->m_modelCondition) == conditions.end())
				conditions.push_back(tmpl->m_modelCondition);
		}
	}
	const ModelConditionFlags previousConditions = obj->getLogicModelConditions();
	for (Int condition : m_appliedConditions)
	{
		if (std::find(conditions.begin(), conditions.end(), condition) == conditions.end())
			obj->clearModelConditionState((ModelConditionFlagType)condition);
	}
	for (Int condition : conditions)
	{
		if (std::find(m_appliedConditions.begin(), m_appliedConditions.end(), condition) == m_appliedConditions.end())
			obj->setModelConditionState((ModelConditionFlagType)condition);
	}
	m_appliedConditions = conditions;
	if (previousConditions != obj->getLogicModelConditions())
		RetailBuildingEvents::modelConditionsChanged(*obj, previousConditions);
}

void AttributeModifierHelper::scheduleWake()
{
	if (getObject()->testStatus(OBJECT_STATUS_DESTROYED))
		return;

	UnsignedInt next = m_stack.nextExpiry();
	for (const PendingUpgrade& pending : m_pendingUpgrades)
	{
		if (next == 0 || pending.frame < next)
			next = pending.frame;
	}
	const UnsignedInt now = TheGameLogic->getFrame();
	const UnsignedInt wake = next == 0 ? FOREVER : std::max(next, now + 1);
	if (friend_getIndexInLogic() == -1)
		friend_setNextCallFrame(wake);
	else
		sleepUntil(wake);
}

UpdateSleepTime AttributeModifierHelper::update()
{
	const UnsignedInt now = TheGameLogic->getFrame();
	std::vector<PendingUpgrade> due;
	for (size_t index = 0; index < m_pendingUpgrades.size();)
	{
		if (m_pendingUpgrades[index].frame <= now)
		{
			due.push_back(m_pendingUpgrades[index]);
			m_pendingUpgrades.erase(m_pendingUpgrades.begin() + static_cast<std::ptrdiff_t>(index));
		}
		else
			++index;
	}
	for (const PendingUpgrade& pending : due)
	{
		const ModifierListTemplate* tmpl = TheAttributeModifierStore->get(pending.list);
		if (!tmpl || !m_stack.contains(pending.list))
			continue;
		for (const AsciiString& name : tmpl->m_upgradeNames)
		{
			const UpgradeTemplate* upgrade = upgradeNamed(name);
			if (upgrade && upgrade->getUpgradeType() == UPGRADE_TYPE_OBJECT && !getObject()->hasUpgrade(upgrade))
				getObject()->giveUpgrade(upgrade);
		}
	}

	const std::vector<int> expired = m_stack.expire(now);
	if (!expired.empty())
		changed({}, std::vector<Int>(expired.begin(), expired.end()));

	UnsignedInt next = m_stack.nextExpiry();
	for (const PendingUpgrade& pending : m_pendingUpgrades)
	{
		if (next == 0 || pending.frame < next)
			next = pending.frame;
	}
	if (next == 0)
		return UPDATE_SLEEP_FOREVER;
	return UPDATE_SLEEP(next > now ? next - now : 1);
}

void AttributeModifierHelper::crc( Xfer *xfer )
{
	ObjectHelper::crc( xfer );
	for (const Active& active : m_stack.active())
	{
		Int list = active.list;
		UnsignedInt endFrame = active.endFrame;
		UnsignedInt antiMask = active.antiMask;
		xfer->xferInt( &list );
		xfer->xferUnsignedInt( &endFrame );
		xfer->xferUnsignedInt( &antiMask );
		for (const Hold& hold : active.holds)
		{
			UnsignedInt ownerObject = ownerObjectOf(hold.owner);
			UnsignedInt holdEnd = hold.endFrame;
			xfer->xferUnsignedInt( &ownerObject );
			xfer->xferUnsignedInt( &holdEnd );
		}
	}
	xferRetailBlocks(xfer, TRUE);
}

void AttributeModifierHelper::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 3;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );
	if (version < 2)
		throw XFER_INVALID_VERSION;
	ObjectHelper::xfer( xfer );

	UnsignedShort count = static_cast<UnsignedShort>(m_stack.active().size());
	xfer->xferUnsignedShort( &count );
	if (xfer->getXferMode() == XFER_LOAD)
		m_stack.clear();
	for (UnsignedShort index = 0; index < count; ++index)
	{
		Active active;
		AsciiString name;
		if (xfer->getXferMode() != XFER_LOAD)
		{
			active = m_stack.active()[index];
			if (const ModifierListTemplate* tmpl = TheAttributeModifierStore->get(active.list))
				name = tmpl->m_name;
		}
		xfer->xferAsciiString( &name );
		xfer->xferUnsignedInt( &active.endFrame );
		xfer->xferUnsignedInt( &active.antiMask );
		xferHolds( xfer, active.holds );
		if (xfer->getXferMode() == XFER_LOAD)
		{
			active.list = TheAttributeModifierStore ? TheAttributeModifierStore->findIndex(name) : -1;
			if (active.list >= 0)
				m_stack.mutableActive().push_back(active);
		}
	}

	UnsignedShort pending = static_cast<UnsignedShort>(m_pendingUpgrades.size());
	xfer->xferUnsignedShort( &pending );
	if (xfer->getXferMode() == XFER_LOAD)
		m_pendingUpgrades.clear();
	for (UnsignedShort index = 0; index < pending; ++index)
	{
		PendingUpgrade entry{-1, 0};
		AsciiString name;
		if (xfer->getXferMode() != XFER_LOAD)
		{
			entry = m_pendingUpgrades[index];
			if (const ModifierListTemplate* tmpl = TheAttributeModifierStore->get(entry.list))
				name = tmpl->m_name;
		}
		xfer->xferAsciiString( &name );
		xfer->xferUnsignedInt( &entry.frame );
		if (xfer->getXferMode() == XFER_LOAD)
		{
			entry.list = TheAttributeModifierStore ? TheAttributeModifierStore->findIndex(name) : -1;
			if (entry.list >= 0)
				m_pendingUpgrades.push_back(entry);
		}
	}

	xfer->xferReal( &m_appliedHealthAdd );
	xfer->xferReal( &m_appliedHealthMult );
	xfer->xferReal( &m_appliedShroudFactor );
	UnsignedShort conditions = static_cast<UnsignedShort>(m_appliedConditions.size());
	xfer->xferUnsignedShort( &conditions );
	if (xfer->getXferMode() == XFER_LOAD)
		m_appliedConditions.assign(conditions, -1);
	for (UnsignedShort index = 0; index < conditions; ++index)
		xfer->xferInt( &m_appliedConditions[index] );
	xferRetailBlocks(xfer, version >= 3);
	if (xfer->getXferMode() == XFER_LOAD)
	{
		++m_version;
		m_cacheValid = FALSE;
	}
}

void AttributeModifierHelper::xferRetailBlocks(Xfer* xfer, Bool present)
{
	const Bool loading = xfer->getXferMode() == XFER_LOAD;
	RetailSaveBlocks::transferSection(xfer, loading, present, {
		{"weather-anti-category", [this, loading](Xfer* block)
		{
			UnsignedInt mask = m_stack.weatherAntiMask();
			block->xferUnsignedInt(&mask);
			if (loading)
				m_stack.setWeatherAntiMask(mask);
		}, [this] { m_stack.setWeatherAntiMask(0); }}
	});
}

void AttributeModifierHelper::loadPostProcess()
{
	ObjectHelper::loadPostProcess();
}

namespace AttributeModifiers
{
const Totals* totalsOf( const Object* obj )
{
	const AttributeModifierHelper* helper = helperOf(obj);
	if (!helper || !helper->hasAnyModifier())
		return nullptr;
	return &helper->getTotals();
}

Int findList( const AsciiString& listName )
{
	return TheAttributeModifierStore ? TheAttributeModifierStore->findIndex(listName) : -1;
}

Owner ownerOf( ObjectID objectID, NameKeyType moduleTag )
{
	return makeOwner(static_cast<UnsignedInt>(objectID), moduleTag);
}

Owner ownerOf( const Object* obj, NameKeyType moduleTag )
{
	return obj ? ownerOf(obj->getID(), moduleTag) : ANONYMOUS_OWNER;
}

Bool apply( Object* target, Int list, UnsignedInt durationFrames, UnsignedInt antiMask, Owner owner )
{
	AttributeModifierHelper* helper = helperOf(target);
	if (!helper || list < 0)
		return FALSE;
	return helper->apply(list, durationFrames, antiMask, owner) != IGNORED;
}

Bool apply( Object* target, const AsciiString& listName, UnsignedInt durationFrames, UnsignedInt antiMask, Owner owner )
{
	return apply(target, findList(listName), durationFrames, antiMask, owner);
}

Bool remove( Object* target, const AsciiString& listName, Owner owner )
{
	AttributeModifierHelper* helper = helperOf(target);
	const Int list = findList(listName);
	return helper && list >= 0 && helper->remove(list, owner);
}

UnsignedInt categoryMask( const std::vector<Int>& categories )
{
	UnsignedInt mask = 0;
	for (Int category : categories)
		mask |= categoryBit(category);
	return mask;
}

Real damageDealtFactor( const Object* obj )
{
	const Totals* totals = totalsOf(obj);
	return totals ? totals->damageDealtFactor() : 1.0f;
}

Real damageTakenFactor( const Object* obj, DamageType type )
{
	const Totals* totals = totalsOf(obj);
	return totals ? totals->damageTakenFactor(static_cast<int>(type), TheGlobalData->m_attributeModifierArmorMaxBonus) : 1.0f;
}

Real rangeFactor( const Object* obj )
{
	const Totals* totals = totalsOf(obj);
	return totals ? totals->rangeFactor() : 1.0f;
}

Real rateOfFireFactor( const Object* obj )
{
	return value(obj, RATE_OF_FIRE);
}

Real speedFactor( const Object* obj )
{
	return value(obj, SPEED);
}

Real visionFactor( const Object* obj )
{
	const Totals* totals = totalsOf(obj);
	return totals ? totals->visionFactor() : 1.0f;
}

Real shroudFactor( const Object* obj )
{
	const Totals* totals = totalsOf(obj);
	return totals ? totals->shroudFactor() : 1.0f;
}

Real experienceFactor( const Object* obj )
{
	return value(obj, EXPERIENCE);
}

Real productionFactor( const Object* obj )
{
	return value(obj, PRODUCTION);
}

Real rechargeFactor( const Object* obj )
{
	return value(obj, RECHARGE_TIME);
}

Real value( const Object* obj, Int kind )
{
	const Totals* totals = totalsOf(obj);
	if (totals && kind > KIND_NONE && kind < KIND_COUNT)
		return totals->value[kind];
	return isMultiplicative(kind) ? 1.0f : 0.0f;
}

Bool sidesMatch( const Object* source, const Object* target, const SpecialPowerObjectFilter* filter, Sides sides )
{
	constexpr UnsignedInt RELATION_RULES = (1u << 0) | (1u << 1) | (1u << 2) | (1u << 3) | (1u << 10) | (1u << 13);
	if (filter && (filter->m_rules & RELATION_RULES))
		return TRUE;
	if (sides == SIDES_ALL || source == target)
		return TRUE;
	const Relationship relationship = source->getRelationship(target);
	if (sides == SIDES_ENEMIES)
		return relationship == ENEMIES;
	return relationship == ALLIES || source->getControllingPlayer() == target->getControllingPlayer();
}

void applyInRange( Object* source, const Coord3D* center, Real range, const SpecialPowerObjectFilter* filter, Bool affectSelf,
	Int list, UnsignedInt durationFrames, UnsignedInt antiMask, const AsciiString& fxName, Sides sides, Bool affectAllies )
{
	if (!source || list < 0 || !ThePartitionManager)
		return;
	const Coord3D* pos = center ? center : source->getPosition();
	const FXList* fx = fxName.isNotEmpty() && TheFXListStore ? TheFXListStore->findFXList(fxName.str()) : nullptr;
	const auto affect = [&](Object* target)
	{
		if (apply(target, list, durationFrames, antiMask) && fx)
			FXList::doFXObj(fx, target);
	};
	if (affectSelf)
		affect(source);
	if (range <= 0.0f)
		return;
	SimpleObjectIterator* iter = ThePartitionManager->iterateObjectsInRange(pos, range, FROM_CENTER_2D);
	MemoryPoolObjectHolder hold(iter);
	for (Object* target = iter->first(); target; target = iter->next())
	{
		if (target == source || target->isEffectivelyDead())
			continue;
		if (filter && !objectFilterMatches(*filter, source, target))
			continue;
		if (!sidesMatch(source, target, filter, sides))
			continue;
		if (!affectAllies && target->getControllingPlayer() != source->getControllingPlayer())
			continue;
		affect(target);
	}
}

void describe( const Object* obj, AsciiString& out )
{
	out.clear();
	const AttributeModifierHelper* helper = helperOf(obj);
	if (!helper)
		return;
	const UnsignedInt now = TheGameLogic->getFrame();
	for (const Active& active : helper->getStack().active())
	{
		const ModifierListTemplate* tmpl = TheAttributeModifierStore->get(active.list);
		AsciiString entry;
		entry.format("%s%s(%d)", out.isEmpty() ? "" : ",", tmpl ? tmpl->m_name.str() : "?",
			active.endFrame == 0 ? -1 : static_cast<Int>(active.endFrame - now));
		out.concat(entry);
	}
}
}
