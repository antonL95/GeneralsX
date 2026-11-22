#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace RetailFX
{

using Register = std::array<float, 4>;

struct Registers
{
	std::array<Register, 256> floats{};
	std::array<int, 16> booleans{};
};

struct Binding
{
	std::string name;
	unsigned bank;
	unsigned first;
	unsigned count;
	std::vector<uint32_t> defaults;
};

struct Operand
{
	unsigned bank;
	unsigned offset;
};

struct Instruction
{
	unsigned operation;
	unsigned components;
	bool scalar;
	std::array<Operand, 3> inputs;
	Operand output;
};

struct Program
{
	std::vector<uint32_t> tokens;
	std::vector<Binding> bindings;
	std::vector<Binding> inputs;
	std::vector<double> literals;
	std::vector<Instruction> instructions;
};

using SetBinding = std::function<void(const Binding &, std::span<Register>, std::span<int>)>;

std::optional<Program> readProgram(std::span<const uint8_t> bytes, bool vertex);
std::optional<Program> readStateProgram(std::span<const uint8_t> bytes);
bool fillRegisters(const Program &program, const SetBinding &setBinding, Registers &registers);

}
