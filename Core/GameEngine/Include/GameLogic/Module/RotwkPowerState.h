#pragma once

#include "Common/ModelState.h"
#include "Common/ObjectStatusTypes.h"

struct RotwkPowerState
{
	ModelConditionFlags invisibilityForbidden;
	Real detectionRange = 0.0f;
	UnsignedInt invisibilityEnd = 0;
	Bool powerStealthed = FALSE;
	Int condition = MODELCONDITION_INVALID;
	UnsignedInt conditionEnd = 0;
	ObjectID passenger = INVALID_ID;
	UnsignedInt consumeFrame = 0;
	Real healFraction = 0.0f;
};
