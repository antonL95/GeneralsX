#include <gtest/gtest.h>

#include "Common/INILegacyLines.h"
#include "Common/INIPreprocessor.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace
{
	std::vector<std::string> upstreamReadLines(const std::string& contents, std::size_t limit)
	{
		std::vector<std::string> lines;
		std::vector<char> buffer(limit + 1);
		std::size_t next = 0;
		bool endOfFile = false;
		while (!endOfFile)
		{
			char* p = buffer.data();
			while (p != buffer.data() + limit)
			{
				if (next == contents.size())
				{
					endOfFile = true;
					*p = 0;
					break;
				}
				*p = contents[next++];
				if (*p == '\n')
				{
					*p = 0;
					break;
				}
				if (*p == ';')
					*p = 0;
				else if (*p > 0 && *p < 32)
					*p = ' ';
				p++;
			}
			*p = 0;
			lines.emplace_back(buffer.data(), std::strlen(buffer.data()));
		}
		return lines;
	}

	std::vector<std::string> legacyLines(const std::string& contents)
	{
		std::vector<std::string> lines;
		INILegacyLines::forEach(contents, [&lines](std::string_view line) { lines.emplace_back(line); });
		return lines;
	}

	std::uint32_t foldLines(const std::vector<std::string>& lines)
	{
		std::uint32_t crc = 0;
		for (const std::string& line : lines)
		{
			const auto* bytes = reinterpret_cast<const unsigned char*>(line.data());
			std::size_t offset = 0;
			for (; offset + 4 <= line.size(); offset += 4)
			{
				const std::uint32_t word = (std::uint32_t{bytes[offset]} << 24)
					| (std::uint32_t{bytes[offset + 1]} << 16)
					| (std::uint32_t{bytes[offset + 2]} << 8)
					| std::uint32_t{bytes[offset + 3]};
				crc = (crc << 1) + word + ((crc >> 31) & 1u);
			}
			std::uint32_t tail = 0;
			switch (line.size() - offset)
			{
			case 3:
				tail += std::uint32_t{bytes[offset + 2]} << 16;
				[[fallthrough]];
			case 2:
				tail += std::uint32_t{bytes[offset + 1]} << 8;
				[[fallthrough]];
			case 1:
				tail += bytes[offset];
				crc = (crc << 1) + tail + ((crc >> 31) & 1u);
				break;
			default:
				break;
			}
		}
		return crc;
	}

	std::string zeroHourStyleFile()
	{
		std::string contents;
		contents += "; Zero Hour style weather // slashes inside a semicolon comment\r\n";
		contents += "Weather\r\n";
		contents += "  SnowBoxDimensions = 200 // box size\r\n";
		contents += "  SnowEnabled = No ; plain comment // after the semicolon\r\n";
		contents += "\tSnowTexture = EXSnowFlake.tga\r\n";
		contents += "  Url = http://example.org/a//b\n";
		contents += "// whole line\n";
		contents += std::string("  Nul = 1") + '\0' + " after nul\n";
		contents += ";" + std::string(1100, 'x') + "\n";
		contents += "  Exact = " + std::string(1028 - 10, 'y') + "\n";
		contents += "End";
		return contents;
	}
}

TEST(INILegacyLinesTest, MatchesUpstreamReadLine)
{
	const std::string contents = zeroHourStyleFile();
	const std::vector<std::string> upstream = upstreamReadLines(contents, INILegacyLines::LINE_LIMIT);
	const std::vector<std::string> legacy = legacyLines(contents);
	ASSERT_EQ(legacy, upstream);
	EXPECT_EQ(foldLines(legacy), foldLines(upstream));

	ASSERT_EQ(legacy.size(), 13u);
	EXPECT_EQ(legacy[0], "");
	EXPECT_EQ(legacy[1], "Weather ");
	EXPECT_EQ(legacy[2], "  SnowBoxDimensions = 200 // box size ");
	EXPECT_EQ(legacy[3], "  SnowEnabled = No ");
	EXPECT_EQ(legacy[4], " SnowTexture = EXSnowFlake.tga ");
	EXPECT_EQ(legacy[5], "  Url = http://example.org/a//b");
	EXPECT_EQ(legacy[6], "// whole line");
	EXPECT_EQ(legacy[7], "  Nul = 1");
	EXPECT_EQ(legacy[8], "");
	EXPECT_EQ(legacy[9], std::string(1101 - INILegacyLines::LINE_LIMIT, 'x'));
	EXPECT_EQ(legacy[10], "  Exact = " + std::string(1028 - 10, 'y'));
	EXPECT_EQ(legacy[11], "");
	EXPECT_EQ(legacy[12], "End");
}

TEST(INILegacyLinesTest, MatchesUpstreamAtFileBoundaries)
{
	for (const std::string& contents : {std::string(), std::string("\n"), std::string("End\n"), std::string("a\n\nb"),
		std::string(INILegacyLines::LINE_LIMIT, 'z'), std::string(INILegacyLines::LINE_LIMIT * 2 + 1, 'z')})
	{
		SCOPED_TRACE(contents.size());
		EXPECT_EQ(legacyLines(contents), upstreamReadLines(contents, INILegacyLines::LINE_LIMIT));
	}
}

TEST(INILegacyLinesTest, PreprocessedLinesWouldChangeTheCrc)
{
	const std::string contents = zeroHourStyleFile();
	INIPreprocessor preprocessor(nullptr);
	const std::string processed = preprocessor.process("Data\\INI\\Weather.ini", contents);
	EXPECT_TRUE(preprocessor.getWarnings().empty());

	const std::vector<std::string> branchBefore = upstreamReadLines(processed, 16384);
	const std::vector<std::string> upstream = upstreamReadLines(contents, INILegacyLines::LINE_LIMIT);
	EXPECT_EQ(branchBefore[2], "  SnowBoxDimensions = 200 ");
	EXPECT_EQ(branchBefore[6], "");
	EXPECT_NE(foldLines(branchBefore), foldLines(upstream));
	EXPECT_EQ(foldLines(legacyLines(contents)), foldLines(upstream));
}
