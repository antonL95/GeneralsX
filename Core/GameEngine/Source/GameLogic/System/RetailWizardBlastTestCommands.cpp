#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/Locomotor.h"
#include "WWMath/wwmath.h"
#endif

namespace RetailTestCommands
{
void registerRetailWizardBlastTestCommands()
{
#if !RTS_GENERALS
	registerAction("wizard-blast-probe", [](const Context& ctx) {
		const Object* caster = ctx.requireObject(true);
		const Object* target = ctx.findTarget(ctx.argument, true);
		if (!caster || !target)
			return;

		const auto& conditions = caster->getLogicModelConditions();
		const AIUpdateInterface* ai = caster->getAI();
		const Locomotor* locomotor = ai ? ai->getCurLocomotor() : nullptr;
		const Coord3D& from = *caster->getPosition();
		const Coord3D& to = *target->getPosition();
		const Real distance = WWMath::Sqrt(ThePartitionManager->getDistanceSquared(caster, target, FROM_BOUNDINGSPHERE_2D));
		std::fprintf(stderr, "TEST wizard-blast frame=%u caster=%s distance=%.3f ai=%d moving=%d drawmove=%d unpack=%d prepare=%d weapon2=%d weapon3=%d hp=%.1f facing=%.4f delta=%.2f,%.2f loco=%s packing=%d\n",
			ctx.frame, ctx.targetName, distance, ai ? static_cast<Int>(ai->getAIStateType()) : -1, ai && ai->isMoving(),
			conditions.test(MODELCONDITION_MOVING), conditions.test(MODELCONDITION_UNPACKING),
			conditions.test(MODELCONDITION_PREPARING), conditions.test(MODELCONDITION_SPECIAL_WEAPON_TWO),
			conditions.test(MODELCONDITION_SPECIAL_WEAPON_THREE), target->getBodyModule()->getHealth(),
			caster->getOrientation(), to.x - from.x, to.y - from.y,
			locomotor ? locomotor->getTemplateName().str() : "-", conditions.test(MODELCONDITION_PACKING));
	});
#endif
}
}
