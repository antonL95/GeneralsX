#include "PreRTS.h"

#include "GameClient/UiFrame.h"

#include "Common/INI.h"
#include "GameClient/Display.h"
#include "GameClient/Image.h"
#include "GameClient/UiScale.h"

#include <cctype>

UiFrameCollection* TheUiFrameCollection = nullptr;

namespace
{
	std::string frameKey(const AsciiString& name)
	{
		std::string key(name.str());
		for (char& ch : key)
			ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
		return key;
	}

	UiFrame* frameOf(void* instance)
	{
		return static_cast<UiFrame*>(instance);
	}

	UiNineSlice::Fill scanFill(INI* ini)
	{
		UiNineSlice::Fill fill = UiNineSlice::Fill::Stretch;
		if (!UiNineSlice::parseFill(ini->getNextToken(), fill))
			throw INI_INVALID_DATA;
		return fill;
	}

	Int scanInset(INI* ini)
	{
		const Int value = INI::scanInt(ini->getNextToken());
		if (value < 0)
			throw INI_INVALID_DATA;
		return value;
	}
}

const FieldParse UiFrame::s_fieldParseTable[] =
{
	{ "Image", UiFrame::parseImageName, nullptr, 0 },
	{ "Texture", UiFrame::parseTexture, nullptr, 0 },
	{ "TextureWidth", UiFrame::parseTextureWidth, nullptr, 0 },
	{ "TextureHeight", UiFrame::parseTextureHeight, nullptr, 0 },
	{ "Coords", UiFrame::parseCoords, nullptr, 0 },
	{ "Insets", UiFrame::parseInsets, nullptr, 0 },
	{ "CornerScale", UiFrame::parseCornerScale, nullptr, 0 },
	{ "Edges", UiFrame::parseEdges, nullptr, 0 },
	{ "Center", UiFrame::parseCenter, nullptr, 0 },
	{ "Color", UiFrame::parseColor, nullptr, 0, true },
	{ nullptr, nullptr, nullptr, 0 }
};

UiFrame::UiFrame(const AsciiString& name)
	: m_name(name)
	, m_image(nullptr)
	, m_cornerScale(1.0f)
	, m_edges(UiNineSlice::Fill::Stretch)
	, m_center(UiNineSlice::Fill::Stretch)
	, m_color(GameMakeColor(255, 255, 255, 255))
{
}

UiFrame::~UiFrame()
{
	if (m_image)
		deleteInstance(m_image);
}

Image* UiFrame::ownImage()
{
	if (m_image == nullptr)
	{
		m_image = newInstance(Image);
		m_image->setName(m_name);
	}
	return m_image;
}

const Image* UiFrame::getImage() const
{
	if (m_image)
		return m_image;
	if (m_imageName.isEmpty() || TheMappedImageCollection == nullptr)
		return nullptr;
	return TheMappedImageCollection->findImageByName(m_imageName);
}

void UiFrame::draw(Int startX, Int startY, Int endX, Int endY) const
{
	const Image* image = getImage();
	if (TheDisplay == nullptr || image == nullptr)
		return;
	const Real scale = static_cast<Real>(UiScale::scale(TheDisplay->getWidth(), TheDisplay->getHeight())) * m_cornerScale;
	TheDisplay->drawImageNineSlice(image, m_insets, startX, startY, endX, endY, scale, m_edges, m_center, m_color);
}

void UiFrame::parseDefinition(INI* ini)
{
	const char* name = ini->getNextToken();
	if (TheUiFrameCollection == nullptr)
	{
		DEBUG_CRASH(("UiFrame '%s' parsed without a frame collection", name));
		throw INI_INVALID_DATA;
	}
	UiFrame* frame = TheUiFrameCollection->findOrCreate(name);
	ini->initFromINI(frame, s_fieldParseTable);
}

void UiFrame::parseImageName(INI* ini, void* instance, void*, const void*)
{
	frameOf(instance)->m_imageName = ini->getNextToken();
}

void UiFrame::parseTexture(INI* ini, void* instance, void*, const void*)
{
	frameOf(instance)->ownImage()->setFilename(ini->getNextToken());
}

void UiFrame::parseTextureWidth(INI* ini, void* instance, void*, const void*)
{
	frameOf(instance)->ownImage()->setTextureWidth(INI::scanInt(ini->getNextToken()));
}

void UiFrame::parseTextureHeight(INI* ini, void* instance, void*, const void*)
{
	frameOf(instance)->ownImage()->setTextureHeight(INI::scanInt(ini->getNextToken()));
}

void UiFrame::parseCoords(INI* ini, void* instance, void*, const void*)
{
	Image::parseImageCoords(ini, frameOf(instance)->ownImage(), nullptr, nullptr);
}

void UiFrame::parseInsets(INI* ini, void* instance, void*, const void*)
{
	UiNineSlice::Insets insets;
	insets.left = scanInset(ini);
	insets.top = scanInset(ini);
	insets.right = scanInset(ini);
	insets.bottom = scanInset(ini);
	frameOf(instance)->m_insets = insets;
}

void UiFrame::parseCornerScale(INI* ini, void* instance, void*, const void*)
{
	const Real scale = INI::scanReal(ini->getNextToken());
	if (!(scale > 0.0f))
		throw INI_INVALID_DATA;
	frameOf(instance)->m_cornerScale = static_cast<Real>(UiNineSlice::clampCornerScale(scale));
}

void UiFrame::parseEdges(INI* ini, void* instance, void*, const void*)
{
	frameOf(instance)->m_edges = scanFill(ini);
}

void UiFrame::parseCenter(INI* ini, void* instance, void*, const void*)
{
	frameOf(instance)->m_center = scanFill(ini);
}

void UiFrame::parseColor(INI* ini, void* instance, void*, const void*)
{
	INI::parseColorInt(ini, nullptr, &frameOf(instance)->m_color, nullptr);
}

UiFrameCollection::~UiFrameCollection()
{
	for (auto& entry : m_frames)
		delete entry.second;
}

void UiFrameCollection::load()
{
	INI ini;
	ini.loadText("GeneralsX\\UiFrames.ini", UiFramesIniText, INI_LOAD_OVERWRITE, "UiFrame", UiFrame::parseDefinition);
}

UiFrame* UiFrameCollection::findOrCreate(const AsciiString& name)
{
	UiFrame*& frame = m_frames[frameKey(name)];
	if (frame == nullptr)
		frame = new UiFrame(name);
	return frame;
}

const UiFrame* UiFrameCollection::find(const AsciiString& name) const
{
	const auto it = m_frames.find(frameKey(name));
	return it == m_frames.end() ? nullptr : it->second;
}
