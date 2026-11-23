#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/ThingTemplate.h"
#include "GameClient/View.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/Weapon.h"
#include "WWMath/wwmath.h"

namespace
{
void traceFlight(const RetailTestCommands::Context& ctx)
{
	const Object* actor = ctx.requireObject(true);
	if (!actor)
		return;
	const AIUpdateInterface* ai = actor->getAI();
	const PhysicsBehavior* physics = actor->getPhysics();
	const Locomotor* locomotor = ai ? ai->getCurLocomotor() : nullptr;
	if (!physics || !locomotor)
		return;

	const Coord3D& at = *actor->getPosition();
	const Coord3D& velocity = *physics->getVelocity();
	const Vector3 lateral = actor->getTransformMatrix()->Get_Y_Vector();
	const Coord3D& heading = *actor->getUnitDirectionVector2D();
	const Real bank = WWMath::Atan2Origin(-lateral.Z, heading.x * lateral.Y - heading.y * lateral.X);
	const Weapon* weapon = actor->getCurrentWeapon();
	std::fprintf(stderr,
		"TEST air-swoop frame=%u actor=%s loco=%s swoop=%d x=%.3f y=%.3f height=%.3f vx=%.3f vy=%.3f vz=%.3f yaw=%.5f bank=%.5f preferred=%.3f shot=%u\n",
		ctx.frame, actor->getTemplate()->getName().str(), locomotor->getTemplateName().str(),
		actor->getLogicModelConditions().test(MODELCONDITION_SWOOPING) ? 1 : 0,
		at.x, at.y, actor->getHeightAboveTerrain(), velocity.x, velocity.y, velocity.z,
		actor->getOrientation(), bank, locomotor->getPreferredHeight(), weapon ? weapon->getLastShotFrame() : 0u);
}

void frameFlight(const RetailTestCommands::Context& ctx)
{
	const Object* actor = ctx.requireObject(true);
	if (actor && TheTacticalView)
		TheTacticalView->lookAt(actor->getPosition());
}
}
#endif

namespace RetailTestCommands
{
void registerRetailAirSwoopTestCommands()
{
#if !RTS_GENERALS
	registerAction("air-swoop-trace", traceFlight);
	registerAction("air-swoop-camera", frameFlight);
#endif
}
}
