#pragma once

#include <functional>
#include <memory>

template<class States, class Flags, class IsFallback, class GetConditions = std::identity>
const typename States::value_type* findRetailConditionMatch(const States& states, const Flags& active,
	IsFallback isFallback, GetConditions getConditions = {})
{
	const typename States::value_type* fallback = nullptr;
	for (const auto& state : states)
	{
		const auto& conditions = std::invoke(getConditions, state);
		for (int i = 0; i < conditions.getConditionsYesCount(); ++i)
		{
			const auto& required = conditions.getNthConditionsYes(i);
			if (required.any())
			{
				if (active.testForAll(required))
					return std::addressof(state);
			}
			else if (!fallback && isFallback(state))
			{
				fallback = std::addressof(state);
			}
		}
	}
	return fallback;
}
