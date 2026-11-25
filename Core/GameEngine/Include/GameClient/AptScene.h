#pragma once

#include "Lib/BaseType.h"
#include "Lib/Coord2D.h"

#include <cstddef>
#include <functional>
#include <initializer_list>
#include <memory>
#include <string>
#include <utility>

class AptScene
{
public:
	struct Box
	{
		Real left = 0, top = 0, right = 0, bottom = 0;
		Bool contains(Real x, Real y) const;
		Bool valid() const;
	};

	struct Text
	{
		Box box;
		std::string value;
		std::string font;
		Real size;
		Int alignment;
	};

	using Visibility = std::function<Bool(const std::string &)>;
	using DrawText = std::function<void(const std::string &, const Text &)>;
	using HorizontalScale = std::function<Real(const std::string &)>;

	explicit AptScene(const char *movie, const char *frame = nullptr,
		std::initializer_list<std::pair<const char*, const char*>> nestedFrames = {});
	struct AnimatedExport { const char *name; };
	AptScene(const char *movie, AnimatedExport exported);
	struct LabeledLoop { const char *label; };
	AptScene(const char *movie, LabeledLoop loop);
	~AptScene();
	Bool valid() const;
	Box bounds(const std::string &path) const;
	Coord2D position(const std::string &path, Coord2D fallback) const;
	void draw(const Visibility &visible, const DrawText &text, const HorizontalScale &scale = {}) const;
	void drawAt(const Visibility &visible, Real x, Real y, Real sx, Real sy, Real opacity) const;
	Int frameCount() const;
	void drawFrameAt(Int frame, Real x, Real y, Real sx, Real sy) const;
	Real xScale() const;
	Real yScale() const;

private:
	void loadAnimated(const char *movie, const char *exportName, const char *loopLabel, size_t maxFrames);
	void drawArt(const Visibility &visible, const HorizontalScale &scale, Real x, Real y, Real sx, Real sy, Real opacity, Int frame = -1) const;
	class Data;
	std::unique_ptr<Data> m_data;
};
