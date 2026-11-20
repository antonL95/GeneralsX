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

// FILE: GameWindowTransitions.cpp /////////////////////////////////////////////////
//-----------------------------------------------------------------------------
//
//                       Electronic Arts Pacific.
//
//                       Confidential Information
//                Copyright (C) 2002 - All Rights Reserved
//
//-----------------------------------------------------------------------------
//
//	created:	Dec 2002
//
//	Filename: 	GameWindowTransitions.cpp
//
//	author:		Chris Huybregts
//
//	purpose:
//
//-----------------------------------------------------------------------------
///////////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
// SYSTEM INCLUDES ////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

//-----------------------------------------------------------------------------
// USER INCLUDES //////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
#include "GameLogic/GameLogic.h"
#include "GameClient/ControlBar.h"
#include "GameClient/Display.h"
#include "GameClient/GameWindowTransitions.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"
#include "Common/AudioAffect.h"
#include "Common/FramePacer.h"
#include "Common/GameAudio.h"
#include "Common/GlobalData.h"
#include "Common/INIAudit.h"
#include "Common/RetailFileLayers.h"
#include <algorithm>
#include <limits>
#include <memory>
#include <vector>
//-----------------------------------------------------------------------------
// DEFINES ////////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
GameWindowTransitionsHandler *TheTransitionHandler = nullptr;
const FieldParse GameWindowTransitionsHandler::m_gameWindowTransitionsFieldParseTable[] =
{

	{ "Window",		GameWindowTransitionsHandler::parseWindow,	nullptr, 0	},
	{ "FireOnce",	INI::parseBool,															nullptr, offsetof( TransitionGroup, m_fireOnce) 	},

	{ nullptr,										nullptr,													nullptr, 0 }

};

void INI::parseWindowTransitions( INI* ini )
{
	AsciiString name;
	TransitionGroup *g;

	// read the name
	const char* c = ini->getNextToken();
	name.set( c );

	// find existing item if present

	DEBUG_ASSERTCRASH( TheTransitionHandler, ("parseWindowTransitions: TheTransitionHandler doesn't exist yet") );
	if( !TheTransitionHandler )
		return;

	// If we have a previously allocated control bar, this will return a cleared out pointer to it so we
	// can overwrite it
	g = TheTransitionHandler->getNewGroup( name );

	// sanity
	DEBUG_ASSERTCRASH( g, ("parseWindowTransitions: Unable to allocate group '%s'", name.str()) );

	// parse the ini definition
	ini->initFromINI( g, TheTransitionHandler->getFieldParse() );


}
//-----------------------------------------------------------------------------
// PUBLIC FUNCTIONS ///////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
Transition *getTransitionForStyle( Int style )
{
	switch (style) {
	case TRANSITION_FLASH:
		return NEW FlashTransition;
	case BUTTON_TRANSITION_FLASH:
		return NEW ButtonFlashTransition;
	case WIN_FADE_TRANSITION:
		return NEW FadeTransition;
	case WIN_SCALE_UP_TRANSITION:
		return NEW ScaleUpTransition;
	case MAINMENU_SCALE_UP_TRANSITION:
		return NEW MainMenuScaleUpTransition;
	case TEXT_TYPE_TRANSITION:
		return NEW TextTypeTransition;
	case SCREEN_FADE_TRANSITION:
		return NEW ScreenFadeTransition;
	case COUNT_UP_TRANSITION:
		return NEW CountUpTransition;
	case FULL_FADE_TRANSITION:
		return NEW FullFadeTransition;
	case TEXT_ON_FRAME_TRANSITION:
		return NEW TextOnFrameTransition;
	case REVERSE_SOUND_TRANSITION:
		return NEW ReverseSoundTransition;

	case MAINMENU_MEDIUM_SCALE_UP_TRANSITION:
		return NEW MainMenuMediumScaleUpTransition;
	case MAINMENU_SMALL_SCALE_DOWN_TRANSITION:
		return NEW MainMenuSmallScaleDownTransition;
	case CONTROL_BAR_ARROW_TRANSITION:
		return NEW ControlBarArrowTransition;
	case SCORE_SCALE_UP_TRANSITION:
		return NEW ScoreScaleUpTransition;

	default:
		DEBUG_CRASH(("getTransitionForStyle:: An invalid style was passed in. Style = %d", style));
		return nullptr;
	}
	return nullptr;
}

namespace
{
	constexpr Int SoundFadeStyle = MAX_TRANSITION_WINDOW_STYLES;
	constexpr AudioAffect SoundFadeChannels[] = {
		AudioAffect_Music, AudioAffect_Sound, AudioAffect_Sound3D, AudioAffect_Speech
	};

	class SoundFadeTransition final : public Transition
	{
	public:
		explicit SoundFadeTransition( Int frames )
		{
			m_frameLength = frames;
			m_win = nullptr;
		}

		~SoundFadeTransition() override
		{
			restore();
		}

		void init( GameWindow * ) override
		{
			m_isForward = TRUE;
			m_isFinished = FALSE;
			saveVolumes();
		}

		void update( Int frame ) override
		{
			m_isFinished = m_isForward ? frame == m_frameLength : frame == 0;
			if (m_isFinished)
			{
				restore();
				return;
			}
			if (m_audio && m_audio == TheAudio)
			{
				const Real gain = 1.0f - (Real)frame / m_frameLength;
				for (size_t i = 0; i < ARRAY_SIZE(SoundFadeChannels); ++i)
					m_audio->setVolume(m_originalVolumes[i] * gain, SoundFadeChannels[i]);
			}
		}

		void reverse() override
		{
			m_isForward = FALSE;
			m_isFinished = FALSE;
			if (!m_audio)
				saveVolumes();
		}

		void draw() override {}

		void skip() override
		{
			restore();
			m_isFinished = TRUE;
		}

	private:
		void saveVolumes()
		{
			m_audio = TheAudio;
			if (m_audio)
				for (size_t i = 0; i < ARRAY_SIZE(SoundFadeChannels); ++i)
					m_originalVolumes[i] = m_audio->getScriptVolume(SoundFadeChannels[i]);
		}

		void restore()
		{
			if (m_audio && m_audio == TheAudio)
				for (size_t i = 0; i < ARRAY_SIZE(SoundFadeChannels); ++i)
					m_audio->setVolume(m_originalVolumes[i], SoundFadeChannels[i]);
			m_audio = nullptr;
		}

		AudioManager *m_audio = nullptr;
		Real m_originalVolumes[ARRAY_SIZE(SoundFadeChannels)] = {};
	};

	Bool requiresWindow( Int style )
	{
		return style != SCREEN_FADE_TRANSITION && style != SoundFadeStyle &&
			style != REVERSE_SOUND_TRANSITION && style != CONTROL_BAR_ARROW_TRANSITION;
	}

	AsciiString requiredCompanion( Int style, const AsciiString &name )
	{
		switch (style)
		{
		case MAINMENU_SCALE_UP_TRANSITION:
			return "MainMenu.wnd:WinGrowMarker";
		case CONTROL_BAR_ARROW_TRANSITION:
			return "ControlBar.wnd:ButtonGeneral";
		case MAINMENU_MEDIUM_SCALE_UP_TRANSITION:
		case MAINMENU_SMALL_SCALE_DOWN_TRANSITION:
		{
			AsciiString companion = name;
			companion.concat(style == MAINMENU_MEDIUM_SCALE_UP_TRANSITION ? "Medium" : "Small");
			return companion;
		}
		default:
			return AsciiString::TheEmptyString;
		}
	}

	struct RetailTransitionSpec
	{
		Int style = -1;
		Int startFrame = 0;
		Int endFrame = -1;
		Color fadeColor = 0;
		AsciiString viewsToFade;
		Bool leaveSilent = FALSE;
	};

	struct RetailTransitionWindow
	{
		AsciiString name;
		Int frameDelay = 0;
		Int legacyStyle = TRANSITION_FLASH;
		Bool hasNested = FALSE;
		std::vector<RetailTransitionSpec> transitions;
	};

	void parseRetailTransition( INI* ini, void *instance, void *, const void * )
	{
		auto &window = *static_cast<RetailTransitionWindow *>(instance);
		window.hasNested = TRUE;
		const char *styleName = ini->getNextToken();
		RetailTransitionSpec spec;
		if (stricmp(styleName, "SOUNDFADE") == 0)
			spec.style = SoundFadeStyle;
		else
			for (const auto &style : TransitionStyleNames)
				if (style.name && stricmp(style.name, styleName) == 0)
				{
					spec.style = style.value;
					break;
				}
		if (spec.style < 0)
		{
			ini->recordSkipped("subblock", styleName);
			ini->skipBlock("Transition");
			return;
		}

		static const FieldParse fields[] = {
			{ "StartFrame", INI::parseInt, nullptr, offsetof(RetailTransitionSpec, startFrame) },
			{ "EndFrame", INI::parseInt, nullptr, offsetof(RetailTransitionSpec, endFrame) },
			{ "FadeColor", INI::parseColorInt, nullptr, offsetof(RetailTransitionSpec, fadeColor) },
			{ "ViewsToFade", INI::parseAsciiString, nullptr, offsetof(RetailTransitionSpec, viewsToFade) },
			{ "LeaveSilent", INI::parseBool, nullptr, offsetof(RetailTransitionSpec, leaveSilent) },
			{ nullptr, nullptr, nullptr, 0 }
		};
		ini->initFromINI(&spec, fields);
		if (spec.viewsToFade.isNotEmpty() || spec.leaveSilent)
		{
			ini->recordSkipped("subblock", "SOUNDFADE with a view or a persistent mute");
			return;
		}
		if (spec.startFrame < 0 || spec.endFrame < -1 ||
			(spec.endFrame >= 0 && spec.endFrame < spec.startFrame))
		{
			ini->recordSkipped("field-error", "invalid transition frame range");
			return;
		}
		window.transitions.push_back(spec);
	}
}

TransitionWindow::TransitionWindow()
{
	m_currentFrameDelay = m_frameDelay = 0;
	m_style = 0;
	m_startFrame = 0;
	m_endFrame = -1;
	m_fadeColor = 0;
	m_duration = 0;
	m_lastStyleFrame = 0;

	m_winID = NAMEKEY_INVALID;
	m_win = nullptr;
	m_transition = nullptr;
}

TransitionWindow::~TransitionWindow()
{
	if (m_win)
		m_win->unlinkTransitionWindow(this);

	m_win = nullptr;

	delete m_transition;
	m_transition = nullptr;
}

Bool TransitionWindow::init()
{
	const Bool retail = RetailFileLayers::enabled();
	if(retail && m_win)
		m_win->unlinkTransitionWindow(this);

	m_winID = retail && m_winName.isEmpty() ? NAMEKEY_INVALID : TheNameKeyGenerator->nameToKey(m_winName);
	m_win = retail && m_winID == NAMEKEY_INVALID ? nullptr : TheWindowManager->winGetWindowFromId(nullptr, m_winID);

	delete m_transition;
	m_transition = nullptr;
	m_currentFrameDelay = m_frameDelay;
	m_lastStyleFrame = 0;
	if (retail)
	{
		const AsciiString companion = requiredCompanion(m_style, m_winName);
		const char *missing = nullptr;
		if (!m_win && requiresWindow(m_style))
			missing = m_winName.isEmpty() ? "missing window name" : m_winName.str();
		else if (!companion.isEmpty() && !TheWindowManager->winGetWindowFromId(nullptr, TheNameKeyGenerator->nameToKey(companion)))
			missing = companion.str();
		else if (m_style == CONTROL_BAR_ARROW_TRANSITION && (!TheControlBar || !TheControlBar->getArrowImage()))
			missing = "missing control bar arrow";
		if (missing)
		{
			if (INIAudit::isEnabled())
				INIAudit::recordWithContext("window-transition", "TransitionWindow::init", missing, "", 0);
			m_win = nullptr;
			return FALSE;
		}
	}

	const Int explicitDuration = m_endFrame >= 0 ? std::max(1, m_endFrame - m_startFrame) : 0;
	if (m_style == SCREEN_FADE_TRANSITION && explicitDuration)
		m_transition = NEW ScreenFadeTransition(explicitDuration, m_fadeColor);
	else if (m_style == SoundFadeStyle)
		m_transition = NEW SoundFadeTransition(explicitDuration ? explicitDuration : 30);
	else
		m_transition = getTransitionForStyle(m_style);

	const Int64 end = (Int64)m_frameDelay + m_startFrame +
		std::max(explicitDuration, m_transition->getFrameLength());
	if (retail && end >= std::numeric_limits<Int>::max())
	{
		if (INIAudit::isEnabled())
			INIAudit::recordWithContext("window-transition", "TransitionWindow::init",
				"transition exceeds the frame range", "", 0);
		delete m_transition;
		m_transition = nullptr;
		m_win = nullptr;
		return FALSE;
	}
	m_currentFrameDelay += m_startFrame;
	m_transition->init(m_win);
	m_duration = explicitDuration ? explicitDuration : m_transition->getFrameLength();

	// TheSuperHackers @fix Mauller 15/05/2025 Link TransitionWindow to the GameWindow so the GameWindow can unlink itself when it is destroyed
	if(m_win)
		m_win->linkTransitionWindow(this);

	return TRUE;
}

void TransitionWindow::update( Int frame )
{
	if (!m_transition)
		return;

	const Int styleLength = m_transition->getFrameLength();
	const Int duration = m_endFrame < 0 ? styleLength : m_duration;
	if (frame < m_currentFrameDelay || frame > m_currentFrameDelay + duration)
		return;

	const Int localFrame = frame - m_currentFrameDelay;
	if (duration == styleLength)
	{
		m_transition->update(localFrame);
		return;
	}

	const Int styleFrame = (Int)((Int64)localFrame * styleLength / duration);
	while (m_lastStyleFrame != styleFrame)
	{
		m_lastStyleFrame += styleFrame > m_lastStyleFrame ? 1 : -1;
		m_transition->update(m_lastStyleFrame);
	}
}

Bool TransitionWindow::isFinished()
{
	if(m_transition)
		return m_transition->isFinished();
	return TRUE;
}

void TransitionWindow::reverse( Int totalFrames )
{
	//m_currentFrameDelay = totalFrames - (m_transition->getFrameLength() + m_frameDelay);
	if(m_transition)
	{
		m_transition->reverse();
		m_lastStyleFrame = m_transition->getFrameLength();
	}
}

void TransitionWindow::skip()
{
	if(m_transition)
		m_transition->skip();
}

void TransitionWindow::draw()
{
	if(m_transition)
		m_transition->draw();
}

void TransitionWindow::unlinkGameWindow(GameWindow* win)
{
	if (m_win != win)
		return;

	m_transition->unlinkGameWindow(win);
	m_win = nullptr;
	if (RetailFileLayers::enabled() && requiresWindow(m_style))
	{
		delete m_transition;
		m_transition = nullptr;
	}
}

Int TransitionWindow::getTotalFrames()
{
	if(m_transition)
	{
		const Int duration = m_endFrame < 0 ? m_transition->getFrameLength() : m_duration;
		return m_currentFrameDelay + duration;
	}

	return RetailFileLayers::enabled() ? 0 : m_frameDelay;
}

//-----------------------------------------------------------------------------
TransitionGroup::TransitionGroup()
{
	m_currentFrame = 0.0f;
	m_fireOnce = FALSE;
}

TransitionGroup::~TransitionGroup()
{
	TransitionWindowList::iterator it = m_transitionWindowList.begin();
	while (it != m_transitionWindowList.end())
	{
		TransitionWindow *tWin = *it;
		delete tWin;
		it = m_transitionWindowList.erase(it);
	}
}

void TransitionGroup::init()
{
	m_currentFrame = 0.0f;
	m_directionMultiplier = 1;
	TransitionWindowList::iterator it = m_transitionWindowList.begin();
	while (it != m_transitionWindowList.end())
	{
		TransitionWindow *tWin = *it;
		tWin->init();
		it++;
	}

}

void TransitionGroup::update()
{
	// TheSuperHackers @tweak bobtista GUI transition timing is now decoupled from the render update.
	// Step every integer frame between the old and new accumulator value so discrete-state-machine
	// transitions cannot skip a state when the render frame rate dips below the base rate.
	// TheSuperHackers @feature bobtista 28/06/2026 Scale by the user game window transition speed preference.
	const Real timeScale = TheFramePacer->getBaseOverUpdateFpsRatio() * TheGlobalData->m_gameWindowTransitionSpeedMultiplier;
	const Int prevFrame = (Int)m_currentFrame;
	m_currentFrame += m_directionMultiplier * timeScale;
	const Int newFrame = (Int)m_currentFrame;

	if( newFrame == prevFrame )
	{
		return;
	}

	const Int step = (newFrame > prevFrame) ? 1 : -1;
	for( Int frame = prevFrame + step; frame != newFrame + step; frame += step )
	{
		Bool isFinished = TRUE;
		TransitionWindowList::iterator it = m_transitionWindowList.begin();
		while (it != m_transitionWindowList.end())
		{
			TransitionWindow *tWin = *it;
			tWin->update(frame);
			isFinished &= tWin->isFinished();
			it++;
		}

		if( isFinished )
		{
			break;
		}
	}
}

Bool TransitionGroup::isFinished()
{
	TransitionWindowList::iterator it = m_transitionWindowList.begin();
	while (it != m_transitionWindowList.end())
	{
		TransitionWindow *tWin = *it;
		if(tWin->isFinished() == FALSE)
			return FALSE;
		it++;
	}

	return TRUE;
}

void TransitionGroup::reverse()
{
	Int totalFrames =0;
	m_directionMultiplier = -1;

	TransitionWindowList::iterator it = m_transitionWindowList.begin();
	while (it != m_transitionWindowList.end())
	{
		TransitionWindow *tWin = *it;
		Int winFrames = tWin->getTotalFrames();
		if(winFrames > totalFrames)
			totalFrames = winFrames;
		it++;
	}
	it = m_transitionWindowList.begin();
	while (it != m_transitionWindowList.end())
	{
		TransitionWindow *tWin = *it;
		tWin->reverse(totalFrames);
		it++;
	}
	m_currentFrame = (Real)totalFrames;
//	m_currentFrame ++;
}

Bool TransitionGroup::isReversed()
{
	if(m_directionMultiplier < 0)
		return TRUE;
	return FALSE;
}

void TransitionGroup::skip ()
{
	TransitionWindowList::iterator it = m_transitionWindowList.begin();
	while (it != m_transitionWindowList.end())
	{
		TransitionWindow *tWin = *it;
		tWin->skip();
		it++;
	}
}

void TransitionGroup::draw ()
{
	TransitionWindowList::iterator it = m_transitionWindowList.begin();
	while (it != m_transitionWindowList.end())
	{
		TransitionWindow *tWin = *it;
		tWin->draw();
		it++;
	}
}

void TransitionGroup::addWindow( TransitionWindow *transWin )
{
	if(!transWin)
		return;
	m_transitionWindowList.push_back(transWin);
}

//-----------------------------------------------------------------------------

GameWindowTransitionsHandler::GameWindowTransitionsHandler()
{
	m_currentGroup = nullptr;
	m_pendingGroup = nullptr;
	m_drawGroup = nullptr;
	m_secondaryDrawGroup = nullptr;

}

GameWindowTransitionsHandler::~GameWindowTransitionsHandler()
{
	m_currentGroup = nullptr;
	m_pendingGroup = nullptr;
	m_drawGroup = nullptr;
	m_secondaryDrawGroup = nullptr;

	TransitionGroupList::iterator it = m_transitionGroupList.begin();
	while( it != m_transitionGroupList.end() )
	{
		TransitionGroup *g = *it;
		delete g;
		it = m_transitionGroupList.erase(it);
	}
}

void GameWindowTransitionsHandler::init()
{
	m_currentGroup = nullptr;
	m_pendingGroup = nullptr;
	m_drawGroup = nullptr;
	m_secondaryDrawGroup = nullptr;
}

void GameWindowTransitionsHandler::load()
{
	INI ini;
	// Read from INI all the ControlBarSchemes
	ini.loadFileDirectory( "Data\\INI\\WindowTransitions", INI_LOAD_OVERWRITE, nullptr );

}

void GameWindowTransitionsHandler::reset()
{
	m_currentGroup = nullptr;
	m_pendingGroup = nullptr;
	m_drawGroup = nullptr;
	m_secondaryDrawGroup = nullptr;

}

void GameWindowTransitionsHandler::update()
{
	if(m_drawGroup != m_currentGroup)
		m_secondaryDrawGroup = m_drawGroup;
	else
		m_secondaryDrawGroup = nullptr;

	m_drawGroup = m_currentGroup;
	if(m_currentGroup && !m_currentGroup->isFinished())
		m_currentGroup->update();

	if(m_currentGroup && m_currentGroup->isFinished() && m_currentGroup->isFireOnce())
	{
		m_currentGroup = nullptr;
	}

	if(m_currentGroup && m_pendingGroup && m_currentGroup->isFinished())
	{
		m_currentGroup = m_pendingGroup;
		m_pendingGroup = nullptr;
	}

	if(!m_currentGroup && m_pendingGroup)
	{
		m_currentGroup = m_pendingGroup;
		m_pendingGroup = nullptr;
	}

	if(m_currentGroup && m_currentGroup->isFinished() && m_currentGroup->isReversed())
		m_currentGroup = nullptr;
}


void GameWindowTransitionsHandler::draw()
{
//	if( TheGameLogic->getFrame() > 0 )//if( areTransitionsEnabled() ) //KRIS
	if(m_drawGroup)
			m_drawGroup->draw();
	if(m_secondaryDrawGroup)
		m_secondaryDrawGroup->draw();
}

void GameWindowTransitionsHandler::setGroup(AsciiString groupName, Bool immediate )
{
	if(groupName.isEmpty() && immediate)
		m_currentGroup = nullptr;
	if(immediate && m_currentGroup)
	{
		m_currentGroup->skip();
		m_currentGroup = findGroup(groupName);
		if(m_currentGroup)
			m_currentGroup->init();
		return;
	}

	if(m_currentGroup)
	{
		if(!m_currentGroup->isFireOnce() && !m_currentGroup->isReversed())
			m_currentGroup->reverse();
		m_pendingGroup = findGroup(groupName);
		if(m_pendingGroup)
			m_pendingGroup->init();
		return;
	}

	m_currentGroup = findGroup(groupName);
	if(m_currentGroup)
		m_currentGroup->init();



}

void GameWindowTransitionsHandler::reverse( AsciiString groupName )
{
	TransitionGroup *g = findGroup(groupName);
	// GeneralsX @bugfix fbraz3 18/09/2026 Guard against null transition group to prevent crash
	if (!g)
		return;

	if( m_currentGroup == g )
	{
		m_currentGroup->reverse();
		return;
	}
	if( m_pendingGroup == g)
	{
		m_pendingGroup = nullptr;
		return;
	}
	if(m_currentGroup)
		m_currentGroup->skip();
	if(m_pendingGroup)
		m_pendingGroup->skip();

	m_currentGroup = g;
	m_currentGroup->init();
	m_currentGroup->skip();
	m_currentGroup->reverse();
	m_pendingGroup = nullptr;
}

void GameWindowTransitionsHandler::remove( AsciiString groupName,  Bool skipPending )
{
	TransitionGroup* g = findGroup(groupName);

	if (!g)
	{
		return;
	}

	if(m_pendingGroup == g)
	{
		if(skipPending)
			m_pendingGroup->skip();

		m_pendingGroup = nullptr;
	}
	if(m_currentGroup == g)
	{
		m_currentGroup->skip();
		m_currentGroup = nullptr;
		if(m_pendingGroup)
			m_currentGroup = m_pendingGroup;
	}
}

TransitionGroup *GameWindowTransitionsHandler::getNewGroup( AsciiString name )
{
	if(name.isEmpty())
		return nullptr;

	// test to see if we're trying to add an already existing group.
	if(findGroup(name))
	{
		DEBUG_CRASH(("GameWindowTransitionsHandler::getNewGroup - We already have a group %s", name.str()));
		return nullptr;
	}
	TransitionGroup *g = NEW TransitionGroup;
	g->setName(name);
	m_transitionGroupList.push_back(g);
	return g;
}

Bool GameWindowTransitionsHandler::isFinished()
{
	if(m_currentGroup)
		return m_currentGroup->isFinished();
	return TRUE;
}

//-----------------------------------------------------------------------------
// PRIVATE FUNCTIONS //////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
TransitionGroup *GameWindowTransitionsHandler::findGroup( AsciiString groupName )
{
	if(groupName.isEmpty())
		return nullptr;

	TransitionGroupList::iterator it = m_transitionGroupList.begin();
	while( it != m_transitionGroupList.end() )
	{
		TransitionGroup *g = *it;
		if(groupName.compareNoCase(g->getName()) == 0)
			return g;
		it++;
	}
	return nullptr;
}

void GameWindowTransitionsHandler::parseWindow( INI* ini, void *instance, void *store, const void *userData )
{
	if (RetailFileLayers::enabled() || INIAudit::isEnabled())
	{
		static const FieldParse fields[] = {
			{ "WinName", INI::parseAsciiString, nullptr, offsetof(RetailTransitionWindow, name) },
			{ "Style", INI::parseLookupList, TransitionStyleNames, offsetof(RetailTransitionWindow, legacyStyle) },
			{ "FrameDelay", INI::parseInt, nullptr, offsetof(RetailTransitionWindow, frameDelay) },
			{ "Transition", parseRetailTransition, nullptr, 0 },
			{ nullptr, nullptr, nullptr, 0 }
		};
		RetailTransitionWindow window;
		ini->initFromINI(&window, fields);
		if (window.hasNested && window.frameDelay < 0)
		{
			ini->recordSkipped("field-error", "negative transition frame delay");
			return;
		}
		if (!window.hasNested)
		{
			RetailTransitionSpec spec;
			spec.style = window.legacyStyle;
			window.transitions.push_back(spec);
		}
		for (const auto &spec : window.transitions)
		{
			auto transition = std::make_unique<TransitionWindow>();
			transition->m_winName = window.name;
			transition->m_style = spec.style;
			transition->m_frameDelay = window.frameDelay;
			transition->m_startFrame = spec.startFrame;
			transition->m_endFrame = spec.endFrame;
			transition->m_fadeColor = spec.fadeColor;
			static_cast<TransitionGroup *>(instance)->addWindow(transition.get());
			transition.release();
		}
		return;
	}

	static const FieldParse myFieldParse[] =
		{
			{ "WinName",				INI::parseAsciiString,		nullptr,									offsetof( TransitionWindow, m_winName ) },
      { "Style",					INI::parseLookupList,			TransitionStyleNames,	offsetof( TransitionWindow, m_style ) },
			{ "FrameDelay",			INI::parseInt,						nullptr,									offsetof( TransitionWindow, m_frameDelay ) },
			{ nullptr,							nullptr,											nullptr, 0 }
		};
	TransitionWindow *transWin = NEW TransitionWindow;
	ini->initFromINI(transWin, myFieldParse);
	((TransitionGroup*)instance)->addWindow(transWin);
}

