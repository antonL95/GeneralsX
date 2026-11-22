#pragma once

#include "GameLogic/Module/UpdateModule.h"

class Object;

class ClickReactionBehaviorModuleData : public UpdateModuleData
{
public:
	Int m_clickReactionTimer = 600;
	Int m_reactionFrames[5] = { 0, 0, 0, 0, 0 };

	static void buildFieldParse(MultiIniFieldParse& p);
};

class ClickReactionBehavior : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(ClickReactionBehavior, "ClickReactionBehavior")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(ClickReactionBehavior, ClickReactionBehaviorModuleData)

public:
	ClickReactionBehavior(Thing* thing, const ModuleData* moduleData);

	UpdateSleepTime update() override;
	void onClick();
	Int getTimer() const { return m_timer; }
	Int getReactionCountdown() const { return m_reactionCountdown; }
	Int getActiveReactionLevel() const { return m_activeLevel; }

	static ClickReactionBehavior* find(Object* obj);

private:
	void clearReactions();

	Int m_timer = 0;
	Int m_reactionCountdown = 0;
	Int m_activeLevel = 0;
};
