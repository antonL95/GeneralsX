#pragma once

#include "Common/AsciiString.h"
#include "GameClient/Color.h"
#include "GameClient/UiNineSlice.h"

#include <map>
#include <string>

class Image;
class INI;
struct FieldParse;

class UiFrame
{
public:
	explicit UiFrame(const AsciiString& name);
	~UiFrame();

	UiFrame(const UiFrame&) = delete;
	UiFrame& operator=(const UiFrame&) = delete;

	const AsciiString& getName() const { return m_name; }
	const Image* getImage() const;
	const UiNineSlice::Insets& getInsets() const { return m_insets; }
	Real getCornerScale() const { return m_cornerScale; }
	UiNineSlice::Fill getEdges() const { return m_edges; }
	UiNineSlice::Fill getCenter() const { return m_center; }
	Color getColor() const { return m_color; }

	void draw(Int startX, Int startY, Int endX, Int endY) const;

	static void parseDefinition(INI* ini);

private:
	Image* ownImage();

	static void parseImageName(INI* ini, void* instance, void* store, const void* userData);
	static void parseTexture(INI* ini, void* instance, void* store, const void* userData);
	static void parseTextureWidth(INI* ini, void* instance, void* store, const void* userData);
	static void parseTextureHeight(INI* ini, void* instance, void* store, const void* userData);
	static void parseCoords(INI* ini, void* instance, void* store, const void* userData);
	static void parseInsets(INI* ini, void* instance, void* store, const void* userData);
	static void parseCornerScale(INI* ini, void* instance, void* store, const void* userData);
	static void parseEdges(INI* ini, void* instance, void* store, const void* userData);
	static void parseCenter(INI* ini, void* instance, void* store, const void* userData);
	static void parseColor(INI* ini, void* instance, void* store, const void* userData);

	static const FieldParse s_fieldParseTable[];

	AsciiString m_name;
	AsciiString m_imageName;
	Image* m_image;
	UiNineSlice::Insets m_insets;
	Real m_cornerScale;
	UiNineSlice::Fill m_edges;
	UiNineSlice::Fill m_center;
	Color m_color;
};

class UiFrameCollection
{
public:
	UiFrameCollection() = default;
	~UiFrameCollection();

	UiFrameCollection(const UiFrameCollection&) = delete;
	UiFrameCollection& operator=(const UiFrameCollection&) = delete;

	void load();
	UiFrame* findOrCreate(const AsciiString& name);
	const UiFrame* find(const AsciiString& name) const;
	Int count() const { return static_cast<Int>(m_frames.size()); }

private:
	std::map<std::string, UiFrame*> m_frames;
};

extern UiFrameCollection* TheUiFrameCollection;
extern const char* const UiFramesIniText;
