#include "PreRTS.h"

#if !RTS_GENERALS
#include <cstdio>
#include <string_view>

#include "Common/ThingTemplate.h"
#include "GameLogic/Module/StealthUpdate.h"
#include "GameLogic/Object.h"

void runRetailPackage41TestCommand(std::string_view action, Object* source, UnsignedInt frame)
{
	if (!source)
		return;
	StealthUpdate* stealth = source->getStealth();
	if (action == "pkg41-toggle")
	{
		if (stealth)
			stealth->toggleRing();
		std::fprintf(stderr, "TEST pkg41-toggle frame=%u object=%s can=%d\n",
			frame, source->getTemplate()->getName().str(), stealth && stealth->canToggleRing());
	}
	else if (action == "pkg41-ringstate")
	{
		const auto& conds = source->getLogicModelConditions();
		std::fprintf(stderr,
			"TEST pkg41-ringstate frame=%u object=%s worn=%d stealthed=%d putting=%d taking=%d can_toggle=%d toggle_frame=%u anim_end=%u blocked_until=%u\n",
			frame,
			source->getTemplate()->getName().str(),
			source->testWeaponSetFlag(WEAPONSET_ONE_RING_MODE) ? 1 : 0,
			source->testStatus(OBJECT_STATUS_STEALTHED) ? 1 : 0,
			conds.test(MODELCONDITION_PUTTING_ON_RING) ? 1 : 0,
			conds.test(MODELCONDITION_TAKING_OFF_RING) ? 1 : 0,
			stealth && stealth->canToggleRing() ? 1 : 0,
			stealth ? stealth->getRingToggleFrame() : 0u,
			stealth ? stealth->getRingAnimEndFrame() : 0u,
			stealth ? stealth->getRingBlockedUntilFrame() : 0u);
	}
}
#endif
