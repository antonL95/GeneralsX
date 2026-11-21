#pragma once

#include <string_view>

class DamageInfo;
class Object;
struct Coord3D;

namespace RetailTestCommands
{
void runFrame(unsigned int frame);
void saveCheckpoint(unsigned int frame);
bool tracesCombat();
void logHordeDeath(const Object* object, const DamageInfo* damage);
bool runBuildingCommand(std::string_view action, Object* object, const char* target, const Coord3D* location, unsigned int frame);
}
