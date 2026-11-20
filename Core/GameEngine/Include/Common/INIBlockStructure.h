#pragma once

#include <string_view>
#include <vector>

namespace INIBlockStructure
{
enum class Shape { Bare, Assign, Positional };

struct LineHead
{
	std::string_view keyword;
	Shape shape;
	std::string_view value;
};

bool parseLineHead(std::string_view line, LineHead& head);
bool isEnd(const LineHead& head);
std::string_view opensRawBlock(const LineHead& head);
bool opensBlock(const LineHead& head, std::string_view enclosing);
bool isSingleLineTopLevel(std::string_view keyword);

class BlockEnd
{
public:
	explicit BlockEnd(std::string_view enclosing);
	bool consume(std::string_view line);

private:
	std::vector<std::string_view> m_stack;
	std::string_view m_rawEnd;
};

std::size_t findBlockEnd(std::string_view buffer, std::size_t next, std::string_view enclosing);
}
