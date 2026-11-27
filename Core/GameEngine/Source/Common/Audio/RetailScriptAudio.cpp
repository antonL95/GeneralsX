#include "PreRTS.h"

#include "Common/RetailScriptAudio.h"

#include "Common/GameCommon.h"
#include "Common/RetailSaveBlocks.h"
#include "Common/Xfer.h"
#include "GameLogic/PolygonTrigger.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace
{
constexpr std::array<const char*, RetailScriptAudio::RoomCount> roomNames = {
	"None", "Padded Cell", "Room", "Bathroom", "Living Room", "Stone Room", "Auditorium",
	"Concert Hall", "Cave", "Arena", "Hangar", "Carpeted Hallway", "Hallway", "Stone Corridor",
	"Alley", "Forest", "City", "Mountains", "Quarry", "Plain", "Parking Lot", "Sewer Pipe",
	"Underwater", "Drugged", "Dizzy", "Psychotic"
};

Real unitGain(Real gain)
{
	return std::isfinite(gain) ? std::clamp(gain, 0.0f, 1.0f) : 0.0f;
}

Real duration(Real seconds)
{
	return std::isfinite(seconds) ? std::max(0.0f, seconds) : 0.0f;
}
}

Real RetailScriptAudio::Fade::at(UnsignedInt frame) const
{
	if (frame <= startFrame)
		return start;
	Real elapsed = static_cast<Real>(frame - startFrame) / LOGICFRAMES_PER_SECONDS_REAL;
	if (elapsed < attack)
		return start + (target - start) * (elapsed / attack);
	elapsed -= attack;
	if (elapsed <= hold)
		return target;
	elapsed -= hold;
	if (elapsed < release)
		return target + (start - target) * (elapsed / release);
	return start;
}

void RetailScriptAudio::reset()
{
	m_fade = {};
	m_fadeMultiplier = 1.0f;
	m_refreshFade = FALSE;
	m_fadeImmune.clear();
	m_room = None;
	m_suppression.clear();
}

Bool RetailScriptAudio::updateFade(UnsignedInt frame)
{
	const Real gain = m_fade.at(frame);
	const Bool changed = m_refreshFade || gain != m_fadeMultiplier;
	m_fadeMultiplier = gain;
	m_refreshFade = FALSE;
	return changed;
}

void RetailScriptAudio::fade(Real start, Real target, Real attack, Real hold, Real release, UnsignedInt frame)
{
	m_fade = {unitGain(start), unitGain(target), duration(attack), duration(hold), duration(release), frame};
	m_fadeMultiplier = m_fade.start;
	m_refreshFade = TRUE;
}

void RetailScriptAudio::makeFadeImmune(AsciiString name)
{
	if (name.isEmpty())
		return;
	name.toLower();
	m_refreshFade |= m_fadeImmune.insert(name).second;
}

Real RetailScriptAudio::fadeMultiplier(AsciiString name) const
{
	if (m_fadeMultiplier == 1.0f || m_fadeImmune.empty())
		return m_fadeMultiplier;
	name.toLower();
	return m_fadeImmune.contains(name) ? 1.0f : m_fadeMultiplier;
}

void RetailScriptAudio::setRoom(const AsciiString& name)
{
	if (name.compareNoCase("Hanger") == 0)
	{
		m_room = Hangar;
		return;
	}
	m_room = None;
	for (UnsignedInt room = PaddedCell; room < RoomCount; ++room)
	{
		if (name.compareNoCase(roomNames[room]) == 0)
		{
			m_room = static_cast<Room>(room);
			return;
		}
	}
}

void RetailScriptAudio::setSuppression(const AsciiString& polygonName, Real multiplier)
{
	if (!std::isfinite(multiplier))
		return;
	multiplier = unitGain(multiplier);
	if (multiplier == 1.0f)
		m_suppression.erase(polygonName);
	else
		m_suppression.insert_or_assign(polygonName, multiplier);
}

Real RetailScriptAudio::suppressionAt(const Coord3D* position) const
{
	if (!position || m_suppression.empty())
		return 1.0f;
	const auto fits = [](Real value) {
		return value >= std::numeric_limits<Int>::min() &&
			static_cast<double>(value) <= std::numeric_limits<Int>::max();
	};
	if (!fits(position->x) || !fits(position->y))
		return 1.0f;
	ICoord3D point{static_cast<Int>(position->x), static_cast<Int>(position->y), 0};
	Real gain = 1.0f;
	for (const auto& [name, multiplier] : m_suppression)
	{
		if (multiplier >= gain)
			continue;
		const PolygonTrigger* polygon = PolygonTrigger::findByName(name);
		if (polygon && polygon->pointInTrigger(point))
			gain = multiplier;
	}
	return gain;
}

void RetailScriptAudio::xfer(Xfer* xfer)
{
	const Bool loading = xfer->getXferMode() == XFER_LOAD;
	RetailSaveBlocks::transferSection(xfer, loading, TRUE, {
		{"fade", [this, loading](Xfer* x) {
			x->xferReal(&m_fade.start);
			x->xferReal(&m_fade.target);
			x->xferReal(&m_fade.attack);
			x->xferReal(&m_fade.hold);
			x->xferReal(&m_fade.release);
			x->xferUnsignedInt(&m_fade.startFrame);
			if (loading)
				fade(m_fade.start, m_fade.target, m_fade.attack, m_fade.hold, m_fade.release, m_fade.startFrame);
		}, [this] { m_fade = {}; }},
		{"fade-immunity", [this, loading](Xfer* x) {
			UnsignedInt count = static_cast<UnsignedInt>(m_fadeImmune.size());
			x->xferUnsignedInt(&count);
			if (loading)
			{
				m_fadeImmune.clear();
				for (UnsignedInt i = 0; i < count; ++i)
				{
					AsciiString name;
					x->xferAsciiString(&name);
					makeFadeImmune(name);
				}
			}
			else
			{
				for (AsciiString name : m_fadeImmune)
					x->xferAsciiString(&name);
			}
		}, [this] { m_fadeImmune.clear(); }},
		{"reverb-room", [this, loading](Xfer* x) {
			AsciiString name(roomNames[m_room]);
			x->xferAsciiString(&name);
			if (loading)
				setRoom(name);
		}, [this] { m_room = None; }},
		{"reverb-suppression", [this, loading](Xfer* x) {
			UnsignedInt count = static_cast<UnsignedInt>(m_suppression.size());
			x->xferUnsignedInt(&count);
			if (loading)
			{
				m_suppression.clear();
				for (UnsignedInt i = 0; i < count; ++i)
				{
					AsciiString name;
					Real gain = 1.0f;
					x->xferAsciiString(&name);
					x->xferReal(&gain);
					setSuppression(name, gain);
				}
			}
			else
			{
				for (const auto& [name, multiplier] : m_suppression)
				{
					AsciiString polygon(name);
					Real gain = multiplier;
					x->xferAsciiString(&polygon);
					x->xferReal(&gain);
				}
			}
		}, [this] { m_suppression.clear(); }},
	});
}
