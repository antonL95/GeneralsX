#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <utility>

namespace RetailGameText
{
	inline constexpr size_t maxFileSize = 16 * 1024 * 1024;
	inline constexpr size_t maxEntries = 131072;
	inline constexpr size_t maxLabelSize = 1024;
	inline constexpr size_t maxValueSize = 10240;

	struct Entry
	{
		std::string label;
		std::wstring text;
		std::string speech;
	};

	using Catalogue = std::map<std::string, Entry>;

	inline std::string labelKey(std::string_view label)
	{
		std::string key(label);
		for (char& c : key)
			if (c >= 'A' && c <= 'Z')
				c += 'a' - 'A';
		return key;
	}

	inline void addEntry(Catalogue& entries, Entry entry)
	{
		entries.try_emplace(labelKey(entry.label), std::move(entry));
	}

	inline void appendCodePoint(std::wstring& text, uint32_t codePoint)
	{
		if constexpr (sizeof(wchar_t) == 2)
		{
			if (codePoint > 0xffff)
			{
				codePoint -= 0x10000;
				text.push_back(static_cast<wchar_t>(0xd800 + (codePoint >> 10)));
				codePoint = 0xdc00 + (codePoint & 0x3ff);
			}
		}
		text.push_back(static_cast<wchar_t>(codePoint));
	}

	class CsfReader
	{
	public:
		explicit CsfReader(std::string_view data) : m_data(data) {}

		size_t remaining() const { return m_data.size(); }

		bool readBytes(size_t size, std::string_view& bytes)
		{
			if (size > m_data.size())
				return false;
			bytes = m_data.substr(0, size);
			m_data.remove_prefix(size);
			return true;
		}

		bool readInt(uint32_t& value)
		{
			std::string_view bytes;
			if (!readBytes(4, bytes))
				return false;
			value = 0;
			for (unsigned i = 0; i < 4; ++i)
				value |= uint32_t(static_cast<unsigned char>(bytes[i])) << (8 * i);
			return true;
		}

		bool readAscii(std::string_view& value)
		{
			uint32_t size;
			return readInt(size) && size <= maxLabelSize && readBytes(size, value)
				&& value.find('\0') == std::string_view::npos;
		}

	private:
		std::string_view m_data;
	};

	inline bool readCsfString(CsfReader& reader, Entry& entry, bool keep)
	{
		std::string_view tag, bytes, speech;
		uint32_t length;
		if (!reader.readBytes(4, tag) || (tag != " RTS" && tag != "WRTS")
			|| !reader.readInt(length) || length > maxValueSize
			|| !reader.readBytes(length * 2, bytes)
			|| (tag == "WRTS" && !reader.readAscii(speech)))
			return false;

		if (!keep)
			return true;

		const auto unit = [bytes](size_t i) {
			return 0xffffu ^ (uint32_t(static_cast<unsigned char>(bytes[2 * i]))
				| (uint32_t(static_cast<unsigned char>(bytes[2 * i + 1])) << 8));
		};
		for (size_t i = 0; i < length; ++i)
		{
			uint32_t codePoint = unit(i);
			if (!codePoint)
				return false;
			if (codePoint >= 0xd800 && codePoint <= 0xdfff)
			{
				if (codePoint >= 0xdc00 || i + 1 == length)
					return false;
				const auto low = unit(++i);
				if (low < 0xdc00 || low > 0xdfff)
					return false;
				codePoint = 0x10000 + ((codePoint - 0xd800) << 10) + (low - 0xdc00);
			}
			appendCodePoint(entry.text, codePoint);
		}
		entry.speech = speech;
		return true;
	}

	inline bool parseCsf(std::string_view bytes, Catalogue& entries)
	{
		if (bytes.size() > maxFileSize)
			return false;
		CsfReader reader(bytes);
		std::string_view tag;
		uint32_t version, labels, strings, reserved, language;
		if (!reader.readBytes(4, tag) || tag != " FSC"
			|| !reader.readInt(version) || (version != 0 && version != 3)
			|| !reader.readInt(labels) || labels > maxEntries
			|| !reader.readInt(strings)
			|| !reader.readInt(reserved) || reserved != 0
			|| !reader.readInt(language) || language > 255
			|| labels > reader.remaining() / 12 || strings > reader.remaining() / 8)
			return false;

		Catalogue parsed;
		for (uint32_t i = 0; i < labels; ++i)
		{
			uint32_t count;
			std::string_view label;
			if (!reader.readBytes(4, tag) || tag != " LBL"
				|| !reader.readInt(count) || count > strings
				|| !reader.readAscii(label) || label.empty()
				|| count > reader.remaining() / 8)
				return false;

			Entry entry{std::string(label), {}, {}};
			for (uint32_t j = 0; j < count; ++j)
				if (!readCsfString(reader, entry, j == 0))
					return false;
			strings -= count;
			if (count != 0)
				addEntry(parsed, std::move(entry));
		}
		if (strings != 0 || reader.remaining() != 0)
			return false;
		entries.merge(parsed);
		return true;
	}

	inline bool space(char c)
	{
		return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
	}

	inline std::wstring decodeStrText(std::string_view bytes)
	{
		std::wstring text;
		for (size_t i = 0; i < bytes.size(); ++i)
		{
			const auto first = static_cast<unsigned char>(bytes[i]);
			uint32_t value = first;
			const size_t extra = first >= 0xf0 ? 3 : first >= 0xe0 ? 2 : 1;
			if (first >= 0xc2 && first <= 0xf4 && extra < bytes.size() - i)
			{
				uint32_t codePoint = first & (0x7f >> (extra + 1));
				size_t j = 1;
				for (; j <= extra; ++j)
				{
					const auto next = static_cast<unsigned char>(bytes[i + j]);
					if ((next & 0xc0) != 0x80)
						break;
					codePoint = (codePoint << 6) | (next & 0x3f);
				}
				constexpr uint32_t minimum[]{0, 0x80, 0x800, 0x10000};
				if (j > extra && codePoint >= minimum[extra] && codePoint <= 0x10ffff
					&& (codePoint < 0xd800 || codePoint > 0xdfff))
				{
					value = codePoint;
					i += extra;
				}
			}
			appendCodePoint(text, value);
		}
		return text;
	}

	inline std::string_view trimStrLine(std::string_view line)
	{
		while (!line.empty() && space(line.front()))
			line.remove_prefix(1);
		while (!line.empty() && space(line.back()))
			line.remove_suffix(1);
		return line;
	}

	inline std::string_view nextStrLine(std::string_view& data)
	{
		while (!data.empty())
		{
			const size_t end = data.find('\n');
			const auto line = trimStrLine(data.substr(0, end));
			data.remove_prefix(end == std::string_view::npos ? data.size() : end + 1);
			if (!line.empty() && line.front() != ';' && !line.starts_with("//"))
				return line;
		}
		return {};
	}

	inline bool readStrValue(std::string_view line, std::wstring& text)
	{
		const auto close = line.rfind('"');
		if (line.empty() || line.front() != '"' || close == 0 || close > maxValueSize + 1)
			return false;

		line = line.substr(1, close - 1);
		while (!line.empty() && space(line.front()))
			line.remove_prefix(1);
		if (line.find('\0') != std::string_view::npos)
			return false;

		std::string decoded;
		for (size_t i = 0; i < line.size(); ++i)
		{
			char c = line[i];
			if (c == '\\')
			{
				if (++i == line.size())
					break;
				c = line[i];
				switch (c)
				{
					case 'n': c = '\n'; break;
					case 'r': c = '\r'; break;
					case 't': c = '\t'; break;
					case 'v': c = '\v'; break;
				}
			}
			decoded.push_back(c);
		}
		text = decodeStrText(decoded);
		return true;
	}

	inline bool isStrLabel(std::string_view line)
	{
		return !line.empty() && line.front() != '"' && line.size() <= maxLabelSize
			&& line.find('\0') == std::string_view::npos
			&& (line.size() != 3 || labelKey(line) != "end");
	}

	inline bool parseStr(std::string_view bytes, Catalogue& entries)
	{
		if (bytes.size() > maxFileSize)
			return false;
		if (bytes.starts_with("\xef\xbb\xbf"))
			bytes.remove_prefix(3);

		bool valid = true;
		size_t count = 0;
		auto line = nextStrLine(bytes);
		while (!line.empty())
		{
			const auto label = line;
			line = nextStrLine(bytes);
			if (!isStrLabel(label))
			{
				valid = false;
				continue;
			}
			if (++count > maxEntries)
				return false;

			Entry entry{std::string(label), {}, {}};
			const bool valueRead = readStrValue(line, entry.text);
			if (line.starts_with('"'))
				line = nextStrLine(bytes);
			const bool hasEnd = line.size() >= 3 && labelKey(line.substr(0, 3)) == "end"
				&& (line.size() == 3 || space(line[3]) || line[3] == ';' || line.substr(3).starts_with("//"));
			if (hasEnd)
				line = nextStrLine(bytes);
			if (valueRead && hasEnd)
				addEntry(entries, std::move(entry));
			else
				valid = false;
		}
		return valid;
	}
}
