#pragma once

#include "Common/AsciiString.h"

class Condition;
class DataChunkInput;
class INI;
class SidesInfo;
struct DataChunkInfo;
class OrCondition;
class Parameter;
class Script;
class ScriptAction;
class ScriptGroup;
class ScriptList;
class SidesList;

struct BfmeScriptOpcode
{
	AsciiString name;
	Bool enabled = true;
	Bool supported = true;
	Bool inverted = false;
	Bool fromMap = false;
};

class BfmeMapScripts
{
public:
	static Bool parsePlayerLists(DataChunkInput &file, DataChunkInfo *info, void *);
	static Bool parseLibraryLists(DataChunkInput &file, DataChunkInfo *info, void *);
	static void parsePlayerAIType(INI *ini);
	static void loadLibraries(SidesList &sides, Bool multiplayer);
	static void reportRuntime(SidesList &sides);
	static void attachPlayerTemplate(SidesList &sides, SidesInfo &source, SidesInfo &target, Int start);
	static void append(ScriptList &target, ScriptList &source);

	static ScriptAction *copyAction(const ScriptAction &action, const AsciiString *qualifier = nullptr,
		const AsciiString *playerTemplate = nullptr, const AsciiString *newPlayer = nullptr);
	static Condition *copyCondition(const Condition &condition, const AsciiString *qualifier = nullptr,
		const AsciiString *playerTemplate = nullptr, const AsciiString *newPlayer = nullptr);

private:
	class Libraries;
	static void mergeSide(SidesInfo &target, SidesInfo &source, const AsciiString &qualifier);
	static void readContents(DataChunkInput &file, SidesList &sides, ScriptList &list,
		ScriptGroup *parent, ScriptGroup *&groupTail, Int depth);
	static void readScript(DataChunkInput &file, SidesList &sides, Script &script, Int version, Int depth);
	static void readOr(DataChunkInput &file, SidesList &sides, OrCondition &clause, Int depth);
	static Int readOpcode(DataChunkInput &file, BfmeScriptOpcode &opcode, Parameter **parameters,
		Int &count, Int version, Bool condition);
	static Bool readParameter(DataChunkInput &file, Parameter *&parameter);
};
