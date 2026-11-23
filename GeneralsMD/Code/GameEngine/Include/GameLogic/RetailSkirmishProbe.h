#pragma once

class SkirmishGameInfo;
class Object;
class Player;
class DamageInfo;

namespace RetailSkirmishProbe
{
	extern Bool active;
	inline Bool enabled() { return active; }
	Bool configure(SkirmishGameInfo& game);
	void configureResume();
	void checkpoint(UnsignedInt frame);
	void crc(UnsignedInt frame, UnsignedInt value);
	void update(UnsignedInt frame);
	void death(const Object& object, ObjectID source);
	void structureDamage(const Object& object, const DamageInfo& damage);
	void cast(Int player, const AsciiString& power, ObjectID source);
	void purchase(const Player& player, const AsciiString& science);
	void buildPermission(const Player* player, const AsciiString& name, Bool found, Bool allowed);
	void rebuild(const Object& hole, const Object& replacement);
}
