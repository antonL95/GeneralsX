#pragma once

#include <vector>

#include "Common/AsciiString.h"
#include "Common/SpecialPowerINI.h"
#include "Lib/BaseType.h"

class INI;
class Object;

struct FormationUnitDefinition
{
	AsciiString m_name;
	AsciiString m_previewObject;
	SpecialPowerObjectFilter m_filter;
};

struct FormationTemplate
{
	using Cell = std::vector<Int>;
	using Row = std::vector<Cell>;

	AsciiString m_name;
	std::vector<Row> m_rows;
};

struct FormationSelection
{
	Int m_template = -1;
	Real m_maxDragLength = 0.0f;
	UnsignedInt m_maxUnitsSelected = 0;
};

struct FormationSlot
{
	Object* m_unit = nullptr;
	Int m_definition = -1;
	Coord3D m_position = {0.0f, 0.0f, 0.0f};
};

struct FormationAssistant
{
	Real m_rowPadding = 0.0f;
	Real m_columnPadding = 0.0f;
	Real m_activationDragDistance = 0.0f;
	std::vector<FormationUnitDefinition> m_definitions;
	std::vector<FormationTemplate> m_templates;
	std::vector<FormationSelection> m_selections;

	static const FormationAssistant& get();
	static void parseDefinition(INI* ini);

	Int chooseTemplate(Real dragLength, UnsignedInt unitCount) const;
	std::vector<FormationSlot> layout(Int templateIndex, std::vector<Object*> units, const Coord3D& anchor, Real angle) const;
};
