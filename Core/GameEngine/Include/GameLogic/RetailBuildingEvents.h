#pragma once

#include "Common/ModelState.h"

class Object;

namespace RetailBuildingEvents
{
void created(Object& object);
void complete(Object& object);
void riderChanged(Object& object, bool mounted);
void trollEatingChanged(Object& object, bool enabled);
#if !RTS_GENERALS
void modelConditionsChanged(Object& object, const ModelConditionFlags& previous);
void restoreHordeConditionUpgrades();
#endif
void reallyDamaged(Object& object);
int enrageAction(const Object& object, const char* event);
}
