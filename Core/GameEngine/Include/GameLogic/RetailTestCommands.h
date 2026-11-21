#pragma once

class DamageInfo;
class Object;

namespace RetailTestCommands
{
void runFrame(unsigned int frame);
void saveCheckpoint(unsigned int frame);
bool tracesCombat();
void logHordeDeath(const Object* object, const DamageInfo* damage);
}
