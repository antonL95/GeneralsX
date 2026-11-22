#pragma once

#include "WW3D2/retailfx.h"

#include <memory>

namespace RetailFX
{

inline constexpr unsigned LookupSize = 32;

class LookupPass
{
public:
	~LookupPass();
	using CreateShader = void *(*)(bool, const uint32_t *);
	static std::unique_ptr<LookupPass> make(void *device, const Effect &effect, CreateShader createShader,
		std::span<const uint8_t> pixels, unsigned pixelSize, bool topOrigin);
	bool draw(void *device, void *frameBuffer, int width, int height, float blend) const;

private:
	struct Data;
	explicit LookupPass(std::unique_ptr<Data> data);
	std::unique_ptr<Data> m_data;
};

}
