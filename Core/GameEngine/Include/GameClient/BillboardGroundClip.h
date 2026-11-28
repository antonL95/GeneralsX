#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace BillboardGroundClip
{
	struct Vec3
	{
		float x;
		float y;
		float z;
	};

	struct ClipVertex
	{
		float x;
		float y;
		float z;
		float u;
		float v;
		float clearance;
		float alphaScale;
	};

	constexpr float GroundClearanceBias = 0.25f;
	constexpr float MinFadeHeight = 1.0f;
	constexpr float MaxFadeHeight = 16.0f;
	constexpr float FadeHeightFraction = 0.30f;

	inline float computeFadeHeight(float verticalSpan)
	{
		return std::min(MaxFadeHeight, std::max(MinFadeHeight, verticalSpan * FadeHeightFraction));
	}

	inline ClipVertex interpolateVertex(const ClipVertex &a, const ClipVertex &b, float threshold)
	{
		const float denom = b.clearance - a.clearance;
		const float t = std::fabs(denom) < 1.0e-6f ? 0.5f : std::clamp((threshold - a.clearance) / denom, 0.0f, 1.0f);
		return ClipVertex{
			a.x + (b.x - a.x) * t,
			a.y + (b.y - a.y) * t,
			a.z + (b.z - a.z) * t,
			a.u + (b.u - a.u) * t,
			a.v + (b.v - a.v) * t,
			threshold,
			0.0f
		};
	}

	inline int clipPolygonHalfSpace(
		const ClipVertex *inVerts,
		int inCount,
		ClipVertex *outVerts,
		float threshold,
		bool keepGreaterEqual)
	{
		if (inCount < 3)
			return 0;

		int outCount = 0;
		const auto isInside = [threshold, keepGreaterEqual](float clearance) -> bool
		{
			return keepGreaterEqual ? (clearance >= threshold) : (clearance <= threshold);
		};

		ClipVertex prev = inVerts[inCount - 1];
		bool prevInside = isInside(prev.clearance);
		for (int i = 0; i < inCount; ++i)
		{
			const ClipVertex curr = inVerts[i];
			const bool currInside = isInside(curr.clearance);
			if (currInside != prevInside)
				outVerts[outCount++] = interpolateVertex(prev, curr, threshold);
			if (currInside)
				outVerts[outCount++] = curr;
			prev = curr;
			prevInside = currInside;
		}
		return outCount;
	}

	inline void appendPolygonFan(
		const ClipVertex *poly,
		int count,
		bool rampAlpha,
		float fadeHeight,
		std::vector<ClipVertex> &outVerts,
		std::vector<uint16_t> &outIndices)
	{
		if (count < 3)
			return;

		const uint16_t baseIndex = static_cast<uint16_t>(outVerts.size());
		const float invFade = fadeHeight > 1.0e-6f ? 1.0f / fadeHeight : 1.0f;
		for (int i = 0; i < count; ++i)
		{
			ClipVertex v = poly[i];
			v.alphaScale = rampAlpha ? std::clamp(v.clearance * invFade, 0.0f, 1.0f) : 1.0f;
			outVerts.push_back(v);
		}
		for (int i = 1; i + 1 < count; ++i)
		{
			outIndices.push_back(baseIndex);
			outIndices.push_back(static_cast<uint16_t>(baseIndex + i));
			outIndices.push_back(static_cast<uint16_t>(baseIndex + i + 1));
		}
	}

	inline void buildQuadCorners(
		const Vec3 &center,
		const Vec3 &camRight,
		const Vec3 &camUp,
		float size,
		uint8_t angleIndex,
		std::array<ClipVertex, 4> &corners)
	{
		constexpr std::array<float, 4> Ox = { -0.5f, -0.5f, 0.5f, 0.5f };
		constexpr std::array<float, 4> Oy = { 0.5f, -0.5f, -0.5f, 0.5f };
		constexpr std::array<float, 4> U = { 0.0f, 0.0f, 1.0f, 1.0f };
		constexpr std::array<float, 4> V = { 0.0f, 1.0f, 1.0f, 0.0f };
		constexpr float TwoPi = 6.28318530717958647692f;
		const float angle = static_cast<float>(angleIndex) * (TwoPi / 256.0f);
		const float c = std::cos(angle);
		const float s = std::sin(angle);

		for (int j = 0; j < 4; ++j)
		{
			const float rx = (Ox[j] * c - Oy[j] * s) * size;
			const float ry = (Ox[j] * s + Oy[j] * c) * size;
			corners[j] = ClipVertex{
				center.x + camRight.x * rx + camUp.x * ry,
				center.y + camRight.y * rx + camUp.y * ry,
				center.z + camRight.z * rx + camUp.z * ry,
				U[j],
				V[j],
				0.0f,
				1.0f
			};
		}
	}

	inline void clipGroundBillboard(
		std::array<ClipVertex, 4> &corners,
		const std::array<float, 4> &groundHeights,
		std::vector<ClipVertex> &outVerts,
		std::vector<uint16_t> &outIndices)
	{
		float minH = corners[0].z - (groundHeights[0] + GroundClearanceBias);
		float maxH = minH;
		corners[0].clearance = minH;
		for (int i = 1; i < 4; ++i)
		{
			const float h = corners[i].z - (groundHeights[i] + GroundClearanceBias);
			corners[i].clearance = h;
			minH = std::min(minH, h);
			maxH = std::max(maxH, h);
		}

		if (maxH <= 0.0f)
			return;

		const float fadeHeight = computeFadeHeight(maxH - minH);
		if (minH >= fadeHeight)
		{
			appendPolygonFan(corners.data(), 4, false, fadeHeight, outVerts, outIndices);
			return;
		}

		std::array<ClipVertex, 8> aboveGround{};
		const int aboveCount = clipPolygonHalfSpace(corners.data(), 4, aboveGround.data(), 0.0f, true);
		if (aboveCount >= 3)
		{
			if (maxH > fadeHeight)
			{
				std::array<ClipVertex, 8> fadeBand{};
				const int fadeCount = clipPolygonHalfSpace(aboveGround.data(), aboveCount, fadeBand.data(), fadeHeight, false);
				appendPolygonFan(fadeBand.data(), fadeCount, true, fadeHeight, outVerts, outIndices);
			}
			else
			{
				appendPolygonFan(aboveGround.data(), aboveCount, true, fadeHeight, outVerts, outIndices);
			}
		}

		if (maxH > fadeHeight)
		{
			std::array<ClipVertex, 8> upperBand{};
			const int upperCount = clipPolygonHalfSpace(corners.data(), 4, upperBand.data(), fadeHeight, true);
			appendPolygonFan(upperBand.data(), upperCount, false, fadeHeight, outVerts, outIndices);
		}
	}
}
