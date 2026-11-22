#pragma once

class Object;

namespace RetailBuildingEvents
{
void created(Object& object);
void complete(Object& object);
void riderChanged(Object& object, bool mounted);
void trollEatingChanged(Object& object, bool enabled);
void reallyDamaged(Object& object);
int enrageAction(const Object& object, const char* event);
}
