#pragma once

#include "Common/SpecialPowerINI.h"
#include "GameLogic/Module/UpdateModule.h"

#include <cstdint>

class Player;
class ThingTemplate;

class TerrainResourceBehaviorModuleData : public UpdateModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& p);

	Real m_radius = 0.0f;
	Int m_maxIncome = 0;
	UnsignedInt m_interval = UPDATE_SLEEP_FOREVER;
	Bool m_highPriority = FALSE;
	Bool m_visible = TRUE;
	AsciiString m_upgrade;
	Real m_upgradeScale = 1.0f;
	SpecialPowerObjectFilter m_upgradeFilter;
};

class TerrainResourceBehavior final : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(TerrainResourceBehavior, "TerrainResourceBehavior")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(TerrainResourceBehavior, TerrainResourceBehaviorModuleData)

public:
	TerrainResourceBehavior(Thing* thing, const ModuleData* data);
	UpdateSleepTime update() override;

	static Bool getDisplayInfo(const ThingTemplate& type, const Coord3D& position, const Player& player,
		const Object* object, Real& radius, Real& efficiency);
	static Real getActiveClaimRadius(const Object& object);

private:
	struct ClaimYield
	{
		std::uint64_t available = 0;
		std::uint64_t total = 0;
		Real upgradeScale = 1.0f;
		Real playerScale = 1.0f;
	};

	static ClaimYield calculateClaim(const TerrainResourceBehaviorModuleData& data, const ThingTemplate& type,
		const Coord3D& position, const Player& player, const Object* object, UnsignedInt frame, UnsignedInt id);

	UnsignedInt m_claimFrame = 0;
	Bool m_claiming = FALSE;
};
