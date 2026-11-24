#pragma once

#include <array>
#include <vector>

#include "Common/AsciiString.h"
#include "Lib/BaseType.h"

namespace RetailWallMesh
{
struct Mesh
{
	std::vector<Coord3D> vertices;
	std::vector<std::array<UnsignedInt, 3>> faces;
};

Bool load(const AsciiString& model, const AsciiString& name, Mesh& mesh);
}
