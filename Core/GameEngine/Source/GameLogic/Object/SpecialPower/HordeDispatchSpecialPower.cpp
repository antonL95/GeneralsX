#include "PreRTS.h"

#include "GameLogic/Module/HordeDispatchSpecialPower.h"

#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Object.h"

namespace
{
template<typename Cast>
void dispatchToMembers(Object* horde, const SpecialPowerTemplate* power, Cast cast)
{
	const ContainedItemsList* members = horde->getContain()->getContainedItemsList();
	if (!members)
		return;

	std::vector<ObjectID> memberIDs;
	memberIDs.reserve(members->size());
	for (const Object* member : *members)
		memberIDs.push_back(member->getID());

	for (ObjectID id : memberIDs)
	{
		Object* member = TheGameLogic->findObjectByID(id);
		if (!member || member->isEffectivelyDead() || member->getContainedBy() != horde)
			continue;
		if (SpecialPowerModuleInterface* module = member->getSpecialPowerModule(power))
			cast(module);
	}
}
}

HordeDispatchSpecialPower::~HordeDispatchSpecialPower() = default;

void HordeDispatchSpecialPower::doSpecialPower(UnsignedInt commandOptions)
{
	Object* horde = getObject();
	if (!horde->isKindOf(KINDOF_HORDE) || !horde->getContain() || m_pausedCount > 0 || horde->isDisabled() || !isRetailCastAllowed(commandOptions))
		return;

	SpecialPowerModule::doSpecialPower(commandOptions);
	if (!wasLastCastAccepted())
		return;
	dispatchToMembers(horde, getSpecialPowerTemplate(), [commandOptions](SpecialPowerModuleInterface* member)
	{
		member->doSpecialPower(commandOptions);
	});
}

void HordeDispatchSpecialPower::doSpecialPowerAtObject(Object* target, UnsignedInt commandOptions)
{
	Object* horde = getObject();
	if (!target || !horde->isKindOf(KINDOF_HORDE) || !horde->getContain() || m_pausedCount > 0 || horde->isDisabled() || !isRetailCastAllowed(commandOptions))
		return;

	SpecialPowerModule::doSpecialPowerAtObject(target, commandOptions);
	if (!wasLastCastAccepted())
		return;
	dispatchToMembers(horde, getSpecialPowerTemplate(), [target, commandOptions](SpecialPowerModuleInterface* member)
	{
		member->doSpecialPowerAtObject(target, commandOptions);
	});
}

void HordeDispatchSpecialPower::doSpecialPowerAtLocation(const Coord3D* position, Real angle, UnsignedInt commandOptions)
{
	Object* horde = getObject();
	if (!position || !horde->isKindOf(KINDOF_HORDE) || !horde->getContain() || m_pausedCount > 0 || horde->isDisabled() || !isRetailCastAllowed(commandOptions))
		return;

	SpecialPowerModule::doSpecialPowerAtLocation(position, angle, commandOptions);
	if (!wasLastCastAccepted())
		return;
	dispatchToMembers(horde, getSpecialPowerTemplate(), [position, angle, commandOptions](SpecialPowerModuleInterface* member)
	{
		member->doSpecialPowerAtLocation(position, angle, commandOptions);
	});
}
