#pragma once

#include "Common/ModelState.h"

class Object;

namespace RetailBuildingEvents
{
void created(Object& object);
void complete(Object& object);
void damaged(Object& object);
void riderChanged(Object& object, bool mounted);
void trollEatingChanged(Object& object, bool enabled);
#if !RTS_GENERALS
void modelConditionsChanged(Object& object, const ModelConditionFlags& previous);
void restoreHordeConditionUpgrades();
void aflame(Object& object);
#endif
void reallyDamaged(Object& object);
int enrageAction(const Object& object, const char* event);
#if !RTS_GENERALS
void weaponEvent(Object& recipient, const char* event, const Object* source);
#endif
}
