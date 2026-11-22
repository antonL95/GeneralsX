#pragma once

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

namespace CreateAHeroRules
{

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
		return true;
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

inline std::string sanitizeToken(std::string_view input)
{
	std::string out;
	out.reserve(input.size());
	for (char c : input)
	{
		if (c == '|' || c == ',' || c == ';' || c == ':' || c == '=' || c == '\r' || c == '\n')
			continue;
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

inline std::vector<std::string> splitList(std::string_view input)
{
	std::vector<std::string> out;
	if (input.empty())
		return out;
	size_t start = 0;
	while (start <= input.size())
	{
		const size_t comma = input.find(',', start);
		const std::string_view part = comma == std::string_view::npos
			? input.substr(start)
			: input.substr(start, comma - start);
		if (!part.empty())
			out.emplace_back(part.data(), part.size());
		if (comma == std::string_view::npos)
			break;
		start = comma + 1;
	}
	return out;
}

inline std::string encodeSelection(const SelectionData& sel)
{
	if (sel.empty())
		return {};
	std::string out;
	out.append(sanitizeToken(sel.classUpgrade));
	out.push_back('|');
	out.append(sanitizeToken(sel.subClassUpgrade));
	out.push_back('|');
	out.append(sanitizeToken(sel.heroName));
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
	if (encoded.empty())
		return false;
	std::vector<std::string_view> fields;
	size_t start = 0;
	while (start <= encoded.size())
	{
		const size_t bar = encoded.find('|', start);
		if (bar == std::string_view::npos)
		{
			fields.push_back(encoded.substr(start));
			break;
		}
		fields.push_back(encoded.substr(start, bar - start));
		start = bar + 1;
	}
	if (fields.size() != 7 || fields[0].empty() || fields[1].empty())
		return false;
	out.classUpgrade.assign(fields[0].data(), fields[0].size());
	out.subClassUpgrade.assign(fields[1].data(), fields[1].size());
	out.heroName.assign(fields[2].data(), fields[2].size());
	out.blingUpgrades = splitList(fields[3]);
	out.attributeUpgrades = splitList(fields[4]);
	out.powerButtons = splitList(fields[5]);
	const std::vector<std::string> colorParts = splitList(fields[6]);
	if (colorParts.size() != 9)
		return false;
	for (int i = 0; i < 3; ++i)
	{
		out.primaryColor[i] = std::clamp(std::atoi(colorParts[i].c_str()), 0, 255);
		out.secondaryColor[i] = std::clamp(std::atoi(colorParts[i + 3].c_str()), 0, 255);
		out.tertiaryColor[i] = std::clamp(std::atoi(colorParts[i + 6].c_str()), 0, 255);
	}
	return true;
}

}
