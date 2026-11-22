#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <string>
#include <string_view>

namespace RetailTestCommands
{
template<typename Handler>
class Registry
{
public:
	bool registerAction(std::string_view action, Handler handler)
	{
		if (action.empty() || !handler)
			return false;
		return m_exact.emplace(std::string(action), handler).second;
	}

	bool registerPrefix(std::string_view prefix, Handler handler)
	{
		if (prefix.empty() || !handler)
			return false;
		return m_prefixes.emplace(std::string(prefix), handler).second;
	}

	Handler find(std::string_view action) const
	{
		if (const auto it = m_exact.find(action); it != m_exact.end())
			return it->second;

		Handler best = nullptr;
		std::size_t bestLength = 0;
		for (const auto& [prefix, handler] : m_prefixes)
		{
			if (action.starts_with(prefix) && prefix.size() > bestLength)
			{
				best = handler;
				bestLength = prefix.size();
			}
		}
		return best;
	}

	std::size_t exactCount() const { return m_exact.size(); }
	std::size_t prefixCount() const { return m_prefixes.size(); }

	void clear()
	{
		m_exact.clear();
		m_prefixes.clear();
	}

private:
	std::map<std::string, Handler, std::less<>> m_exact;
	std::map<std::string, Handler, std::less<>> m_prefixes;
};
}
