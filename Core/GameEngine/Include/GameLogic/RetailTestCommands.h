#pragma once

#include <string_view>

class DamageInfo;
class GameInfo;
class Object;
class Player;
struct Coord3D;

namespace RetailTestCommands
{
bool configurePackage18Match(GameInfo& game);
void runFrame(unsigned int frame);
void saveCheckpoint(unsigned int frame);
bool tracesCombat();
void logHordeDeath(const Object* object, const DamageInfo* damage);
bool runBuildingCommand(std::string_view action, Object* object, const char* target, const Coord3D* location, unsigned int frame);
bool runPowerCommand(std::string_view action, Player* player, Object* object, const char* argument, unsigned int frame);
}
