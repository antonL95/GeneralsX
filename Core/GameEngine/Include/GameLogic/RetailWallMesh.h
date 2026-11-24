#pragma once

#include <array>
#include <vector>

#include "Common/AsciiString.h"
#include "Lib/BaseType.h"

class Object;

namespace RetailWallMesh
{
struct Mesh
{
	std::vector<Coord3D> localVertices;
	std::vector<Coord3D> vertices;
	std::vector<std::array<UnsignedInt, 3>> faces;
};

struct Ramp
{
	Coord3D from;
	Coord3D to;
	Coord3D fromLeft;
	Coord3D fromRight;
	Coord3D toLeft;
	Coord3D toRight;
	Real width;
};

struct Geometry
{
	std::vector<Coord3D> wall;
	Real wallHeight = 0.0f;
	Ramp ramps[2]{};
	Bool hasRamp[2] = {FALSE, FALSE};
};

Bool load(const AsciiString& model, const AsciiString& name, Mesh& mesh);
Bool polygon(const Mesh& mesh, std::vector<Coord3D>& points);
Bool ramp(const Mesh& mesh, Ramp& result);
const Geometry* forObject(const Object& object);
Bool contains(const Geometry& geometry, const Coord3D& position);
void clearCache();
}
