#pragma once

#include "Lib/BaseType.h"

class AsciiString;
class INI;
class Object;
class Player;
class SpecialPowerTemplate;
struct Coord3D;

namespace RetailEva
{
enum class Ping
{
	None,
	Attack,
	Generic,
	Information,
	Beacon,
	Upgrade,
	OneRing
};

void load();
void reset();
void update();
void parseEvent(INI *ini);
void parseSettings(INI *ini);
void queue(const Player *recipient, const AsciiString &name, const Coord3D *position = nullptr,
	Ping ping = Ping::None, const Coord3D *radarPosition = nullptr);
void markFirstSighted(const Object& object);
void underAttack(const Object &object);
void destroyed(const Object &object);
void watchRecharge(const Object &object, const SpecialPowerTemplate &power);
}
