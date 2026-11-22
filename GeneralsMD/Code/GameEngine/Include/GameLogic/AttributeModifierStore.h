#pragma once

#include <map>
#include <string>
#include <vector>

#include "Common/AsciiString.h"
#include "Common/AttributeModifierRules.h"
#include "Common/SubsystemInterface.h"
#include "GameLogic/Module/HordeContain.h"

class INI;
class FXList;
class Object;
class UpgradeTemplate;

struct ModifierListTemplate
{
	AsciiString m_name;
	AttributeModifierRules::ListDef m_def;
	Int m_modelCondition = -1;
	Int m_clearModelCondition = -1;
	AsciiString m_fx[3];
	AsciiString m_endFx[3];
	Bool m_multiLevelFX = FALSE;
	std::vector<AsciiString> m_upgradeNames;
	UnsignedInt m_upgradeDelayFrames = 0;

	void playFX(const Object* obj, Bool end) const;
};

struct StanceTemplateData
{
	AsciiString m_name;
	AsciiString m_modifiers[AttributeModifierRules::STANCE_COUNT];
	HordeContainModuleData::MeleeBehavior m_melee[AttributeModifierRules::STANCE_COUNT];
};

class AttributeModifierStore : public SubsystemInterface
{
public:
	virtual void init() override {}
	virtual void reset() override;
	virtual void update() override {}

	void markDefaults();
	Int findIndex(const AsciiString& name) const;
	Int findIndex(const char* name) const;
	const ModifierListTemplate* get(Int index) const;
	const AttributeModifierRules::ListDef& def(Int index) const;
	Int count() const { return static_cast<Int>(m_lists.size()); }
	const StanceTemplateData* findStanceTemplate(const AsciiString& name) const;

	static void parseModifierListDefinition(INI* ini);
	static void parseStanceTemplateDefinition(INI* ini);

private:
	ModifierListTemplate& listFor(const AsciiString& name);

	std::vector<ModifierListTemplate> m_lists;
	std::map<std::string, Int> m_index;
	std::vector<StanceTemplateData> m_stances;

	Bool m_hasDefaults = FALSE;
	std::vector<ModifierListTemplate> m_defaultLists;
	std::map<std::string, Int> m_defaultIndex;
	std::vector<StanceTemplateData> m_defaultStances;
};

extern AttributeModifierStore* TheAttributeModifierStore;
