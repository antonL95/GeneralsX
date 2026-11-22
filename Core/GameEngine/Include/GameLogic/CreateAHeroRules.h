#pragma once

#include <algorithm>
#include <span>
#include <cctype>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace CreateAHeroRules
{

constexpr size_t MAX_ENCODED_LENGTH = 512;
constexpr size_t MAX_COMPACT_LENGTH = 98;
constexpr size_t MAX_HERO_NAME_LENGTH = 32;
constexpr size_t MAX_BLING_COUNT = 16;
constexpr size_t MAX_ATTRIBUTE_COUNT = 8;
constexpr size_t MAX_POWER_COUNT = 10;
constexpr size_t MAX_COMPACT_BYTES = 14 + MAX_BLING_COUNT + MAX_ATTRIBUTE_COUNT + MAX_POWER_COUNT;

inline bool equalsIgnoreCase(std::string_view a, std::string_view b)
{
	if (a.size() != b.size())
		return false;
	for (size_t i = 0; i < a.size(); ++i)
	{
		if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i])))
			return false;
	}
	return true;
}

inline bool parseStrictInt(std::string_view token, int minVal, int maxVal, int& out)
{
	if (token.empty() || token.size() > 11)
		return false;
	size_t idx = 0;
	bool neg = false;
	if (token[0] == '-')
	{
		if (minVal >= 0 || token.size() == 1)
			return false;
		neg = true;
		idx = 1;
	}
	else if (token[0] == '+')
	{
		return false;
	}
	if (token[idx] == '0' && (neg || token.size() != 1))
		return false;
	int64_t val = 0;
	for (; idx < token.size(); ++idx)
	{
		const unsigned char c = static_cast<unsigned char>(token[idx]);
		if (c < '0' || c > '9')
			return false;
		val = val * 10 + (c - '0');
		if (val > 2147483648LL)
			return false;
	}
	if (neg)
		val = -val;
	if (val < minVal || val > maxVal)
		return false;
	out = static_cast<int>(val);
	return true;
}

inline bool parseHexByte(std::string_view twoChars, int& out)
{
	if (twoChars.size() != 2)
		return false;
	auto hexVal = [](char ch) -> int
	{
		if (ch >= '0' && ch <= '9')
			return ch - '0';
		if (ch >= 'a' && ch <= 'f')
			return 10 + (ch - 'a');
		if (ch >= 'A' && ch <= 'F')
			return 10 + (ch - 'A');
		return -1;
	};
	const int hi = hexVal(twoChars[0]);
	const int lo = hexVal(twoChars[1]);
	if (hi < 0 || lo < 0)
		return false;
	out = (hi << 4) | lo;
	return true;
}

inline void appendHexByte(std::string& out, int value)
{
	static const char hex[] = "0123456789abcdef";
	const int clamped = std::clamp(value, 0, 255);
	out.push_back(hex[(clamped >> 4) & 0xF]);
	out.push_back(hex[clamped & 0xF]);
}

struct ParsedBlingRef
{
	std::string upgradeName;
	bool isDefault = false;
};

inline ParsedBlingRef parseBlingToken(std::string_view token)
{
	ParsedBlingRef out;
	if (!token.empty() && token.front() == '@')
	{
		out.isDefault = true;
		token.remove_prefix(1);
	}
	out.upgradeName.assign(token.data(), token.size());
	return out;
}

inline bool isFactionAllowed(const std::vector<std::string>& usableFactions, std::string_view factionName)
{
	if (usableFactions.empty())
		return true;
	for (const std::string& f : usableFactions)
	{
		if (equalsIgnoreCase(f, factionName))
			return true;
	}
	return false;
}

inline bool isPowerAllowedForUpgrades(
	const std::vector<std::string>& allowableUpgrades,
	const std::vector<std::string>& heroUpgrades)
{
	if (allowableUpgrades.empty())
		return false;
	for (const std::string& allowed : allowableUpgrades)
	{
		for (const std::string& owned : heroUpgrades)
		{
			if (equalsIgnoreCase(allowed, owned))
				return true;
		}
	}
	return false;
}

inline bool isPowerEligibleAtLevel(
	int minimumLevel,
	int selectedLevel,
	std::string_view prerequisiteButton,
	const std::vector<std::string>& selectedButtonsBefore)
{
	if (selectedLevel < minimumLevel)
		return false;
	if (prerequisiteButton.empty() || equalsIgnoreCase(prerequisiteButton, "None"))
		return true;
	for (const std::string& chosen : selectedButtonsBefore)
	{
		if (equalsIgnoreCase(chosen, prerequisiteButton))
			return true;
	}
	return false;
}

inline int discountedPowerCost(int baseCost, int minimumLevel, int selectedLevel, int discountPercentPerLevel)
{
	if (baseCost <= 0)
		return 0;
	const int extraLevels = std::max(0, selectedLevel - std::max(1, minimumLevel));
	const int totalDiscount = std::clamp(extraLevels * std::max(0, discountPercentPerLevel), 0, 100);
	return (baseCost * (100 - totalDiscount)) / 100;
}

inline int discountedRevivalCost(int baseCost, int revivalDiscountPercent)
{
	if (baseCost <= 0)
		return 0;
	const int clamped = std::clamp(revivalDiscountPercent, 0, 100);
	return (baseCost * (100 - clamped)) / 100;
}

struct AttributeRange
{
	std::string groupName;
	int minIndex = 0;
	int maxIndex = 0;
	int defaultIndex = 0;
};

inline bool validateAttributeSpend(
	const std::vector<AttributeRange>& ranges,
	const std::vector<int>& chosenIndices,
	int spendablePoints)
{
	if (ranges.size() != chosenIndices.size() || spendablePoints < 0)
		return false;
	int spent = 0;
	for (size_t i = 0; i < ranges.size(); ++i)
	{
		if (chosenIndices[i] < ranges[i].minIndex || chosenIndices[i] > ranges[i].maxIndex)
			return false;
		spent += (chosenIndices[i] - ranges[i].minIndex);
	}
	return spent <= spendablePoints;
}

struct SelectionData
{
	std::string classUpgrade;
	std::string subClassUpgrade;
	std::string heroName;
	std::vector<std::string> blingUpgrades;
	std::vector<std::string> attributeUpgrades;
	std::vector<std::string> powerButtons;
	int primaryColor[3] = { 160, 160, 160 };
	int secondaryColor[3] = { 160, 160, 160 };
	int tertiaryColor[3] = { 160, 160, 160 };

	bool empty() const
	{
		return classUpgrade.empty() && subClassUpgrade.empty();
	}
};

struct CompactSelection
{
	int classIndex = -1;
	int subClassIndex = -1;
	std::vector<int> blingIndices;
	std::vector<int> attributeIndices;
	std::vector<int> powerIndices;
	int primaryColor[3] = { 160, 160, 160 };
	int secondaryColor[3] = { 160, 160, 160 };
	int tertiaryColor[3] = { 160, 160, 160 };
	std::string heroName;
};

inline bool isSafeTokenChar(char c)
{
	const unsigned char u = static_cast<unsigned char>(c);
	return u >= 0x20 && u <= 0x7E && c != '|' && c != ',' && c != ';' && c != ':' && c != '=' && c != '.';
}

inline std::string sanitizeToken(std::string_view input, size_t maxLen = 64)
{
	std::string out;
	out.reserve(std::min(input.size(), maxLen));
	for (char c : input)
	{
		if (out.size() >= maxLen)
			break;
		if (isSafeTokenChar(c))
			out.push_back(c);
	}
	return out;
}

inline std::string joinList(const std::vector<std::string>& items)
{
	std::string out;
	for (size_t i = 0; i < items.size(); ++i)
	{
		if (i > 0)
			out.push_back(',');
		out.append(sanitizeToken(items[i]));
	}
	return out;
}

inline std::span<std::string_view> splitFields(std::string_view input, char separator, std::span<std::string_view> fields)
{
	size_t count = 0;
	for (;;)
	{
		if (count == fields.size())
			return {};
		const size_t end = input.find(separator);
		fields[count++] = input.substr(0, end);
		if (end == std::string_view::npos)
			return fields.first(count);
		input.remove_prefix(end + 1);
	}
}

inline bool isCanonicalToken(std::string_view token, size_t maxLength)
{
	return !token.empty() && token.size() <= maxLength && std::all_of(token.begin(), token.end(), isSafeTokenChar);
}

inline bool readTokenList(std::string_view value, std::span<std::string_view> storage, std::span<std::string_view>& tokens)
{
	if (value.empty())
		return true;
	tokens = splitFields(value, ',', storage);
	return !tokens.empty() && std::all_of(tokens.begin(), tokens.end(), [](std::string_view token) {
		return isCanonicalToken(token, 64);
	});
}

inline std::string encodeSelection(const SelectionData& sel)
{
	if (sel.empty() || !isCanonicalToken(sel.heroName, MAX_HERO_NAME_LENGTH))
		return {};
	std::string out;
	out.append(sanitizeToken(sel.classUpgrade));
	out.push_back('|');
	out.append(sanitizeToken(sel.subClassUpgrade));
	out.push_back('|');
	out.append(sanitizeToken(sel.heroName, MAX_HERO_NAME_LENGTH));
	out.push_back('|');
	out.append(joinList(sel.blingUpgrades));
	out.push_back('|');
	out.append(joinList(sel.attributeUpgrades));
	out.push_back('|');
	out.append(joinList(sel.powerButtons));
	out.push_back('|');
	for (int c = 0; c < 3; ++c)
	{
		if (c > 0)
			out.push_back(',');
		out.append(std::to_string(std::clamp(sel.primaryColor[c], 0, 255)));
	}
	out.push_back(',');
	for (int c = 0; c < 3; ++c)
	{
		if (c > 0)
			out.push_back(',');
		out.append(std::to_string(std::clamp(sel.secondaryColor[c], 0, 255)));
	}
	out.push_back(',');
	for (int c = 0; c < 3; ++c)
	{
		if (c > 0)
			out.push_back(',');
		out.append(std::to_string(std::clamp(sel.tertiaryColor[c], 0, 255)));
	}
	return out;
}

inline bool decodeSelection(std::string_view encoded, SelectionData& out)
{
	out = SelectionData{};
	if (encoded.empty() || encoded.size() > MAX_ENCODED_LENGTH)
		return false;
	std::string_view fields[7];
	if (splitFields(encoded, '|', fields).size() != std::size(fields) ||
		!isCanonicalToken(fields[0], 64) || !isCanonicalToken(fields[1], 64) ||
		!isCanonicalToken(fields[2], MAX_HERO_NAME_LENGTH))
		return false;
	std::string_view blingStorage[MAX_BLING_COUNT], attributeStorage[MAX_ATTRIBUTE_COUNT], powerStorage[MAX_POWER_COUNT];
	std::span<std::string_view> bling, attributes, powers;
	if (!readTokenList(fields[3], blingStorage, bling) || !readTokenList(fields[4], attributeStorage, attributes) ||
		!readTokenList(fields[5], powerStorage, powers))
		return false;
	std::string_view colors[9];
	if (splitFields(fields[6], ',', colors).size() != std::size(colors))
		return false;
	int rgb[9];
	for (size_t i = 0; i < std::size(colors); ++i)
		if (!parseStrictInt(colors[i], 0, 255, rgb[i]))
			return false;
	out.classUpgrade = fields[0];
	out.subClassUpgrade = fields[1];
	out.heroName = fields[2];
	out.blingUpgrades.assign(bling.begin(), bling.end());
	out.attributeUpgrades.assign(attributes.begin(), attributes.end());
	out.powerButtons.assign(powers.begin(), powers.end());
	std::copy_n(rgb, 3, out.primaryColor);
	std::copy_n(rgb + 3, 3, out.secondaryColor);
	std::copy_n(rgb + 6, 3, out.tertiaryColor);
	return true;
}

inline uint32_t applyColorMask(uint32_t diffuse, uint32_t mask, const uint32_t (&colors)[3])
{
	const unsigned coverage = mask >> 24;
	const unsigned weights[] = { (mask >> 16) & 255, (mask >> 8) & 255, mask & 255 };
	uint32_t result = diffuse & 0xff000000;
	for (unsigned shift = 0; shift < 24; shift += 8)
	{
		unsigned tinted = 0;
		for (size_t i = 0; i < 3; ++i)
			tinted += ((colors[i] >> shift) & 255) * weights[i];
		tinted = std::min(255u, (tinted + 127) / 255);
		const unsigned original = (diffuse >> shift) & 255;
		result |= ((original * (255 - coverage) + tinted * coverage + 127) / 255) << shift;
	}
	return result;
}

inline std::string base64UrlEncode(const std::vector<uint8_t>& bytes)
{
	static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
	std::string out;
	out.reserve(((bytes.size() + 2) / 3) * 4);
	size_t i = 0;
	while (i + 3 <= bytes.size())
	{
		const uint32_t v = (static_cast<uint32_t>(bytes[i]) << 16) |
			(static_cast<uint32_t>(bytes[i + 1]) << 8) |
			static_cast<uint32_t>(bytes[i + 2]);
		out.push_back(table[(v >> 18) & 0x3F]);
		out.push_back(table[(v >> 12) & 0x3F]);
		out.push_back(table[(v >> 6) & 0x3F]);
		out.push_back(table[v & 0x3F]);
		i += 3;
	}
	if (i + 1 == bytes.size())
	{
		const uint32_t v = static_cast<uint32_t>(bytes[i]) << 16;
		out.push_back(table[(v >> 18) & 0x3F]);
		out.push_back(table[(v >> 12) & 0x3F]);
	}
	else if (i + 2 == bytes.size())
	{
		const uint32_t v = (static_cast<uint32_t>(bytes[i]) << 16) |
			(static_cast<uint32_t>(bytes[i + 1]) << 8);
		out.push_back(table[(v >> 18) & 0x3F]);
		out.push_back(table[(v >> 12) & 0x3F]);
		out.push_back(table[(v >> 6) & 0x3F]);
	}
	return out;
}

inline bool base64UrlDecode(std::string_view input, std::span<uint8_t> out)
{
	if (input.empty() || (input.size() % 4) == 1)
		return false;
	auto valOf = [](char c) -> int
	{
		if (c >= 'A' && c <= 'Z') return c - 'A';
		if (c >= 'a' && c <= 'z') return 26 + (c - 'a');
		if (c >= '0' && c <= '9') return 52 + (c - '0');
		if (c == '-') return 62;
		if (c == '_') return 63;
		return -1;
	};
	if ((input.size() * 3) / 4 != out.size())
		return false;
	size_t written = 0;
	size_t i = 0;
	while (i + 4 <= input.size())
	{
		const int a = valOf(input[i]);
		const int b = valOf(input[i + 1]);
		const int c = valOf(input[i + 2]);
		const int d = valOf(input[i + 3]);
		if (a < 0 || b < 0 || c < 0 || d < 0)
			return false;
		const uint32_t v = (static_cast<uint32_t>(a) << 18) | (static_cast<uint32_t>(b) << 12) |
			(static_cast<uint32_t>(c) << 6) | static_cast<uint32_t>(d);
		out[written++] = static_cast<uint8_t>((v >> 16) & 0xFF);
		out[written++] = static_cast<uint8_t>((v >> 8) & 0xFF);
		out[written++] = static_cast<uint8_t>(v & 0xFF);
		i += 4;
	}
	const size_t rem = input.size() - i;
	if (rem == 2)
	{
		const int a = valOf(input[i]);
		const int b = valOf(input[i + 1]);
		if (a < 0 || b < 0 || (b & 0x0F) != 0)
			return false;
		out[written++] = static_cast<uint8_t>((a << 2) | (b >> 4));
	}
	else if (rem == 3)
	{
		const int a = valOf(input[i]);
		const int b = valOf(input[i + 1]);
		const int c = valOf(input[i + 2]);
		if (a < 0 || b < 0 || c < 0 || (c & 0x03) != 0)
			return false;
		out[written++] = static_cast<uint8_t>((a << 2) | (b >> 4));
		out[written++] = static_cast<uint8_t>(((b & 0x0F) << 4) | (c >> 2));
	}
	return true;
}

inline std::string encodeCompact(const CompactSelection& compact)
{
	if (compact.classIndex < 0 || compact.classIndex > 255 ||
		compact.subClassIndex < 0 || compact.subClassIndex > 255 ||
		compact.blingIndices.size() > MAX_BLING_COUNT ||
		compact.attributeIndices.size() > MAX_ATTRIBUTE_COUNT ||
		compact.powerIndices.size() > MAX_POWER_COUNT || !isCanonicalToken(compact.heroName, MAX_HERO_NAME_LENGTH))
		return {};
	std::vector<uint8_t> bytes;
	bytes.push_back(static_cast<uint8_t>(compact.classIndex));
	bytes.push_back(static_cast<uint8_t>(compact.subClassIndex));
	bytes.push_back(static_cast<uint8_t>(compact.blingIndices.size()));
	for (int idx : compact.blingIndices)
	{
		if (idx < 0 || idx > 255)
			return {};
		bytes.push_back(static_cast<uint8_t>(idx));
	}
	bytes.push_back(static_cast<uint8_t>(compact.attributeIndices.size()));
	for (int idx : compact.attributeIndices)
	{
		if (idx < 0 || idx > 255)
			return {};
		bytes.push_back(static_cast<uint8_t>(idx));
	}
	bytes.push_back(static_cast<uint8_t>(compact.powerIndices.size()));
	for (int idx : compact.powerIndices)
	{
		if (idx < 0 || idx > 255)
			return {};
		bytes.push_back(static_cast<uint8_t>(idx));
	}
	for (int i = 0; i < 3; ++i)
		bytes.push_back(static_cast<uint8_t>(std::clamp(compact.primaryColor[i], 0, 255)));
	for (int i = 0; i < 3; ++i)
		bytes.push_back(static_cast<uint8_t>(std::clamp(compact.secondaryColor[i], 0, 255)));
	for (int i = 0; i < 3; ++i)
		bytes.push_back(static_cast<uint8_t>(std::clamp(compact.tertiaryColor[i], 0, 255)));

	std::string out;
	out.push_back('~');
	out.append(base64UrlEncode(bytes));
	out.push_back('.');
	out.append(compact.heroName);
	return out;
}

inline bool decodeCompact(std::string_view encoded, CompactSelection& out)
{
	out = CompactSelection{};
	if (encoded.size() < 2 || encoded.size() > MAX_COMPACT_LENGTH || encoded[0] != '~')
		return false;
	encoded.remove_prefix(1);
	const size_t dot = encoded.find('.');
	if (dot == std::string_view::npos)
		return false;
	const auto payload = encoded.substr(0, dot);
	const auto namePart = encoded.substr(dot + 1);
	if (!isCanonicalToken(namePart, MAX_HERO_NAME_LENGTH))
		return false;
	const size_t size = payload.size() * 3 / 4;
	uint8_t storage[MAX_COMPACT_BYTES];
	if (size < 14 || size > std::size(storage))
		return false;
	const auto bytes = std::span(storage).first(size);
	if (!base64UrlDecode(payload, bytes))
		return false;
	size_t pos = 2;
	const auto readIndices = [&](size_t cap, std::span<uint8_t>& indices) {
		if (pos >= bytes.size())
			return false;
		const size_t count = bytes[pos++];
		if (count > cap || count > bytes.size() - pos)
			return false;
		indices = bytes.subspan(pos, count);
		pos += count;
		return true;
	};
	std::span<uint8_t> bling, attributes, powers;
	if (!readIndices(MAX_BLING_COUNT, bling) || !readIndices(MAX_ATTRIBUTE_COUNT, attributes) ||
		!readIndices(MAX_POWER_COUNT, powers) || bytes.size() - pos != 9)
		return false;
	out.classIndex = bytes[0];
	out.subClassIndex = bytes[1];
	out.blingIndices.assign(bling.begin(), bling.end());
	out.attributeIndices.assign(attributes.begin(), attributes.end());
	out.powerIndices.assign(powers.begin(), powers.end());
	for (int i = 0; i < 3; ++i)
		out.primaryColor[i] = bytes[pos++];
	for (int i = 0; i < 3; ++i)
		out.secondaryColor[i] = bytes[pos++];
	for (int i = 0; i < 3; ++i)
		out.tertiaryColor[i] = bytes[pos++];
	out.heroName = namePart;
	return true;
}

}
