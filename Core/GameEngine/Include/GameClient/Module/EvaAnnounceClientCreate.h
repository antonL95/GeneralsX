#pragma once

#include "Common/ClientUpdateModule.h"

class EvaAnnounceClientCreateModuleData : public ClientUpdateModuleData
{
public:
	AsciiString enemy;
	AsciiString ally;
	AsciiString owner;
	Bool onlyIfVisible = FALSE;
	Bool firstSighted = FALSE;
	Bool usePosition = FALSE;
	Bool fakeRadar = FALSE;
	UnsignedInt delay = 0;

	static void buildFieldParse(MultiIniFieldParse& p);
};

class EvaAnnounceClientCreate final : public ClientUpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(EvaAnnounceClientCreate, "EvaAnnounceClientCreate")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(EvaAnnounceClientCreate, EvaAnnounceClientCreateModuleData)

public:
	EvaAnnounceClientCreate(Thing* thing, const ModuleData* data);
	void clientUpdate() override;

private:
	UnsignedInt m_due = 0;
	Bool m_pending = TRUE;
};
