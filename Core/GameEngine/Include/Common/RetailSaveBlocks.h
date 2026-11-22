#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <initializer_list>
#include <string>
#include <string_view>

namespace RetailSaveBlocks
{
template<typename XferType>
struct Block
{
	std::string_view name;
	std::function<void(XferType*)> transfer;
	std::function<void()> reset;
};

constexpr std::size_t MAX_BLOCKS = 64;

template<typename XferType>
bool hasUniqueNames(std::initializer_list<Block<XferType>> blocks)
{
	for (auto first = blocks.begin(); first != blocks.end(); ++first)
	{
		if (first->name.empty())
			return false;
		for (auto second = first + 1; second != blocks.end(); ++second)
			if (first->name == second->name)
				return false;
	}
	return blocks.size() <= MAX_BLOCKS;
}

template<typename XferType>
void transferName(XferType* xfer, std::string& name)
{
	std::uint16_t length = static_cast<std::uint16_t>(name.size());
	xfer->xferUnsignedShort(&length);
	name.resize(length);
	for (char& character : name)
	{
		unsigned char byte = static_cast<unsigned char>(character);
		xfer->xferUnsignedByte(&byte);
		character = static_cast<char>(byte);
	}
}

template<typename XferType>
void resetAll(std::initializer_list<Block<XferType>> blocks)
{
	for (const Block<XferType>& block : blocks)
		if (block.reset)
			block.reset();
}

template<typename XferType>
void transfer(XferType* xfer, bool loading, std::initializer_list<Block<XferType>> blocks)
{
	if (!hasUniqueNames(blocks))
		std::abort();

	if (!loading)
	{
		std::uint16_t count = static_cast<std::uint16_t>(blocks.size());
		xfer->xferUnsignedShort(&count);
		for (const Block<XferType>& block : blocks)
		{
			std::string name(block.name);
			transferName(xfer, name);
			xfer->beginBlock();
			block.transfer(xfer);
			xfer->endBlock();
		}
		return;
	}

	std::uint64_t loaded = 0;
	std::uint16_t count = 0;
	xfer->xferUnsignedShort(&count);
	for (std::uint16_t index = 0; index < count; ++index)
	{
		std::string name;
		transferName(xfer, name);
		const int size = xfer->beginBlock();
		std::size_t position = 0;
		const Block<XferType>* match = nullptr;
		for (const Block<XferType>& block : blocks)
		{
			if (block.name == name && (loaded & (std::uint64_t{1} << position)) == 0)
			{
				match = &block;
				loaded |= std::uint64_t{1} << position;
				break;
			}
			++position;
		}
		if (match)
			match->transfer(xfer);
		else
			xfer->skip(size);
		xfer->endBlock();
	}

	std::size_t position = 0;
	for (const Block<XferType>& block : blocks)
	{
		if ((loaded & (std::uint64_t{1} << position)) == 0 && block.reset)
			block.reset();
		++position;
	}
}

template<typename XferType>
void transferSection(XferType* xfer, bool loading, bool present, std::initializer_list<Block<XferType>> blocks)
{
	if (present)
		transfer(xfer, loading, blocks);
	else if (loading)
		resetAll(blocks);
}
}
