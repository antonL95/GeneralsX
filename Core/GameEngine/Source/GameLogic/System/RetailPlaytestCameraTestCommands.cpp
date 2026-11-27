#include "PreRTS.h"

#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "GameClient/View.h"
#include "GameLogic/Object.h"

namespace
{
void focusSubject(const RetailTestCommands::Context& context)
{
	const Object* subject = context.requireObject(true);
	char* end = nullptr;
	const Real zoom = std::strtof(context.argument, &end);
	if (!subject || !TheTacticalView || end == context.argument || *end || !std::isfinite(zoom) || zoom <= 0.0f)
		return;

	const Coord3D* position = subject->getPosition();
	TheTacticalView->setZoom(zoom);
	TheTacticalView->lookAt(position);
	std::fprintf(stderr, "TEST frame %u: camera at %.1f %.1f zoom %.2f\n",
		context.frame, position->x, position->y, TheTacticalView->getZoom());
	std::fflush(stderr);
}

void measureSubject(const RetailTestCommands::Context& context)
{
	const Object* subject = context.requireObject(true);
	if (!subject || !TheTacticalView || TheTacticalView->getWidth() <= 0 || TheTacticalView->getHeight() <= 0)
		return;

	ICoord2D screen{};
	const Bool visible = TheTacticalView->worldToScreen(subject->getPosition(), &screen);
	Int x, y;
	TheTacticalView->getOrigin(&x, &y);
	const Real horizontal = static_cast<Real>(screen.x - x) / TheTacticalView->getWidth();
	const Real vertical = static_cast<Real>(screen.y - y) / TheTacticalView->getHeight();
	std::fprintf(stderr, "TEST camera subject frame=%u visible=%d view=%.3f,%.3f\n",
		context.frame, visible ? 1 : 0, horizontal, vertical);
	std::fflush(stderr);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailPlaytestCameraTestCommands()
{
#if !RTS_GENERALS
	registerAction("playtest-camera-follow", focusSubject);
	registerAction("playtest-camera-measure", measureSubject);
#endif
}
}
