#pragma once

#include <algorithm>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace UiHudRules
{
	enum class HealthStage
	{
		Green,
		Yellow,
		Red,
	};

	inline HealthStage healthStage(double fraction)
	{
		if (fraction > 0.5)
			return HealthStage::Green;
		if (fraction > 0.25)
			return HealthStage::Yellow;
		return HealthStage::Red;
	}

	inline std::wstring trimmed(const std::wstring& text)
	{
		const std::size_t first = text.find_first_not_of(L" \t\r");
		if (first == std::wstring::npos)
			return std::wstring();
		const std::size_t last = text.find_last_not_of(L" \t\r");
		return text.substr(first, last - first + 1);
	}

	inline std::vector<std::wstring> tooltipLines(const std::wstring& text)
	{
		std::vector<std::wstring> lines;
		std::size_t start = 0;
		while (start <= text.size())
		{
			std::size_t end = text.find(L'\n', start);
			if (end == std::wstring::npos)
				end = text.size();
			std::wstring line = trimmed(text.substr(start, end - start));
			if (!line.empty())
				lines.push_back(line);
			start = end + 1;
		}
		return lines;
	}

	template <class Measure>
	std::vector<std::wstring> wrapLine(const std::wstring& line, int maxWidth, Measure measure)
	{
		std::vector<std::wstring> wrapped;
		std::wstring current;
		std::size_t position = 0;
		while (position < line.size())
		{
			const std::size_t wordStart = line.find_first_not_of(L' ', position);
			if (wordStart == std::wstring::npos)
				break;
			std::size_t wordEnd = line.find(L' ', wordStart);
			if (wordEnd == std::wstring::npos)
				wordEnd = line.size();
			const std::wstring word = line.substr(wordStart, wordEnd - wordStart);
			const std::wstring candidate = current.empty() ? word : current + L" " + word;
			if (current.empty() || measure(candidate) <= maxWidth)
				current = candidate;
			else
			{
				wrapped.push_back(current);
				current = word;
			}
			position = wordEnd;
		}
		if (!current.empty())
			wrapped.push_back(current);
		return wrapped;
	}

	inline std::wstring clockText(unsigned int totalSeconds)
	{
		wchar_t buffer[32];
		std::swprintf(buffer, sizeof(buffer) / sizeof(buffer[0]), L"%02u:%02u", totalSeconds / 60, totalSeconds % 60);
		return buffer;
	}

	template <class Key>
	bool reflow(std::vector<Key>& order, const std::vector<Key>& present)
	{
		std::vector<Key> next;
		next.reserve(present.size());
		for (const Key& key : order)
			if (std::find(present.begin(), present.end(), key) != present.end())
				next.push_back(key);
		for (const Key& key : present)
			if (std::find(next.begin(), next.end(), key) == next.end())
				next.push_back(key);
		const bool changed = next != order;
		order = std::move(next);
		return changed;
	}

	struct Candidate
	{
		double distanceSquared;
		unsigned int id;
		bool idle;
	};

	inline std::vector<unsigned int> builderOrder(std::vector<Candidate> candidates)
	{
		std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
			if (a.idle != b.idle)
				return a.idle;
			if (a.distanceSquared != b.distanceSquared)
				return a.distanceSquared < b.distanceSquared;
			return a.id < b.id;
		});
		std::vector<unsigned int> ids;
		ids.reserve(candidates.size());
		for (const Candidate& candidate : candidates)
			ids.push_back(candidate.id);
		return ids;
	}

	inline int nextBuilder(const std::vector<unsigned int>& order, unsigned int previous, bool cycling)
	{
		if (order.empty())
			return -1;
		if (cycling)
		{
			const auto found = std::find(order.begin(), order.end(), previous);
			if (found != order.end())
				return static_cast<int>((found - order.begin() + 1) % static_cast<std::ptrdiff_t>(order.size()));
		}
		return 0;
	}

	inline double pulse(unsigned int elapsedMs, unsigned int periodMs)
	{
		if (periodMs == 0)
			return 1.0;
		const double phase = static_cast<double>(elapsedMs % periodMs) / periodMs;
		return phase < 0.5 ? phase * 2.0 : (1.0 - phase) * 2.0;
	}

	inline double fadeAlpha(unsigned int elapsedMs, unsigned int holdMs, unsigned int fadeMs)
	{
		if (elapsedMs <= holdMs)
			return 1.0;
		if (fadeMs == 0 || elapsedMs >= holdMs + fadeMs)
			return 0.0;
		return 1.0 - static_cast<double>(elapsedMs - holdMs) / fadeMs;
	}
}
