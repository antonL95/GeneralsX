#pragma once

#include "Lib/BaseType.h"

class AsciiString;
class DamageInfo;
class INI;
class Object;
class Player;
class SpecialPowerTemplate;
class Xfer;
struct Coord3D;

namespace RetailEva
{
enum class Ping
{
	None,
	Attack,
	Generic,
	JumpOnly,
	Upgrade,
	OneRing
};

void load();
void unload();
void reset();
void update();
void parseEvent(INI *ini);
void parseSettings(INI *ini);
void parseScoredKillAnnouncer(INI *ini);
void scoredKill(const Object& killer, const Object& victim);
void xferScoredKills(Xfer* xfer, Bool present);
void queue(const Player *recipient, const AsciiString &name, const Coord3D *position = nullptr,
	Ping ping = Ping::None, const Coord3D *radarPosition = nullptr);
void markFirstSighted(const Object& object);
#if !RTS_GENERALS
void underAttack(Object &object, const DamageInfo &damage);
#endif
void destroyed(const Object &object);
void castleDestroyed(const Object &castle, const Object *keep);
void watchRecharge(const Object &object, const SpecialPowerTemplate &power);
Bool getLastPlayed(const AsciiString &name, UnsignedInt &frame, Coord3D *position = nullptr);
}
