#include <cstdlib>
#include <string>
#include <utility>
#include <vector>
#include <gtest/gtest.h>

#include "Common/INIMath.h"

namespace
{
class Tokens
{
public:
	explicit Tokens(std::vector<std::string> tokens) : m_tokens(std::move(tokens)) {}

	const char* next()
	{
		if (m_index >= m_tokens.size())
			throw INIMathError("out of tokens");
		return m_tokens[m_index++].c_str();
	}

	bool consumed() const
	{
		return m_index == m_tokens.size();
	}

private:
	std::vector<std::string> m_tokens;
	std::size_t m_index = 0;
};

float scanFloat(Tokens& tokens, const char* token);
int scanInt(Tokens& tokens, const char* token);

float scanFloat(Tokens& tokens, const char* token)
{
	const INIMathFunction function = findINIMathFunction(token);
	if (function != INI_MATH_NONE)
		return evaluateINIMath<float>(function, [&tokens]() { return tokens.next(); }, [&tokens](const char* t) { return scanFloat(tokens, t); });
	return std::strtof(token, nullptr);
}

int scanInt(Tokens& tokens, const char* token)
{
	const INIMathFunction function = findINIMathFunction(token);
	if (function != INI_MATH_NONE)
		return evaluateINIMath<int>(function, [&tokens]() { return tokens.next(); }, [&tokens](const char* t) { return scanInt(tokens, t); });
	return static_cast<int>(std::strtol(token, nullptr, 10));
}

float evaluateFloat(const std::vector<std::string>& line)
{
	Tokens tokens(std::vector<std::string>(line.begin() + 1, line.end()));
	const float result = scanFloat(tokens, line[0].c_str());
	EXPECT_TRUE(tokens.consumed());
	return result;
}

int evaluateInt(const std::vector<std::string>& line)
{
	Tokens tokens(std::vector<std::string>(line.begin() + 1, line.end()));
	const int result = scanInt(tokens, line[0].c_str());
	EXPECT_TRUE(tokens.consumed());
	return result;
}
}

TEST(INIMath, RecognizesOnlyExactUppercaseFunctionTokens)
{
	EXPECT_EQ(findINIMathFunction("#ADD("), INI_MATH_ADD);
	EXPECT_EQ(findINIMathFunction("#SUBTRACT("), INI_MATH_SUBTRACT);
	EXPECT_EQ(findINIMathFunction("#MULTIPLY("), INI_MATH_MULTIPLY);
	EXPECT_EQ(findINIMathFunction("#DIVIDE("), INI_MATH_DIVIDE);
	EXPECT_EQ(findINIMathFunction("#add("), INI_MATH_NONE);
	EXPECT_EQ(findINIMathFunction("#ADD"), INI_MATH_NONE);
	EXPECT_EQ(findINIMathFunction("#(MODEL)_WLK"), INI_MATH_NONE);
	EXPECT_EQ(findINIMathFunction("12"), INI_MATH_NONE);
	EXPECT_EQ(findINIMathFunction(nullptr), INI_MATH_NONE);
}

TEST(INIMath, AddAndMultiplyTakeAnyNumberOfOperands)
{
	EXPECT_EQ(evaluateFloat({"#ADD(", "2", ")"}), 2.0f);
	EXPECT_EQ(evaluateFloat({"#ADD(", "2", "3", "4.5", ")"}), 9.5f);
	EXPECT_EQ(evaluateFloat({"#MULTIPLY(", "2", "3", "0.5", ")"}), 3.0f);
}

TEST(INIMath, SubtractAndDivideTakeExactlyTwoOperands)
{
	EXPECT_EQ(evaluateFloat({"#SUBTRACT(", "2", "3", ")"}), -1.0f);
	EXPECT_EQ(evaluateFloat({"#DIVIDE(", "1", "4", ")"}), 0.25f);
	Tokens tokens({"1", "2", "3", ")"});
	EXPECT_THROW(scanFloat(tokens, "#SUBTRACT("), INIMathError);
}

TEST(INIMath, NestsThroughTheScanFunction)
{
	EXPECT_EQ(evaluateFloat({"#ADD(", "#MULTIPLY(", "100", "1.5", ")", "#DIVIDE(", "100", "4", ")", ")"}), 175.0f);
}

TEST(INIMath, IntegerFieldsUseIntegerArithmetic)
{
	EXPECT_EQ(evaluateInt({"#DIVIDE(", "7", "2", ")"}), 3);
	EXPECT_EQ(evaluateInt({"#DIVIDE(", "-7", "2", ")"}), -3);
	EXPECT_EQ(evaluateInt({"#ADD(", "1", "2", ")"}), 3);
	Tokens tokens({"1", "0", ")"});
	EXPECT_THROW(scanInt(tokens, "#DIVIDE("), INIMathError);
}

TEST(INIMath, FloatResultsAreRoundedToFloatAtEveryStep)
{
	const float third = evaluateFloat({"#DIVIDE(", "1", "3", ")"});
	EXPECT_EQ(third, 1.0f / 3.0f);
	const float sum = evaluateFloat({"#ADD(", "0.1", "0.2", "0.3", ")"});
	EXPECT_EQ(sum, (0.1f + 0.2f) + 0.3f);
}

TEST(INIMath, OperandsUseTheFieldScanner)
{
	EXPECT_EQ(evaluateFloat({"#MULTIPLY(", "50%", "2", ")"}), 100.0f);
	EXPECT_EQ(evaluateFloat({"#DIVIDE(", "1.0,", "0.5", ")"}), 2.0f);
}

TEST(INIMath, MissingClosingParenthesisRunsOutOfTokens)
{
	Tokens tokens({"1", "2"});
	EXPECT_THROW(scanFloat(tokens, "#ADD("), INIMathError);
}
