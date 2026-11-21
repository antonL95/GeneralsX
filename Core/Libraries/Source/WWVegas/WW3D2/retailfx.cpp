#include "retailfx.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace RetailFX
{
namespace
{

constexpr uint32_t noIndex = 0xffffffff;
constexpr size_t maxEffectSize = 16 * 1024 * 1024;
constexpr unsigned maxDepth = 32;

class Reader
{
public:
	explicit Reader(std::span<const uint8_t> bytes) : m_bytes(bytes) {}

	size_t remaining() const { return m_bytes.size() - m_offset; }

	std::span<const uint8_t> take(size_t count)
	{
		if (count > remaining())
			throw std::runtime_error("truncated effect record");
		const auto result = m_bytes.subspan(m_offset, count);
		m_offset += count;
		return result;
	}

	uint32_t word()
	{
		const auto bytes = take(4);
		return uint32_t(bytes[0]) | uint32_t(bytes[1]) << 8 |
			uint32_t(bytes[2]) << 16 | uint32_t(bytes[3]) << 24;
	}

private:
	std::span<const uint8_t> m_bytes;
	size_t m_offset = 0;
};

class Parser
{
public:
	explicit Parser(std::span<const uint8_t> body) : m_body(body), m_budget(body.size() * 16) {}

	Effect read(uint32_t tableOffset)
	{
		if (tableOffset > m_body.size())
			throw std::runtime_error("invalid effect table offset");
		Reader table(m_body.subspan(tableOffset));
		m_body = m_body.first(tableOffset);
		const auto parameters = table.word();
		const auto techniques = table.word();
		table.word();
		const auto objects = table.word();
		chargeRecords<std::vector<uint8_t>>(objects);
		m_effect.objects.resize(objects);

		m_effect.parameters = records<Parameter>(table, parameters, 16, [&] {
			auto parameter = readValue(table, 0);
			const auto flags = table.word();
			auto annotations = readAnnotations(table, table.word());
			return Parameter{std::move(parameter), flags, std::move(annotations)};
		});
		m_effect.techniques = records<Technique>(table, techniques, 12, [&] {
			auto name = readName(table.word());
			const auto annotations = table.word();
			const auto passes = table.word();
			auto values = readAnnotations(table, annotations);
			auto passList = records<Pass>(table, passes, 12, [&] {
				auto passName = readName(table.word());
				const auto passAnnotations = table.word();
				const auto states = table.word();
				auto passValues = readAnnotations(table, passAnnotations);
				return Pass{std::move(passName), std::move(passValues), readStates(table, states, 0)};
			});
			return Technique{std::move(name), std::move(values), std::move(passList)};
		});

		const auto strings = table.word();
		const auto resources = table.word();
		for (uint32_t i = 0; i < strings; ++i)
		{
			const auto id = table.word();
			checkObject(id);
			m_effect.objects[id] = readPayload(table);
		}
		for (uint32_t i = 0; i < resources; ++i)
			readResource(table);
		if (table.remaining())
			throw std::runtime_error("unexpected data after effect table");
		return std::move(m_effect);
	}

private:
	void charge(size_t bytes)
	{
		if (bytes > m_budget)
			throw std::runtime_error("effect exceeds decoded size limit");
		m_budget -= bytes;
	}

	template<typename T>
	void chargeRecords(size_t count)
	{
		if (count > m_budget / sizeof(T))
			throw std::runtime_error("effect exceeds decoded size limit");
		m_budget -= count * sizeof(T);
	}

	template<typename T, typename Read>
	std::vector<T> records(Reader &reader, uint32_t count, size_t recordSize, Read read)
	{
		if (count > reader.remaining() / recordSize)
			throw std::runtime_error("invalid effect record count");
		chargeRecords<T>(count);
		std::vector<T> result;
		result.reserve(count);
		for (uint32_t i = 0; i < count; ++i)
			result.push_back(read());
		return result;
	}

	Reader data(uint32_t offset) const
	{
		if (offset > m_body.size())
			throw std::runtime_error("invalid effect data offset");
		return Reader(m_body.subspan(offset));
	}

	std::string readName(uint32_t offset)
	{
		auto reader = data(offset);
		return readString(reader.take(reader.word()));
	}

	std::string readString(std::span<const uint8_t> bytes)
	{
		if (bytes.empty())
			return {};
		const auto end = std::find(bytes.begin(), bytes.end(), 0);
		if (end == bytes.end() || !std::all_of(end, bytes.end(), [](uint8_t byte) { return byte == 0; }))
			throw std::runtime_error("invalid effect name");
		charge(bytes.size());
		return {bytes.begin(), end};
	}

	static void checkDepth(unsigned depth)
	{
		if (depth >= maxDepth)
			throw std::runtime_error("effect type nesting limit exceeded");
	}

	Value readType(Reader &type, unsigned depth)
	{
		checkDepth(depth);
		Value value;
		const auto typeCode = type.word();
		const auto classCode = type.word();
		if (classCode > uint32_t(ValueClass::Struct) || typeCode > uint32_t(ValueType::VertexShader))
			throw std::runtime_error("unsupported effect value type");
		value.type = static_cast<ValueType>(typeCode);
		value.valueClass = static_cast<ValueClass>(classCode);
		value.name = readName(type.word());
		value.semantic = readName(type.word());
		value.arraySize = type.word();

		switch (value.valueClass)
		{
			case ValueClass::Vector:
				value.columns = type.word();
				value.rows = type.word();
				break;
			case ValueClass::Scalar:
			case ValueClass::MatrixRows:
			case ValueClass::MatrixColumns:
				value.rows = type.word();
				value.columns = type.word();
				break;
			case ValueClass::Struct:
				value.members = records<Value>(type, type.word(), 20, [&] { return readType(type, depth + 1); });
				break;
			case ValueClass::Object:
				if (value.type < ValueType::String)
					throw std::runtime_error("invalid effect object type");
				break;
		}
		if (value.valueClass <= ValueClass::MatrixColumns &&
			(value.rows < 1 || value.rows > 4 || value.columns < 1 || value.columns > 4 ||
			 value.type < ValueType::Bool || value.type > ValueType::Float))
			throw std::runtime_error("invalid numeric effect type");
		return value;
	}

	void chargeShape(const Value &value)
	{
		charge(value.name.size() + value.semantic.size() + value.members.size() * sizeof(Value));
		for (const auto &member : value.members)
			chargeShape(member);
	}

	void readInitial(Value &value, Reader &reader, unsigned depth)
	{
		checkDepth(depth);
		if (value.arraySize)
		{
			value.elements = records<Value>(reader, value.arraySize, 4, [&] {
				chargeShape(value);
				Value element = value;
				element.arraySize = 0;
				readInitial(element, reader, depth + 1);
				return element;
			});
			return;
		}

		switch (value.valueClass)
		{
			case ValueClass::Scalar:
			case ValueClass::Vector:
			case ValueClass::MatrixRows:
			case ValueClass::MatrixColumns:
				value.words = records<uint32_t>(reader, value.rows * value.columns, 4, [&] { return reader.word(); });
				break;
			case ValueClass::Struct:
				for (auto &member : value.members)
					readInitial(member, reader, depth + 1);
				break;
			case ValueClass::Object:
				if (value.type >= ValueType::Sampler && value.type <= ValueType::SamplerCube)
					value.samplerStates = readStates(reader, reader.word(), depth + 1);
				else
				{
					const auto id = reader.word();
					checkObject(id);
					value.object = id;
				}
				break;
		}
	}

	Value readValue(Reader &record, unsigned depth)
	{
		auto type = data(record.word());
		auto initial = data(record.word());
		auto value = readType(type, depth);
		readInitial(value, initial, depth);
		return value;
	}

	std::vector<Value> readAnnotations(Reader &reader, uint32_t count)
	{
		return records<Value>(reader, count, 8, [&] { return readValue(reader, 0); });
	}

	std::vector<State> readStates(Reader &reader, uint32_t count, unsigned depth)
	{
		return records<State>(reader, count, 16, [&] {
			const auto operation = reader.word();
			const auto index = reader.word();
			return State{operation, index, readValue(reader, depth), {}, {}, {}};
		});
	}

	void checkObject(uint32_t id) const
	{
		if (id >= m_effect.objects.size())
			throw std::runtime_error("invalid effect object index");
	}

	std::vector<uint8_t> readPayload(Reader &table)
	{
		const auto size = table.word();
		const auto bytes = table.take(size);
		table.take((4u - size % 4u) % 4u);
		charge(size);
		return {bytes.begin(), bytes.end()};
	}

	void readResource(Reader &table)
	{
		const auto technique = table.word();
		const auto index = table.word();
		const auto element = table.word();
		const auto stateIndex = table.word();
		const auto usage = table.word();
		if (usage > uint32_t(ResourceUsage::ArraySelector))
			throw std::runtime_error("unsupported effect resource usage");

		std::vector<State> *states;
		if (technique == noIndex)
		{
			if (index >= m_effect.parameters.size())
				throw std::runtime_error("invalid sampler parameter index");
			auto *sampler = &m_effect.parameters[index].value;
			if (!sampler->elements.empty())
			{
				if (element >= sampler->elements.size())
					throw std::runtime_error("invalid sampler element index");
				sampler = &sampler->elements[element];
			}
			states = &sampler->samplerStates;
		}
		else
		{
			if (technique >= m_effect.techniques.size() || index >= m_effect.techniques[technique].passes.size())
				throw std::runtime_error("invalid effect resource pass");
			states = &m_effect.techniques[technique].passes[index].states;
		}

		if (stateIndex >= states->size() || (*states)[stateIndex].resourceUsage)
			throw std::runtime_error("invalid effect resource state");
		auto &state = (*states)[stateIndex];
		state.resourceUsage = static_cast<ResourceUsage>(usage);
		state.resource = readPayload(table);
		if (state.resourceUsage == ResourceUsage::Parameter)
		{
			state.parameterReference = readString(state.resource);
		}
		else if (state.resourceUsage == ResourceUsage::ArraySelector)
		{
			Reader selector(state.resource);
			const auto nameSize = selector.word();
			if (nameSize % 4)
				throw std::runtime_error("unaligned effect selector name");
			state.parameterReference = readString(selector.take(nameSize));
		}
		if (state.resourceUsage != ResourceUsage::Program && state.parameterReference.empty())
			throw std::runtime_error("missing effect parameter reference");
	}

	std::span<const uint8_t> m_body;
	size_t m_budget;
	Effect m_effect;
};

}

std::optional<Effect> parse(std::span<const uint8_t> bytes, std::string &error)
{
	error.clear();
	try
	{
		if (bytes.size() > maxEffectSize)
			throw std::runtime_error("effect exceeds file size limit");
		Reader header(bytes);
		if (header.word() != 0xfeff0901)
			throw std::runtime_error("unsupported effect signature");
		const auto tableOffset = header.word();
		return Parser(header.take(header.remaining())).read(tableOffset);
	}
	catch (const std::runtime_error &failure)
	{
		error = failure.what();
		return std::nullopt;
	}
}

}
