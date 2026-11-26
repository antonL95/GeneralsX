#include "PreRTS.h"
#include "GameLogic/FormationAssistant.h"

#include <algorithm>
#include <string_view>

#include "Common/INI.h"
#include "Common/GameEngine.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/Object.h"
#include "Lib/trig.h"

namespace
{
void skipName(INI* ini, void*, void*, const void*)
{
	ini->getNextToken();
}

void parseUnitDefinition(INI* ini, void* instance, void*, const void*)
{
	static const FieldParse fields[] =
	{
		{ "PreviewObject", INI::parseAsciiString, nullptr, offsetof(FormationUnitDefinition, m_previewObject) },
		{ "ObjectFilter", SpecialPowerObjectFilter::parse, nullptr, offsetof(FormationUnitDefinition, m_filter) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	FormationUnitDefinition definition;
	definition.m_name = ini->getNextToken();
	ini->initFromINI(&definition, fields);
	static_cast<FormationAssistant*>(instance)->m_definitions.push_back(std::move(definition));
}

Int findDefinition(const std::string_view name)
{
	const std::vector<FormationUnitDefinition>& definitions = FormationAssistant::get().m_definitions;
	for (size_t i = 0; i < definitions.size(); ++i)
	{
		if (name == definitions[i].m_name.str())
			return static_cast<Int>(i);
	}
	throw INI_INVALID_DATA;
}

void parseRow(INI* ini, void* instance, void*, const void*)
{
	FormationTemplate::Row row;
	for (const char* token = ini->getNextTokenOrNull(); token; token = ini->getNextTokenOrNull())
	{
		FormationTemplate::Cell cell;
		std::string_view rest = token;
		while (!rest.empty())
		{
			const size_t split = rest.find('|');
			cell.push_back(findDefinition(rest.substr(0, split)));
			rest = split == std::string_view::npos ? std::string_view() : rest.substr(split + 1);
		}
		row.push_back(std::move(cell));
	}
	static_cast<FormationTemplate*>(instance)->m_rows.push_back(std::move(row));
}

void parseRows(INI* ini, void* instance, void*, const void*)
{
	static const FieldParse fields[] =
	{
		{ "Row", parseRow, nullptr, 0 },
		{ nullptr, nullptr, nullptr, 0 }
	};
	ini->initFromINI(instance, fields);
}

void parseTemplate(INI* ini, void* instance, void*, const void*)
{
	static const FieldParse fields[] =
	{
		{ "Rows", parseRows, nullptr, 0 },
		{ nullptr, nullptr, nullptr, 0 }
	};
	FormationTemplate formation;
	formation.m_name = ini->getNextToken();
	ini->initFromINI(&formation, fields);
	static_cast<FormationAssistant*>(instance)->m_templates.push_back(std::move(formation));
}

void parseSelection(INI* ini, void* instance, void*, const void*)
{
	static const FieldParse fields[] =
	{
		{ "MaxDragLength", INI::parseReal, nullptr, offsetof(FormationSelection, m_maxDragLength) },
		{ "MaxUnitsSelected", INI::parseUnsignedInt, nullptr, offsetof(FormationSelection, m_maxUnitsSelected) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	FormationAssistant* assistant = static_cast<FormationAssistant*>(instance);
	const AsciiString name = ini->getNextToken();
	FormationSelection selection;
	ini->initFromINI(&selection, fields);
	for (size_t i = 0; i < assistant->m_templates.size(); ++i)
	{
		if (assistant->m_templates[i].m_name == name)
			selection.m_template = static_cast<Int>(i);
	}
	if (selection.m_template < 0)
		throw INI_INVALID_DATA;
	assistant->m_selections.push_back(selection);
}

void cellSize(const Object& unit, Int& width, Int& depth)
{
	const GeometryInfo& geometry = unit.getGeometryInfo();
	if (unit.isKindOf(KINDOF_HORDE))
	{
		width = static_cast<Int>(geometry.getMinorRadius() * 2.0f + 4.0f);
		depth = static_cast<Int>(geometry.getMajorRadius() * 2.0f + 4.0f);
		return;
	}
	width = depth = static_cast<Int>(geometry.getBoundingCircleRadius() * 2.0f + 1.0f);
}

Bool matches(const FormationUnitDefinition& definition, const Object& unit)
{
	return definition.m_filter.matchesTemplate(*unit.getTemplate());
}

Real distanceSquared(const Coord3D& a, const Coord3D& b)
{
	const Real dx = a.x - b.x;
	const Real dy = a.y - b.y;
	return dx * dx + dy * dy;
}

struct Placement
{
	Object* unit = nullptr;
	Int definition = -1;
	Int width = 0;
	Int depth = 0;
};

std::vector<Placement> fillRow(const std::vector<FormationUnitDefinition>& definitions, const FormationTemplate::Row& row,
	std::vector<Object*>& pool)
{
	const Int columns = static_cast<Int>(row.size());
	std::vector<Placement> filled(columns);
	Int column = (columns - 1) / 2;
	for (Int step = 1; step <= columns; ++step)
	{
		for (const Int index : row[column])
		{
			const auto unit = std::find_if(pool.begin(), pool.end(),
				[&](const Object* candidate) { return matches(definitions[index], *candidate); });
			if (unit == pool.end())
				continue;
			Placement& placement = filled[column];
			placement.unit = *unit;
			placement.definition = index;
			cellSize(**unit, placement.width, placement.depth);
			pool.erase(unit);
			break;
		}
		column += step % 2 ? step : -step;
	}
	return filled;
}

void uncross(std::vector<FormationSlot>& slots)
{
	size_t budget = slots.size() * slots.size();
	for (size_t i = 0; i < slots.size(); ++i)
	{
		for (size_t j = i + 1; j < slots.size(); ++j)
		{
			FormationSlot& first = slots[i];
			FormationSlot& second = slots[j];
			if (first.m_definition != second.m_definition)
				continue;
			const Real kept = distanceSquared(first.m_position, *first.m_unit->getPosition())
				+ distanceSquared(second.m_position, *second.m_unit->getPosition());
			const Real swapped = distanceSquared(first.m_position, *second.m_unit->getPosition())
				+ distanceSquared(second.m_position, *first.m_unit->getPosition());
			if (swapped >= kept)
				continue;
			if (budget == 0)
				return;
			--budget;
			std::swap(first.m_unit, second.m_unit);
			j = i;
		}
	}
}
}

const FormationAssistant& FormationAssistant::get()
{
	return TheGameEngine->formationAssistant();
}

void FormationAssistant::parseDefinition(INI* ini)
{
	static const FieldParse fields[] =
	{
		{ "RowPadding", INI::parseReal, nullptr, offsetof(FormationAssistant, m_rowPadding) },
		{ "ColumnPadding", INI::parseReal, nullptr, offsetof(FormationAssistant, m_columnPadding) },
		{ "FacingArrowHeadTemplate", skipName, nullptr, 0 },
		{ "FacingArrowBodyTemplate", skipName, nullptr, 0 },
		{ "FacingArrowBaseTemplate", skipName, nullptr, 0 },
		{ "ActivationDragDistance", INI::parseReal, nullptr, offsetof(FormationAssistant, m_activationDragDistance) },
		{ "UnitDefinition", parseUnitDefinition, nullptr, 0 },
		{ "FormationTemplate", parseTemplate, nullptr, 0 },
		{ "FormationSelection", parseSelection, nullptr, 0 },
		{ nullptr, nullptr, nullptr, 0 }
	};
	FormationAssistant& assistant = TheGameEngine->formationAssistant();
	assistant = FormationAssistant();
	ini->initFromINI(&assistant, fields);
}

Int FormationAssistant::chooseTemplate(const Real dragLength, const UnsignedInt unitCount) const
{
	if (m_selections.empty())
		return -1;
	if (dragLength >= m_activationDragDistance)
	{
		for (const FormationSelection& selection : m_selections)
		{
			if (selection.m_maxDragLength >= dragLength)
				return selection.m_template;
		}
		return m_selections.back().m_template;
	}
	const FormationSelection* best = nullptr;
	const FormationSelection* largest = &m_selections.front();
	for (const FormationSelection& selection : m_selections)
	{
		if (selection.m_maxUnitsSelected > largest->m_maxUnitsSelected)
			largest = &selection;
		if (selection.m_maxUnitsSelected > unitCount && (!best || selection.m_maxUnitsSelected < best->m_maxUnitsSelected))
			best = &selection;
	}
	return (best ? best : largest)->m_template;
}

std::vector<FormationSlot> FormationAssistant::layout(const Int templateIndex, std::vector<Object*> units,
	const Coord3D& anchor, const Real angle) const
{
	std::vector<FormationSlot> slots;
	if (templateIndex < 0 || templateIndex >= static_cast<Int>(m_templates.size()))
		return slots;

	std::stable_sort(units.begin(), units.end(), [&](const Object* a, const Object* b)
	{
		return distanceSquared(*a->getPosition(), anchor) < distanceSquared(*b->getPosition(), anchor);
	});

	const Real turn = angle + PI / 2.0f;
	const Real cosine = Cos(turn);
	const Real sine = Sin(turn);
	Int rowDistance = 0;
	for (const FormationTemplate::Row& row : m_templates[templateIndex].m_rows)
	{
		const auto fitsRow = [&](const Object* unit)
		{
			return std::any_of(row.begin(), row.end(), [&](const FormationTemplate::Cell& cell)
			{
				return std::any_of(cell.begin(), cell.end(), [&](const Int index) { return matches(m_definitions[index], *unit); });
			});
		};
		while (std::any_of(units.begin(), units.end(), fitsRow))
		{
			const std::vector<Placement> filled = fillRow(m_definitions, row, units);
			std::vector<Int> centers(filled.size(), 0);
			Int cursor = 0;
			Int depth = 0;
			for (size_t i = 0; i < filled.size(); ++i)
			{
				if (!filled[i].unit)
					continue;
				centers[i] = filled[i].width / 2 + cursor;
				cursor = static_cast<Int>(filled[i].width + cursor + m_columnPadding);
				depth = std::max(depth, filled[i].depth);
			}
			if (cursor == 0)
				break;
			const Int halfWidth = static_cast<Int>((cursor - m_columnPadding) * 0.5f);
			for (size_t i = 0; i < filled.size(); ++i)
			{
				if (!filled[i].unit)
					continue;
				const Real across = static_cast<Real>(centers[i] - halfWidth);
				const Real back = static_cast<Real>(rowDistance);
				FormationSlot slot;
				slot.m_unit = filled[i].unit;
				slot.m_definition = filled[i].definition;
				slot.m_position.x = anchor.x + cosine * across - sine * back;
				slot.m_position.y = anchor.y + sine * across + cosine * back;
				slot.m_position.z = anchor.z;
				slots.push_back(slot);
			}
			rowDistance = static_cast<Int>(depth + rowDistance + m_rowPadding);
		}
	}
	uncross(slots);
	return slots;
}
