#include "PreRTS.h"
#include "GameLogic/RetailWallMesh.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "Common/BfmeAssetPath.h"
#include "Common/File.h"
#include "Common/FileSystem.h"
#include "WWMath/matrix3d.h"
#include "WWMath/quat.h"

namespace
{
struct Chunk
{
	std::uint32_t type;
	size_t start;
	size_t end;
};

Bool word(const std::vector<unsigned char>& data, size_t offset, std::uint32_t& value)
{
	if (offset > data.size() || data.size() - offset < 4)
		return FALSE;
	value = std::uint32_t(data[offset]) | (std::uint32_t(data[offset + 1]) << 8) |
		(std::uint32_t(data[offset + 2]) << 16) | (std::uint32_t(data[offset + 3]) << 24);
	return TRUE;
}

Bool real(const std::vector<unsigned char>& data, size_t offset, Real& value)
{
	std::uint32_t bits;
	if (!word(data, offset, bits))
		return FALSE;
	std::memcpy(&value, &bits, sizeof(value));
	return TRUE;
}

Bool chunks(const std::vector<unsigned char>& data, size_t start, size_t end, std::vector<Chunk>& result)
{
	while (start < end)
	{
		std::uint32_t type, size;
		if (end - start < 8 || !word(data, start, type) || !word(data, start + 4, size))
			return FALSE;
		size &= 0x7fffffff;
		start += 8;
		if (size > end - start)
			return FALSE;
		result.push_back({type, start, start + size});
		start += size;
	}
	return start == end;
}

std::string fixedName(const std::vector<unsigned char>& data, size_t start, size_t length)
{
	const char* begin = reinterpret_cast<const char*>(data.data() + start);
	size_t count = 0;
	while (count < length && begin[count])
		++count;
	return std::string(begin, count);
}

Bool equalName(const std::string& a, const std::string& b)
{
	return AsciiString(a.c_str()).compareNoCase(b.c_str()) == 0;
}

Bool readModel(const AsciiString& model, std::vector<unsigned char>& data)
{
	std::string filename = model.str();
	if (filename.find('.') == std::string::npos)
		filename += ".w3d";
	const std::string paths[] = {"Art/W3D/" + filename, makeBfmeAssetPath("Art/W3D/", filename)};
	for (const auto& path : paths)
	{
		if (path.empty())
			continue;
		File* file = TheFileSystem->openFile(path.c_str(), File::READ | File::BINARY);
		if (!file)
			continue;
		const Int size = file->size();
		if (size > 0)
		{
			data.resize(size);
			const Int read = file->read(data.data(), size);
			file->close();
			if (read == size)
				return TRUE;
			data.clear();
		}
		else
			file->close();
	}
	return FALSE;
}

Bool readPivots(const std::vector<unsigned char>& data, const Chunk& hierarchy, std::vector<Matrix3D>& transforms)
{
	std::vector<Chunk> children;
	if (!chunks(data, hierarchy.start, hierarchy.end, children))
		return FALSE;
	for (const auto& child : children)
	{
		if (child.type != 0x102)
			continue;
		if ((child.end - child.start) % 60 != 0)
			return FALSE;
		const size_t count = (child.end - child.start) / 60;
		transforms.resize(count);
		for (size_t i = 0; i < count; ++i)
		{
			const size_t p = child.start + i * 60;
			std::uint32_t parent;
			Real x, y, z, qx, qy, qz, qw;
			if (!word(data, p + 16, parent) || !real(data, p + 20, x) || !real(data, p + 24, y) ||
				!real(data, p + 28, z) || !real(data, p + 44, qx) || !real(data, p + 48, qy) ||
				!real(data, p + 52, qz) || !real(data, p + 56, qw))
				return FALSE;
			transforms[i].Make_Identity();
			if (i == 0)
				continue;
			if (parent >= i)
				return FALSE;
			Matrix3D rotation;
			Matrix3D local;
			local.Make_Identity();
			local.Translate(Vector3(x, y, z));
			local.postMul(Build_Matrix3D(Quaternion(qx, qy, qz, qw), rotation));
			Matrix3D::Multiply(transforms[parent], local, &transforms[i]);
		}
		return TRUE;
	}
	return FALSE;
}

Bool findBone(const std::vector<unsigned char>& data, const Chunk& hlod, const std::string& name, std::uint32_t& bone)
{
	std::vector<Chunk> children;
	if (!chunks(data, hlod.start, hlod.end, children))
		return FALSE;
	for (const auto& child : children)
	{
		if (child.type != 0x702)
			continue;
		std::vector<Chunk> entries;
		if (!chunks(data, child.start, child.end, entries))
			return FALSE;
		for (const auto& entry : entries)
		{
			if (entry.type != 0x704 || entry.end - entry.start < 36)
				continue;
			const std::string full = fixedName(data, entry.start + 4, 32);
			const auto dot = full.rfind('.');
			if (equalName(dot == std::string::npos ? full : full.substr(dot + 1), name))
				return word(data, entry.start, bone);
		}
		break;
	}
	return FALSE;
}
}

Bool RetailWallMesh::load(const AsciiString& model, const AsciiString& name, Mesh& mesh)
{
	mesh = {};
	if (model.isEmpty() || name.isEmpty())
		return FALSE;
	std::vector<unsigned char> data;
	if (!readModel(model, data))
		return FALSE;
	std::vector<Chunk> root;
	if (!chunks(data, 0, data.size(), root))
		return FALSE;
	std::vector<Matrix3D> pivots;
	std::uint32_t bone = 0;
	Bool foundBone = FALSE;
	for (const auto& chunk : root)
	{
		if (chunk.type == 0x100 && !readPivots(data, chunk, pivots))
			return FALSE;
		if (chunk.type == 0x700 && findBone(data, chunk, name.str(), bone))
			foundBone = TRUE;
	}
	if (!foundBone || bone >= pivots.size())
		return FALSE;
	for (const auto& chunk : root)
	{
		if (chunk.type != 0)
			continue;
		std::vector<Chunk> parts;
		if (!chunks(data, chunk.start, chunk.end, parts))
			return FALSE;
		Bool match = FALSE;
		for (const auto& part : parts)
			if (part.type == 0x1f && part.end - part.start >= 24 &&
				equalName(fixedName(data, part.start + 8, 16), name.str()))
				match = TRUE;
		if (!match)
			continue;
		for (const auto& part : parts)
		{
			if (part.type == 2)
			{
				if ((part.end - part.start) % 12)
					return FALSE;
				for (size_t p = part.start; p < part.end; p += 12)
				{
					Real x, y, z;
					if (!real(data, p, x) || !real(data, p + 4, y) || !real(data, p + 8, z))
						return FALSE;
					Vector3 transformed;
					Matrix3D::Transform_Vector(pivots[bone], Vector3(x, y, z), &transformed);
					mesh.vertices.push_back({transformed.X, transformed.Y, transformed.Z});
				}
			}
			else if (part.type == 0x20)
			{
				if ((part.end - part.start) % 32)
					return FALSE;
				for (size_t p = part.start; p < part.end; p += 32)
				{
					std::array<UnsignedInt, 3> face;
					if (!word(data, p, face[0]) || !word(data, p + 4, face[1]) || !word(data, p + 8, face[2]))
						return FALSE;
					mesh.faces.push_back(face);
				}
			}
		}
		for (const auto& face : mesh.faces)
			for (const auto index : face)
				if (index >= mesh.vertices.size())
					return FALSE;
		return !mesh.vertices.empty() && !mesh.faces.empty();
	}
	return FALSE;
}
