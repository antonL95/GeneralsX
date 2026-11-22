#include "retailfxprogram.h"

#include <algorithm>
#include <bit>
#include <cmath>

namespace RetailFX
{
namespace
{

using Words = std::span<const uint32_t>;

constexpr uint32_t constantTable = 0x42415443;
constexpr uint32_t preshaderChunk = 0x53455250;
constexpr uint32_t literalTable = 0x54494c43;
constexpr uint32_t expressionList = 0x434c5846;
constexpr uint32_t preshaderInfo = 0x49535250;
constexpr size_t maxDecodedProgramBytes = 512 * 1024;

enum class ShaderStage { Vertex11, Vertex20, Pixel20 };

enum class RegisterClass : uint32_t
{
	Temporary, Input, Constant, Texture, RasterOutput, AttributeOutput, TexcoordOutput, Integer,
	ColorOutput = 8, DepthOutput, Sampler, Boolean = 14
};

bool spend(size_t &remaining, size_t bytes)
{
	if (bytes > remaining)
		return false;
	remaining -= bytes;
	return true;
}

unsigned floatRegisterCount(ShaderStage stage)
{
	switch (stage)
	{
		case ShaderStage::Vertex11: return 96;
		case ShaderStage::Vertex20: return 256;
		case ShaderStage::Pixel20: return 32;
	}
	return 0;
}

uint32_t readWord(std::span<const uint8_t> bytes)
{
	return uint32_t(bytes[0]) | uint32_t(bytes[1]) << 8 | uint32_t(bytes[2]) << 16 | uint32_t(bytes[3]) << 24;
}

std::string readName(Words table, size_t offset)
{
	std::string name;
	for (; offset / 4 < table.size() && name.size() < 255; ++offset)
	{
		const auto byte = static_cast<char>(table[offset / 4] >> (offset % 4 * 8));
		if (!byte)
			return name;
		name.push_back(byte);
	}
	return {};
}

bool readBindings(Words table, std::vector<Binding> &bindings, size_t &remaining,
	std::optional<ShaderStage> stage = {})
{
	if (table.size() < 7 || table[0] != 28 || table[4] % 4)
		return false;
	const size_t start = table[4] / 4;
	const size_t count = table[3];
	if (start > table.size() || count > (table.size() - start) / 5 || count > 256 ||
		!spend(remaining, count * sizeof(Binding)))
		return false;
	bindings.reserve(count);
	for (size_t i = 0; i < count; ++i)
	{
		const auto entry = table.subspan(start + i * 5, 5);
		Binding binding{readName(table, entry[0]), entry[1] & 0xffffu, entry[1] >> 16, entry[2] & 0xffffu, {}};
		const unsigned limit = binding.bank == 2 ? (stage ? floatRegisterCount(*stage) : 256) : 16;
		if (binding.name.empty() || binding.bank > 3 || !binding.count ||
			binding.first + binding.count > limit || binding.bank == 1 ||
			(stage == ShaderStage::Pixel20 && binding.bank == 0) ||
			(stage && stage != ShaderStage::Pixel20 && binding.bank == 3) ||
			!spend(remaining, binding.name.capacity() + 1))
			return false;
		if (entry[4] && binding.bank != 3)
		{
			const size_t offset = entry[4] / 4;
			const size_t defaults = binding.count * (binding.bank == 2 ? 4u : 1u);
			if (entry[4] % 4 || offset > table.size() || defaults > table.size() - offset ||
				!spend(remaining, defaults * sizeof(uint32_t)))
				return false;
			binding.defaults.assign(table.begin() + offset, table.begin() + offset + defaults);
		}
		bindings.push_back(std::move(binding));
	}
	return true;
}

Words takeComment(Words &words)
{
	const size_t size = (words.front() >> 16) & 0x7fff;
	if (!size || size >= words.size())
		return {};
	const auto chunk = words.subspan(1, size);
	words = words.subspan(size + 1);
	return chunk;
}

size_t shaderOperands(uint32_t opcode)
{
	switch (opcode)
	{
		case 0x00: case 0x27: case 0x2a: case 0x2b: return 0;
		case 0x26: case 0x28: return 1;
		case 0x01: case 0x06: case 0x07: case 0x0e: case 0x0f: case 0x10:
		case 0x13: case 0x1f: case 0x24: case 0x2e: return 2;
		case 0x02: case 0x03: case 0x05: case 0x08: case 0x09: case 0x0a:
		case 0x0b: case 0x0c: case 0x0d: case 0x11: case 0x20: case 0x42:
			return 3;
		case 0x04: case 0x12: case 0x25: case 0x58: return 4;
		case 0x30: case 0x51: return 5;
		default: return 16;
	}
}

RegisterClass registerClass(uint32_t token)
{
	return static_cast<RegisterClass>(((token >> 28) & 7u) | ((token >> 8) & 24u));
}

unsigned registerIndex(uint32_t token)
{
	return token & 0x7ffu;
}

bool validSource(uint32_t token, ShaderStage stage)
{
	const auto index = registerIndex(token);
	const bool pixel = stage == ShaderStage::Pixel20;
	const unsigned modifier = (token >> 24) & 0xfu;
	if (modifier > (pixel ? 12u : 1u))
		return false;
	switch (registerClass(token))
	{
		case RegisterClass::Temporary: return index < 12;
		case RegisterClass::Input: return index < (pixel ? 2u : 16u);
		case RegisterClass::Constant: return index < floatRegisterCount(stage);
		case RegisterClass::Texture: return pixel && index < 8;
		default: return false;
	}
}

bool validDestination(uint32_t token, ShaderStage stage)
{
	if (!(token & 0x000f0000u))
		return false;
	const auto index = registerIndex(token);
	const bool pixel = stage == ShaderStage::Pixel20;
	switch (registerClass(token))
	{
		case RegisterClass::Temporary: return index < 12;
		case RegisterClass::RasterOutput: return !pixel && index < 3;
		case RegisterClass::AttributeOutput: return !pixel && index < 2;
		case RegisterClass::TexcoordOutput: return !pixel && index < 8;
		case RegisterClass::ColorOutput: return pixel && index < 4;
		case RegisterClass::DepthOutput: return pixel && index == 0;
		default: return false;
	}
}

bool validDeclaration(uint32_t declaration, uint32_t token, ShaderStage stage)
{
	const auto index = registerIndex(token);
	if (!(declaration & 0x80000000u) || !(token & 0x000f0000u))
		return false;
	if (stage != ShaderStage::Pixel20)
		return registerClass(token) == RegisterClass::Input && index < 16 &&
			(declaration & 0xfu) <= 13;
	switch (registerClass(token))
	{
		case RegisterClass::Input: return index < 2;
		case RegisterClass::Texture: return index < 8;
		case RegisterClass::Sampler:
		{
			const unsigned type = (declaration >> 27) & 0xfu;
			return index < 16 && type >= 2 && type <= 4;
		}
		default: return false;
	}
}

bool validShaderInstruction(Words words, ShaderStage stage, size_t &length)
{
	const auto token = words.front();
	const auto opcode = token & 0xffffu;
	const bool legacy = stage == ShaderStage::Vertex11;
	const bool vertex = stage != ShaderStage::Pixel20;
	const unsigned control = (token >> 16) & 0xffu;
	const size_t operands = shaderOperands(opcode);
	const size_t encoded = (token >> 24) & 0xfu;
	length = legacy ? operands : encoded;
	if (operands >= 16 || length < operands || length >= words.size() - 1 ||
		(legacy && encoded) || (token & 0xf0000000u) || control > (opcode == 0x42 ? 2u : 0u))
		return false;
	const auto direct = [](uint32_t word) { return (word & 0x80002000u) == 0x80000000u; };
	if (operands && !direct(words[opcode == 0x1f ? 2 : 1]))
		return false;
	switch (opcode)
	{
		case 0x00: return length == 0;
		case 0x1f: return length == operands && validDeclaration(words[1], words[2], stage);
		case 0x25:
		{
			if (length != operands || stage != ShaderStage::Vertex20 ||
				registerClass(words[1]) != RegisterClass::Temporary || !validDestination(words[1], stage) ||
				(words[1] & 0x000c0000u) || !direct(words[2]) || !validSource(words[2], stage))
				return false;
			const auto swizzle = (words[2] >> 16) & 0xffu;
			const auto polynomial = [&](uint32_t word) {
				return direct(word) && registerClass(word) == RegisterClass::Constant &&
					((word >> 16) & 0xfffu) == 0xe4u && validSource(word, stage);
			};
			return swizzle == (swizzle & 3u) * 0x55u && polynomial(words[3]) && polynomial(words[4]) &&
				registerIndex(words[3]) != registerIndex(words[4]);
		}
		case 0x26: return length == operands && stage == ShaderStage::Vertex20 &&
			registerClass(words[1]) == RegisterClass::Integer && registerIndex(words[1]) < 16 &&
			((words[1] >> 16) & 0xfffu) == 0xe4u;
		case 0x28: return length == operands && stage == ShaderStage::Vertex20 &&
			registerClass(words[1]) == RegisterClass::Boolean && registerIndex(words[1]) < 16;
		case 0x27: case 0x2a: case 0x2b: return length == operands && stage == ShaderStage::Vertex20;
		case 0x30: return length == operands && stage == ShaderStage::Vertex20 &&
			registerClass(words[1]) == RegisterClass::Integer && registerIndex(words[1]) < 16;
		case 0x42: return length == operands && !vertex && direct(words[2]) && direct(words[3]) &&
			registerClass(words[1]) == RegisterClass::Temporary && validDestination(words[1], stage) &&
			validSource(words[2], stage) && registerClass(words[3]) == RegisterClass::Sampler &&
			registerIndex(words[3]) < 16;
		case 0x51: return length == operands && registerClass(words[1]) == RegisterClass::Constant &&
			registerIndex(words[1]) < floatRegisterCount(stage);
	}
	const bool writesAddress = registerClass(words[1]) == RegisterClass::Texture && !registerIndex(words[1]) &&
		((words[1] >> 16) & 0xfu) == (legacy ? 1u : 8u);
	if ((opcode == 0x2e && (stage != ShaderStage::Vertex20 || !writesAddress)) ||
		(opcode != 0x2e && !(opcode == 1 && legacy && writesAddress) && !validDestination(words[1], stage)))
		return false;
	size_t cursor = 2;
	for (size_t i = 1; i < operands; ++i)
	{
		if (cursor > length || !(words[cursor] & 0x80000000u) || !validSource(words[cursor], stage))
			return false;
		const auto source = words[cursor++];
		if (source & 0x2000u)
		{
			if (!vertex || registerClass(source) != RegisterClass::Constant)
				return false;
			if (!legacy)
			{
				if (cursor > length)
					return false;
				const auto address = words[cursor++];
				if (!direct(address) || registerClass(address) != RegisterClass::Texture || registerIndex(address) ||
					((address >> 16) & 0xfffu) != 0xffu)
					return false;
			}
		}
	}
	return cursor == length + 1;
}

unsigned inputCount(unsigned operation)
{
	switch (operation)
	{
		case 0x100: case 0x101: case 0x103: case 0x104: case 0x107: case 0x108: case 0x109:
			return 1;
		case 0x200: case 0x201: case 0x202: case 0x204: case 0x205: case 0x500:
			return 2;
		case 0x300:
			return 3;
		default:
			return 0;
	}
}

bool readInstructions(Words words, Program &program, size_t &remaining)
{
	if (words.empty() || words[0] > 512)
		return false;
	const auto count = words.front();
	if (!spend(remaining, count * sizeof(Instruction)))
		return false;
	program.instructions.reserve(count);
	words = words.subspan(1);
	for (unsigned i = 0; i < count; ++i)
	{
		if (words.size() < 2)
			return false;
		const unsigned operation = (words[0] >> 20) & 0x7ff;
		const unsigned components = words[0] & 0xffff;
		const auto inputs = inputCount(operation);
		if (!inputs || !components || components > 4 || words[1] != inputs || words.size() < 2 + 3 * (inputs + 1))
			return false;
		Instruction ins{operation, components, (words[0] >> 31) != 0, {}, {}};
		words = words.subspan(2);
		for (unsigned j = 0; j < inputs; ++j)
		{
			if (words[0] || (words[1] != 1 && words[1] != 2 && words[1] != 4 && words[1] != 7))
				return false;
			ins.inputs[j] = {words[1], words[2]};
			words = words.subspan(3);
		}
		if (words[0] || (words[1] != 4 && words[1] != 7))
			return false;
		ins.output = {words[1], words[2]};
		words = words.subspan(3);
		program.instructions.push_back(ins);
	}
	return true;
}

bool readPreshader(Words words, Program &program, size_t &remaining)
{
	if (words.empty())
		return true;
	if ((words[0] & 0xffff0000u) != 0x46580000u)
		return false;
	words = words.subspan(1);
	bool constantsSeen = false, literalsSeen = false, expressionsSeen = false, infoSeen = false;
	while (!words.empty() && (words.front() & 0xffffu) == 0xfffeu)
	{
		const auto chunk = takeComment(words);
		if (chunk.empty())
			return false;
		switch (chunk.front())
		{
			case constantTable:
				if (constantsSeen || !readBindings(chunk.subspan(1), program.inputs, remaining))
					return false;
				constantsSeen = true;
				break;
			case literalTable:
				if (literalsSeen || chunk.size() < 2 || chunk[1] > (chunk.size() - 2) / 2 ||
					!spend(remaining, chunk[1] * sizeof(double)))
					return false;
				literalsSeen = true;
				program.literals.reserve(chunk[1]);
				for (size_t i = 0; i < chunk[1]; ++i)
					program.literals.push_back(std::bit_cast<double>(uint64_t(chunk[2 + i * 2]) | (uint64_t(chunk[3 + i * 2]) << 32)));
				break;
			case expressionList:
				if (expressionsSeen || !readInstructions(chunk.subspan(1), program, remaining))
					return false;
				expressionsSeen = true;
				break;
			case preshaderInfo:
				if (infoSeen)
					return false;
				infoSeen = true;
				break;
			default:
				return false;
		}
	}
	return !program.instructions.empty();
}

void applyBindings(const std::vector<Binding> &bindings, const SetBinding &setBinding, Registers &registers)
{
	for (const auto &binding : bindings)
	{
		if (binding.bank == 3)
			continue;
		if (binding.bank == 0)
		{
			auto values = std::span(registers.booleans).subspan(binding.first, binding.count);
			std::transform(binding.defaults.begin(), binding.defaults.end(), values.begin(),
				[](uint32_t value) { return value != 0; });
			setBinding(binding, {}, values);
		}
		else
		{
			auto values = std::span(registers.floats).subspan(binding.first, binding.count);
			for (size_t i = 0; i < binding.defaults.size(); ++i)
				values[i / 4][i % 4] = std::bit_cast<float>(binding.defaults[i]);
			setBinding(binding, values, {});
		}
	}
}

double evaluate(unsigned operation, double a, double b, double c)
{
	switch (operation)
	{
		case 0x100: return a;
		case 0x101: return -a;
		case 0x103: return 1.0 / a;
		case 0x104: return a - std::floor(a);
		case 0x107: return 1.0 / std::sqrt(std::abs(a));
		case 0x108: return std::sin(a);
		case 0x109: return std::cos(a);
		case 0x200: return std::fmin(a, b);
		case 0x201: return std::fmax(a, b);
		case 0x202: return a < b ? 1.0 : 0.0;
		case 0x204: return a + b;
		case 0x205: return a * b;
		case 0x300: return a >= 0.0 ? b : c;
	}
	return 0.0;
}

}

std::optional<Program> readProgram(std::span<const uint8_t> bytes, bool vertex)
{
	size_t remaining = maxDecodedProgramBytes;
	if (bytes.size() < 8 || bytes.size() % 4 || !spend(remaining, bytes.size()))
		return std::nullopt;
	Program program;
	program.tokens.reserve(bytes.size() / 4);
	for (; !bytes.empty(); bytes = bytes.subspan(4))
		program.tokens.push_back(readWord(bytes));
	auto words = Words(program.tokens);
	const bool legacy = vertex && words.front() == 0xfffe0101u;
	if (!legacy && words.front() != (vertex ? 0xfffe0200u : 0xffff0200u))
		return std::nullopt;
	const auto stage = legacy ? ShaderStage::Vertex11 : vertex ? ShaderStage::Vertex20 : ShaderStage::Pixel20;
	words = words.subspan(1);
	bool constantsSeen = false, preshaderSeen = false;
	std::array<bool, 24> elseSeen{};
	std::array<unsigned, 16> repeatCount{};
	size_t nesting = 0;
	bool repeating = false;
	while (words.size() > 1)
	{
		const auto opcode = words.front() & 0xffffu;
		if (opcode == 0xfffeu)
		{
			const auto chunk = takeComment(words);
			if (chunk.empty())
				return std::nullopt;
			if (chunk.front() == constantTable)
			{
				if (constantsSeen || !readBindings(chunk.subspan(1), program.bindings, remaining, stage))
					return std::nullopt;
				constantsSeen = true;
			}
			if (chunk.front() == preshaderChunk)
			{
				if (preshaderSeen || !readPreshader(chunk.subspan(1), program, remaining))
					return std::nullopt;
				preshaderSeen = true;
			}
		}
		else
		{
			size_t operands;
			if (!validShaderInstruction(words, stage, operands))
				return std::nullopt;
			if (opcode == 0x30)
			{
				if (repeating || nesting)
					return std::nullopt;
				repeatCount[registerIndex(words[1])] = words[2];
			}
			else if (opcode == 0x26)
			{
				const auto count = repeatCount[registerIndex(words[1])];
				if (repeating || nesting || !count || count > 16)
					return std::nullopt;
				repeating = true;
			}
			else if (opcode == 0x27)
			{
				if (!repeating || nesting)
					return std::nullopt;
				repeating = false;
			}
			if (opcode == 0x28)
			{
				if (nesting == elseSeen.size())
					return std::nullopt;
				elseSeen[nesting++] = false;
			}
			else if (opcode == 0x2a)
			{
				if (!nesting || elseSeen[nesting - 1])
					return std::nullopt;
				elseSeen[nesting - 1] = true;
			}
			else if (opcode == 0x2b)
			{
				if (!nesting)
					return std::nullopt;
				--nesting;
			}
			words = words.subspan(1 + operands);
		}
	}
	if (words.size() != 1 || words.front() != 0x0000ffffu || !constantsSeen || nesting || repeating)
		return std::nullopt;
	return program;
}

bool fillRegisters(const Program &program, const SetBinding &setBinding, Registers &registers)
{
	Registers inputs;
	std::array<float, 1024> temps{};
	applyBindings(program.bindings, setBinding, registers);
	applyBindings(program.inputs, setBinding, inputs);
	for (const auto &ins : program.instructions)
	{
		const auto count = inputCount(ins.operation);
		const auto dot = ins.operation == 0x500;
		std::array<std::array<double, 4>, 3> inputsByComponent{};
		std::array<double, 4> values{};
		for (unsigned j = 0; j < count; ++j)
		{
			const auto [bank, offset] = ins.inputs[j];
			const unsigned components = j == 0 && ins.scalar ? 1 : ins.components;
			const auto size = bank == 1 ? program.literals.size() : temps.size();
			if (offset >= size || components > size - offset)
				return false;
			for (unsigned c = 0; c < ins.components; ++c)
			{
				const unsigned index = offset + (components == 1 ? 0 : c);
				const double value = bank == 1 ? program.literals[index] :
					bank == 2 ? inputs.floats[index / 4][index % 4] :
					bank == 4 ? registers.floats[index / 4][index % 4] : temps[index];
				inputsByComponent[j][c] = value;
			}
		}
		for (unsigned c = 0; c < ins.components; ++c)
		{
			if (dot)
				values[0] += inputsByComponent[0][c] * inputsByComponent[1][c];
			else
				values[c] = evaluate(ins.operation, inputsByComponent[0][c], inputsByComponent[1][c], inputsByComponent[2][c]);
		}
		const auto [bank, offset] = ins.output;
		const auto components = dot ? 1 : ins.components;
		if (offset >= temps.size() || components > temps.size() - offset)
			return false;
		for (unsigned c = 0; c < components; ++c)
		{
			const auto index = offset + c;
			const auto value = static_cast<float>(values[c]);
			if (!std::isfinite(value))
				return false;
			(bank == 4 ? registers.floats[index / 4][index % 4] : temps[index]) = value;
		}
	}
	return true;
}

}
