#pragma once

#include <functional>
#include <memory>

enum class RetailEmptyState
{
	First,
	Last
};

template<class States, class Flags, class GetConditions = std::identity>
const typename States::value_type* findRetailConditionMatch(const States& states, const Flags& active,
	RetailEmptyState emptyState, GetConditions getConditions = {},
	const typename States::value_type* defaultState = nullptr)
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
			else if (!fallback || emptyState == RetailEmptyState::Last)
			{
				fallback = std::addressof(state);
			}
		}
	}
	return defaultState ? defaultState : fallback;
}
