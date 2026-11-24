#pragma once

#include "Lib/BaseType.h"

namespace UiResolution
{
	Bool followsWindow();
	Bool changeDisplayMode( Int width, Int height );
	void displayModeChanged( Int width, Int height );
	Bool followWindowPixels( Int pixelWidth, Int pixelHeight );
}
