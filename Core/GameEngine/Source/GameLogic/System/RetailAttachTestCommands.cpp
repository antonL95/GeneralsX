#include "PreRTS.h"

#if !RTS_GENERALS
#include <cstdio>
#include <string_view>

#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/SlaughterHordeContain.h"
#include "GameLogic/Object.h"

void runRetailPackage12AttachTestCommand(std::string_view action, Object* source, UnsignedInt frame)
{
	if (action != "pkg12-attach")
		return;

	const AttachUpdate* attach = nullptr;
	const SlavedUpdateInterface* slaved = nullptr;
	for (BehaviorModule** module = source->getBehaviorModules(); *module; ++module)
	{
		if ((*module)->getModuleNameKey() == NAMEKEY("AttachUpdate"))
			attach = static_cast<const AttachUpdate*>(*module);
		if (const auto* candidate = (*module)->getSlavedUpdateInterface())
			slaved = candidate;
	}
	const ObjectID parentID = attach ? attach->getParentID() : INVALID_ID;
	const Object* parent = TheGameLogic->findObjectByID(parentID);
	const Coord3D* position = source->getPosition();
	std::fprintf(stderr, "TEST pkg12 attach frame=%u object=%u parent=%u slaver=%u attached=%d dead=%d x=%.2f y=%.2f z=%.2f anchorZ=%.2f\n",
		frame, static_cast<unsigned>(source->getID()), static_cast<unsigned>(parentID),
		static_cast<unsigned>(slaved ? slaved->getSlaverID() : INVALID_ID),
		source->testStatus(OBJECT_STATUS_ATTACHED), source->isEffectivelyDead(), position->x, position->y, position->z,
		parent ? parent->getPosition()->z + parent->getGeometryInfo().getMaxHeightAbovePosition() : 0.0f);
}
#endif
