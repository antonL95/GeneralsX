#include "PreRTS.h"

#include "Common/DataChunk.h"
#include "Common/Errors.h"
#include "Common/AudioSettings.h"
#include "Common/GameAudio.h"
#include "Common/GlobalData.h"
#include "Common/INI.h"
#include "Common/INIAudit.h"
#include "Common/KindOf.h"
#include "Common/LatchRestore.h"
#include "Common/MapReaderWriterInfo.h"
#include "Common/PlayerTemplate.h"
#include "Common/RetailFileLayers.h"
#include "Common/RetailINIStartup.h"
#include "Common/NameKeyGenerator.h"
#include "Common/WellKnownKeys.h"
#include "GameLogic/BfmeScriptRules.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Scripts.h"
#include "GameLogic/SidesList.h"

#include <cstdio>
#include <memory>
#include <limits>
#include <map>
#include <new>
#include <set>
#include <string>
#include <string_view>
#include <tuple>

namespace
{
template<class T> struct DeletePoolObject
{
	void operator()(T *object) const { deleteInstance(object); }
};

void require(Bool valid)
{
	if (!valid)
		throw ERROR_CORRUPT_FILE_FORMAT;
}

void need(DataChunkInput &file, UnsignedInt bytes)
{
	require(file.getChunkDataSizeLeft() >= bytes);
}

AsciiString readString(DataChunkInput &file)
{
	need(file, 2);
	const auto low = static_cast<UnsignedByte>(file.readByte());
	const auto high = static_cast<UnsignedByte>(file.readByte());
	const Int size = low | (high << 8);
	require(size < AsciiString::MAX_LEN);
	need(file, size);
	AsciiString result;
	if (size)
	{
		char *buffer = result.getBufferForRead(size);
		file.readArrayOfBytes(buffer, size);
		buffer[size] = '\0';
	}
	return result;
}

void reportOnce(const char *problem, Bool condition, const AsciiString &name)
{
	static std::set<std::tuple<std::string, Bool, std::string>> reported;
	if (reported.emplace(problem, condition, name.str()).second)
		fprintf(stderr, "BFME %s script %s %s\n", problem, condition ? "condition" : "action", name.str());
}

AsciiString readChild(DataChunkInput &file, DataChunkVersionType &version)
{
	constexpr UnsignedInt headerSize = 10;
	const UnsignedInt remaining = file.getChunkDataSizeLeft();
	require(remaining >= headerSize);
	const AsciiString label = file.openDataChunk(&version);
	require(label.isNotEmpty() && file.getChunkDataSize() <= remaining - headerSize);
	return label;
}
}

Bool BfmeMapScripts::parsePlayerLists(DataChunkInput &file, DataChunkInfo *info, void *)
{
	SidesList &sides = *TheSidesList;
	if (!sides.usesRotwkRoster())
		return true;
	require(info->version == 1 || info->version == 5 || info->version == 6);
	require(info->dataSize >= 0);

	Int index = 0;
	while (!file.atEndOfChunk())
	{
		DataChunkVersionType version;
		const AsciiString label = readChild(file, version);
		require(label == "ScriptList" && version == 1 && index < sides.getNumSides());
		SidesInfo &side = *sides.getSideInfo(index++);
		require(side.getScriptList() == nullptr);

		std::unique_ptr<ScriptList, DeletePoolObject<ScriptList>> list(newInstance(ScriptList));
		if (sides.m_scriptDump)
			sides.m_scriptDump("list", AsciiString::TheEmptyString, 0);
		ScriptGroup *groupTail = nullptr;
		readContents(file, sides, *list, nullptr, groupTail, 1);
		side.setScriptList(list.release());
		file.closeDataChunk();
	}
	return index == sides.getNumSides();
}

void BfmeMapScripts::readContents(DataChunkInput &file, SidesList &sides, ScriptList &list,
	ScriptGroup *parent, ScriptGroup *&groupTail, Int depth)
{
	require(depth <= 64);
	Script **firstScript = parent ? &parent->m_firstScript : &list.m_firstScript;
	Script *scriptTail = nullptr;
	while (!file.atEndOfChunk())
	{
		DataChunkVersionType version;
		const AsciiString label = readChild(file, version);
		const Int order = ++list.m_bfmeNextOrder;
		if (label == "Script")
		{
			require(version >= 1 && version <= 4);
			Script *script = newInstance(Script);
			if (scriptTail)
				scriptTail->setNextScript(script);
			else
				*firstScript = script;
			scriptTail = script;
			script->m_bfmeOrder = order;
			readScript(file, sides, *script, version, depth);
		}
		else
		{
			require(label == "ScriptGroup" && version >= 1 && version <= 3);
			ScriptGroup *group = newInstance(ScriptGroup);
			if (groupTail)
				groupTail->setNextGroup(group);
			else
				list.m_firstGroup = group;
			groupTail = group;
			group->m_bfmeDepth = depth;
			group->m_bfmeOrder = order;
			group->m_groupName = readString(file);
			need(file, version >= 2 ? 2 : 1);
			group->m_isGroupActive = file.readByte() != 0;
			if (version >= 2)
				group->m_isGroupSubroutine = file.readByte() != 0;
			if (sides.m_scriptDump)
				sides.m_scriptDump("group", group->m_groupName, depth);
			readContents(file, sides, list, group, groupTail, depth + 1);
		}
		file.closeDataChunk();
	}
}

void BfmeMapScripts::readScript(DataChunkInput &file, SidesList &sides, Script &script, Int version, Int depth)
{
	script.m_scriptName = readString(file);
	script.m_comment = readString(file);
	script.m_conditionComment = readString(file);
	script.m_actionComment = readString(file);
	need(file, 6 + (version >= 2 ? 4 : 0));
	script.m_isActive = file.readByte() != 0;
	script.m_isOneShot = file.readByte() != 0;
	script.m_easy = file.readByte() != 0;
	script.m_normal = file.readByte() != 0;
	script.m_hard = file.readByte() != 0;
	script.m_isSubroutine = file.readByte() != 0;
	if (version >= 2)
	{
		script.m_delayEvaluationSeconds = file.readInt();
		require(script.m_delayEvaluationSeconds >= 0 &&
			script.m_delayEvaluationSeconds <= std::numeric_limits<Int>::max() / LOGICFRAMES_PER_SECOND);
	}
	if (version >= 3)
	{
		need(file, 7);
		script.m_bfme.sequential = file.readByte() != 0;
		script.m_bfme.loop = file.readByte() != 0;
		script.m_bfme.loopCount = file.readInt();
		script.m_bfme.targetType = file.readByte();
		script.m_bfme.target = readString(file);
	}
	if (version >= 4)
		script.m_bfme.filter = readString(file);
	if (sides.m_scriptDump)
		sides.m_scriptDump("script", script.m_scriptName, depth);

	OrCondition *orTail = nullptr;
	ScriptAction *actionTail = nullptr;
	ScriptAction *falseTail = nullptr;
	while (!file.atEndOfChunk())
	{
		DataChunkVersionType childVersion;
		const AsciiString label = readChild(file, childVersion);
		if (label == "OrCondition")
		{
			require(childVersion == 1);
			OrCondition *clause = newInstance(OrCondition);
			if (orTail)
				orTail->setNextOrCondition(clause);
			else
				script.m_condition = clause;
			orTail = clause;
			if (sides.m_scriptDump)
				sides.m_scriptDump("or", AsciiString::TheEmptyString, depth + 1);
			readOr(file, sides, *clause, depth + 2);
		}
		else
		{
			const Bool isFalse = label == "ScriptActionFalse";
			require((isFalse || label == "ScriptAction") && childVersion >= 1 && childVersion <= 3);
			ScriptAction *action = newInstance(ScriptAction);
			ScriptAction *&tail = isFalse ? falseTail : actionTail;
			if (tail)
				tail->setNextAction(action);
			else
				(isFalse ? script.m_actionFalse : script.m_action) = action;
			tail = action;
			readAction(file, *action);
			if (sides.m_scriptDump)
				sides.m_scriptDump(isFalse ? "false-action" : "action", action->m_bfme.name, depth + 1);
		}
		file.closeDataChunk();
	}
}

void BfmeMapScripts::readAction(DataChunkInput &file, ScriptAction &action)
{
	action.m_actionType = static_cast<ScriptAction::ScriptActionType>(
		readOpcode(file, action.m_bfme, action.m_parms, action.m_numParms, file.getChunkVersion(), false));
}

void BfmeMapScripts::readOr(DataChunkInput &file, SidesList &sides, OrCondition &clause, Int depth)
{
	Condition *tail = nullptr;
	while (!file.atEndOfChunk())
	{
		DataChunkVersionType version;
		const AsciiString label = readChild(file, version);
		require(label == "Condition" && version >= 1 && version <= 6);
		Condition *condition = newInstance(Condition);
		if (tail)
			tail->setNextCondition(condition);
		else
			clause.setFirstAndCondition(condition);
		tail = condition;
		condition->m_conditionType = static_cast<Condition::ConditionType>(
			readOpcode(file, condition->m_bfme, condition->m_parms, condition->m_numParms, version, true));
		if (sides.m_scriptDump)
			sides.m_scriptDump("condition", condition->m_bfme.name, depth);
		file.closeDataChunk();
	}
}

Int BfmeMapScripts::readOpcode(DataChunkInput &file, BfmeScriptOpcode &opcode, Parameter **parameters,
	Int &count, Int version, Bool condition)
{
	opcode.fromMap = true;
	const Bool named = version >= (condition ? 4 : 2);
	need(file, named ? 12 : 8);
	const Int authoredType = file.readInt();
	const NameKeyType key = named ? file.readNameKey() : NAMEKEY_INVALID;
	if (named)
	{
		opcode.name = KEYNAME(key);
		require(opcode.name.isNotEmpty());
	}
	const Int numParameters = file.readInt();
	require(numParameters >= 0 && numParameters <= MAX_PARMS);
	need(file, 14 * numParameters);
	Int validPrefix = numParameters;
	while (count < numParameters)
	{
		const Int index = count++;
		if (!readParameter(file, parameters[index]) && index < validPrefix)
			validPrefix = index;
	}
	if (version >= (condition ? 5 : 3))
	{
		need(file, condition ? 8 : 4);
		opcode.enabled = file.readInt() != 0;
		if (condition)
			opcode.inverted = file.readInt() != 0;
	}
	require(file.atEndOfChunk());

	opcode.supported = false;
	if (TheScriptEngine)
	{
#if RTS_GENERALS
		const Int actionCount = ScriptAction::NUM_ITEMS;
#else
		const Int actionCount = ScriptEngine::getActionTemplateCount();
#endif
		const Int limit = condition ? static_cast<Int>(Condition::NUM_ITEMS) : actionCount;
#if !RTS_GENERALS
		Int mappedType = authoredType;
		if (RetailFileLayers::enabled() && !named && !condition && authoredType >= ScriptAction::FIRST_ROTWK)
			mappedType = ScriptEngine::findRotwkActionByRetailIndex(authoredType);
		if (!named && condition)
		{
			const Int retailType = ScriptEngine::findRotwkConditionByRetailIndex(authoredType);
			if (retailType >= 0)
				mappedType = retailType;
		}
#endif
		for (Int i = 0; i < limit; ++i)
		{
			const Template *definition = condition
				? static_cast<const Template *>(TheScriptEngine->getConditionTemplate(i))
				: static_cast<const Template *>(TheScriptEngine->getActionTemplate(i));
#if RTS_GENERALS
			if (named || i != authoredType)
#else
			if (named ? definition->m_internalNameKey != key : i != mappedType)
#endif
				continue;

			const Int expected = definition->getNumParameters();
#if !RTS_GENERALS
			if (!condition && named && i == ScriptAction::SHOW_MILITARY_CAPTION && count == 2 &&
				parameters[0]->getParameterType() == Parameter::LOCALIZED_TEXT &&
				parameters[1]->getParameterType() == Parameter::REAL)
			{
				parameters[0]->m_paramType = Parameter::TEXT_STRING;
				parameters[1]->m_paramType = Parameter::INT;
				parameters[1]->m_int = BfmeScriptRules::militaryCaptionMilliseconds(parameters[1]->m_real);
			}
#endif
			const Bool ignoreTrailing = !condition && named && i == ScriptAction::TEAM_FOLLOW_WAYPOINTS &&
				count == expected + 1;
			opcode.supported = validPrefix >= expected && (count == expected || ignoreTrailing);
			for (Int p = 0; opcode.supported && p < expected; ++p)
				opcode.supported = parameters[p]->getParameterType() == definition->getParameterType(p);
			if (opcode.supported)
			{
				if (ignoreTrailing)
				{
					deleteInstance(parameters[--count]);
					parameters[count] = nullptr;
				}
#if !RTS_GENERALS
				const Int firstRotwk = condition ? static_cast<Int>(Condition::FIRST_ROTWK) : static_cast<Int>(ScriptAction::FIRST_ROTWK);
				if (i >= firstRotwk && !ScriptEngine::isRotwkImplemented(condition, i))
					reportOnce("unimplemented", condition, definition->m_internalName);
#endif
				return i;
			}
			if (named)
				reportOnce("unsupported signature of", condition, opcode.name);
			return condition ? static_cast<Int>(Condition::CONDITION_FALSE) : static_cast<Int>(ScriptAction::NO_OP);
		}
		if (opcode.name.isNotEmpty())
			reportOnce("unknown", condition, opcode.name);
	}
	return condition ? static_cast<Int>(Condition::CONDITION_FALSE) : static_cast<Int>(ScriptAction::NO_OP);
}

Bool BfmeMapScripts::readParameter(DataChunkInput &file, Parameter *&parameter)
{
	parameter = nullptr;
	need(file, 4);
	const Int wireType = file.readInt();
#if RTS_GENERALS
	const Int mapped = wireType > 29 ? wireType - 1 : wireType;
	const auto type = wireType == 29 || mapped < 0 || mapped >= Parameter::NUM_ITEMS
		? Parameter::NUM_ITEMS : static_cast<Parameter::ParameterType>(mapped);
#else
	const Parameter::ParameterType type = Parameter::fromBfmeWireType(wireType);
#endif
	parameter = newInstance(Parameter)(type);
	parameter->m_initialized = true;
	parameter->m_bfmeType = wireType;
	if (type == Parameter::COORD3D)
	{
		need(file, 12);
		parameter->m_coord.x = file.readReal();
		parameter->m_coord.y = file.readReal();
		parameter->m_coord.z = file.readReal();
	}
	else
	{
		need(file, 8);
		parameter->m_int = file.readInt();
		parameter->m_real = file.readReal();
		parameter->m_string = readString(file);
	}
	if (type == Parameter::COUNTER || type == Parameter::FLAG)
		parameter->m_int = 0;
	if (type == Parameter::OBJECT_STATUS)
	{
		for (Int i = 0; i < OBJECT_STATUS_COUNT; ++i)
		{
			if (parameter->m_string.compareNoCase(ObjectStatusMaskType::getBitNames()[i]) == 0)
			{
				parameter->setStatus(MAKE_OBJECT_STATUS_MASK(i));
				return true;
			}
		}
		return false;
	}
	if (type == Parameter::KIND_OF_PARAM)
	{
		if (parameter->m_string.isEmpty())
			return parameter->m_int >= 0 && parameter->m_int < KINDOF_COUNT;
		const char *const *names = KindOfMaskType::getBitNames();
		for (Int i = 0; names[i]; ++i)
		{
			if (parameter->m_string.compareNoCase(names[i]) == 0)
			{
				parameter->m_int = i;
				return true;
			}
		}
		return false;
	}
	return true;
}

Bool BfmeMapScripts::parseLibraryLists(DataChunkInput &file, DataChunkInfo *info, void *)
{
	SidesList &sides = *TheSidesList;
	if (!sides.usesRotwkRoster())
		return true;
	require(info->dataSize >= 0);
	Int index = 0;
	while (!file.atEndOfChunk())
	{
		DataChunkVersionType version;
		const AsciiString label = readChild(file, version);
		require(label == "LibraryMaps" && version == 1 && index < sides.getNumSides());
		need(file, 4);
		const Int count = file.readInt();
		require(count >= 0 && static_cast<UnsignedInt>(count) <= file.getChunkDataSizeLeft() / 2);
		auto &list = sides.m_scriptLibraries.emplace_back();
		list.owner = sides.getSideInfo(index++)->getDict()->getAsciiString(TheKey_playerName);
		for (Int i = 0; i < count; ++i)
			list.paths.push_back(readString(file));
		require(file.atEndOfChunk());
		file.closeDataChunk();
	}
	return true;
}

void BfmeMapScripts::append(ScriptList &target, ScriptList &source)
{
	for (Script *script = source.m_firstScript; script; script = script->getNext())
		script->m_bfmeOrder += target.m_bfmeNextOrder;
	for (ScriptGroup *group = source.m_firstGroup; group; group = group->getNext())
	{
		group->m_bfmeOrder += target.m_bfmeNextOrder;
		for (Script *script = group->getScript(); script; script = script->getNext())
			script->m_bfmeOrder += target.m_bfmeNextOrder;
	}

	Script *scriptTail = target.m_firstScript;
	if (!scriptTail)
		target.m_firstScript = source.m_firstScript;
	else
	{
		while (scriptTail->getNext())
			scriptTail = scriptTail->getNext();
		scriptTail->setNextScript(source.m_firstScript);
	}
	ScriptGroup *groupTail = target.m_firstGroup;
	if (!groupTail)
		target.m_firstGroup = source.m_firstGroup;
	else
	{
		while (groupTail->getNext())
			groupTail = groupTail->getNext();
		groupTail->setNextGroup(source.m_firstGroup);
	}
	target.m_bfmeNextOrder += source.m_bfmeNextOrder;
	source.m_firstScript = nullptr;
	source.m_firstGroup = nullptr;
	source.m_bfmeNextOrder = 0;
}

ScriptAction *BfmeMapScripts::copyAction(const ScriptAction &head, const AsciiString *qualifier,
	const AsciiString *playerTemplate, const AsciiString *newPlayer)
{
	std::unique_ptr<ScriptAction, DeletePoolObject<ScriptAction>> first;
	ScriptAction *tail = nullptr;
	for (auto *source = &head; source; source = source->m_nextAction)
	{
		ScriptAction *copy = newInstance(ScriptAction);
		if (tail)
			tail->setNextAction(copy);
		else
			first.reset(copy);
		tail = copy;
		copy->m_actionType = source->m_actionType;
		copy->m_bfme = source->m_bfme;
		for (; copy->m_numParms < source->m_numParms; ++copy->m_numParms)
		{
			const Int i = copy->m_numParms;
			copy->m_parms[i] = newInstance(Parameter)(source->m_parms[i]->getParameterType());
			*copy->m_parms[i] = *source->m_parms[i];
			if (qualifier)
				copy->m_parms[i]->qualify(*qualifier, *playerTemplate, *newPlayer);
		}
	}
	return first.release();
}

Condition *BfmeMapScripts::copyCondition(const Condition &head, const AsciiString *qualifier,
	const AsciiString *playerTemplate, const AsciiString *newPlayer)
{
	std::unique_ptr<Condition, DeletePoolObject<Condition>> first;
	Condition *tail = nullptr;
	for (auto *source = &head; source; source = source->m_nextAndCondition)
	{
		Condition *copy = newInstance(Condition);
		if (tail)
			tail->setNextCondition(copy);
		else
			first.reset(copy);
		tail = copy;
		copy->m_conditionType = source->m_conditionType;
		copy->m_bfme = source->m_bfme;
		for (; copy->m_numParms < source->m_numParms; ++copy->m_numParms)
		{
			const Int i = copy->m_numParms;
			copy->m_parms[i] = newInstance(Parameter)(source->m_parms[i]->getParameterType());
			*copy->m_parms[i] = *source->m_parms[i];
			if (qualifier)
				copy->m_parms[i]->qualify(*qualifier, *playerTemplate, *newPlayer);
		}
	}
	return first.release();
}

namespace
{
AsciiString prefixed(const char *prefix, const AsciiString &suffix)
{
	AsciiString result(prefix);
	result.concat(suffix);
	return result;
}

std::string normalized(std::string path)
{
	for (char &ch : path)
	{
		if (ch == '\\')
			ch = '/';
		else if (ch >= 'A' && ch <= 'Z')
			ch += 'a' - 'A';
	}
	return path;
}

std::string libraryKey(const std::string &path)
{
	std::string key;
	const std::string_view input(path);
	for (size_t start = 0; start < input.size();)
	{
		const size_t separator = input.find('/', start);
		const auto component = input.substr(start, separator - start);
		if (!component.empty() && component != ".")
		{
			if (!key.empty())
				key += '/';
			key += component;
		}
		if (separator == std::string_view::npos)
			break;
		start = separator + 1;
	}
	return key;
}

void reportInventory(const std::string &path, SidesList &sides)
{
	if (!std::getenv("GENERALSX_MAP_SCRIPT_DUMP"))
		return;

	std::map<std::pair<std::string, std::string>, Int> names;
	Int scripts = 0;
	Int groups = 0;
	const auto countScripts = [&](Script *head) {
		for (Script *script = head; script; script = script->getNext())
		{
			++scripts;
			for (OrCondition *clause = script->getOrCondition(); clause; clause = clause->getNextOrCondition())
				for (Condition *condition = clause->getFirstAndCondition(); condition; condition = condition->getNext())
					++names[{"condition", condition->getBfmeOpcode().name.str()}];
			for (ScriptAction *action = script->getAction(); action; action = action->getNext())
				++names[{"action", action->getBfmeOpcode().name.str()}];
			for (ScriptAction *action = script->getFalseAction(); action; action = action->getNext())
				++names[{"action", action->getBfmeOpcode().name.str()}];
		}
	};
	for (Int i = 0; i < sides.getNumSides(); ++i)
	{
		ScriptList *list = sides.getSideInfo(i)->getScriptList();
		if (!list)
			continue;
		countScripts(list->getScript());
		for (ScriptGroup *group = list->getScriptGroup(); group; group = group->getNext())
		{
			++groups;
			countScripts(group->getScript());
		}
	}
	fprintf(stderr, "BFME_SCRIPT_SOURCE\t%s\t%d\t%d\n", path.c_str(), scripts, groups);
	for (const auto &[name, count] : names)
		fprintf(stderr, "BFME_SCRIPT_NAME\t%s\t%s\t%s\t%d\n", path.c_str(), name.first.c_str(), name.second.c_str(), count);
}

void mergeTeams(SidesList &target, SidesList &source, const AsciiString &from, const AsciiString &to, Bool skirmish)
{
	const AsciiString fromDefault = prefixed("team", from);
	for (Int i = 0; i < source.getNumTeams(); ++i)
	{
		Dict team = *source.getTeamInfo(i)->getDict();
		if (team.getAsciiString(TheKey_teamOwner) != from)
			continue;
		const AsciiString name = team.getAsciiString(TheKey_teamName);
		if (name == fromDefault || name.isEmpty())
			continue;
		team.setAsciiString(TheKey_teamOwner, to);
		if (skirmish)
			target.addSkirmishTeam(&team);
		else if (!target.findTeamInfo(name))
			target.addTeam(&team);
	}
}
}

class BfmeMapScripts::Libraries
{
public:
	explicit Libraries(const std::string &mapPath)
	{
		m_loaded.try_emplace(libraryKey(mapPath));
	}

	template<class Apply>
	void walk(const std::vector<AsciiString> &roots, std::set<std::string> &seen, const Apply &apply)
	{
		for (const AsciiString &path : roots)
		{
			const std::string normalizedPath = normalized(path.str());
			if (normalizedPath.empty())
				continue;
			const std::string key = libraryKey(normalizedPath);
			if (key.empty() || normalizedPath.front() == '/' || key.find(':') != std::string::npos ||
				key == ".." || key.compare(0, 3, "../") == 0 ||
				key.find("/../") != std::string::npos || key.ends_with("/.."))
			{
				if (INIAudit::isEnabled())
					INIAudit::record("map-library", "invalid path", path.str(), 0);
				else
					fprintf(stderr, "BFME invalid script library path %s\n", path.str());
				continue;
			}
			if (!seen.insert(key).second)
				continue;

			SidesList *library = read(path, key);
			if (!library)
				continue;
			apply(*library, *library->getSideInfo(1));
			if (library->m_scriptLibraries.size() >= 2)
				walk(library->m_scriptLibraries[1].paths, seen, apply);
		}
	}

private:
	static constexpr size_t MAX_LIBRARY_FILES = 512;
	std::map<std::string, std::unique_ptr<SidesList>> m_loaded;
	Bool m_reportedLimit = false;

	static void reportFailure(const AsciiString &path, const char *reason)
	{
		if (INIAudit::isEnabled())
			INIAudit::record("map-library", reason, path.str(), 0);
		else
		{
			fprintf(stderr, "BFME script library skipped (%s): %s\n", reason, path.str());
			fflush(stderr);
		}
	}

	SidesList *read(const AsciiString &path, const std::string &key)
	{
		auto found = m_loaded.find(key);
		if (found != m_loaded.end())
			return found->second.get();
		if (m_loaded.size() > MAX_LIBRARY_FILES)
		{
			if (!m_reportedLimit)
			{
				reportFailure(path, "512 library file limit");
				m_reportedLimit = true;
			}
			return nullptr;
		}
		auto &entry = m_loaded[key];
		try
		{
			CachedFileInputStream stream;
			if (!stream.open(path))
			{
				reportFailure(path, "missing");
				return nullptr;
			}
			auto sides = std::make_unique<SidesList>();
			const LatchRestore<SidesList *> setSides(TheSidesList, sides.get());
			DataChunkInput file(&stream);
			const AsciiString &root = AsciiString::TheEmptyString;
			file.registerVersionedParser("SidesList", root, SidesList::ParseSidesDataChunk, SidesList::MAX_MAP_SIDES_VERSION, true);
			file.registerVersionedParser("Teams", root, SidesList::ParseMapTeamsDataChunk, 1, true);
			file.registerVersionedParser("PlayerScriptsList", root, parsePlayerLists, 6, true);
			file.registerVersionedParser("LibraryMapLists", root, parseLibraryLists, 1, true);
			require(file.parse() && !file.getUnsupportedRequiredChunk() && sides->usesRotwkRoster() &&
				sides->getNumSides() >= 2);
			reportInventory(key, *sides);
			entry = std::move(sides);
		}
		catch (ErrorCode)
		{
			reportFailure(path, "corrupt data");
		}
		catch (const std::bad_array_new_length &)
		{
			reportFailure(path, "corrupt data");
		}
		return entry.get();
	}
};

void BfmeMapScripts::parsePlayerAIType(INI *ini)
{
	const std::string name = normalized(ini->getNextToken());
	static const FieldParse fields[] = {
		{ "LibraryMap", INI::parseQuotedAsciiString, nullptr, 0 },
		{ nullptr, nullptr, nullptr, 0 }
	};
	AsciiString path;
	ini->initFromINI(&path, fields);
	if (TheSidesList)
		TheSidesList->m_playerAILibraries[name] = path;
}

void BfmeMapScripts::mergeSide(SidesInfo &target, SidesInfo &source, const AsciiString &qualifier)
{
	if (!source.getScriptList() || !source.getScriptList()->hasBfmeOrder())
		return;

	const AsciiString to = target.getDict()->getAsciiString(TheKey_playerName);
	AsciiString from = source.getDict()->getAsciiString(TheKey_playerName);
	from.concat(qualifier);
	std::unique_ptr<ScriptList, DeletePoolObject<ScriptList>> copy(
		from == to && qualifier.isEmpty()
			? source.getScriptList()->duplicate()
			: source.getScriptList()->duplicateAndQualify(qualifier, from, to));
	if (target.getScriptList())
		append(*target.getScriptList(), *copy);
	else
		target.setScriptList(copy.release());
}

void BfmeMapScripts::attachPlayerTemplate(SidesList &sides, SidesInfo &source, SidesInfo &target, Int start)
{
	AsciiString qualifier;
	qualifier.format("%d", start);
	mergeSide(target, source, qualifier);
	const AsciiString from = source.getDict()->getAsciiString(TheKey_playerName);
	const AsciiString to = target.getDict()->getAsciiString(TheKey_playerName);
	const NameKeyType hooks[] = {
		TheKey_teamProductionCondition, TheKey_teamOnCreateScript, TheKey_teamOnIdleScript,
		TheKey_teamOnUnitDestroyedScript, TheKey_teamOnDestroyedScript, TheKey_teamEnemySightedScript,
		TheKey_teamAllClearScript
	};
	const AsciiString genericKeyName = KEYNAME(TheKey_teamGenericScriptHook);
	const std::string_view genericPrefix(genericKeyName.str());
	for (Int i = 0; i < sides.getNumSkirmishTeams(); ++i)
	{
		Dict team = *sides.getSkirmishTeamInfo(i)->getDict();
		if (team.getAsciiString(TheKey_teamOwner) != from)
			continue;
		AsciiString name = team.getAsciiString(TheKey_teamName);
		if (name == prefixed("team", from))
			continue;
		name.concat(qualifier);
		if (sides.findTeamInfo(name))
			continue;
		team.setAsciiString(TheKey_teamName, name);
		team.setAsciiString(TheKey_teamOwner, to);
		const auto qualifyHook = [&](NameKeyType key) {
			Bool exists;
			AsciiString hook = team.getAsciiString(key, &exists);
			if (exists && hook.isNotEmpty())
			{
				hook.concat(qualifier);
				team.setAsciiString(key, hook);
			}
		};
		for (NameKeyType key : hooks)
			qualifyHook(key);
		for (Int field = 0; field < team.getPairCount(); ++field)
		{
			const NameKeyType key = team.getNthKey(field);
			const AsciiString fieldName = KEYNAME(key);
			const std::string_view keyName(fieldName.str());
			if (keyName.starts_with(genericPrefix) && keyName.size() > genericPrefix.size() &&
				keyName.find_first_not_of("0123456789", genericPrefix.size()) == std::string_view::npos)
				qualifyHook(key);
		}
		sides.addTeam(&team);
	}
}

void BfmeMapScripts::loadLibraries(SidesList &sides, Bool multiplayer)
{
	if (!sides.usesRotwkRoster())
		return;
	const std::string mapPath = normalized(TheGlobalData->m_mapName.str());
	reportInventory(mapPath, sides);
	for (Int i = 0; multiplayer && i < sides.getNumSides(); ++i)
	{
		SidesInfo &slot = *sides.getSideInfo(i);
		const AsciiString name = slot.getDict()->getAsciiString(TheKey_playerName);
		if (SidesInfo *authored = sides.findSkirmishSideInfo(name))
			mergeSide(slot, *authored, AsciiString::TheEmptyString);
	}
	Libraries libraries(mapPath);
	SidesInfo *neutral = sides.findSideInfo(AsciiString::TheEmptyString);

	for (const auto &list : sides.m_scriptLibraries)
	{
		SidesInfo *target = sides.findSideInfo(list.owner);
		const Bool skirmish = target == nullptr;
		if (skirmish)
			target = sides.findSkirmishSideInfo(list.owner);
		if (!target || list.paths.empty())
			continue;

		std::set<std::string> seen;
		libraries.walk(list.paths, seen, [&](SidesList &library, SidesInfo &from) {
			mergeSide(*target, from, AsciiString::TheEmptyString);
			mergeTeams(sides, library, from.getDict()->getAsciiString(TheKey_playerName), list.owner, skirmish);
		});
	}

	if (neutral && TheAudio && (sides.m_mapSidesVersion < 6 || sides.m_mapSideFlag))
	{
#if !RTS_GENERALS
		const ScriptList *neutralScripts = neutral->getScriptList();
		sides.m_musicOrderBegin = neutralScripts ? neutralScripts->m_bfmeNextOrder + 1 : 1;
		sides.m_retailMusicSide.getDict()->setAsciiString(TheKey_playerName, AsciiString::TheEmptyString);
#endif
		std::set<std::string> musicSeen;
		libraries.walk({TheAudio->getAudioSettings()->m_musicScriptLibraryName}, musicSeen, [&](SidesList &library, SidesInfo &from) {
			mergeSide(*neutral, from, AsciiString::TheEmptyString);
#if !RTS_GENERALS
			mergeSide(sides.m_retailMusicSide, from, AsciiString::TheEmptyString);
#endif
			mergeTeams(sides, library, from.getDict()->getAsciiString(TheKey_playerName), AsciiString::TheEmptyString, false);
		});
#if !RTS_GENERALS
		sides.m_musicCompatibilityList = neutral->getScriptList();
		sides.m_musicOrderEnd = sides.m_musicCompatibilityList ? sides.m_musicCompatibilityList->m_bfmeNextOrder + 1 : 1;
#endif
	}
#if !RTS_GENERALS
	if (!multiplayer || !RetailFileLayers::enabled() || INIAudit::isEnabled())
		return;

	if (!RetailINIStartup::hasSubsystem("ThePlayerAITypeSet"))
	{
		INI ini;
		sides.m_playerAILibraries.clear();
		ini.loadFileDirectory("Data\\INI\\PlayerAITypes", INI_LOAD_OVERWRITE, nullptr, INI::LoadFlags_SearchSubDirs);
	}
	std::map<std::string, std::set<std::string>> seenByTemplate;
	for (Int i = 0; i < sides.getNumSides(); ++i)
	{
		SidesInfo &slot = *sides.getSideInfo(i);
		const Dict &dict = *slot.getDict();
		Bool isSlot = false;
		dict.getInt(TheKey_multiplayerStartIndex, &isSlot);
		if (!isSlot)
			continue;

		const PlayerTemplate *faction = ThePlayerTemplateStore->findPlayerTemplate(NAMEKEY(dict.getAsciiString(TheKey_playerFaction)));
		if (!faction || faction->isObserver() || dict.getAsciiString(TheKey_playerName) == "ReplayObserver")
			continue;

		const Bool human = dict.getBool(TheKey_playerIsHuman);
		const AsciiString aiType = human ? AsciiString("Multiplayer_Human") : faction->getDefaultPlayerAIType();
		const auto config = sides.m_playerAILibraries.find(normalized(aiType.str()));
		if (config == sides.m_playerAILibraries.end() || config->second.isEmpty())
			continue;

		const AsciiString templateName = prefixed("Skirmish", human ? AsciiString("Human") : faction->getSide());
		SidesInfo *target = sides.findSkirmishSideInfo(templateName);
		if (!target)
		{
			Dict side;
			side.setAsciiString(TheKey_playerName, templateName);
			side.setAsciiString(TheKey_playerFaction, dict.getAsciiString(TheKey_playerFaction));
			sides.addSkirmishSide(&side);
			target = sides.findSkirmishSideInfo(templateName);
		}
		if (!target)
			continue;

		libraries.walk({config->second}, seenByTemplate[templateName.str()], [&](SidesList &library, SidesInfo &from) {
			mergeSide(*target, from, AsciiString::TheEmptyString);
			mergeTeams(sides, library, from.getDict()->getAsciiString(TheKey_playerName), templateName, true);
		});
	}
#endif
}

void BfmeMapScripts::reportRuntime(SidesList &sides)
{
	if (!sides.usesRotwkRoster() || !std::getenv("GENERALSX_MAP_SCRIPT_DUMP"))
		return;
	for (Int i = 0; i < sides.getNumSides(); ++i)
	{
		SidesInfo &side = *sides.getSideInfo(i);
		ScriptList *list = side.getScriptList();
		if (!list)
			continue;
		Int scripts = 0;
		const auto count = [&](Script *script) {
			for (; script; script = script->getNext())
				++scripts;
		};
		count(list->getScript());
		for (ScriptGroup *group = list->getScriptGroup(); group; group = group->getNext())
			count(group->getScript());
		fprintf(stderr, "BFME_SCRIPT_RUNTIME\t%s\t%d\n", side.getDict()->getAsciiString(TheKey_playerName).str(), scripts);
	}
}
