#include "PreRTS.h"

#include <cstdio>
#include <cstdlib>
#include <string_view>

#include "Common/BuildAssistant.h"
#include "Common/Player.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/Damage.h"
#include "GameLogic/Module/TerrainResourceBehavior.h"
#include "GameLogic/Object.h"

void runRetailPackage23TestCommand(std::string_view action, Player* player, Object* object, Object* target,
	const char* name, const Coord3D& point, UnsignedInt frame)
{
	if (!player)
		return;
	Money* money = player->getMoney();
	if (action == "pkg23-cash")
	{
		std::fprintf(stderr, "TEST pkg23 cash frame=%u label=%s amount=%u\n", frame, name, money->countMoney());
	}
	else if (action == "pkg23-set-cash")
	{
		money->setStartingCash(static_cast<UnsignedInt>(std::strtoul(name, nullptr, 10)));
	}
	else if (action == "pkg23-display")
	{
		const auto* type = object ? object->getTemplate() : TheThingFactory->findTemplate(name, FALSE);
		if (!type)
			return;
		Real radius = 0.0f;
		Real efficiency = 0.0f;
		const Bool indicator = TerrainResourceBehavior::getDisplayInfo(*type, object ? *object->getPosition() : point,
			*player, object, radius, efficiency);
		std::fprintf(stderr, "TEST pkg23 display frame=%u type=%s mode=%s indicator=%d radius=%.1f efficiency=%.3f\n",
			frame, type->getName().str(), object ? "selected" : "placement", indicator, radius, efficiency);
	}
	else if (action == "pkg23-hit" && object && target)
	{
		const Money* victimMoney = target->getControllingPlayer()->getMoney();
		const Int before = money->countMoney();
		const Int victimBefore = victimMoney->countMoney();
		DamageInfo damage;
		damage.in.m_sourceID = object->getID();
		damage.in.m_damageType = DAMAGE_UNRESISTABLE;
		damage.in.m_amount = point.x;
		target->attemptDamage(&damage);
		std::fprintf(stderr, "TEST pkg23 hit frame=%u source=%s target=%s damage=%.1f gained=%d lost=%d\n", frame,
			object->getTemplate()->getName().str(), target->getTemplate()->getName().str(),
			damage.out.m_actualDamageClipped, static_cast<Int>(money->countMoney()) - before,
			victimBefore - static_cast<Int>(victimMoney->countMoney()));
	}
	else if (action == "pkg23-pay" && object)
	{
		const UnsignedInt quote = std::string_view(name) == "quote" ? object->getTemplate()->calcCostToBuild(player) :
			static_cast<UnsignedInt>(std::strtoul(name, nullptr, 10));
		const UnsignedInt charge = money->withdraw(quote, FALSE);
		object->setConstructionCostPaid(charge);
		std::fprintf(stderr, "TEST pkg23 paid frame=%u type=%s charge=%u money=%u\n", frame,
			object->getTemplate()->getName().str(), charge, money->countMoney());
	}
	else if (action == "pkg23-quote" && object)
	{
		std::fprintf(stderr, "TEST pkg23 quote frame=%u type=%s price=%d receipt=%d\n", frame,
			object->getTemplate()->getName().str(), object->getTemplate()->calcCostToBuild(player),
			object->getConstructionCostPaid());
	}
	else if (action == "pkg23-sell" && object)
	{
		TheBuildAssistant->sellObject(object);
	}
	else if (action == "pkg23-kill" && object)
	{
		const Int before = money->countMoney();
		object->kill();
		std::fprintf(stderr, "TEST pkg23 destroy frame=%u name=%s refund=%d\n",
			frame, object->getTemplate()->getName().str(), static_cast<Int>(money->countMoney()) - before);
	}
}
