#include "PreRTS.h"
#include "GameClient/FormationDragXlat.h"

#include "GameClient/Drawable.h"
#include "GameClient/InGameUI.h"
#include "GameClient/View.h"
#include "GameLogic/FormationAssistant.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "Lib/Coord2D.h"

void FormationDragTranslator::begin(const ICoord2D& pixel)
{
	if (!TheGameLogic->isInGame() || !TheInGameUI->areSelectedObjectsControllable())
		return;
	m_dragging = TRUE;
	m_swallowClicks = TRUE;
	m_start = pixel;
	m_current = pixel;
	updatePreview();
}

Bool FormationDragTranslator::computeDrag(Coord3D& anchor, Real& angle, Real& dragLength, Int& templateIndex) const
{
	Coord3D end;
	if (!TheTacticalView->screenToTerrain(&m_start, &anchor) || !TheTacticalView->screenToTerrain(&m_current, &end))
		return FALSE;

	UnsignedInt count = 0;
	Coord2D fromUnits = {0.0f, 0.0f};
	for (const Drawable* drawable : *TheInGameUI->getAllSelectedDrawables())
	{
		const Object* object = drawable->getObject();
		if (!object || !object->getAI() || object->isKindOf(KINDOF_IMMOBILE))
			continue;
		++count;
		fromUnits.x += anchor.x - object->getPosition()->x;
		fromUnits.y += anchor.y - object->getPosition()->y;
	}
	if (count == 0)
		return FALSE;

	Coord2D direction = {end.x - anchor.x, end.y - anchor.y};
	dragLength = direction.length();
	if (dragLength == 0.0f)
		direction = fromUnits;

	templateIndex = FormationAssistant::get().chooseTemplate(dragLength, count);
	if (templateIndex < 0)
		return FALSE;

	angle = direction.toAngle();
	return TRUE;
}

void FormationDragTranslator::updatePreview()
{
	InGameUI::FormationDragPreview preview;
	if (computeDrag(preview.anchor, preview.angle, preview.dragLength, preview.templateIndex))
	{
		preview.active = TRUE;
		TheInGameUI->setFormationDragPreview(preview);
		return;
	}
	TheInGameUI->clearFormationDragPreview();
}

void FormationDragTranslator::finish()
{
	m_dragging = FALSE;
	TheInGameUI->clearFormationDragPreview();

	Coord3D anchor;
	Real angle = 0.0f;
	Real dragLength = 0.0f;
	Int templateIndex = -1;
	if (!computeDrag(anchor, angle, dragLength, templateIndex))
		return;

	GameMessage* move = TheMessageStream->appendMessage(GameMessage::MSG_DO_MOVETO_FORMATION);
	move->appendLocationArgument(anchor);
	move->appendRealArgument(angle);
	move->appendIntegerArgument(templateIndex);
	move->appendBooleanArgument(TheInGameUI->isInAttackMoveToMode());
}

GameMessageDisposition FormationDragTranslator::translateGameMessage(const GameMessage* msg)
{
	switch (msg->getType())
	{
		case GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_DOWN:
		case GameMessage::MSG_RAW_MOUSE_RIGHT_BUTTON_DOWN:
		{
			const Bool left = msg->getType() == GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_DOWN;
			(left ? m_leftDown : m_rightDown) = TRUE;
			if (!(left ? m_rightDown : m_leftDown))
			{
				m_swallowClicks = FALSE;
				return KEEP_MESSAGE;
			}
			begin(msg->getArgument(0)->pixel);
			if (m_dragging)
				TheMessageStream->insertMessage(GameMessage::MSG_CLIENT_FORMATION_DRAG, const_cast<GameMessage*>(msg));
			return m_dragging ? DESTROY_MESSAGE : KEEP_MESSAGE;
		}
		case GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_UP:
		case GameMessage::MSG_RAW_MOUSE_RIGHT_BUTTON_UP:
		{
			(msg->getType() == GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_UP ? m_leftDown : m_rightDown) = FALSE;
			if (m_dragging)
				finish();
			return KEEP_MESSAGE;
		}
		case GameMessage::MSG_RAW_MOUSE_POSITION:
		{
			if (!m_dragging)
				return KEEP_MESSAGE;
			m_current = msg->getArgument(0)->pixel;
			updatePreview();
			return DESTROY_MESSAGE;
		}
		case GameMessage::MSG_RAW_MOUSE_LEFT_DRAG:
		case GameMessage::MSG_RAW_MOUSE_RIGHT_DRAG:
			return m_dragging ? DESTROY_MESSAGE : KEEP_MESSAGE;
		case GameMessage::MSG_MOUSE_LEFT_CLICK:
		case GameMessage::MSG_MOUSE_RIGHT_CLICK:
		case GameMessage::MSG_MOUSE_LEFT_DOUBLE_CLICK:
		case GameMessage::MSG_MOUSE_RIGHT_DOUBLE_CLICK:
			return m_swallowClicks ? DESTROY_MESSAGE : KEEP_MESSAGE;
		default:
			return KEEP_MESSAGE;
	}
}
