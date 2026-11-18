#pragma once

#include <cstdio>
#include <string>
#include <vector>

class INIAudit
{
public:
	static const char* environmentVariable() { return "GENERALSX_INI_AUDIT"; }

	static bool isEnabled();
	static void open(const char* reportPath);
	static void close();

	static void record(const char* kind, const std::string& detail, const std::string& file, unsigned line);
	static void recordWithContext(const char* kind, const std::string& context, const std::string& detail, const std::string& file, unsigned line);
	static std::string currentContext();

	static std::string trim(const std::string& text);
	enum { TAB_WIDTH = 4 };

	static std::size_t indentation(const std::string& line);
	static bool closesSkippedBlock(const std::string& line, std::size_t headerIndentation);
	static bool nextLineIsDeeper(const char* buffer, std::size_t next, std::size_t used, std::size_t currentIndentation);
	static std::string formatRecord(const char* kind, const std::string& context, const std::string& detail, const std::string& file, unsigned line);

	class Scope
	{
	public:
		explicit Scope(const char* headerLine);
		~Scope();

		Scope(const Scope&) = delete;
		Scope& operator=(const Scope&) = delete;

	private:
		bool m_pushed;
	};

private:
	static void ensureOpened();
	static std::string sanitize(const std::string& text);

	static std::FILE* s_report;
	static bool s_checked;
	static std::vector<std::string> s_context;
};
