#include "Common/INIAudit.h"

#include <cstdlib>

std::FILE* INIAudit::s_report = nullptr;
bool INIAudit::s_checked = false;
std::vector<std::string> INIAudit::s_context;

void INIAudit::ensureOpened()
{
	if (s_checked)
		return;
	s_checked = true;
	const char* path = std::getenv(environmentVariable());
	if (path != nullptr && path[0] != '\0')
		s_report = std::fopen(path, "w");
}

bool INIAudit::isEnabled()
{
	ensureOpened();
	return s_report != nullptr;
}

void INIAudit::open(const char* reportPath)
{
	close();
	s_checked = true;
	s_report = std::fopen(reportPath, "w");
}

void INIAudit::close()
{
	if (s_report != nullptr)
		std::fclose(s_report);
	s_report = nullptr;
	s_checked = false;
	s_context.clear();
}

std::string INIAudit::trim(const std::string& text)
{
	const std::size_t begin = text.find_first_not_of(" \t\r\n");
	if (begin == std::string::npos)
		return std::string();
	const std::size_t end = text.find_last_not_of(" \t\r\n");
	return text.substr(begin, end - begin + 1);
}
std::string INIAudit::sanitize(const std::string& text)
{
	std::string clean;
	clean.reserve(text.size());
	bool lastWasSpace = false;
	for (char c : trim(text))
	{
		const bool space = c == ' ' || c == '\t' || c == '\r' || c == '\n';
		if (space)
		{
			if (!lastWasSpace)
				clean += ' ';
		}
		else
		{
			clean += c;
		}
		lastWasSpace = space;
	}
	return clean;
}

std::string INIAudit::formatRecord(const char* kind, const std::string& context, const std::string& detail, const std::string& file, unsigned line)
{
	return std::string(kind) + '\t' + sanitize(context) + '\t' + sanitize(detail) + '\t' + file + '\t' + std::to_string(line) + '\n';
}

std::string INIAudit::currentContext()
{
	std::string joined;
	for (const std::string& header : s_context)
	{
		if (!joined.empty())
			joined += " > ";
		joined += header;
	}
	return joined;
}

void INIAudit::recordWithContext(const char* kind, const std::string& context, const std::string& detail, const std::string& file, unsigned line)
{
	if (!isEnabled())
		return;
	const std::string text = formatRecord(kind, context, detail, file, line);
	std::fwrite(text.data(), 1, text.size(), s_report);
	std::fflush(s_report);
}

void INIAudit::record(const char* kind, const std::string& detail, const std::string& file, unsigned line)
{
	recordWithContext(kind, currentContext(), detail, file, line);
}

INIAudit::Scope::Scope(const char* headerLine) : m_pushed(false)
{
	if (!isEnabled())
		return;
	s_context.push_back(sanitize(headerLine));
	m_pushed = true;
}

INIAudit::Scope::~Scope()
{
	if (m_pushed && !s_context.empty())
		s_context.pop_back();
}
