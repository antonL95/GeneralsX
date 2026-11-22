#pragma once

class Object;

namespace RetailBuildingEvents
{
void complete(Object& object);
void riderChanged(Object& object, bool mounted);
int enrageAction(const Object& object, const char* event);
}
