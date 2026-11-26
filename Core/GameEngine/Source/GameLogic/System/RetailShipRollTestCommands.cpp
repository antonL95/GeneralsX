#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/Module.h"
#include "Common/ThingTemplate.h"
#include "GameClient/Drawable.h"
#include "Common/DrawModule.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"
#include "WWMath/wwmath.h"

namespace
{
Real roll(const Matrix3D& transform)
{
	return WWMath::Atan2Origin(-transform.Get_Y_Vector().Z, transform.Get_Z_Vector().Z);
}

void sampleShipRoll(const RetailTestCommands::Context& ctx)
{
	Object* ship = ctx.requireObject(true);
	if (!ship || !ship->getDrawable() || !ship->getAI())
		return;
	const Locomotor* locomotor = ship->getAI()->getCurLocomotor();
	if (!locomotor)
		return;

	const NameKeyType hull = TheNameKeyGenerator->nameToKey(ctx.argument);
	for (const DrawModule** module = static_cast<const Drawable*>(ship->getDrawable())->getDrawModules();
		module && *module; ++module)
	{
		if ((*module)->getModuleTagNameKey() != hull)
			continue;
		const ObjectDrawInterface* draw = (*module)->getObjectDrawInterface();
		Coord3D renderedPosition;
		Real radius;
		Matrix3D drawn;
		if (!draw || !draw->clientOnly_getRenderObjInfo(&renderedPosition, &radius, &drawn))
			return;

		const Coord3D& pose = *ship->getPosition();
		Real water = 0.0f;
		const Bool underwater = TheTerrainLogic->isUnderwater(pose.x, pose.y, &water);
		std::fprintf(stderr,
			"TEST ship-roll frame=%u ship=%s loco=%s drawn=%.8f logic=%.8f x=%.5f y=%.5f z=%.5f yaw=%.7f water=%.5f underwater=%d\n",
			ctx.frame, ship->getTemplate()->getName().str(), locomotor->getTemplateName().str(),
			roll(drawn), roll(*ship->getTransformMatrix()), pose.x, pose.y, pose.z, ship->getOrientation(), water, int(underwater));
		return;
	}
}
}
#endif

namespace RetailTestCommands
{
void registerRetailShipRollTestCommands()
{
#if !RTS_GENERALS
	registerAction("ship-roll-sample", sampleShipRoll);
#endif
}
}
