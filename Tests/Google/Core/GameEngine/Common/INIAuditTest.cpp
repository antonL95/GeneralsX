#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <gtest/gtest.h>

#include "Common/INIAudit.h"

namespace
{
std::string readAll(const std::string& path)
{
	std::ifstream in(path);
	std::ostringstream buffer;
	buffer << in.rdbuf();
	return buffer.str();
}

std::string reportPath()
{
	return testing::TempDir() + "ini_audit_test.tsv";
}
}

TEST(INIAudit, FormatsOneTabSeparatedLinePerRecord)
{
	EXPECT_EQ(INIAudit::formatRecord("field", "Object  Foo", "  Side\t= Men ", "Data\\INI\\A.ini", 12),
		"field\tObject Foo\tSide = Men\tData\\INI\\A.ini\t12\n");
}

TEST(INIAudit, DisabledAuditRecordsNothingAndPushesNoContext)
{
	INIAudit::close();
	if (INIAudit::isEnabled())
		GTEST_SKIP() << INIAudit::environmentVariable() << " is set in the environment";
	{
		INIAudit::Scope scope("Object Foo");
		EXPECT_EQ(INIAudit::currentContext(), "");
	}
	INIAudit::close();
}

TEST(INIAudit, RecordsCarryTheNestedBlockContext)
{
	const std::string path = reportPath();
	INIAudit::open(path.c_str());
	ASSERT_TRUE(INIAudit::isEnabled());
	{
		INIAudit::Scope object("Object Foo");
		{
			INIAudit::Scope module("  Behavior = SlowDeathBehavior ModuleTag_Death");
			INIAudit::record("field", "DeathTypes", "Data\\INI\\Object\\Foo.ini", 7);
		}
		INIAudit::record("module", "RadiusDecalUpdate", "Data\\INI\\Object\\Foo.ini", 9);
	}
	EXPECT_EQ(INIAudit::currentContext(), "");
	INIAudit::close();
	EXPECT_EQ(readAll(path),
		"field\tObject Foo > Behavior = SlowDeathBehavior ModuleTag_Death\tDeathTypes\tData\\INI\\Object\\Foo.ini\t7\n"
		"module\tObject Foo\tRadiusDecalUpdate\tData\\INI\\Object\\Foo.ini\t9\n");
	std::remove(path.c_str());
}

TEST(INIAudit, ContextUnwindsWhenAParseThrows)
{
	const std::string path = reportPath();
	INIAudit::open(path.c_str());
	try
	{
		INIAudit::Scope object("Object Foo");
		throw 1;
	}
	catch (int)
	{
	}
	EXPECT_EQ(INIAudit::currentContext(), "");
	INIAudit::close();
	std::remove(path.c_str());
}
