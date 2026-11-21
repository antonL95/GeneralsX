#pragma once

#include "GameLogic/Module/UpgradeModule.h"

class DoCommandUpgradeModuleData : public UpgradeModuleData
{
public:
	AsciiString m_grantCommand;
	AsciiString m_removeCommand;
	static void buildFieldParse(MultiIniFieldParse& fields);
};

class DoCommandUpgrade final : public UpgradeModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(DoCommandUpgrade, "DoCommandUpgrade")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(DoCommandUpgrade, DoCommandUpgradeModuleData)

public:
	DoCommandUpgrade(Thing* thing, const ModuleData* data) : UpgradeModule(thing, data) {}
	Bool isSubObjectsUpgrade() override { return FALSE; }
	Bool resetUpgrade(const UpgradeMaskType& mask) override;
	void inheritExecution() { setUpgradeExecuted(TRUE); }
	AsciiString getExecutionKey() const;

protected:
	void upgradeImplementation() override;

private:
	void issueCommand(const AsciiString& name);
};
