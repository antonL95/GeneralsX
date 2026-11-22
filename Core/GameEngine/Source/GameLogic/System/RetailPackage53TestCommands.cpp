#include "PreRTS.h"

#if !RTS_GENERALS
#include "Common/ThingTemplate.h"
#include "GameClient/Drawable.h"
#include "GameLogic/Object.h"
#include "GameLogic/Weapon.h"

#include <cstdio>
#include <string_view>

void runRetailPackage53TestCommand(std::string_view action, Object* source, Object* target, UnsignedInt frame)
{
	if (action == "pkg53-flyer-hit" && target)
	{
		const auto* weapon = TheWeaponStore->findWeaponTemplate("EaglePlowAttack");
		if (weapon)
		{
			std::fprintf(stderr, "TEST pkg53 flyer-hit frame=%u source=%s target=%s structure=%d\n", frame,
				source->getTemplate()->getName().str(), target->getTemplate()->getName().str(), target->isKindOf(KINDOF_STRUCTURE));
			TheWeaponStore->createAndFireTempWeapon(weapon, source, target, TRUE);
		}
	}
	else if (action == "pkg53-disable")
		source->setDisabled(DISABLED_EMP);
	else if (action == "pkg53-tint")
	{
		Vector3 composed;
		const Drawable* draw = source->getDrawable();
		const Vector3* tint = draw ? draw->getTintColor(composed) : nullptr;
		std::fprintf(stderr, "TEST pkg53 tint frame=%u disabled=%d rgb=(%.3f,%.3f,%.3f)\n", frame,
			source->isDisabledByType(DISABLED_EMP), tint ? tint->X : 0.0f, tint ? tint->Y : 0.0f, tint ? tint->Z : 0.0f);
	}
}
#endif
