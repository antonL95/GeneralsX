#include "PreRTS.h"

#include "GameClient/UiResolution.h"

#include "Common/GlobalData.h"
#include "Common/RetailFileLayers.h"
#include "GameClient/Display.h"
#include "GameClient/HeaderTemplate.h"
#include "GameClient/InGameUI.h"
#include "GameClient/Mouse.h"
#include "GameClient/Shell.h"
#include "GameClient/UiPixelSpace.h"
#include "GameClient/UiTest.h"

#include <cstdio>

namespace
{
	void displayModeChanged( Int width, Int height )
	{
		TheWritableGlobalData->m_xResolution = width;
		TheWritableGlobalData->m_yResolution = height;
		if( TheHeaderTemplateManager )
			TheHeaderTemplateManager->onResolutionChanged();
		if( TheMouse )
			TheMouse->onResolutionChanged();
		if( TheShell )
			TheShell->onResolutionChanged();
		if( TheInGameUI )
		{
			TheInGameUI->recreateControlBar();
			TheInGameUI->refreshCustomUiResources();
		}
		std::fprintf( stderr, "UI: display is now %d x %d\n", width, height );
	}
}

Bool UiResolution::followsWindow()
{
	return RetailFileLayers::enabled() || UiTest::isActive();
}

Bool UiResolution::changeDisplayMode( Int width, Int height )
{
	if( TheDisplay == nullptr ||
			TheDisplay->setDisplayMode( width, height, TheDisplay->getBitDepth(), TheDisplay->getWindowed() ) == FALSE )
		return FALSE;
	displayModeChanged( width, height );
	return TRUE;
}

Bool UiResolution::followWindowPixels( Int pixelWidth, Int pixelHeight )
{
	if( TheDisplay == nullptr || TheGlobalData == nullptr || TheGlobalData->m_headless || !followsWindow() ||
			TheDisplay->getWindowed() == FALSE )
		return FALSE;
	if( !UiPixelSpace::followsWindow( pixelWidth, pixelHeight, TheDisplay->getWidth(), TheDisplay->getHeight() ) )
		return FALSE;
	if( TheDisplay->setRenderSize( pixelWidth, pixelHeight ) == FALSE )
		return FALSE;
	displayModeChanged( pixelWidth, pixelHeight );
	return TRUE;
}
