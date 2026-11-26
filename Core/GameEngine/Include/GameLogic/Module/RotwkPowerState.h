#pragma once

#include <vector>

#include "Common/AsciiString.h"
#include "Common/ModelState.h"
#include "Common/ObjectStatusTypes.h"

struct RotwkPowerState
{
	struct InvisibilitySource
	{
		ObjectID object = INVALID_ID;
		AsciiString module;
		UnsignedInt expires = 0;
	};
	std::vector<InvisibilitySource> invisibilitySources;
	UnsignedInt detectionEnd = 0;
	UnsignedInt reinvisibilityFrame = 0;
	Bool hinting = FALSE;
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
