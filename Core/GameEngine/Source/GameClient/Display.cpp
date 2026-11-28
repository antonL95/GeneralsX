/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// FILE: Display.cpp //////////////////////////////////////////////////////////
// The implementation of the Display class
// Author: Michael S. Booth, March 2001

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameClient/Display.h"
#include "GameClient/Mouse.h"
#include "GameClient/VideoPlayer.h"
#include "GameClient/DisplayStringManager.h"
#include "GameClient/GameText.h"
#include "GameClient/GlobalLanguage.h"
#include "GameClient/Image.h"
//#include "GameLogic/ScriptEngine.h"
//#include "GameLogic/GameLogic.h"

/// The Display singleton instance.
Display *TheDisplay = nullptr;


Display::Display()
{
	m_viewList = nullptr;
	m_width = 0;
	m_height = 0;
	m_bitDepth = 0;
	m_windowed = FALSE;
	m_videoBuffer = nullptr;
	m_videoStream = nullptr;
	m_debugDisplayCallback = nullptr;
	m_debugDisplayUserData = nullptr;
	m_debugDisplay = nullptr;
	m_letterBoxFadeLevel = 0;
	m_letterBoxEnabled = FALSE;
	m_cinematicText = AsciiString::TheEmptyString;
	m_cinematicFont = nullptr;
	m_cinematicTextFrames = 0;

	m_currentlyPlayingMovie.clear();
	m_movieCancellable = TRUE;
	m_letterBoxFadeStartTime = 0;
	m_isBatching = FALSE;
}

/**
 * Destructor for the Display.  Destroy all views attached to it.
 */
Display::~Display()
{

	stopMovie();
	// delete all our views if present
	deleteViews();

}

/**
	* Delete all views in the Display
	*/
void Display::deleteViews()
{
	View *v, *next;

	for( v = m_viewList; v; v = next )
	{
		next = v->getNextView();
		delete v;
	}
	m_viewList = nullptr;
}

/**
 * Attach the given view to the world
 * @todo Rethink the "attachView" notion...
 */
void Display::attachView( View *view )
{
	// prepend to head of list
	m_viewList = view->prependViewToList( m_viewList );
}

/**
 * Render all views of the world
 */
void Display::drawViews()
{

	for( View *v = m_viewList; v; v = v->getNextView() )
		v->drawView();

}

/**
 * Updates all views of the world.  This forces state variables
   to refresh without actually drawing anything.
 */
void Display::updateViews()
{

	for( View *v = m_viewList; v; v = v->getNextView() )
		v->updateView();

}

void Display::stepViews()
{

	for( View *v = m_viewList; v; v = v->getNextView() )
		v->stepView();

}

/// Redraw the entire display
void Display::draw()
{
	// redraw all views
	drawViews();

	// redraw the in-game user interface
	/// @todo Switch between in-game and shell interfaces

}

/** Sets screen resolution/mode*/
Bool Display::setDisplayMode( UnsignedInt xres, UnsignedInt yres, UnsignedInt bitdepth, Bool windowed )
{
	//Get old values
	UnsignedInt oldDisplayHeight=getHeight();
	UnsignedInt oldDisplayWidth=getWidth();
	Int oldViewWidth=TheTacticalView->getWidth();
	Int oldViewHeight=TheTacticalView->getHeight();
	Int oldViewOriginX,oldViewOriginY;
	TheTacticalView->getOrigin(&oldViewOriginX,&oldViewOriginY);

	setWidth(xres);
	setHeight(yres);

	//Adjust view to match previous proportions
	TheTacticalView->setWidth((Real)oldViewWidth/(Real)oldDisplayWidth*(Real)xres);
	TheTacticalView->setHeight((Real)oldViewHeight/(Real)oldDisplayHeight*(Real)yres);
	TheTacticalView->setOrigin((Real)oldViewOriginX/(Real)oldDisplayWidth*(Real)xres,
	(Real)oldViewOriginY/(Real)oldDisplayHeight*(Real)yres);
	return TRUE;
}

// Display::setWidth ==========================================================
/** Set the width of the display */
//=============================================================================
void Display::setWidth( UnsignedInt width )
{

	// set the new width
	m_width = width;

	// set the new mouse limits
	if( TheMouse )
		TheMouse->setMouseLimits();

}

// Display::setHeight =========================================================
/** Set the height of the display */
//=============================================================================
void Display::setHeight( UnsignedInt height )
{

	// se the new height
	m_height = height;

	// set the new mouse limits
	if( TheMouse )
		TheMouse->setMouseLimits();

}

//============================================================================
// Display::playMovie
//============================================================================

void Display::playMovie( AsciiString movieName, Bool allowCancel )
{
	if (TheGlobalData->m_headless)
		return;

	stopMovie();



	m_videoStream = TheVideoPlayer->open( movieName );

	if ( m_videoStream == nullptr )
	{
		return;
	}

	m_currentlyPlayingMovie = movieName;
	m_movieCancellable = allowCancel;

	m_videoBuffer = createVideoBuffer();
	if (	m_videoBuffer == nullptr ||
				!m_videoBuffer->allocate(	m_videoStream->width(),
													m_videoStream->height())
		)
	{
		stopMovie();
		return;
	}

}

//============================================================================
// Display::stopMovie
//============================================================================

void Display::stopMovie()
{
	delete m_videoBuffer;
	m_videoBuffer = nullptr;

	if ( m_videoStream )
	{
		m_videoStream->close();
		m_videoStream = nullptr;
	}

	if (!m_currentlyPlayingMovie.isEmpty()) {
		//TheScriptEngine->notifyOfCompletedVideo(m_currentlyPlayingMovie); // Removing this sync-error cause MDC
		m_currentlyPlayingMovie = AsciiString::TheEmptyString;
	}
	m_movieCancellable = TRUE;
}

//============================================================================
// Display::update
//============================================================================

void Display::update()
{
	if ( m_videoStream && m_videoBuffer )
	{
		if ( m_videoStream->isFrameReady())
		{
			m_videoStream->frameDecompress();
			m_videoStream->frameRender( m_videoBuffer );
			if( m_videoStream->frameIndex() != m_videoStream->frameCount() - 1)
			{
				m_videoStream->frameNext();
			}
			else
			{
				stopMovie();
			}
		}
	}
}

//============================================================================
// Display::reset
//============================================================================

void Display::reset()
{
	//Remove letterbox border that may have been enabled by a script
	m_letterBoxFadeLevel = 0;
	m_letterBoxEnabled = FALSE;
	stopMovie();

	// Reset all views that need resetting
	for( View *v = m_viewList; v; v = v->getNextView() )
		v->reset();
}

//============================================================================
// Display::isMoviePlaying
//============================================================================

Bool Display::isMoviePlaying()
{
	return m_videoStream != nullptr && m_videoBuffer != nullptr;
}

//============================================================================
// Display::setDebugDisplayCallback
//============================================================================

void Display::setDebugDisplayCallback( DebugDisplayCallback *callback, void *userData )
{
	m_debugDisplayCallback = callback;
	m_debugDisplayUserData = userData;
}

//============================================================================
// Display::getDebugDisplayCallback
//============================================================================

Display::DebugDisplayCallback *Display::getDebugDisplayCallback()
{
	return m_debugDisplayCallback;
}

void Display::beginBatch()
{
	if (m_isBatching)
	{
		return;
	}
	m_isBatching = TRUE;
	onBeginBatch();
}

void Display::endBatch()
{
	if (!m_isBatching)
	{
		return;
	}
	onFlush();
	m_isBatching = FALSE;
	onEndBatch();
}

void Display::flush()
{
	if (m_isBatching)
	{
		onFlush();
	}
}

void Display::drawImagePart( const Image *image, Int startX, Int startY, Int endX, Int endY,
	const Region2D &, Color color, DrawImageMode mode )
{
	drawImage(image, startX, startY, endX, endY, color, mode);
}

void Display::drawTexturedLine(const Image *image, ICoord2D start, ICoord2D end, Real width)
{
	const Int halfWidth = static_cast<Int>(width * 0.5f);
	const Bool horizontal = std::abs(end.x - start.x) >= std::abs(end.y - start.y);
	const Int xPad = horizontal ? 0 : halfWidth;
	const Int yPad = horizontal ? halfWidth : 0;
	drawImage(image, MIN(start.x, end.x) - xPad, MIN(start.y, end.y) - yPad,
		MAX(start.x, end.x) + xPad, MAX(start.y, end.y) + yPad);
}

void Display::drawImageNineSlice( const Image *image, const UiNineSlice::Insets &insets, Int startX, Int startY,
	Int endX, Int endY, Real scale, UiNineSlice::Fill edges, UiNineSlice::Fill center, Color color, DrawImageMode mode )
{
	if (image == nullptr)
		return;

	const Int width = image->getImageWidth();
	const Int height = image->getImageHeight();
	if (width <= 0 || height <= 0)
	{
		drawImage(image, startX, startY, endX, endY, color, mode);
		return;
	}

	const UiNineSlice::Rect dest = { startX, startY, endX, endY };
	const std::vector<UiNineSlice::Piece> pieces = UiNineSlice::slice(width, height, insets, dest, scale, edges, center);
	if (pieces.empty())
		return;

	const Bool ownBatch = !isBatching();
	if (ownBatch)
		beginBatch();
	for (const UiNineSlice::Piece &piece : pieces)
	{
		Region2D part;
		part.lo.x = static_cast<Real>(piece.source.x0 / width);
		part.lo.y = static_cast<Real>(piece.source.y0 / height);
		part.hi.x = static_cast<Real>(piece.source.x1 / width);
		part.hi.y = static_cast<Real>(piece.source.y1 / height);
		drawImagePart(image, piece.dest.x0, piece.dest.y0, piece.dest.x1, piece.dest.y1, part, color, mode);
	}
	if (ownBatch)
		endBatch();
}
