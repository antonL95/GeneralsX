#include <map>
#include <string>
#include <gtest/gtest.h>

#include "Common/INIPreprocessor.h"

namespace
{
class FakeFiles
{
public:
	void add(const std::string& path, const std::string& contents)
	{
		m_files[path] = contents;
	}

	INIPreprocessor::FileReader reader()
	{
		return [this](const std::string& path, std::string& contents) {
			std::map<std::string, std::string>::const_iterator it = m_files.find(path);
			if (it == m_files.end())
				return false;
			contents = it->second;
			return true;
		};
	}

private:
	std::map<std::string, std::string> m_files;
};

std::string run(INIPreprocessor& preprocessor, const std::string& text)
{
	return preprocessor.process("Data\\INI\\Test.ini", text);
}
}

TEST(INIPreprocessor, LeavesPlainTextUnchanged)
{
	INIPreprocessor preprocessor(nullptr);
	EXPECT_EQ(run(preprocessor, "Object Foo\n  Side = Men\nEnd\n"), "Object Foo\n  Side = Men\nEnd\n");
	EXPECT_TRUE(preprocessor.getWarnings().empty());
}

TEST(INIPreprocessor, DefineLinesBecomeBlankLines)
{
	INIPreprocessor preprocessor(nullptr);
	EXPECT_EQ(run(preprocessor, "#define DAMAGE 50\nDamage = DAMAGE\n"), "\nDamage = 50\n");
	EXPECT_EQ(preprocessor.getDefineCount(), 1u);
}

TEST(INIPreprocessor, SubstitutesWholeTokensOnly)
{
	INIPreprocessor preprocessor(nullptr);
	EXPECT_EQ(run(preprocessor, "#define ARCHER 7\nA = ARCHER ARCHERS XARCHER ARCHER\n"), "\nA = 7 ARCHERS XARCHER 7\n");
}

TEST(INIPreprocessor, NamesAreCaseSensitive)
{
	INIPreprocessor preprocessor(nullptr);
	EXPECT_EQ(run(preprocessor, "#define CRITICAL 13\nPriority = critical\n"), "\nPriority = critical\n");
}

TEST(INIPreprocessor, LowercaseNamesWarn)
{
	INIPreprocessor preprocessor(nullptr);
	run(preprocessor, "#define Mixed 1\n");
	ASSERT_EQ(preprocessor.getWarnings().size(), 1u);
	EXPECT_NE(preprocessor.getWarnings()[0].find("not uppercase"), std::string::npos);
}

TEST(INIPreprocessor, MultiTokenDefinesExpandToAllTokens)
{
	INIPreprocessor preprocessor(nullptr);
	EXPECT_EQ(run(preprocessor, "#define FILTER ANY  +Farm\t+Forge\nObjectFilter = FILTER\n"), "\nObjectFilter = ANY +Farm +Forge\n");
}

TEST(INIPreprocessor, AllDefinesOfAFileAreKnownBeforeItsFirstLine)
{
	INIPreprocessor preprocessor(nullptr);
	const std::string text =
		"Value = AWARD\n"
		"#define AWARD #ADD( TARGET 0 )\n"
		"#define TARGET 25\n";
	EXPECT_EQ(run(preprocessor, text), "Value = #ADD( 25 0 )\n\n\n");
	EXPECT_TRUE(preprocessor.getWarnings().empty());
}

TEST(INIPreprocessor, LeavesMathFunctionsForTheTypedScanners)
{
	INIPreprocessor preprocessor(nullptr);
	EXPECT_EQ(run(preprocessor, "#define BASE 100\nDamage = #MULTIPLY( BASE 1.5 )\n"), "\nDamage = #MULTIPLY( 100 1.5 )\n");
}

TEST(INIPreprocessor, DefinesPersistAcrossFiles)
{
	INIPreprocessor preprocessor(nullptr);
	preprocessor.process("Data\\INI\\GameData.ini", "#define HEAL 30\n");
	EXPECT_EQ(preprocessor.process("Data\\INI\\Object\\Hero.ini", "Amount = HEAL\n"), "Amount = 30\n");
	preprocessor.clearDefines();
	EXPECT_EQ(preprocessor.process("Data\\INI\\Object\\Hero.ini", "Amount = HEAL\n"), "Amount = HEAL\n");
}

TEST(INIPreprocessor, DuplicateDefinesWarnAndTheLastOneWins)
{
	INIPreprocessor preprocessor(nullptr);
	EXPECT_EQ(run(preprocessor, "#define A 1\n#define A 2\nV = A\n"), "\n\nV = 2\n");
	ASSERT_EQ(preprocessor.getWarnings().size(), 1u);
	EXPECT_NE(preprocessor.getWarnings()[0].find("more than once"), std::string::npos);
}

TEST(INIPreprocessor, RepeatingADefineWithTheSameValueIsSilent)
{
	INIPreprocessor preprocessor(nullptr);
	preprocessor.process("Data\\INI\\GameData.ini", "#define BONUS 10\n");
	EXPECT_EQ(preprocessor.process("Data\\INI\\GameData.ini", "#define BONUS 10\nV = BONUS\n"), "\nV = 10\n");
	EXPECT_TRUE(preprocessor.getWarnings().empty());
}

TEST(INIPreprocessor, DefineWithoutValueWarns)
{
	INIPreprocessor preprocessor(nullptr);
	run(preprocessor, "#define EMPTY\n");
	ASSERT_EQ(preprocessor.getWarnings().size(), 1u);
	EXPECT_EQ(preprocessor.getDefineCount(), 0u);
}

TEST(INIPreprocessor, LeavesOtherHashTokensAlone)
{
	INIPreprocessor preprocessor(nullptr);
	EXPECT_EQ(run(preprocessor, "Animation = #(MODEL)_WLK\n"), "Animation = #(MODEL)_WLK\n");
}

TEST(INIPreprocessor, StripsBothCommentStyles)
{
	EXPECT_EQ(INIPreprocessor::stripComment("A = 1 ; note"), "A = 1 ");
	EXPECT_EQ(INIPreprocessor::stripComment("A = 1500// 1467"), "A = 1500");
	EXPECT_EQ(INIPreprocessor::stripComment("A = 1 // x ; y"), "A = 1 ");
	EXPECT_EQ(INIPreprocessor::stripComment("URL = http://www.ea.com // home"), "URL = http://www.ea.com ");
	EXPECT_EQ(INIPreprocessor::stripComment("URL = file:///c:\\dev"), "URL = file:///c:\\dev");
	INIPreprocessor preprocessor(nullptr);
	EXPECT_EQ(run(preprocessor, "#define D 350 // was 400\nV = D\n"), "\nV = 350\n");
}

TEST(INIPreprocessor, ResolvesIncludePaths)
{
	EXPECT_EQ(INIPreprocessor::resolveIncludePath("Data\\INI\\Weapon.ini", "Includes\\Damage.inc"), "Data\\INI\\Includes\\Damage.inc");
	EXPECT_EQ(INIPreprocessor::resolveIncludePath("Data\\INI\\CommandButton.ini", "\\includes\\highpass.inc"), "Data\\INI\\includes\\highpass.inc");
	EXPECT_EQ(INIPreprocessor::resolveIncludePath("Data\\INI\\Campaigns\\Scenarios\\A.inc", "..\\Common\\B.inc"), "Data\\INI\\Campaigns\\Common\\B.inc");
	EXPECT_EQ(INIPreprocessor::resolveIncludePath("Data/INI/Object/Hero.ini", "./Hero.inc"), "Data\\INI\\Object\\Hero.inc");
}

TEST(INIPreprocessor, InlinesIncludedFiles)
{
	FakeFiles files;
	files.add("Data\\INI\\Includes\\Defines.inc", "#define HP 400\n");
	files.add("Data\\INI\\Includes\\Body.inc", "  MaxHealth = HP\n");
	INIPreprocessor preprocessor(files.reader());
	const std::string text =
		"#include \"Includes\\Defines.inc\"\n"
		"Object Foo\n"
		"#include \"Includes\\Body.inc\"\n"
		"End\n";
	EXPECT_EQ(run(preprocessor, text), "\n\nObject Foo\n\n  MaxHealth = 400\nEnd\n");
	EXPECT_TRUE(preprocessor.getWarnings().empty());
}

TEST(INIPreprocessor, DefinesFromIncludesAreKnownBeforeTheIncludeLine)
{
	FakeFiles files;
	files.add("Data\\INI\\Late.inc", "#define LATE 9\n");
	INIPreprocessor preprocessor(files.reader());
	EXPECT_EQ(run(preprocessor, "V = LATE\n#include \"Late.inc\"\n"), "V = 9\n\n\n");
}

TEST(INIPreprocessor, IncludesResolveRelativeToTheIncludingFile)
{
	FakeFiles files;
	files.add("Data\\INI\\A\\Outer.inc", "#include \"Inner.inc\"\n");
	files.add("Data\\INI\\A\\Inner.inc", "X = 1\n");
	INIPreprocessor preprocessor(files.reader());
	EXPECT_EQ(run(preprocessor, "#include \"A\\Outer.inc\"\n"), "\n\nX = 1\n");
}

TEST(INIPreprocessor, MissingIncludeWarns)
{
	FakeFiles files;
	INIPreprocessor preprocessor(files.reader());
	EXPECT_EQ(run(preprocessor, "#include \"Nope.inc\"\nX = 1\n"), "\nX = 1\n");
	ASSERT_EQ(preprocessor.getWarnings().size(), 1u);
	EXPECT_NE(preprocessor.getWarnings()[0].find("Data\\INI\\Nope.inc"), std::string::npos);
}

TEST(INIPreprocessor, RecursiveIncludeStopsAtDepthLimit)
{
	FakeFiles files;
	files.add("Data\\INI\\Self.inc", "#include \"Self.inc\"\n");
	INIPreprocessor preprocessor(files.reader());
	run(preprocessor, "#include \"Self.inc\"\n");
	ASSERT_EQ(preprocessor.getWarnings().size(), 1u);
	EXPECT_NE(preprocessor.getWarnings()[0].find("nested too deeply"), std::string::npos);
}

TEST(INIPreprocessor, RecursiveDefineStopsAtDepthLimit)
{
	INIPreprocessor preprocessor(nullptr);
	run(preprocessor, "#define LOOP LOOP\nV = LOOP\n");
	ASSERT_EQ(preprocessor.getWarnings().size(), 1u);
	EXPECT_NE(preprocessor.getWarnings()[0].find("nested too deeply"), std::string::npos);
}
