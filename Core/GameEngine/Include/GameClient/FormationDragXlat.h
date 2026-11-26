#pragma once

#include "Common/MessageStream.h"
#include "Lib/BaseType.h"

class FormationDragTranslator : public GameMessageTranslator
{
public:
	virtual GameMessageDisposition translateGameMessage(const GameMessage* msg) override;

private:
	void begin(const ICoord2D& pixel);
	Bool computeDrag(Coord3D& anchor, Real& angle, Real& dragLength, Int& templateIndex) const;
	void updatePreview();
	void finish();

	Bool m_leftDown = FALSE;
	Bool m_rightDown = FALSE;
	Bool m_dragging = FALSE;
	Bool m_swallowClicks = FALSE;
	ICoord2D m_start = {0, 0};
	ICoord2D m_current = {0, 0};
};
