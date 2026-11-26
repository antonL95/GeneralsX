#pragma once

#include <array>

#include "GameLogic/Module/AIUpdate.h"

class FXList;

class GiantBirdAIUpdateModuleData final : public AIUpdateModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& p);

	Real m_followThroughDistance = 0.0f;
	Real m_followThroughCheckStep = 0.0f;
	Real m_followThroughGradient = 0.0f;
	Real m_grabTossTimeTrigger = 0.0f;
	Real m_grabTossHeightTrigger = 0.0f;
	const FXList* m_tossFX = nullptr;
	UnsignedInt m_specialContactPoints = 0;
	AsciiString m_attackLocomotorType;
	AsciiString m_returnForAmmoLocomotorType;
};

class GiantBirdAIUpdate final : public AIUpdateInterface
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(GiantBirdAIUpdate, "GiantBirdAIUpdate")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(GiantBirdAIUpdate, GiantBirdAIUpdateModuleData)

public:
	GiantBirdAIUpdate(Thing* thing, const ModuleData* data);
	UpdateSleepTime update() override;
	void aiDoCommand(const AICommandParms* parms) override;
	static Bool canStrike(const Object& bird, const Coord3D& target);
	static void trackGrabbedPassenger(Object& bird, const Object& passenger);
#if !RTS_GENERALS
	Real getFlightPathSpeed() const { return m_flightSpeed; }
	Real getFlightPathSlope() const { return m_flightSlope; }
	Real getFlightPathDistance() const { return m_flightDistance; }
	UnsignedInt getFlightPathSpeedUpdateFrame() const { return m_nextSpeedFrame; }
#endif

protected:
	UpdateSleepTime doLocomotor() override;

private:
	void setSwooping(Bool swooping);
	void releaseGrabbedPassenger();
	void tryGrabDebrisPortion(Object* victim);
	void xferFlightState(Xfer* xfer);
	void xferRetailBlocks(Xfer* xfer, Bool present);

	Coord3D m_followThroughGoal = {};
	Bool m_followingThrough = FALSE;
	Bool m_swooping = FALSE;
	ObjectID m_grabbedPassenger = INVALID_ID;
	Bool m_killPassengerOnLanding = FALSE;
	ObjectID m_swoopVictim = INVALID_ID;

#if !RTS_GENERALS
	static constexpr UnsignedInt MAX_CLEARANCE_INSERTIONS = 5;
	static constexpr UnsignedInt MAX_FLIGHT_POINTS = 6 + MAX_CLEARANCE_INSERTIONS;
	static constexpr UnsignedInt SAMPLES_PER_SEGMENT = 10;

	struct FlightSample
	{
		Coord3D position;
		Real distance;
	};

	enum class FlightPathShape { Normal, Lift, LowExit, PositionStrike };

	void clearFlightPath();
	void buildFlightPath(const Coord3D& goal, const Coord3D* next, FlightPathShape shape);
	void sampleFlightPath();
	Coord3D flightPosition(Real distance) const;
	Real flightClearance(const Coord3D& point, Coord3D* offset = nullptr) const;
	void advanceFlightPath(Locomotor& locomotor, Real desiredSpeed);
	Coord3D findContact(const Object& victim) const;
	void startFollowThrough(Bool lift);
	Bool updateFollowThrough(Locomotor& locomotor);
	Bool updateSwoop();
	Bool updateTravelFlight(Locomotor& locomotor);

	std::array<Coord3D, MAX_FLIGHT_POINTS> m_flightPoints{};
	std::array<FlightSample, 1 + (MAX_FLIGHT_POINTS - 3) * SAMPLES_PER_SEGMENT> m_flightSamples{};
	UnsignedByte m_flightPointCount = 0;
	UnsignedInt m_flightSampleCount = 0;
	Coord3D m_flightGoal = {};
	Real m_flightDistance = 0.0f;
	Real m_flightSpeed = 0.0f;
	Real m_flightSlope = 0.0f;
	Real m_flightTurn = 0.0f;
	UnsignedInt m_nextSpeedFrame = 0;
	UnsignedInt m_followThroughUntil = 0;
#endif
};
