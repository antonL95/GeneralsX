#include "PreRTS.h"

#include "GameLogic/AttributeModifierStore.h"

#include <cctype>
#include <cstring>

#include "Common/INI.h"
#include "Common/INIException.h"
#include "Common/ModelState.h"
#include "Common/RotwkDuration.h"
#include "GameClient/FXList.h"
#include "GameLogic/Damage.h"
#include "GameLogic/Object.h"

AttributeModifierStore* TheAttributeModifierStore = nullptr;

namespace
{
using namespace AttributeModifierRules;

std::string lowercaseKey(const AsciiString& name)
{
	std::string key(name.str());
	for (char& c : key)
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	return key;
}

UnsignedInt delayFrames(UnsignedInt msecs)
{
	return RotwkDuration::engineFrames(RotwkDuration::retailFrames(msecs), LOGICFRAMES_PER_SECOND);
}

void parseCategory(INI* ini, void*, void* store, const void*)
{
	const Int index = indexOf(categoryNames(), ini->getNextToken());
	if (index < 0)
		throw INI_INVALID_DATA;
	*static_cast<int*>(store) = index;
}

void parseModifier(INI* ini, void*, void* store, const void*)
{
	Entry entry;
	entry.kind = indexOf(kindNames(), ini->getNextToken());
	if (entry.kind <= KIND_NONE)
		throw INI_INVALID_DATA;
	const char* token = ini->getNextToken();
	const Bool percent = std::strchr(token, '%') != nullptr;
	const Real scanned = INI::scanReal(token);
	entry.value = percent ? scanned / 100.0f : scanned;
	for (const char* name = ini->getNextTokenOrNull(); name; name = ini->getNextTokenOrNull())
	{
		const Int type = DamageTypeFlags::getSingleBitFromName(name);
		if (type >= 0)
			entry.damageTypes.push_back(type);
	}
	static_cast<std::vector<Entry>*>(store)->push_back(std::move(entry));
}

void parseCondition(INI* ini, void*, void* store, const void*)
{
	const Int bit = ModelConditionFlags::getSingleBitFromName(ini->getNextToken());
	if (bit < 0)
		throw INI_INVALID_DATA;
	*static_cast<Int*>(store) = bit;
}

void parseUpgrade(INI* ini, void* instance, void*, const void*)
{
	ModifierListTemplate* list = static_cast<ModifierListTemplate*>(instance);
	for (const char* token = ini->getNextTokenOrNull(); token; token = ini->getNextTokenOrNull())
	{
		if (strnicmp(token, "Delay:", 6) == 0)
		{
			const char* value = token[6] ? token + 6 : ini->getNextToken();
			list->m_upgradeDelayFrames = delayFrames(INI::scanUnsignedInt(value));
			continue;
		}
		if (stricmp(token, "Delay") == 0)
		{
			list->m_upgradeDelayFrames = delayFrames(INI::scanUnsignedInt(ini->getNextToken(ini->getSepsColon())));
			continue;
		}
		list->m_upgradeNames.emplace_back(token);
	}
}

void parseDuration(INI* ini, void*, void* store, const void*)
{
	UnsignedInt frames = 0;
	INI::parseDurationUnsignedInt(ini, nullptr, &frames, nullptr);
	*static_cast<unsigned*>(store) = frames;
}

void parseFlag(INI* ini, void*, void* store, const void*)
{
	*static_cast<bool*>(store) = INI::scanBool(ini->getNextToken());
}

const FieldParse s_listFields[] =
{
	{ "Category", parseCategory, nullptr, offsetof(ModifierListTemplate, m_def.category) },
	{ "Modifier", parseModifier, nullptr, offsetof(ModifierListTemplate, m_def.entries) },
	{ "Duration", parseDuration, nullptr, offsetof(ModifierListTemplate, m_def.durationFrames) },
	{ "ReplaceInCategoryIfLongest", parseFlag, nullptr, offsetof(ModifierListTemplate, m_def.replaceInCategoryIfLongest) },
	{ "IgnoreIfAnticategoryActive", parseFlag, nullptr, offsetof(ModifierListTemplate, m_def.ignoreIfAnticategoryActive) },
	{ "ModelCondition", parseCondition, nullptr, offsetof(ModifierListTemplate, m_modelCondition) },
	{ "ClearModelCondition", parseCondition, nullptr, offsetof(ModifierListTemplate, m_clearModelCondition) },
	{ "FX", INI::parseAsciiString, nullptr, offsetof(ModifierListTemplate, m_fx[0]) },
	{ "FX2", INI::parseAsciiString, nullptr, offsetof(ModifierListTemplate, m_fx[1]) },
	{ "FX3", INI::parseAsciiString, nullptr, offsetof(ModifierListTemplate, m_fx[2]) },
	{ "EndFX", INI::parseAsciiString, nullptr, offsetof(ModifierListTemplate, m_endFx[0]) },
	{ "EndFX2", INI::parseAsciiString, nullptr, offsetof(ModifierListTemplate, m_endFx[1]) },
	{ "EndFX3", INI::parseAsciiString, nullptr, offsetof(ModifierListTemplate, m_endFx[2]) },
	{ "MultiLevelFX", INI::parseBool, nullptr, offsetof(ModifierListTemplate, m_multiLevelFX) },
	{ "Upgrade", parseUpgrade, nullptr, 0 },
	{ nullptr, nullptr, nullptr, 0 }
};

struct StanceEntry
{
	AsciiString modifier;
	AsciiString melee;
};

void parseMelee(INI* ini, void*, void* store, const void*)
{
	*static_cast<AsciiString*>(store) = ini->getNextAsciiString();
	static const FieldParse noFields[] = { { nullptr, nullptr, nullptr, 0 } };
	Int unused = 0;
	ini->initFromINI(&unused, noFields);
}

const FieldParse s_stanceFields[] =
{
	{ "AttributeModifier", INI::parseAsciiString, nullptr, offsetof(StanceEntry, modifier) },
	{ "MeleeBehavior", parseMelee, nullptr, offsetof(StanceEntry, melee) },
	{ nullptr, nullptr, nullptr, 0 }
};

void parseStance(INI* ini, void*, void* store, const void*)
{
	const Int stance = indexOf(stanceNames(), ini->getNextToken());
	if (stance <= STANCE_UNINITIALIZED)
		throw INI_INVALID_DATA;
	StanceEntry entry;
	ini->initFromINI(&entry, s_stanceFields);
	static_cast<StanceTemplateData*>(store)->m_modifiers[stance] = entry.modifier;
}

const FieldParse s_stanceTemplateFields[] =
{
	{ "Stance", parseStance, nullptr, 0 },
	{ nullptr, nullptr, nullptr, 0 }
};
}

void ModifierListTemplate::playFX(const Object* obj, Bool end) const
{
	if (!obj || !TheFXListStore)
		return;
	const AsciiString* names = end ? m_endFx : m_fx;
	for (Int index = 0; index < 3; ++index)
	{
		if (names[index].isEmpty())
			continue;
		if (const FXList* fx = TheFXListStore->findFXList(names[index].str()))
			FXList::doFXObj(fx, obj);
		if (!m_multiLevelFX)
			break;
	}
}

Int AttributeModifierStore::findIndex(const AsciiString& name) const
{
	if (name.isEmpty())
		return -1;
	const auto found = m_index.find(lowercaseKey(name));
	return found == m_index.end() ? -1 : found->second;
}

Int AttributeModifierStore::findIndex(const char* name) const
{
	return name && *name ? findIndex(AsciiString(name)) : -1;
}

const ModifierListTemplate* AttributeModifierStore::get(Int index) const
{
	return index >= 0 && index < count() ? &m_lists[static_cast<size_t>(index)] : nullptr;
}

const ListDef& AttributeModifierStore::def(Int index) const
{
	static const ListDef empty;
	const ModifierListTemplate* list = get(index);
	return list ? list->m_def : empty;
}

const StanceTemplateData* AttributeModifierStore::findStanceTemplate(const AsciiString& name) const
{
	for (const StanceTemplateData& data : m_stances)
	{
		if (data.m_name.compareNoCase(name) == 0)
			return &data;
	}
	return nullptr;
}

ModifierListTemplate& AttributeModifierStore::listFor(const AsciiString& name)
{
	const std::string key = lowercaseKey(name);
	const auto found = m_index.find(key);
	if (found != m_index.end())
	{
		ModifierListTemplate& list = m_lists[static_cast<size_t>(found->second)];
		list = ModifierListTemplate();
		list.m_name = name;
		return list;
	}
	m_index[key] = static_cast<Int>(m_lists.size());
	m_lists.emplace_back();
	m_lists.back().m_name = name;
	return m_lists.back();
}

void AttributeModifierStore::parseModifierListDefinition(INI* ini)
{
	if (ini->skipBlockWithoutOwner(TheAttributeModifierStore, "ModifierList"))
		return;
	const AsciiString name = ini->getNextAsciiString();
	ModifierListTemplate parsed;
	parsed.m_name = name;
	ini->initFromINI(&parsed, s_listFields);
	TheAttributeModifierStore->listFor(name) = std::move(parsed);
}

void AttributeModifierStore::parseStanceTemplateDefinition(INI* ini)
{
	if (ini->skipBlockWithoutOwner(TheAttributeModifierStore, "StanceTemplate"))
		return;
	StanceTemplateData data;
	data.m_name = ini->getNextAsciiString();
	ini->initFromINI(&data, s_stanceTemplateFields);
	for (StanceTemplateData& existing : TheAttributeModifierStore->m_stances)
	{
		if (existing.m_name.compareNoCase(data.m_name) == 0)
		{
			existing = std::move(data);
			return;
		}
	}
	TheAttributeModifierStore->m_stances.push_back(std::move(data));
}
