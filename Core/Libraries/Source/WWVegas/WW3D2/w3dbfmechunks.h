#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace W3dBfme
{
using Byte = uint8_t;
inline constexpr uint32_t SteppedMovement = 0x80000000u;

inline uint16_t ReadWord(const Byte* data)
{
	return uint16_t(data[0]) | (uint16_t(data[1]) << 8);
}

inline uint32_t ReadDword(const Byte* data)
{
	return uint32_t(ReadWord(data)) | (uint32_t(ReadWord(data + 2)) << 16);
}

struct Influence
{
	uint16_t Bone0;
	uint16_t Bone1;
	uint16_t Weight0;
	uint16_t Weight1;
};

inline Influence ReadInfluence(const Byte* data)
{
	return {ReadWord(data), ReadWord(data + 2), ReadWord(data + 4), ReadWord(data + 6)};
}

template<typename StoreVector>
bool ReadVectorStream(std::span<const Byte> data, unsigned count, StoreVector store)
{
	if (data.size() % 12 != 0 || data.size() / 12 != count)
		return false;

	for (unsigned index = 0; index < count; ++index) {
		const Byte* vector = data.data() + std::size_t(index) * 12;
		store(index, std::bit_cast<float>(ReadDword(vector)),
			std::bit_cast<float>(ReadDword(vector + 4)), std::bit_cast<float>(ReadDword(vector + 8)));
	}
	return true;
}

enum class MotionResult { Invalid, Decoded, UnknownScalar };

struct Motion
{
	uint16_t Pivot = 0;
	Byte Type = 0;
	Byte VectorLength = 0;
	std::vector<uint32_t> Packets;
};

inline MotionResult DecodeMotion(std::span<const Byte> data, unsigned animationFrames,
	const float (&filters)[256], Motion& motion)
{
	if (data.size() < 8 || data[0] != 0)
		return MotionResult::Invalid;

	const unsigned deltaType = data[1];
	const unsigned vectorLength = data[2];
	const unsigned type = data[3];
	const unsigned count = ReadWord(data.data() + 4);
	if (deltaType > 2 || (type > 2 && type != 6 && type != 15) ||
		vectorLength != (type == 6 ? 4u : 1u) || count == 0 || count > animationFrames)
		return MotionResult::Invalid;

	const bool wideDelta = deltaType == 2;
	const unsigned deltaPacketBytes = wideDelta ? 17 : 9;
	const unsigned timeBytes = ((count + 1) & ~1u) * 2;
	const unsigned bodySize = deltaType == 0
		? timeBytes + count * vectorLength * 4
		: 4 + vectorLength * (4 + ((count + 15) / 16) * deltaPacketBytes);
	if (data.size() != 8 + bodySize)
		return MotionResult::Invalid;

	if (type == 15)
		return MotionResult::UnknownScalar;

	motion.Pivot = ReadWord(data.data() + 6);
	motion.Type = type;
	motion.VectorLength = vectorLength;
	const unsigned packetSize = vectorLength + 1;
	const Byte* body = data.data() + 8;

	if (deltaType == 0) {
		const Byte* values = body + timeBytes;
		const bool delayedStart = (ReadWord(body) & 0x7fffu) != 0;
		motion.Packets.resize((count + delayedStart) * packetSize);
		unsigned previous = 0;
		for (unsigned key = 0; key < count; ++key) {
			const unsigned keyWord = ReadWord(body + key * 2);
			const unsigned time = keyWord & 0x7fffu;
			if (time >= animationFrames || (key > 0 && time <= previous))
				return MotionResult::Invalid;
			previous = time;
			uint32_t* packet = motion.Packets.data() + (key + delayedStart) * packetSize;
			packet[0] = time;
			if (keyWord & 0x8000u)
				packet[0] |= SteppedMovement;
			for (unsigned component = 0; component < vectorLength; ++component, values += 4)
				packet[component + 1] = ReadDword(values);
		}
		if (delayedStart) {
			motion.Packets[0] = 0;
			motion.Packets[packetSize] |= SteppedMovement;
			for (unsigned component = 1; component <= vectorLength; ++component)
				motion.Packets[component] = motion.Packets[packetSize + component];
		}
		return MotionResult::Decoded;
	}

	motion.Packets.resize(count * packetSize);
	const float scale = std::bit_cast<float>(ReadDword(body));
	const Byte* blocks = body + 4 + vectorLength * 4;
	for (unsigned frame = 0; frame < count; ++frame)
		motion.Packets[frame * packetSize] = frame;

	for (unsigned component = 0; component < vectorLength; ++component) {
		float value = std::bit_cast<float>(ReadDword(body + 4 + component * 4));
		motion.Packets[component + 1] = std::bit_cast<uint32_t>(value);
		for (unsigned first = 1; first < count; first += 16) {
			const unsigned block = (first - 1) / 16;
			const Byte* deltas = blocks + (block * vectorLength + component) * deltaPacketBytes;
			const float step = filters[*deltas++] * scale * (wideDelta ? 0.0625f : 1.0f);
			for (unsigned slot = 0; slot < 16 && first + slot < count; ++slot) {
				int delta;
				if (wideDelta) {
					delta = int(deltas[slot]) - 128;
				} else {
					delta = (deltas[slot / 2] >> (4 * (slot % 2))) & 15;
					if (delta >= 8)
						delta -= 16;
				}
				value += step * delta;
				motion.Packets[(first + slot) * packetSize + component + 1] = std::bit_cast<uint32_t>(value);
			}
		}
	}
	return MotionResult::Decoded;
}
}
