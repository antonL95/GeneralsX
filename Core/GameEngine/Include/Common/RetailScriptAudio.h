#pragma once

#include "Common/Snapshot.h"

#include <map>
#include <set>

struct Coord3D;

class RetailScriptAudio final : public Snapshot
{
public:
	enum Room : UnsignedInt
	{
		None, PaddedCell, SmallRoom, Bathroom, LivingRoom, StoneRoom, Auditorium, ConcertHall,
		Cave, Arena, Hangar, CarpetedHallway, Hallway, StoneCorridor, Alley, Forest, City,
		Mountains, Quarry, Plain, ParkingLot, SewerPipe, Underwater, Drugged, Dizzy, Psychotic,
		RoomCount
	};

	void reset();
	Bool updateFade(UnsignedInt frame);
	void fade(Real start, Real target, Real attack, Real hold, Real release, UnsignedInt frame);
	void makeFadeImmune(AsciiString name);
	Real fadeMultiplier(AsciiString name) const;
	void setRoom(const AsciiString& name);
	Room getRoom() const { return m_room; }
	void setSuppression(const AsciiString& polygonName, Real multiplier);
	Real suppressionAt(const Coord3D* position) const;

private:
	struct Fade
	{
		Real start = 1.0f;
		Real target = 1.0f;
		Real attack = 0.0f;
		Real hold = 0.0f;
		Real release = 0.0f;
		UnsignedInt startFrame = 0;
		Real at(UnsignedInt frame) const;
	};

	void crc(Xfer*) override {}
	void xfer(Xfer* xfer) override;
	void loadPostProcess() override { m_refreshFade = TRUE; }

	Fade m_fade;
	Real m_fadeMultiplier = 1.0f;
	Bool m_refreshFade = FALSE;
	std::set<AsciiString> m_fadeImmune;
	Room m_room = None;
	std::map<AsciiString, Real> m_suppression;
};
