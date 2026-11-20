#pragma once

#include <cstddef>
#include <vector>

namespace PlayerTemplateRules
{
inline bool parsesRotwkFields(bool retailLayers, bool auditEnabled)
{
	return retailLayers || auditEnabled;
}

inline bool usesMpVariant(bool retailLayers, bool isMultiplayerOrSkirmish)
{
	return retailLayers && isMultiplayerOrSkirmish;
}

inline bool grantsInitialUpgrades(bool retailLayers)
{
	return retailLayers;
}

template <typename T>
inline const T& selectVariant(bool useMp, const T& mpValue, const T& spValue)
{
	return (useMp && !mpValue.empty()) ? mpValue : spValue;
}

inline int selectLevelVariant(bool useMp, int mpLevel, int spLevel)
{
	return (useMp && mpLevel > 0) ? mpLevel : spLevel;
}

inline int resourceModifierPercent(const std::vector<int>& values, std::size_t buildingCount)
{
	if (values.empty())
		return 100;
	const std::size_t index = buildingCount > 0 ? buildingCount - 1 : 0;
	return values[index < values.size() ? index : values.size() - 1];
}
}
