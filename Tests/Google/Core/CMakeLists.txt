# Core GameEngine code needs a game to compile, so g_googletest and z_googletest compile these sources.
set(COREI_GOOGLETEST_SRC
    GoogleTestMain.cpp
    GameEngine/Common/AsciiStringTest.cpp
    GameEngine/Common/INITest.cpp
    GameEngine/Common/INIAuditTest.cpp
    GameEngine/Common/INIRetailToleranceTest.cpp
    GameEngine/Common/HeightMapSamplesTest.cpp
    GameEngine/Common/HordeFormationTest.cpp
    GameEngine/Common/RetailFileLayersTest.cpp
    GameEngine/Common/INIMathTest.cpp
    GameEngine/Common/INIPreprocessorTest.cpp
    GameEngine/Common/INILegacyLinesTest.cpp
    GameEngine/Common/BitFlagsCrcTest.cpp
    GameEngine/Common/UpgradeMaskTest.cpp
    GameEngine/Common/MapPlayerRosterTest.cpp
    GameEngine/Common/BfmeAssetPathTest.cpp
    GameEngine/Common/FontSubstitutionTest.cpp
    GameEngine/Common/UiLayoutDumpTest.cpp
    GameEngine/Common/UiNineSliceTest.cpp
    Libraries/Compression/CompressionManagerTest.cpp
    Libraries/WW3D2/W3dBfmeChunksTest.cpp
    Libraries/WW3D2/W3dFxMaterialTest.cpp
    Libraries/WW3D2/FontNamesTest.cpp
)

add_library(corei_googletest INTERFACE)

target_sources(corei_googletest INTERFACE ${COREI_GOOGLETEST_SRC})

target_include_directories(corei_googletest INTERFACE
    "${CMAKE_SOURCE_DIR}/Core/Libraries/Source/WWVegas/WW3D2")

target_link_libraries(corei_googletest INTERFACE
    comctl32
    depsi_googletest
    GTest::gtest
    imm32
    vfw32
    winmm
)
