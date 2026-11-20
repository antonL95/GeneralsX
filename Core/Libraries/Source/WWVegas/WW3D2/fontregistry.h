#pragma once

#include <memory>
#include <string>
#include <vector>

struct InstalledFontFace
{
	std::string SourceName;
	std::string Family;
	std::string Style;
	std::vector<std::string> Names;
	bool IsBold = false;
	std::shared_ptr<const std::vector<unsigned char>> Data;
};

class FontRegistryClass
{
public:
	static const InstalledFontFace *Register_Memory_Font(const char *source_name, std::vector<unsigned char> data);
	static const InstalledFontFace *Find(const char *name, bool is_bold);
	static int Count();
	static const InstalledFontFace *Get(int index);
	static void Reset();

	static void Add_Strict_Family(const std::string &name);
	static bool Is_Strict_Family(const char *name);
	static bool Has_Strict_Families();
	static bool Allows_Bundled_Fallback();

	class ProbeScope
	{
	public:
		ProbeScope();
		~ProbeScope();
		ProbeScope(const ProbeScope &) = delete;
		ProbeScope &operator=(const ProbeScope &) = delete;
	};
};
