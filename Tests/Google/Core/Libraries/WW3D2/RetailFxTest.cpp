#include "retailfx.h"
#include "retailfxprogram.h"

#include <gtest/gtest.h>

namespace
{

std::vector<uint8_t> encodeWords(std::span<const uint32_t> words)
{
	std::vector<uint8_t> bytes;
	bytes.reserve(words.size_bytes());
	for (const auto word : words)
		for (unsigned shift = 0; shift < 32; shift += 8)
			bytes.push_back(static_cast<uint8_t>(word >> shift));
	return bytes;
}

std::vector<uint8_t> effectFixture()
{
	const uint32_t words[] = {
		0xfeff0901, 64,
		0, 2, 0x00000054, 2, 0x00000050,
		3, 0, 4, 0, 0, 1, 1,
		0x3f000000, 0, 0, 0,
		1, 1, 1, 1,
		20, 48, 8, 0,
		4, 0, 1,
		12, 0, 1,
		27, 0, 20, 48,
		0, 0
	};

	return encodeWords(words);
}

TEST(RetailFxTest, PreservesNamesFlagsAndPassStateValues)
{
	std::string error;
	const auto effect = RetailFX::parse(effectFixture(), error);
	ASSERT_TRUE(effect) << error;
	ASSERT_EQ(effect->parameters.size(), 1u);
	EXPECT_EQ(effect->parameters[0].value.name, "T");
	EXPECT_EQ(effect->parameters[0].flags, 8u);
	ASSERT_EQ(effect->techniques.size(), 1u);
	const auto &technique = effect->techniques[0];
	EXPECT_EQ(technique.name, "T");
	ASSERT_EQ(technique.passes.size(), 1u);
	EXPECT_EQ(technique.passes[0].name, "P");
	ASSERT_EQ(technique.passes[0].states.size(), 1u);
	const auto &state = technique.passes[0].states[0];
	EXPECT_EQ(state.operation, 27u);
	EXPECT_EQ(state.value.words, std::vector<uint32_t>{0x3f000000});
}

TEST(RetailFxTest, RejectsEveryTruncationOfAnEffect)
{
	const auto bytes = effectFixture();
	for (size_t size = 0; size < bytes.size(); ++size)
	{
		std::string error;
		EXPECT_FALSE(RetailFX::parse(std::span(bytes).first(size), error)) << size;
		EXPECT_FALSE(error.empty());
	}
}

TEST(RetailFxTest, RejectsOutOfRangeTypeOffset)
{
	auto bytes = effectFixture();
	bytes[8 + 64 + 16 + 3] = 0x80;
	std::string error;
	EXPECT_FALSE(RetailFX::parse(bytes, error));
	EXPECT_FALSE(error.empty());
}

std::vector<uint8_t> resourceFixture()
{
	const uint32_t words[] = {
		0xfeff0901, 116,
		0, 2, 'S', 2, 'T', 2, 'P',
		12, 4, 4, 0, 0,
		15, 4, 0, 0, 0,
		5, 4, 0, 0, 0,
		1, 164, 256, 68, 108, 0, 1,
		1, 1, 1, 2,
		28, 88, 0, 0,
		12, 0, 1,
		20, 0, 1,
		147, 0, 48, 112,
		0, 2,
		0xffffffff, 0, 0xffffffff, 0, 1, 2, 'M',
		0, 0, 0xffffffff, 0, 0, 8, 0xffff0200, 0x0000ffff
	};
	return encodeWords(words);
}

TEST(RetailFxTest, AttachesSamplerReferenceAndShaderToTheirStates)
{
	std::string error;
	const auto effect = RetailFX::parse(resourceFixture(), error);
	ASSERT_TRUE(effect) << error;
	ASSERT_EQ(effect->parameters[0].value.samplerStates.size(), 1u);
	const auto &samplerState = effect->parameters[0].value.samplerStates[0];
	EXPECT_EQ(samplerState.operation, 164u);
	EXPECT_EQ(samplerState.resourceUsage, RetailFX::ResourceUsage::Parameter);
	EXPECT_EQ(samplerState.parameterReference, "M");
	const auto &passState = effect->techniques[0].passes[0].states[0];
	EXPECT_EQ(passState.value.type, RetailFX::ValueType::PixelShader);
	EXPECT_EQ(passState.resourceUsage, RetailFX::ResourceUsage::Program);
	EXPECT_EQ(passState.resource, (std::vector<uint8_t>{0, 2, 255, 255, 255, 255, 0, 0}));
}

TEST(RetailFxTest, RejectsResourcePointingOutsidePassStates)
{
	auto bytes = resourceFixture();
	bytes[bytes.size() - 20] = 1;
	std::string error;
	EXPECT_FALSE(RetailFX::parse(bytes, error));
	EXPECT_FALSE(error.empty());
}

TEST(RetailFxTest, ReadsShaderConstantTableAndRequiresBoundedTokens)
{
	const uint32_t words[] = {
		0xfffe0200, 0x000efffe, 0x42415443,
		28, 0, 0xfffe0200, 1, 28, 0, 0,
		48, 0x00030002, 1, 0, 0, 'T',
		0x0000ffff
	};
	const auto bytes = encodeWords(words);
	const auto program = RetailFX::readProgram(bytes, true);
	ASSERT_TRUE(program);
	ASSERT_EQ(program->bindings.size(), 1u);
	EXPECT_EQ(program->bindings[0].name, "T");
	EXPECT_EQ(program->bindings[0].first, 3u);
	EXPECT_FALSE(RetailFX::readProgram(bytes, false));
	EXPECT_FALSE(RetailFX::readProgram(std::span(bytes).first(bytes.size() - 4), true));
	auto bad = bytes;
	bad.insert(bad.end() - 4, {0x1f, 0, 0, 0});
	EXPECT_FALSE(RetailFX::readProgram(bad, true));
}

TEST(RetailFxTest, ValidatesShaderOperandRolesAndRegisterLimits)
{
	const auto accepts = [](bool vertex, std::initializer_list<uint32_t> instruction) {
		std::vector<uint32_t> words = {
			vertex ? 0xfffe0200u : 0xffff0200u, 0x000efffe, 0x42415443,
			28, 0, 0, 1, 28, 0, 0,
			48, 0x00030002, 1, 0, 0, 'T'
		};
		words.insert(words.end(), instruction);
		words.push_back(0x0000ffff);
		return RetailFX::readProgram(encodeWords(words), vertex).has_value();
	};

	EXPECT_TRUE(accepts(false, {0x02000001, 0x800f0803, 0x80e40000}));
	EXPECT_FALSE(accepts(false, {0x02000001, 0x800f0804, 0x80e40000}));
	EXPECT_FALSE(accepts(true, {0x02000001, 0x800f0800, 0x80e40000}));
	EXPECT_FALSE(accepts(false, {0x02000001, 0x800f0000, 0xa0e40020}));
	EXPECT_FALSE(accepts(true, {0x02000001, 0x800f0000, 0x90e40010}));
	EXPECT_TRUE(accepts(false, {0x03000042, 0x800f0000, 0xb0e40000, 0xa0e4080f}));
	EXPECT_FALSE(accepts(false, {0x03000042, 0x800f0000, 0xb0e40000, 0xa0e40810}));
	EXPECT_FALSE(accepts(false, {0x03000042, 0x800f0000, 0xb0e40000, 0x80e40000}));
	EXPECT_FALSE(accepts(false, {0x0200001f, 0x90000000, 0xa00f0810}));
}

TEST(RetailFxTest, AcceptsTheLegacyTerrainVertexDeclarations)
{
	const uint32_t words[] = {
		0xfffe0101, 0x000efffe, 0x42415443, 28, 0, 0xfffe0101, 1, 28, 0, 0,
		48, 0x00030002, 1, 0, 0, 'T',
		0x0000001f, 0x80000003, 0x900f0001,
		0x00000001, 0xc00f0000, 0x90e40001,
		0x0000ffff
	};
	EXPECT_TRUE(RetailFX::readProgram(encodeWords(words), true));
}

TEST(RetailFxTest, RequiresALiteralBoundForVertexRepeat)
{
	std::vector<uint32_t> words = {
		0xfffe0200, 0x000efffe, 0x42415443, 28, 0, 0xfffe0200, 1, 28, 0, 0,
		48, 0x00030002, 1, 0, 0, 'T',
		0x05000030, 0xf00f0000, 3, 0, 0, 0,
		0x01000026, 0xf0e40000, 0x00000027,
		0x0000ffff
	};
	EXPECT_TRUE(RetailFX::readProgram(encodeWords(words), true));
	words[18] = 17;
	EXPECT_FALSE(RetailFX::readProgram(encodeWords(words), true));
	words[18] = 3;
	words.erase(words.end() - 2);
	EXPECT_FALSE(RetailFX::readProgram(encodeWords(words), true));
}

TEST(RetailFxTest, RequiresTheVertexAddressRegisterForRelativeConstants)
{
	std::vector<uint32_t> words = {
		0xfffe0200, 0x000efffe, 0x42415443, 28, 0, 0xfffe0200, 1, 28, 0, 0,
		48, 0x00030002, 1, 0, 0, 'T',
		0x0200002e, 0xb0080000, 0x90ff0000,
		0x03000001, 0x800f0000, 0xa0e42003, 0xb0ff0000,
		0x0000ffff
	};
	EXPECT_TRUE(RetailFX::readProgram(encodeWords(words), true));
	words[22] = 0xb0ff0001;
	EXPECT_FALSE(RetailFX::readProgram(encodeWords(words), true));
	words[22] = 0xb0ff0000;
	words[21] = 0x90e42003;
	EXPECT_FALSE(RetailFX::readProgram(encodeWords(words), true));
}

TEST(RetailFxTest, RejectsRepeatedShaderAndPreshaderMetadata)
{
	const uint32_t table[] = {
		0x000efffe, 0x42415443, 28, 0, 0xfffe0200, 1, 28, 0, 0,
		48, 0x00030002, 1, 0, 0, 'T'
	};
	const uint32_t preshader[] = {
		0x000ffffe, 0x53455250, 0x46580201,
		0x0001fffe, 0x49535250,
		0x000afffe, 0x434c5846, 1, 0x10000001, 1, 0, 1, 0, 0, 4, 0
	};
	std::vector<uint32_t> words = {0xfffe0200};
	words.insert(words.end(), std::begin(table), std::end(table));
	const auto tableEnd = words.size();
	words.insert(words.end(), std::begin(preshader), std::end(preshader));
	words.push_back(0x0000ffff);
	ASSERT_TRUE(RetailFX::readProgram(encodeWords(words), true));

	auto duplicate = words;
	duplicate.insert(duplicate.begin() + tableEnd, std::begin(table), std::end(table));
	EXPECT_FALSE(RetailFX::readProgram(encodeWords(duplicate), true));

	duplicate = words;
	duplicate.insert(duplicate.end() - 1, std::begin(preshader), std::end(preshader));
	EXPECT_FALSE(RetailFX::readProgram(encodeWords(duplicate), true));

	duplicate = words;
	duplicate[tableEnd] += 2u << 16;
	duplicate.insert(duplicate.end() - 1, {0x0001fffe, 0x49535250});
	EXPECT_FALSE(RetailFX::readProgram(encodeWords(duplicate), true));
}

TEST(RetailFxTest, BoundsAggregateDecodedRegisterDefaults)
{
	constexpr uint32_t count = 256;
	constexpr uint32_t nameOffset = (7 + count * 5) * 4;
	constexpr uint32_t defaultsOffset = nameOffset + 4;
	constexpr uint32_t defaults = 256 * 4;
	constexpr uint32_t wordsInComment = 1 + defaultsOffset / 4 + defaults;
	std::vector<uint32_t> words = {
		0xfffe0200, (wordsInComment << 16) | 0xfffe, 0x42415443,
		28, 0, 0xfffe0200, count, 28, 0, 0
	};
	for (uint32_t i = 0; i < count; ++i)
		words.insert(words.end(), {nameOffset, 2, 256, 0, defaultsOffset});
	words.push_back('T');
	words.resize(words.size() + defaults);
	words.push_back(0x0000ffff);
	EXPECT_FALSE(RetailFX::readProgram(encodeWords(words), true));
	words[6] = 8;
	EXPECT_TRUE(RetailFX::readProgram(encodeWords(words), true));
}

TEST(RetailFxTest, RunsScalarPreshaderOperandAcrossOutputComponents)
{
	RetailFX::Program program;
	program.inputs.push_back({"Time", 2, 0, 1, {}});
	program.literals = {3.0, 5.0};
	program.instructions.push_back({0x205, 2, true, {{{2, 0}, {1, 0}}}, {4, 12}});
	RetailFX::Registers output;
	const auto bind = [](const auto &, auto floats, auto) { floats[0][0] = 2.0f; };
	ASSERT_TRUE(RetailFX::fillRegisters(program, bind, output));
	EXPECT_EQ(output.floats[3], (RetailFX::Register{6.0f, 10.0f, 0.0f, 0.0f}));
	program.instructions[0].output.offset = 1023;
	EXPECT_FALSE(RetailFX::fillRegisters(program, bind, output));
}

TEST(RetailFxTest, SelectsTheNegativeAndNonnegativePreshaderInputs)
{
	RetailFX::Program program;
	program.literals = {-1.0, 0.0, 3.0, 4.0, 5.0, 6.0};
	program.instructions.push_back({0x300, 2, false, {{{1, 0}, {1, 2}, {1, 4}}}, {4, 0}});
	RetailFX::Registers output;
	ASSERT_TRUE(RetailFX::fillRegisters(program, [](const auto &, auto, auto) {}, output));
	EXPECT_EQ(output.floats[0], (RetailFX::Register{5.0f, 4.0f, 0.0f, 0.0f}));
}

}
