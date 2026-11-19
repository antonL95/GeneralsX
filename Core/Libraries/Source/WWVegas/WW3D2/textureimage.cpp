#include "textureimage.h"

#include "WWLib/ffactory.h"

#include <algorithm>
#include <utility>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_ONLY_TGA
#define STBI_NO_STDIO
#define STBI_MAX_DIMENSIONS 8192
#include <stb_image.h>

namespace
{
using DecodedImage = std::unique_ptr<unsigned char, void (*)(void*)>;

std::vector<unsigned char> Read_Image(const char* name)
{
	file_auto_ptr file(_TheFileFactory, name);
	if (!file->Is_Available() || !file->Open())
		return {};

	const int size = file->Size();
	if (size <= 0 || size > 64 * 1024 * 1024)
		return {};

	std::vector<unsigned char> bytes(size);
	if (file->Read(bytes.data(), size) != size)
		return {};
	return bytes;
}

struct Sample
{
	int first;
	int second;
	int weight;
};

Sample Alpha_Sample(int position, int size, int mask_size)
{
	constexpr int scale = 256;
	const int center = static_cast<int>(((2LL * position + 1) * mask_size * (scale / 2)) / size) -
		scale / 2;
	const int coordinate = std::clamp(center, 0, (mask_size - 1) * scale);
	const int first = coordinate / scale;
	return {first, std::min(first + 1, mask_size - 1), coordinate % scale};
}

void Apply_Alpha(unsigned char* color, int width, int height,
	const unsigned char* mask, int mask_width, int mask_height)
{
	if (width == mask_width && height == mask_height)
	{
		for (int i = 0; i < width * height; ++i)
			color[4 * i + 3] = mask[4 * i + 3];
		return;
	}

	std::vector<Sample> columns;
	columns.reserve(width);
	for (int x = 0; x < width; ++x)
		columns.push_back(Alpha_Sample(x, width, mask_width));
	for (int y = 0; y < height; ++y)
	{
		const auto vertical = Alpha_Sample(y, height, mask_height);
		const auto* top = mask + 4 * vertical.first * mask_width + 3;
		const auto* bottom = mask + 4 * vertical.second * mask_width + 3;
		for (const auto& horizontal : columns)
		{
			const int upper = (256 - horizontal.weight) * top[4 * horizontal.first] +
				horizontal.weight * top[4 * horizontal.second];
			const int lower = (256 - horizontal.weight) * bottom[4 * horizontal.first] +
				horizontal.weight * bottom[4 * horizontal.second];
			color[3] = static_cast<unsigned char>(((256 - vertical.weight) * upper +
				vertical.weight * lower + 32768) / 65536);
			color += 4;
		}
	}
}
}

TextureImage::TextureImage() : Pixels(nullptr, stbi_image_free)
{
}

long TextureImage::Open(const char* name, long mode)
{
	SetImage(nullptr);
	Pixels.reset();
	HasCompanion = false;
	ImageName = name;
	const auto extension = ImageName.find_last_of('.');
	const auto separator = ImageName.find_last_of("/\\");
	const bool has_extension = extension != std::string::npos &&
		(separator == std::string::npos || extension > separator);
	if (has_extension && stricmp(ImageName.c_str() + extension, ".jpg") == 0)
		ImageName.replace(extension, std::string::npos, ".tga");

	const long result = Targa::Open(ImageName.c_str(), mode);
	if (result != TGAERR_OPEN || mode != TGA_READMODE || !has_extension)
		return result;

	const std::string dds = ImageName.substr(0, extension) + ".dds";
	file_auto_ptr file(_TheFileFactory, dds.c_str());
	if (file->Is_Available() && file->Open())
		return result;

	const std::string jpg = ImageName.substr(0, extension) + ".jpg";
	return Load_Jpeg(jpg.c_str()) ? 0 : result;
}

long TextureImage::Load(const char* name, long flags, bool invert_image)
{
	if (Pixels)
		return 0;
	return Targa::Load(ImageName.empty() ? name : ImageName.c_str(), flags, invert_image);
}

bool TextureImage::Load_Jpeg(const char* name)
{
	const auto bytes = Read_Image(name);
	if (bytes.empty())
		return false;

	int width = 0;
	int height = 0;
	DecodedImage color(stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()),
		&width, &height, nullptr, STBI_rgb_alpha), stbi_image_free);
	if (!color)
		return false;

	ImageName = name;
	const std::string png = ImageName.substr(0, ImageName.find_last_of('.')) + ".png";
	const auto mask = Read_Image(png.c_str());
	int mask_width = 0;
	int mask_height = 0;
	const DecodedImage alpha(mask.empty() ? nullptr : stbi_load_from_memory(mask.data(),
		static_cast<int>(mask.size()), &mask_width, &mask_height, nullptr, STBI_rgb_alpha), stbi_image_free);

	HasCompanion = alpha != nullptr;
	if (alpha)
		Apply_Alpha(color.get(), width, height, alpha.get(), mask_width, mask_height);
	for (std::size_t i = 0; i < static_cast<std::size_t>(width) * height * 4; i += 4)
		std::swap(color.get()[i], color.get()[i + 2]);

	Header = {};
	Header.ImageType = TGA_TRUECOLOR;
	Header.Width = static_cast<short>(width);
	Header.Height = static_cast<short>(height);
	Header.PixelDepth = 32;
	Header.ImageDescriptor = TGAIDF_YORIGIN | 8;
	Pixels = std::move(color);
	SetImage(reinterpret_cast<char*>(Pixels.get()));
	return true;
}
