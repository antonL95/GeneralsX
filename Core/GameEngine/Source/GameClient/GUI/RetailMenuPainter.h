#pragma once

#include "GameClient/AptScene.h"
#include "GameClient/Display.h"
#include "GameClient/DisplayString.h"
#include "GameClient/DisplayStringManager.h"
#include "GameClient/GameText.h"
#include "GameClient/Image.h"
#include "GameClient/UiFrame.h"

#include <algorithm>
#include <string_view>

namespace RetailMenuPainter
{
	using Box = AptScene::Box;
	constexpr Color white = 0xFFFFFFFF;
	constexpr Color muted = 0xFFB3CBD5;

	inline UnicodeString translated(const char *text)
	{
		UnicodeString result;
		result.translate(text);
		return result;
	}

	inline UnicodeString localize(const char *key, const char *fallback)
	{
		Bool exists = FALSE;
		UnicodeString text = TheGameText->fetch(key, &exists);
		return exists ? text : translated(fallback);
	}

	inline UnicodeString number(Int n)
	{
		UnicodeString result;
		result.format(L"%d", n);
		return result;
	}

	class Painter
	{
	public:
		explicit Painter(const AptScene &scene) : sx(scene.xScale()), sy(scene.yScale()),
			m_text(TheDisplayStringManager->newDisplayString()) {}
		~Painter() { TheDisplayStringManager->freeDisplayString(m_text); }

		const Real sx, sy;

		void picture(const Image *image, const Box &box) const
		{
			if (image && box.valid())
				TheDisplay->drawImage(image, Int(box.left * sx), Int(box.top * sy), Int(box.right * sx), Int(box.bottom * sy));
		}

		void background(const Image *image) const
		{
			const Int width = TheDisplay->getWidth(), height = TheDisplay->getHeight();
			TheDisplay->drawFillRect(0, 0, width, height, GameMakeColor(19, 29, 30, 255));
			if (image)
				TheDisplay->drawImage(image, 0, 0, width, height, GameMakeColor(70, 99, 97, 255));
		}

		void text(UnicodeString value, const Box &box, Real size, Color color = white,
			const char *fontName = "Albertus MT", Bool centered = TRUE, Bool wrap = FALSE) const
		{
			if (value.isEmpty() || !box.valid())
				return;

			GameFont *font = TheFontLibrary->getFontPixels(fontName, std::max(1, Int(size * sy + 0.5f)), FALSE);
			if (!font)
				font = TheFontLibrary->getFontPixels("Albertus MT", std::max(1, Int(size * sy + 0.5f)), FALSE);
			if (!font)
				return;

			m_text->setFont(font);
			const Int maxWidth = std::max(1, Int((box.right - box.left) * sx));
			m_text->setWordWrap(wrap ? maxWidth : 0);
			m_text->setText(value);
			Int width, height;
			m_text->getSize(&width, &height);
			const Int left = Int(box.left * sx);
			const Int x = centered ? left + std::max(0, (maxWidth - width) / 2) : left;
			const Int y = Int(box.top * sy) + (wrap ? 0 : std::max(0, (Int((box.bottom - box.top) * sy) - height) / 2));
			IRegion2D clip{{left, Int(box.top * sy)}, {left + maxWidth, Int(box.bottom * sy)}};
			m_text->setClipRegion(&clip);
			m_text->draw(x, y, color, GameMakeColor(0, 0, 0, 230));
		}

		void label(UnicodeString value, const AptScene::Text &label, Color color) const
		{
			text(value, label.box, label.size, color, label.font.c_str(), label.alignment == 2);
		}

	private:
		DisplayString *m_text;
	};

	inline Bool childOf(std::string_view path, std::string_view prefix)
	{
		return path.starts_with(prefix) && (path.size() == prefix.size() || path[prefix.size()] == '.');
	}

	inline Box inset(Box box, Real left, Real right = 0)
	{
		box.left += left;
		box.right -= right;
		return box;
	}

	inline void fill(const Painter &p, const Box &box, Color color)
	{
		TheDisplay->drawFillRect(Int(box.left * p.sx), Int(box.top * p.sy),
			Int((box.right - box.left) * p.sx), Int((box.bottom - box.top) * p.sy), color);
	}

	inline Bool moviePart(const Painter &p, const AptScene &scene, const std::string &path, const Box &box, Real opacity = 1.0f)
	{
		const Box source = scene.bounds(path);
		if (!source.valid())
			return FALSE;
		const Real sx = p.sx * (box.right - box.left) / (source.right - source.left);
		const Real sy = p.sy * (box.bottom - box.top) / (source.bottom - source.top);
		scene.drawAt([&](const std::string &part) { return childOf(part, path); },
			box.left * p.sx - source.left * sx, box.top * p.sy - source.top * sy, sx, sy, opacity);
		return TRUE;
	}

	inline void frame(const Painter &p, const Box &box, const char *name)
	{
		const auto *art = TheUiFrameCollection ? TheUiFrameCollection->find(name) : nullptr;
		if (art)
			art->draw(Int(box.left * p.sx), Int(box.top * p.sy), Int(box.right * p.sx), Int(box.bottom * p.sy));
	}

	inline void button(const Painter &p, const Box &box, UnicodeString label)
	{
		fill(p, inset(box, 6, 6), 0xE6172F3C);
		frame(p, box, "MenuButtonMed");
		p.text(label, box, 16);
	}

	inline void control(const Painter &p, const Box &box, UnicodeString label, Bool selected = FALSE,
		Bool dropdown = FALSE, Color color = 0)
	{
		fill(p, box, selected ? 0xFF294A59 : 0xE90A141B);
		if (selected)
			p.picture(TheMappedImageCollection->findImageByName("AptListBoxHiliteSelectedItem"), box);
		if (color)
			fill(p, {box.left + 9, box.top + 5, box.left + 31, box.bottom - 5}, color);
		p.text(label, inset(box, color ? 38 : 9, dropdown ? 22 : 6), 16, white, "Albertus MT", !dropdown);
		if (dropdown)
		{
			const auto *arrow = TheMappedImageCollection->findImageByName("AptVSliderDownButtonEnabled");
			const Box arrowBox{box.right - 22, box.top, box.right, box.bottom};
			if (arrow)
				p.picture(arrow, arrowBox);
			else
				p.text(translated("v"), arrowBox, 14, muted);
		}
	}

}
