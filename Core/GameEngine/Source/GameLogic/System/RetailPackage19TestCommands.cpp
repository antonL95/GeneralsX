#include "PreRTS.h"

#if !RTS_GENERALS
#include <cstdio>
#include <string_view>

#include "Common/ThingTemplate.h"
#include "GameClient/CommandXlat.h"
#include "GameLogic/Module/DumbProjectileBehavior.h"
#include "GameLogic/Object.h"
#include "GameLogic/Weapon.h"

void runRetailPackage19TestCommand(std::string_view action, Object* source, UnsignedInt frame)
{
	if (!source)
		return;
	if (action == "pkg19-secondary")
	{
		source->setWeaponLock(SECONDARY_WEAPON, LOCKED_PERMANENTLY);
		return;
	}
	if (action == "pkg19-voice")
	{
		DrawableList drawables{source->getDrawable()};
		pickAndPlayUnitVoiceResponse(&drawables, GameMessage::MSG_DO_ATTACK_OBJECT);
		return;
	}
	if (action != "pkg19-state")
		return;
	const Weapon* weapon = source->getCurrentWeapon();
	const auto* flight = dynamic_cast<const DumbProjectileBehavior*>(source->getProjectileUpdateInterface());
	std::fprintf(stderr, "TEST pkg19 frame=%u template=%s weapon=%s locked=%d follow=%d draw=%u next=%u ammo=%u flight=%d pos=%.1f,%.1f,%.1f\n",
		frame, source->getTemplate()->getName().str(), weapon ? weapon->getName().str() : "-",
		source->isCurWeaponLocked(), weapon && weapon->isWaitingToIdle(),
		weapon ? weapon->getPreAttackFinishedFrame() : 0, weapon ? weapon->getPossibleNextShotFrame() : 0,
		weapon ? weapon->getRemainingAmmo() : 0, flight && flight->isSelfFlight(),
		source->getPosition()->x, source->getPosition()->y, source->getPosition()->z);
}
#endif
