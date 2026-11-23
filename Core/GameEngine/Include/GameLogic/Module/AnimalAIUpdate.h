#pragma once

#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/CollideModule.h"

class AnimalAIUpdateModuleData final : public AIUpdateModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& p);

	Int m_fleeRange = 0;
	Int m_fleeDistance = 0;
	Int m_wanderPercentage = 0;
	Int m_maxWanderDistance = 0;
	Int m_maxWanderRadius = 0;
	Int m_updateTimer = 0;
	Bool m_afraidOfCastles = FALSE;
};

class AnimalAIUpdate final : public AIUpdateInterface
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(AnimalAIUpdate, "AnimalAIUpdate")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(AnimalAIUpdate, AnimalAIUpdateModuleData)

public:
	AnimalAIUpdate(Thing* thing, const ModuleData* data);
	UpdateSleepTime update() override;
	void fleeFrom(const Object& threat);

private:
	void xferAnimalState(Xfer* xfer);
	Object* findThreat(Real range) const;
	void wander();

	Coord3D m_home = {};
	ObjectID m_fleeFrom = INVALID_ID;
	Bool m_hasHome = FALSE;
};

class FXList;
class ObjectCreationList;

class CritterEmitterUpdateModuleData final : public CollideModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& p);

	std::vector<const FXList*> m_effects;
	std::vector<const ObjectCreationList*> m_spawns;
	UnsignedInt m_reloadTime = 0;
};

class CritterEmitterUpdate final : public CollideModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(CritterEmitterUpdate, "CritterEmitterUpdate")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(CritterEmitterUpdate, CritterEmitterUpdateModuleData)

public:
	CritterEmitterUpdate(Thing* thing, const ModuleData* data);
	Bool wouldLikeToCollideWith(const Object* other) const override;
	void onCollide(Object* other, const Coord3D* location, const Coord3D* normal) override;

private:
	UnsignedInt m_nextEmission = 0;
};
