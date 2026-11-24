#pragma once

#include "GameClient/GameWindow.h"

#include <memory>

namespace RetailMenuPainter
{
	class Painter;
}

class RetailHeroEditorData;

class RetailHeroEditor
{
public:
	static Bool available();
	RetailHeroEditor();
	~RetailHeroEditor();

	void draw(const RetailMenuPainter::Painter &painter) const;
	Bool input(UnsignedInt message, WindowMsgData point, Bool key, Real x, Real y);
	Bool nameFocused() const;

private:
	std::unique_ptr<RetailHeroEditorData> m_data;
};
