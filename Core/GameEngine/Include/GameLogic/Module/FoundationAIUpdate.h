#pragma once

#include "GameLogic/Module/UpdateModule.h"

class Player;
class ThingTemplate;

class FoundationAIUpdateModuleData : public UpdateModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& p);

	Int m_buildVariation = 0;
};

class FoundationAIUpdate final : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(FoundationAIUpdate, "FoundationAIUpdate")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(FoundationAIUpdate, FoundationAIUpdateModuleData)

public:
	FoundationAIUpdate(Thing* thing, const ModuleData* data);

	static FoundationAIUpdate* find(const Object& object);

	UpdateSleepTime update() override;
	Object* construct(const ThingTemplate* what, Player* player, Bool prebuilt = FALSE);
	Bool canBuildOn(const Player* player) const;
	static void advanceConstruction(Object& structure, Object* builder, Int overrideFrames = 0);
	Object* getStructure() const;

private:
	void showFoundation(Bool show);

	ObjectID m_structure = INVALID_ID;
};
