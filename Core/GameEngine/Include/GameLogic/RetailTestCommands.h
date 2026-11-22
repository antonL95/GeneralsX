#pragma once

#include <string_view>

#include "Lib/BaseType.h"

class DamageInfo;
class GameInfo;
class Object;
class Player;

namespace RetailTestCommands
{
struct Context
{
	std::string_view action;
	Player* player = nullptr;
	Object* source = nullptr;
	Object* target = nullptr;
	std::string_view playerName;
	const char* targetName = "";
	int ordinal = 0;
	const char* argument = "";
	Coord3D position = {0.0f, 0.0f, 0.0f};
	bool hasPosition = false;
	Real angle = 0.0f;
	UnsignedInt frame = 0;

	Player* findPlayer(std::string_view name = {}) const;
	Object* findObject(bool livingOnly = false) const;
	Object* requireObject(bool livingOnly = false) const;
	Object* findTarget(bool livingOnly = true) const;
	Object* findTarget(std::string_view spec, bool livingOnly = false) const;
	Coord3D groundLocation() const;
	void selectObject(Object* object) const;
};

using Handler = void (*)(const Context&);

void registerAction(std::string_view action, Handler handler);
void registerPrefix(std::string_view prefix, Handler handler);
void registerAllCommands();

bool configurePackage18Match(GameInfo& game);
void runFrame(unsigned int frame);
void saveCheckpoint(unsigned int frame);
bool tracesCombat();
void logHordeDeath(const Object* object, const DamageInfo* damage);
}
