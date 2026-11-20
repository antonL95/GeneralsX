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
