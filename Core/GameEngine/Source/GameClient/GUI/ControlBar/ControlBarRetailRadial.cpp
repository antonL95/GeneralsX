#include "PreRTS.h"

#include <algorithm>
#include <vector>

#include "Common/INIException.h"
#include "Common/NameKeyGenerator.h"

#include "GameClient/ControlBar.h"
#include "GameClient/Display.h"
#include "GameClient/Drawable.h"
#include "GameClient/GadgetPushButton.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/InGameUI.h"
#include "GameClient/UiRadialRing.h"
#include "GameClient/UiScale.h"
#include "GameClient/View.h"

#include "GameLogic/Object.h"

namespace
{
Bool isRadialMouseMessage(UnsignedInt msg)
{
	return msg == GWM_LEFT_DOWN || msg == GWM_LEFT_UP || msg == GWM_LEFT_DRAG || msg == GWM_LEFT_DOUBLE_CLICK
		|| msg == GWM_RIGHT_DOWN || msg == GWM_RIGHT_UP || msg == GWM_RIGHT_DRAG || msg == GWM_RIGHT_DOUBLE_CLICK
		|| msg == GWM_MIDDLE_DOWN || msg == GWM_MIDDLE_UP || msg == GWM_MOUSE_POS;
}

WindowMsgHandledType retailRadialInput(GameWindow *, UnsignedInt msg, WindowMsgData mData1, WindowMsgData)
{
	if (!isRadialMouseMessage(msg))
		return MSG_IGNORED;
	return TheControlBar->isOverRetailRadial(mData1 & 0xFFFF, mData1 >> 16) ? MSG_HANDLED : MSG_IGNORED;
}

void copyRightClick(GameWindow *target, const GameWindow *source)
{
	if (BitIsSet(const_cast<GameWindow *>(source)->winGetStatus(), WIN_STATUS_RIGHT_CLICK))
	{
		target->winSetStatus(WIN_STATUS_RIGHT_CLICK);
		BitSet(target->winGetInstanceData()->m_status, WIN_STATUS_RIGHT_CLICK);
	}
	else
	{
		target->winClearStatus(WIN_STATUS_RIGHT_CLICK);
		BitClear(target->winGetInstanceData()->m_status, WIN_STATUS_RIGHT_CLICK);
	}
}

struct RingEntry
{
	Int entry;
	Int index;
};
}

void ControlBar::initRetailRadial(GameWindow *master)
{
	m_retailRadialGroup = TheWindowManager->winGetWindowFromId(master, NAMEKEY("ControlBar.wnd:RadialMenu"));
	if (!m_retailRadialGroup)
		throw INIException("RetailHud.wnd is missing the radial menu");
	m_retailRadialGroup->winSetInputFunc(retailRadialInput);
	for (Int i = 0; i < RETAIL_RADIAL_BUTTONS; ++i)
	{
		AsciiString name;
		name.format("ControlBar.wnd:RadialButton%02d", i + 1);
		m_retailRadial[i] = TheWindowManager->winGetWindowFromId(m_retailRadialGroup, TheNameKeyGenerator->nameToKey(name));
		if (!m_retailRadial[i])
			throw INIException("RetailHud.wnd is missing a radial button");
		m_retailRadial[i]->winSetOwner(master);
		m_retailSideButtons[i] = m_retailVisible[RETAIL_RADIAL_FIRST_SLOT + i];
		m_retailRadialSource[i] = nullptr;
		m_retailRadialEntry[i] = -1;
	}
}

void ControlBar::resetRetailRadial()
{
	if (!m_retailRadialGroup)
		return;
	for (Int i = 0; i < RETAIL_RADIAL_BUTTONS; ++i)
	{
		GameWindow *button = m_retailRadial[i];
		button->winHide(TRUE);
		BitClear(button->winGetInstanceData()->m_state, WIN_STATE_SELECTED | WIN_STATE_HILITED);
		GadgetButtonSetData(button, nullptr);
		m_retailRadialSource[i] = nullptr;
		m_retailRadialEntry[i] = -1;
		m_retailVisible[RETAIL_RADIAL_FIRST_SLOT + i] = m_retailSideButtons[i];
	}
	m_retailRadialGroup->winHide(TRUE);
	m_retailRadialMode = FALSE;
}

void ControlBar::beginRetailRadial(Object *obj)
{
	resetRetailRadial();
	if (!m_retailRadialGroup || !obj || !obj->isKindOf(KINDOF_STRUCTURE) || !obj->isLocallyControlled()
		|| !TheInGameUI || TheInGameUI->getSelectCount() != 1)
		return;
	m_retailRadialMode = TRUE;
	for (Int i = 0; i < RETAIL_RADIAL_BUTTONS; ++i)
	{
		m_retailSideButtons[i]->winHide(TRUE);
		m_retailVisible[RETAIL_RADIAL_FIRST_SLOT + i] = m_retailRadial[i];
	}
}

void ControlBar::addRetailRadialMirror(GameWindow *source, const CommandButton *command, Int entry)
{
	if (!m_retailRadialMode || !source || !command)
		return;
	for (Int i = 0; i < RETAIL_RADIAL_BUTTONS; ++i)
	{
		GameWindow *button = m_retailRadial[i];
		if (m_retailRadialEntry[i] >= 0 || GadgetButtonGetData(button))
			continue;
		setControlCommand(button, command, FALSE);
		copyRightClick(button, source);
		button->winEnable(BitIsSet(source->winGetStatus(), WIN_STATUS_ENABLED));
		button->winSetEnabledImage(0, source->winGetEnabledImage(0));
		GadgetButtonSetText(button, UnicodeString::TheEmptyString);
		button->winHide(FALSE);
		m_retailRadialSource[i] = source;
		m_retailRadialEntry[i] = entry;
		return;
	}
}

void ControlBar::finishRetailRadial(const std::vector<RetailPlacement> &placements)
{
	if (!m_retailRadialMode)
		return;
	for (const RetailPlacement &placement : placements)
		if (placement.slot >= RETAIL_RADIAL_FIRST_SLOT && placement.slot < MAX_COMMANDS_PER_SET)
			m_retailRadialEntry[placement.slot - RETAIL_RADIAL_FIRST_SLOT] = placement.entry;
	for (const RetailPlacement &placement : placements)
		if (placement.slot < RETAIL_RADIAL_FIRST_SLOT && placement.command->isRadial())
			addRetailRadialMirror(m_retailVisible[placement.slot], placement.command, placement.entry);
}

const GameWindow *ControlBar::retailRadialSource(const GameWindow *window) const
{
	for (Int i = 0; i < RETAIL_RADIAL_BUTTONS; ++i)
		if (window == m_retailRadial[i] && m_retailRadialSource[i])
			return m_retailRadialSource[i];
	return window;
}

Bool ControlBar::isRetailRadialButton(const GameWindow *window) const
{
	for (Int i = 0; i < RETAIL_RADIAL_BUTTONS; ++i)
		if (window == m_retailRadial[i])
			return TRUE;
	return FALSE;
}

Bool ControlBar::isOverRetailRadial(Int px, Int py) const
{
	for (Int i = 0; i < RETAIL_RADIAL_BUTTONS; ++i)
	{
		GameWindow *button = m_retailRadial[i];
		if (!button || button->winIsHidden())
			continue;
		Int x, y, width, height;
		button->winGetScreenPosition(&x, &y);
		button->winGetSize(&width, &height);
		const Int radius = MIN(width, height) / 2;
		const Int dx = px - (x + width / 2);
		const Int dy = py - (y + height / 2);
		if (dx * dx + dy * dy <= radius * radius)
			return TRUE;
	}
	return FALSE;
}

void ControlBar::updateRetailRadial()
{
	if (!m_retailRadialGroup)
		return;
	Object *obj = m_retailRadialMode && m_currentSelectedDrawable ? m_currentSelectedDrawable->getObject() : nullptr;
	if (!obj || !TheTacticalView || !TheDisplay)
	{
		m_retailRadialGroup->winHide(TRUE);
		return;
	}

	std::vector<RingEntry> ring;
	for (Int i = 0; i < RETAIL_RADIAL_BUTTONS; ++i)
	{
		GameWindow *button = m_retailRadial[i];
		GameWindow *source = m_retailRadialSource[i];
		if (source)
		{
			button->winHide(source->winIsHidden());
			button->winEnable(BitIsSet(source->winGetStatus(), WIN_STATUS_ENABLED));
			button->winSetEnabledImage(0, source->winGetEnabledImage(0));
			const UnicodeString &text = source->winGetInstanceData()->getText();
			if (button->winGetInstanceData()->getText().compare(text) != 0)
				GadgetButtonSetText(button, text);
		}
		if (m_retailRadialEntry[i] >= 0 && GadgetButtonGetData(button) && !button->winIsHidden())
			ring.push_back(RingEntry{ m_retailRadialEntry[i], i });
	}
	if (ring.empty())
	{
		m_retailRadialGroup->winHide(TRUE);
		return;
	}
	std::stable_sort(ring.begin(), ring.end(), [](const RingEntry &a, const RingEntry &b) { return a.entry < b.entry; });

	Coord3D center = *obj->getPosition();
	center.z += obj->getGeometryInfo().getMaxHeightAbovePosition() * 0.5f;
	ICoord2D screen;
	if (TheTacticalView->worldToScreenTriReturn(&center, &screen) == View::WTS_INVALID)
	{
		m_retailRadialGroup->winHide(TRUE);
		return;
	}

	const double scale = UiScale::scale(TheDisplay->getWidth(), TheDisplay->getHeight());
	const Bool compact = obj->isKindOf(KINDOF_EXPANSION_PAD) || obj->isKindOf(KINDOF_CASTLE_KEEP);
	const std::vector<UiRadialRing::PixelButton> buttons = UiRadialRing::layout(static_cast<int>(ring.size()),
		screen.x, screen.y, scale, compact);
	Int left = buttons.front().x;
	Int top = buttons.front().y;
	Int right = left;
	Int bottom = top;
	for (const UiRadialRing::PixelButton &button : buttons)
	{
		left = MIN(left, button.x);
		top = MIN(top, button.y);
		right = MAX(right, button.x + button.size);
		bottom = MAX(bottom, button.y + button.size);
	}
	m_retailRadialGroup->winSetPosition(left, top);
	m_retailRadialGroup->winSetSize(right - left, bottom - top);
	for (size_t i = 0; i < ring.size(); ++i)
	{
		GameWindow *button = m_retailRadial[ring[i].index];
		Int width, height;
		button->winGetSize(&width, &height);
		if (width != buttons[i].size || height != buttons[i].size)
			button->winSetSize(buttons[i].size, buttons[i].size);
		button->winSetPosition(buttons[i].x - left, buttons[i].y - top);
	}
	m_retailRadialGroup->winHide(FALSE);
}
