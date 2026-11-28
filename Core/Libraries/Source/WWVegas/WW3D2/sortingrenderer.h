/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include "WWLib/always.h"

class SortingNodeStruct;
class SphereClass;
struct RenderStateStruct;
struct VertexFormatXYZNDUV2;

struct SortedRetailEffectData
{
	bool twoSided = false;
	bool alphaTest = false;
	bool alphaBlend = true;
	bool depthWrite = false;
	unsigned char alphaRef = 0x60;
	unsigned char srcBlend = 0;
	unsigned char dstBlend = 0;
	float vertexOpacity = 1.0f;
	float overrideOpacity = 1.0f;
	float directional[6][4]{};
	float point[3][4]{};
	float ambientLight[4]{};
	float fog[4][4]{};
	bool hasPointLight = false;
	char meshName[128]{};
};

using SortedEffectDraw = bool (*)(const RenderStateStruct& state, const SortedRetailEffectData& effect,
	const VertexFormatXYZNDUV2* vertices, unsigned vertexCount,
	const unsigned short* indices, unsigned polygonCount);

class SortingRendererClass
{
	static bool _EnableTriangleDraw;

	static void Flush_Sorting_Pool();
	static void Insert_To_Sorted_List(SortingNodeStruct* state);
	static void Insert_To_Sorting_Pool(SortingNodeStruct* state);
	static void Insert_Triangles_Impl(
		const SphereClass& bounding_sphere,
		unsigned short start_index,
		unsigned short polygon_count,
		unsigned short min_vertex_index,
		unsigned short vertex_count,
		const SortedRetailEffectData* effect);

public:
	static void Set_Effect_Draw(SortedEffectDraw draw);
	static void Ensure_Dynamic_Sorting_Capacity(unsigned vertexCount, unsigned indexCount);

	static void Insert_Triangles(
		const SphereClass& bounding_sphere,
		unsigned short start_index,
		unsigned short polygon_count,
		unsigned short min_vertex_index,
		unsigned short vertex_count);

	static void Insert_Triangles_With_Effect(
		const SphereClass& bounding_sphere,
		unsigned short start_index,
		unsigned short polygon_count,
		unsigned short min_vertex_index,
		unsigned short vertex_count,
		const SortedRetailEffectData& effect);

	static void Insert_Triangles(
		unsigned short start_index,
		unsigned short polygon_count,
		unsigned short min_vertex_index,
		unsigned short vertex_count);

	static void Flush();
	static void Deinit();

	static void SetMinVertexBufferSize( unsigned val );

	static void _Enable_Triangle_Draw(bool enable) { _EnableTriangleDraw=enable; }
	static bool _Is_Triangle_Draw_Enabled() { return _EnableTriangleDraw; }
};
