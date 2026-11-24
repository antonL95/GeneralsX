#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <string>
#include <vector>

class INIPreprocessor
{
public:
	typedef std::function<bool(const std::string& path, std::string& contents)> FileReader;

	enum { MAX_INCLUDE_DEPTH = 32, MAX_DEFINE_DEPTH = 32 };

	explicit INIPreprocessor(FileReader reader);

	std::string process(const std::string& path, const std::string& contents, bool expandLabeledDefines = true);

	void clearDefines();
	std::size_t getDefineCount() const;
	bool hasDefine(const std::string& name) const { return m_defines.find(name) != m_defines.end(); }

	const std::vector<std::string>& getWarnings() const;
	void clearWarnings();

	static std::string resolveIncludePath(const std::string& includingFile, const std::string& includeName);
	static std::string stripComment(const std::string& line);

private:
	struct Line
	{
		std::string text;
		std::string path;
		int number;
	};

	struct Piece
	{
		std::string text;
		bool isToken;
	};

	void expandIncludes(const std::string& path, const std::string& contents, std::vector<Line>& lines, int depth);
	void registerDefine(const Line& line);
	bool substitute(const std::vector<Piece>& pieces, std::vector<Piece>& out, int depth, bool expandLabeledDefines) const;
	void warn(const Line& line, const std::string& message);
	void warn(const std::string& path, int lineNumber, const std::string& message);

	static std::vector<Piece> split(const std::string& text);
	static std::string join(const std::vector<Piece>& pieces);

	FileReader m_reader;
	std::map<std::string, std::string> m_defines;
	std::vector<std::string> m_warnings;
};
