#pragma once

#include "Lib/BaseType.h"

namespace UiTest
{
	void enable(const char *layout);
	Bool isActive();
	void update();
	const char *capturePath();
	void markCaptured();
	void finishFrame();
}
