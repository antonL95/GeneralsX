#include "GameLogic/LuaEventTables.h"

#include <gtest/gtest.h>

TEST(LuaEventTables, ReadsEventsAndResolvesListsInEitherOrder)
{
	std::string error;
	const auto tables = LuaEventTables::parse(R"xml(
		<SageLuaScriptSection xmlns="http://tempuri.org/ScriptEvents.xsd">
			<Events>
				<InternalEvent Name="OnCreated"/>
				<ScriptedEvent Name='Fear'/>
				<ModelConditionEvent Name="Mounted"><Conditions>+MOUNTED -DYING</Conditions></ModelConditionEvent>
				<ObjectStatusEvent Name="InCombat"><Conditions>+UNDER_ATTACK</Conditions></ObjectStatusEvent>
				<UnsupportedEvent Name="Forbidden"/>
			</Events>
			<EventList Name="Child" Inherit="Parent">
				<!-- <EventHandler EventName="Mounted" ScriptFunctionName="Ignore"/> -->
				<EventHandler EventName="OnCreated" ScriptFunctionName="ChildCreated"/>
				<EventHandler EventName="Forbidden" ScriptFunctionName="Ignored"/>
			</EventList>
			<EventList Name="Parent" Inherit="Grandparent">
				<EventHandler EventName="OnCreated" ScriptFunctionName="ParentCreated"/>
				<EventHandler EventName="Mounted" ScriptFunctionName="OnMounted" DebugSingleStep="false"/>
			</EventList>
			<EventList Name="Grandparent">
				<EventHandler EventName="Fear" ScriptFunctionName="Fear&amp;Name"/>
			</EventList>
		</SageLuaScriptSection>
	)xml", error);
	ASSERT_TRUE(tables) << error;
	ASSERT_EQ(tables->events.size(), 4u);
	EXPECT_EQ(tables->events[0].kind, LuaEventTables::Kind::Internal);
	EXPECT_EQ(tables->events[1].kind, LuaEventTables::Kind::Scripted);
	EXPECT_EQ(tables->events[2].kind, LuaEventTables::Kind::ModelCondition);
	EXPECT_EQ(tables->events[2].conditions, "+MOUNTED -DYING");
	EXPECT_EQ(tables->events[3].kind, LuaEventTables::Kind::ObjectStatus);
	const auto& handlers = tables->lists.at("Child");
	EXPECT_EQ(handlers.size(), 3u);
	EXPECT_EQ(handlers.at("OnCreated"), "ChildCreated");
	EXPECT_EQ(handlers.at("Mounted"), "OnMounted");
	EXPECT_EQ(handlers.at("Fear"), "Fear&Name");
}

TEST(LuaEventTables, RetainsLocalHandlersWithUnresolvedInheritance)
{
	std::string error;
	const auto tables = LuaEventTables::parse(R"xml(
		<SageLuaScriptSection>
			<Events><InternalEvent Name="OnCreated"/></Events>
			<EventList Name="Orphan" Inherit="Missing">
				<EventHandler EventName="OnCreated" ScriptFunctionName="Created"/>
			</EventList>
			<EventList Name="CycleA" Inherit="CycleB"/>
			<EventList Name="CycleB" Inherit="CycleA"/>
		</SageLuaScriptSection>
	)xml", error);
	ASSERT_TRUE(tables) << error;
	EXPECT_EQ(tables->lists.at("Orphan").at("OnCreated"), "Created");
	EXPECT_TRUE(tables->lists.at("CycleA").empty());
	EXPECT_TRUE(tables->lists.at("CycleB").empty());
}

TEST(LuaEventTables, RejectsMalformedDocumentsAndDoctypes)
{
	std::string error;
	EXPECT_FALSE(LuaEventTables::parse("<SageLuaScriptSection><Events></SageLuaScriptSection>", error));
	EXPECT_FALSE(error.empty());
	EXPECT_FALSE(LuaEventTables::parse(R"xml(
		<!DOCTYPE SageLuaScriptSection [<!ENTITY external SYSTEM 'file:///dev/null'>]>
		<SageLuaScriptSection>&external;</SageLuaScriptSection>
	)xml", error));
	EXPECT_NE(error.find("DOCTYPE"), std::string::npos);
}
