#pragma once

class Object;

namespace RetailBuildingEvents
{
void complete(Object& object);
int enrageAction(const Object& object, const char* event);
}
