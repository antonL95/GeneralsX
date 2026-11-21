#pragma once

class SkirmishGameInfo;
class Object;
class Player;

namespace RetailSkirmishProbe
{
	Bool configure(SkirmishGameInfo& game);
	Bool enabled();
	void crc(UnsignedInt frame, UnsignedInt value);
	void update(UnsignedInt frame);
	void death(const Object& object, ObjectID source);
	void cast(Int player, const AsciiString& power, ObjectID source);
	void purchase(const Player& player, const AsciiString& science);
	void buildPermission(const Player* player, const AsciiString& name, Bool found, Bool allowed);
	void rebuild(const Object& hole, const Object& replacement);
}
