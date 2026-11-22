#pragma once

#include <array>

namespace RetailFX
{

class WaterReflectionImage
{
public:
	WaterReflectionImage() = default;
	~WaterReflectionImage();
	WaterReflectionImage(const WaterReflectionImage &) = delete;
	WaterReflectionImage &operator=(const WaterReflectionImage &) = delete;

	bool prepare(void *device, void *source, const std::array<float, 4> &clipBounds);
	void *texture() const { return m_texture; }

private:
	void *m_texture = nullptr;
};

}
