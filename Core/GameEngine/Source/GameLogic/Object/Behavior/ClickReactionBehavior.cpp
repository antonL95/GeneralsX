#include "PreRTS.h"

#include "Common/NameKeyGenerator.h"
#include "Common/Xfer.h"
#include "GameClient/CommandButtonRules.h"
#include "GameLogic/LogicRandomValue.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/ClickReactionBehavior.h"
#include "GameLogic/Object.h"

static ModelConditionFlagType reactionFlagForLevel(Int level)
{
	switch (level)
	{
		case 1: return MODELCONDITION_REACT_1;
		case 2: return MODELCONDITION_REACT_2;
		case 3: return MODELCONDITION_REACT_3;
		case 4: return MODELCONDITION_REACT_4;
		case 5: return MODELCONDITION_REACT_5;
		default: return MODELCONDITION_INVALID;
	}
}

void ClickReactionBehaviorModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpdateModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "ClickReactionTimer", INI::parseInt, nullptr, offsetof(ClickReactionBehaviorModuleData, m_clickReactionTimer) },
		{ "ReactionFrames1", INI::parseInt, nullptr, offsetof(ClickReactionBehaviorModuleData, m_reactionFrames[0]) },
		{ "ReactionFrames2", INI::parseInt, nullptr, offsetof(ClickReactionBehaviorModuleData, m_reactionFrames[1]) },
		{ "ReactionFrames3", INI::parseInt, nullptr, offsetof(ClickReactionBehaviorModuleData, m_reactionFrames[2]) },
		{ "ReactionFrames4", INI::parseInt, nullptr, offsetof(ClickReactionBehaviorModuleData, m_reactionFrames[3]) },
		{ "ReactionFrames5", INI::parseInt, nullptr, offsetof(ClickReactionBehaviorModuleData, m_reactionFrames[4]) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

ClickReactionBehavior::ClickReactionBehavior(Thing* thing, const ModuleData* moduleData)
	: UpdateModule(thing, moduleData)
{
}

ClickReactionBehavior::~ClickReactionBehavior() = default;

ClickReactionBehavior* ClickReactionBehavior::find(Object* obj)
{
	if (!obj)
		return nullptr;
	static const NameKeyType key = NAMEKEY("ClickReactionBehavior");
	for (BehaviorModule** module = obj->getBehaviorModules(); *module; ++module)
	{
		if ((*module)->getModuleNameKey() == key)
			return static_cast<ClickReactionBehavior*>(*module);
	}
	return nullptr;
}

void ClickReactionBehavior::clearReactions()
{
	Object* obj = getObject();
	m_reactionCountdown = 0;
	m_activeLevel = 0;
	if (!obj)
		return;
	ModelConditionFlags clearFlags;
	clearFlags.set(MODELCONDITION_REACT_1);
	clearFlags.set(MODELCONDITION_REACT_2);
	clearFlags.set(MODELCONDITION_REACT_3);
	clearFlags.set(MODELCONDITION_REACT_4);
	clearFlags.set(MODELCONDITION_REACT_5);
	clearFlags.set(MODELCONDITION_REACT_6);
	obj->clearModelConditionFlags(clearFlags);
}

void ClickReactionBehavior::onClick()
{
	Object* obj = getObject();
	if (!obj || obj->isEffectivelyDead())
		return;
	if (obj->testWeaponSetFlag(WEAPONSET_HERO_MODE))
		return;
	const AIUpdateInterface* ai = obj->getAI();
	if (!ai || !ai->isIdle())
		return;
	const ClickReactionBehaviorModuleData* data = getClickReactionBehaviorModuleData();
	if (data->m_clickReactionTimer < 1)
		return;

	Int randomLow = 1;
	const Int tentativeTimer = MIN(m_timer + data->m_clickReactionTimer, data->m_clickReactionTimer * 5);
	if (m_reactionCountdown <= 0 && (tentativeTimer / data->m_clickReactionTimer) < 4)
		randomLow = GameLogicRandomValue(1, 3);

	const CommandButtonRules::ClickReactionStep step = CommandButtonRules::advanceClickReaction(
		m_timer,
		m_reactionCountdown,
		data->m_clickReactionTimer,
		data->m_reactionFrames,
		randomLow);
	m_timer = step.nextTimer;
	m_reactionCountdown = step.nextCountdown;
	if (step.triggeredLevel > 0)
	{
		m_activeLevel = step.triggeredLevel;
		const ModelConditionFlagType flag = reactionFlagForLevel(step.triggeredLevel);
		if (flag != MODELCONDITION_INVALID)
			obj->setModelConditionState(flag);
		setWakeFrame(obj, UPDATE_SLEEP_NONE);
	}
}

UpdateSleepTime ClickReactionBehavior::update()
{
	Object* obj = getObject();
	const AIUpdateInterface* ai = obj ? obj->getAI() : nullptr;
	if (!obj || obj->isEffectivelyDead() || !ai)
		return UPDATE_SLEEP_FOREVER;
	if (m_timer > 0)
		--m_timer;
	if (m_reactionCountdown > 0)
	{
		--m_reactionCountdown;
		if (m_reactionCountdown == 0 || !ai->isIdle())
			clearReactions();
	}
	return (m_timer > 0 || m_reactionCountdown > 0) ? UPDATE_SLEEP_NONE : UPDATE_SLEEP_FOREVER;
}

void ClickReactionBehavior::crc(Xfer* xfer)
{
	UpdateModule::crc(xfer);
	xfer->xferInt(&m_timer);
	xfer->xferInt(&m_reactionCountdown);
	xfer->xferInt(&m_activeLevel);
}

void ClickReactionBehavior::xfer(Xfer* xfer)
{
	XferVersion version = 1;
	xfer->xferVersion(&version, 1);
	UpdateModule::xfer(xfer);
	xfer->xferInt(&m_timer);
	xfer->xferInt(&m_reactionCountdown);
	xfer->xferInt(&m_activeLevel);
}

void ClickReactionBehavior::loadPostProcess()
{
	UpdateModule::loadPostProcess();
}
