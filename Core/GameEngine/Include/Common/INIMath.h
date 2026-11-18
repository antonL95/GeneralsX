#pragma once

#include <cstring>
#include <stdexcept>
#include <type_traits>

enum INIMathFunction
{
	INI_MATH_NONE,
	INI_MATH_ADD,
	INI_MATH_SUBTRACT,
	INI_MATH_MULTIPLY,
	INI_MATH_DIVIDE,
};

inline INIMathFunction findINIMathFunction(const char* token)
{
	if (token == nullptr || token[0] != '#')
		return INI_MATH_NONE;
	if (std::strcmp(token, "#ADD(") == 0)
		return INI_MATH_ADD;
	if (std::strcmp(token, "#SUBTRACT(") == 0)
		return INI_MATH_SUBTRACT;
	if (std::strcmp(token, "#MULTIPLY(") == 0)
		return INI_MATH_MULTIPLY;
	if (std::strcmp(token, "#DIVIDE(") == 0)
		return INI_MATH_DIVIDE;
	return INI_MATH_NONE;
}

class INIMathError : public std::runtime_error
{
public:
	explicit INIMathError(const char* message) : std::runtime_error(message) {}
};

template <typename Type, typename NextToken, typename Scan>
Type evaluateINIMath(INIMathFunction function, NextToken nextToken, Scan scan)
{
	const Type first = scan(nextToken());

	if (function == INI_MATH_ADD || function == INI_MATH_MULTIPLY)
	{
		Type result = first;
		for (const char* token = nextToken(); std::strcmp(token, ")") != 0; token = nextToken())
		{
			const Type operand = scan(token);
			result = function == INI_MATH_ADD ? static_cast<Type>(result + operand) : static_cast<Type>(result * operand);
		}
		return result;
	}

	const Type second = scan(nextToken());
	if (std::strcmp(nextToken(), ")") != 0)
		throw INIMathError("#SUBTRACT( and #DIVIDE( take exactly 2 operands");

	if (function == INI_MATH_SUBTRACT)
		return static_cast<Type>(first - second);
	if (function != INI_MATH_DIVIDE)
		throw INIMathError("unknown math function");
	if constexpr (std::is_integral_v<Type>)
	{
		if (second == 0)
			throw INIMathError("#DIVIDE( by zero");
	}
	return static_cast<Type>(first / second);
}
