#pragma once

#include "WWLib/TARGA.h"

#include <memory>
#include <string>

class TextureImage : public Targa
{
public:
	TextureImage();
	long Open(const char* name, long mode);
	long Load(const char* name, long flags, bool invert_image = true);
	bool Load_Jpeg(const char* name);
	const char* Resolved_Name() const { return ImageName.c_str(); }
	bool Has_Companion() const { return HasCompanion; }

private:
	std::string ImageName;
	std::unique_ptr<unsigned char, void (*)(void*)> Pixels;
	bool HasCompanion = false;
};
