#pragma once

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

}
