#include "fontregistry.h"
#include "fontnames.h"

#include <algorithm>
#include <set>

#if defined(SAGE_USE_FREETYPE) && !defined(_WIN32)
#include <ft2build.h>
#include FT_FREETYPE_H
#endif

namespace
{
	std::vector<std::unique_ptr<InstalledFontFace>> &Faces()
	{
		static std::vector<std::unique_ptr<InstalledFontFace>> faces;
		return faces;
	}

	std::set<std::string> &Strict_Families()
	{
		static std::set<std::string> families;
		return families;
	}

	int &Probe_Depth()
	{
		static int depth = 0;
		return depth;
	}

	void Add_Name(InstalledFontFace &face, const std::string &name)
	{
		if (name.empty())
			return;
		const std::string normalized = FontNames::Normalize(name);
		const bool known = std::any_of(face.Names.begin(), face.Names.end(),
			[&](const std::string &existing) { return FontNames::Normalize(existing) == normalized; });
		if (!known)
			face.Names.push_back(name);
	}
}

const InstalledFontFace *FontRegistryClass::Register_Memory_Font(const char *source_name, std::vector<unsigned char> data)
{
#if defined(SAGE_USE_FREETYPE) && !defined(_WIN32)
	if (source_name == nullptr || data.empty())
		return nullptr;

	FT_Library library = nullptr;
	if (FT_Init_FreeType(&library) != 0)
		return nullptr;

	FT_Face ft_face = nullptr;
	if (FT_New_Memory_Face(library, data.data(), static_cast<FT_Long>(data.size()), 0, &ft_face) != 0 ||
		!FT_IS_SCALABLE(ft_face) || ft_face->family_name == nullptr)
	{
		if (ft_face != nullptr)
			FT_Done_Face(ft_face);
		FT_Done_FreeType(library);
		return nullptr;
	}

	auto face = std::make_unique<InstalledFontFace>();
	face->SourceName = source_name;
	face->Family = ft_face->family_name;
	face->Style = ft_face->style_name != nullptr ? ft_face->style_name : "";
	face->IsBold = (ft_face->style_flags & FT_STYLE_FLAG_BOLD) != 0;
	Add_Name(*face, face->Family);
	if (const char *postscript = FT_Get_Postscript_Name(ft_face))
		Add_Name(*face, postscript);
	Add_Name(*face, FontNames::Stem(source_name));

	FT_Done_Face(ft_face);
	FT_Done_FreeType(library);

	for (const auto &existing : Faces())
	{
		if (FontNames::Same_Family(existing->Family, face->Family) &&
			FontNames::Normalize(existing->Style) == FontNames::Normalize(face->Style))
			return nullptr;
	}

	face->Data = std::make_shared<const std::vector<unsigned char>>(std::move(data));
	for (const std::string &name : face->Names)
		Add_Strict_Family(name);
	Faces().push_back(std::move(face));
	return Faces().back().get();
#else
	(void)source_name;
	(void)data;
	return nullptr;
#endif
}

const InstalledFontFace *FontRegistryClass::Find(const char *name, bool is_bold)
{
	if (name == nullptr || Faces().empty())
		return nullptr;

	std::vector<FontNames::Candidate> candidates;
	candidates.reserve(Faces().size());
	for (const auto &face : Faces())
		candidates.push_back(FontNames::Candidate{face->Names, face->IsBold});

	const int index = FontNames::Find(candidates, name, is_bold);
	return index >= 0 ? Faces()[index].get() : nullptr;
}

int FontRegistryClass::Count()
{
	return static_cast<int>(Faces().size());
}

const InstalledFontFace *FontRegistryClass::Get(int index)
{
	return index >= 0 && index < Count() ? Faces()[index].get() : nullptr;
}

void FontRegistryClass::Reset()
{
	Faces().clear();
	Strict_Families().clear();
}

void FontRegistryClass::Add_Strict_Family(const std::string &name)
{
	Strict_Families().insert(FontNames::Normalize(name));
}

bool FontRegistryClass::Is_Strict_Family(const char *name)
{
	return Has_Strict_Families() &&
		Strict_Families().count(FontNames::Normalize(name)) != 0;
}

bool FontRegistryClass::Has_Strict_Families()
{
	return !Strict_Families().empty();
}

bool FontRegistryClass::Allows_Bundled_Fallback()
{
	return Probe_Depth() == 0;
}

FontRegistryClass::ProbeScope::ProbeScope()
{
	++Probe_Depth();
}

FontRegistryClass::ProbeScope::~ProbeScope()
{
	--Probe_Depth();
}
