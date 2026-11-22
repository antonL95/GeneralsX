#include "Common/RetailSaveBlocks.h"

#include <cstdint>
#include <cstring>
#include <vector>

#include <gtest/gtest.h>

namespace
{
class FakeXfer
{
public:
	explicit FakeXfer(bool loading, std::vector<unsigned char> bytes = {}) : m_loading(loading), m_bytes(std::move(bytes)) {}

	void xferUnsignedShort(std::uint16_t* value) { raw(value, sizeof(*value)); }
	void xferUnsignedByte(unsigned char* value) { raw(value, sizeof(*value)); }
	void xferInt(int* value) { raw(value, sizeof(*value)); }

	int beginBlock()
	{
		if (m_loading)
		{
			int size = 0;
			raw(&size, sizeof(size));
			return size;
		}
		m_blocks.push_back(m_bytes.size());
		int placeholder = 0;
		raw(&placeholder, sizeof(placeholder));
		return 0;
	}

	void endBlock()
	{
		if (m_loading)
			return;
		const std::size_t start = m_blocks.back();
		m_blocks.pop_back();
		const int size = static_cast<int>(m_bytes.size() - start - sizeof(int));
		std::memcpy(m_bytes.data() + start, &size, sizeof(size));
	}

	void skip(int size) { m_position += static_cast<std::size_t>(size); }

	const std::vector<unsigned char>& bytes() const { return m_bytes; }
	std::size_t position() const { return m_position; }

private:
	void raw(void* value, std::size_t size)
	{
		if (m_loading)
		{
			ASSERT_LE(m_position + size, m_bytes.size());
			std::memcpy(value, m_bytes.data() + m_position, size);
			m_position += size;
			return;
		}
		const auto* source = static_cast<const unsigned char*>(value);
		m_bytes.insert(m_bytes.end(), source, source + size);
	}

	bool m_loading;
	std::vector<unsigned char> m_bytes;
	std::vector<std::size_t> m_blocks;
	std::size_t m_position = 0;
};

using Block = RetailSaveBlocks::Block<FakeXfer>;

std::vector<unsigned char> save(int first, int second)
{
	FakeXfer xfer(false);
	RetailSaveBlocks::transfer<FakeXfer>(&xfer, false, {
		Block{"first", [&](FakeXfer* x) { x->xferInt(&first); }, nullptr},
		Block{"second", [&](FakeXfer* x) { x->xferInt(&second); }, nullptr},
	});
	return xfer.bytes();
}
}

TEST(RetailSaveBlocks, RoundTripsNamedBlocks)
{
	const std::vector<unsigned char> bytes = save(7, 11);
	int first = 0;
	int second = 0;
	FakeXfer xfer(true, bytes);
	RetailSaveBlocks::transfer<FakeXfer>(&xfer, true, {
		Block{"first", [&](FakeXfer* x) { x->xferInt(&first); }, nullptr},
		Block{"second", [&](FakeXfer* x) { x->xferInt(&second); }, nullptr},
	});
	EXPECT_EQ(first, 7);
	EXPECT_EQ(second, 11);
	EXPECT_EQ(xfer.position(), bytes.size());
}

TEST(RetailSaveBlocks, LoadsBlocksInAnyOrder)
{
	const std::vector<unsigned char> bytes = save(3, 5);
	int first = 0;
	int second = 0;
	FakeXfer xfer(true, bytes);
	RetailSaveBlocks::transfer<FakeXfer>(&xfer, true, {
		Block{"second", [&](FakeXfer* x) { x->xferInt(&second); }, nullptr},
		Block{"first", [&](FakeXfer* x) { x->xferInt(&first); }, nullptr},
	});
	EXPECT_EQ(first, 3);
	EXPECT_EQ(second, 5);
	EXPECT_EQ(xfer.position(), bytes.size());
}

TEST(RetailSaveBlocks, SkipsBlocksTheReaderDoesNotKnow)
{
	const std::vector<unsigned char> bytes = save(13, 17);
	int second = 0;
	FakeXfer xfer(true, bytes);
	RetailSaveBlocks::transfer<FakeXfer>(&xfer, true, {
		Block{"second", [&](FakeXfer* x) { x->xferInt(&second); }, nullptr},
	});
	EXPECT_EQ(second, 17);
	EXPECT_EQ(xfer.position(), bytes.size());
}

TEST(RetailSaveBlocks, ResetsBlocksMissingFromTheSave)
{
	const std::vector<unsigned char> bytes = save(19, 23);
	int first = 0;
	int added = 99;
	FakeXfer xfer(true, bytes);
	RetailSaveBlocks::transfer<FakeXfer>(&xfer, true, {
		Block{"first", [&](FakeXfer* x) { x->xferInt(&first); }, nullptr},
		Block{"added", [&](FakeXfer* x) { x->xferInt(&added); }, [&] { added = 0; }},
	});
	EXPECT_EQ(first, 19);
	EXPECT_EQ(added, 0);
	EXPECT_EQ(xfer.position(), bytes.size());
}

TEST(RetailSaveBlocks, EmptySectionRoundTrips)
{
	FakeXfer writer(false);
	RetailSaveBlocks::transfer<FakeXfer>(&writer, false, {});
	FakeXfer reader(true, writer.bytes());
	RetailSaveBlocks::transfer<FakeXfer>(&reader, true, {});
	EXPECT_EQ(reader.position(), writer.bytes().size());
}

TEST(RetailSaveBlocks, ResetAllCallsEveryReset)
{
	int first = 1;
	int second = 2;
	RetailSaveBlocks::resetAll<FakeXfer>({
		Block{"first", [](FakeXfer*) {}, [&] { first = 0; }},
		Block{"second", [](FakeXfer*) {}, [&] { second = 0; }},
	});
	EXPECT_EQ(first, 0);
	EXPECT_EQ(second, 0);
}

TEST(RetailSaveBlocks, RejectsDuplicateOrEmptyNames)
{
	EXPECT_FALSE(RetailSaveBlocks::hasUniqueNames<FakeXfer>({
		Block{"same", [](FakeXfer*) {}, nullptr},
		Block{"same", [](FakeXfer*) {}, nullptr},
	}));
	EXPECT_FALSE(RetailSaveBlocks::hasUniqueNames<FakeXfer>({
		Block{"", [](FakeXfer*) {}, nullptr},
	}));
	EXPECT_TRUE(RetailSaveBlocks::hasUniqueNames<FakeXfer>({
		Block{"one", [](FakeXfer*) {}, nullptr},
		Block{"two", [](FakeXfer*) {}, nullptr},
	}));
}
