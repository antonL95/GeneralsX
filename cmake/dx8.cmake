# DirectX 8 headers and rendering backend selection
# GeneralsX @build BenderAI 10/02/2026 - Session 18
# Fighter19's approach: Fetch ONE OR THE OTHER, never both
#
# On Windows: Use min-dx8-sdk (minimal Windows DirectX headers + libs)
# On Linux:   Use DXVK native pre-built tarball (DirectX→Vulkan translation)
# On macOS:   Build DXVK from source using Meson + MoltenVK (DirectX→Metal bridge)
#
# CRITICAL: Mixing headers causes conflicts - dx8-src has incomplete types,
# DXVK has full DirectX8+Wine headers. Compiler picks first path = wrong headers.
#
# macOS DXVK build (Session 61, 24/02/2026):
#   DXVK 2.6 builds natively on macOS arm64 via its "native" build mode.
#   macOS fixes are maintained in the DXVK fork history consumed by this build.
#
# Reference: docs/WORKDIR/lessons/2026-02-LESSONS.md (historical patch rationale)

set(DXVK_VERSION "v2.6")
option(SAGE_DXVK_LINUX_SOURCE "Build Linux DXVK with the retail effect bridge instead of using the prebuilt x86 package" OFF)
set(DXVK_RETAIL_BASE f66da559fd48cd55ddfebca38ed0f708c0f76a24)
set(DXVK_RETAIL_PATCH "${CMAKE_CURRENT_LIST_DIR}/dxvk-retail-effects.patch")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${DXVK_RETAIL_PATCH}")
file(SHA256 "${DXVK_RETAIL_PATCH}" DXVK_RETAIL_PATCH_HASH)
set(DXVK_RETAIL_PATCH_COMMAND
  "${CMAKE_COMMAND}" "-Dsource=<SOURCE_DIR>" "-Dpatch=${DXVK_RETAIL_PATCH}"
  "-Dpatch_hash=${DXVK_RETAIL_PATCH_HASH}"
  -P "${CMAKE_CURRENT_LIST_DIR}/apply-dxvk-retail-effects.cmake")

if(SAGE_USE_DX8)
  # Windows: Fetch min-dx8-sdk for native DirectX 8
  FetchContent_Declare(
    dx8
    GIT_REPOSITORY https://github.com/TheSuperHackers/min-dx8-sdk.git
    GIT_TAG        7bddff8c01f5fb931c3cb73d4aa8e66d303d97bc
  )
  FetchContent_MakeAvailable(dx8)
  message(STATUS "Using DirectX 8 SDK (Windows native)")

elseif(APPLE AND SAGE_USE_MOLTENVK)
  # macOS: Build DXVK 2.6 from source using Meson + MoltenVK
  # GeneralsX @build BenderAI 24/02/2026 - Phase 5 macOS port (Session 61)
  find_program(MESON_EXECUTABLE meson HINTS /opt/homebrew/bin /usr/local/bin)
  find_program(NINJA_EXECUTABLE ninja HINTS /opt/homebrew/bin /usr/local/bin)

  if(NOT MESON_EXECUTABLE)
    message(FATAL_ERROR "DXVK macOS build requires meson: brew install meson")
  endif()
  if(NOT NINJA_EXECUTABLE)
    message(FATAL_ERROR "DXVK macOS build requires ninja: brew install ninja")
  endif()

  # Detect host architecture so Clang targets the correct slice.
  # IMPORTANT: prefer CMAKE_OSX_ARCHITECTURES (set by the preset) over uname -m.
  # On Apple Silicon Macs running CMake / meson via Rosetta, uname -m returns
  # x86_64 even though the native executable arch is arm64. Using CMAKE_OSX_ARCHITECTURES
  # (e.g. "arm64" from the macos-vulkan preset) avoids building an x86_64 dylib that
  # the arm64 game binary cannot dlopen.
  if(CMAKE_OSX_ARCHITECTURES)
    # Use the first entry (handles "arm64;x86_64" fat-binary requests too)
    list(GET CMAKE_OSX_ARCHITECTURES 0 DXVK_HOST_ARCH)
  else()
    execute_process(
      COMMAND uname -m
      OUTPUT_VARIABLE DXVK_HOST_ARCH
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )
  endif()
  message(STATUS "Building DXVK ${DXVK_VERSION} for macOS/${DXVK_HOST_ARCH} with Meson (${MESON_EXECUTABLE})")

  include(ExternalProject)
  # GeneralsX @build BenderAI 13/03/2026 Add explicit source mode to keep remote branch updates deterministic by default.
  # GeneralsX @build fbraz 25/09/2026 Default local DXVK fork to workspace generalsx-dxvk repository.
  if(NOT DEFINED DXVK_LOCAL_FORK_DIR)
    if(EXISTS "${CMAKE_SOURCE_DIR}/../generalsx-dxvk/.git")
      set(DXVK_LOCAL_FORK_DIR "${CMAKE_SOURCE_DIR}/../generalsx-dxvk")
    elseif(EXISTS "${CMAKE_SOURCE_DIR}/references/fbraz3-dxvk/.git")
      set(DXVK_LOCAL_FORK_DIR "${CMAKE_SOURCE_DIR}/references/fbraz3-dxvk")
    else()
      set(DXVK_LOCAL_FORK_DIR "${CMAKE_SOURCE_DIR}/../generalsx-dxvk")
    endif()
  endif()
  option(SAGE_DXVK_USE_LOCAL_FORK "Build DXVK from local generalsx-dxvk checkout" OFF)

  if(SAGE_DXVK_USE_LOCAL_FORK AND EXISTS "${DXVK_LOCAL_FORK_DIR}/.git")
    set(DXVK_SOURCE_DIR "${DXVK_LOCAL_FORK_DIR}")
    message(STATUS "DXVK macOS build: using local fork source at ${DXVK_SOURCE_DIR}")
  else()
    set(DXVK_SOURCE_DIR "${CMAKE_BINARY_DIR}/_deps/dxvk-src-generalsx")
    message(STATUS "DXVK macOS build: using GitHub source clone at ${DXVK_SOURCE_DIR}")
  endif()
  set(DXVK_BUILD_DIR  "${CMAKE_BINARY_DIR}/_deps/dxvk-build-macos")
  set(DXVK_D3D8_LIB  "${DXVK_BUILD_DIR}/src/d3d8/libdxvk_d3d8.0.dylib")
  set(DXVK_D3D9_LIB  "${DXVK_BUILD_DIR}/src/d3d9/libdxvk_d3d9.0.dylib")

  # Detect Vulkan SDK location for Meson configuration.
  # VULKAN_SDK must point to the platform subdir (e.g. ~/VulkanSDK/1.4.x/macOS)
  # where lib/libvulkan.dylib and lib/libMoltenVK.dylib live.
  # GeneralsX @build BenderAI 03/03/2026: Normalize env path to macOS platform subdir
  set(VULKAN_SDK_ENV "$ENV{VULKAN_SDK}")

  # If VULKAN_SDK points to the version root (has macOS/ subdir), normalize it
  if(VULKAN_SDK_ENV AND EXISTS "${VULKAN_SDK_ENV}/macOS/lib/libMoltenVK.dylib")
    set(VULKAN_SDK_ENV "${VULKAN_SDK_ENV}/macOS")
    message(STATUS "DXVK macOS build: Normalized VULKAN_SDK to platform subdir: ${VULKAN_SDK_ENV}")
  endif()

  if(NOT VULKAN_SDK_ENV OR NOT EXISTS "${VULKAN_SDK_ENV}/lib/libMoltenVK.dylib")
    # Try home directory: look for ~/VulkanSDK/*/macOS
    file(GLOB VULKAN_HOME_DIRS "$ENV{HOME}/VulkanSDK/*/macOS")
    if(VULKAN_HOME_DIRS)
      list(SORT VULKAN_HOME_DIRS)
      list(REVERSE VULKAN_HOME_DIRS)
      list(GET VULKAN_HOME_DIRS 0 POTENTIAL_SDK)
      if(EXISTS "${POTENTIAL_SDK}/lib/libMoltenVK.dylib")
        set(VULKAN_SDK_ENV "${POTENTIAL_SDK}")
      endif()
    endif()
  endif()

  if(NOT VULKAN_SDK_ENV OR NOT EXISTS "${VULKAN_SDK_ENV}/lib/libMoltenVK.dylib")
    # Try common Homebrew locations
    foreach(BREW_PATH "/usr/local/Caskroom/vulkan-sdk/latest/VulkanSDK/macOS" "/opt/homebrew/Caskroom/vulkan-sdk/latest/VulkanSDK/macOS")
      if(EXISTS "${BREW_PATH}/lib/libMoltenVK.dylib")
        set(VULKAN_SDK_ENV "${BREW_PATH}")
        break()
      endif()
    endforeach()
  endif()

  if(VULKAN_SDK_ENV AND EXISTS "${VULKAN_SDK_ENV}/lib/libMoltenVK.dylib")
    message(STATUS "DXVK macOS build: Using Vulkan SDK at ${VULKAN_SDK_ENV}")
    set(VULKAN_SDK_ENV_VAR "VULKAN_SDK=${VULKAN_SDK_ENV}")
  else()
    message(WARNING "DXVK macOS build: Vulkan SDK / MoltenVK not found; Meson will search system paths")
    if(VULKAN_SDK_ENV)
      message(STATUS "  VULKAN_SDK checked: ${VULKAN_SDK_ENV}")
    endif()
    set(VULKAN_SDK_ENV_VAR "")
  endif()

  if(SAGE_DXVK_USE_LOCAL_FORK AND EXISTS "${DXVK_LOCAL_FORK_DIR}/.git")
    ExternalProject_Add(dxvk_macos_build
      # GeneralsX @build BenderAI 13/03/2026 Build from local fbraz3 fork to avoid stale remote hash pins.
      SOURCE_DIR        ${DXVK_SOURCE_DIR}
      BINARY_DIR        ${DXVK_BUILD_DIR}
      DOWNLOAD_COMMAND  ""
      UPDATE_COMMAND    ""
      PATCH_COMMAND     ""
      CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env CC=clang CXX=clang++ "CFLAGS=-arch ${DXVK_HOST_ARCH} -mcpu=apple-m1" "CXXFLAGS=-arch ${DXVK_HOST_ARCH} -mcpu=apple-m1" "LDFLAGS=-arch ${DXVK_HOST_ARCH}" ${VULKAN_SDK_ENV_VAR} ${MESON_EXECUTABLE} setup ${DXVK_BUILD_DIR} ${DXVK_SOURCE_DIR} --native-file ${CMAKE_SOURCE_DIR}/cmake/meson-arm64-native.ini -Ddxvk_native_wsi=sdl3 --buildtype=release --reconfigure
      BUILD_COMMAND     ${NINJA_EXECUTABLE} -C ${DXVK_BUILD_DIR} src/d3d9/libdxvk_d3d9.0.dylib src/d3d8/libdxvk_d3d8.0.dylib
      BUILD_BYPRODUCTS  "${DXVK_D3D8_LIB}" "${DXVK_D3D9_LIB}"
      INSTALL_COMMAND   ""
      UPDATE_DISCONNECTED TRUE
    )
  else()
    # GeneralsX @build copilot 01/04/2026 Pin remote DXVK to immutable commit produced by fix/macos-size_t-cstddef.
    # GeneralsX @build 12/07/2026 Bumped to the merged commit that includes the macOS HiDPI WSI fix upstream.
    # GeneralsX @build BenderAI 11/09/2026 Bump DXVK commit to fix libc++ try_key_extraction on modern macOS
    set(DXVK_REMOTE_REF ${DXVK_RETAIL_BASE})
    ExternalProject_Add(dxvk_macos_build
      # GeneralsX @build BenderAI 08/04/2026 Consume pre-patched source from pinned fork commit.
      # GeneralsX @build 17/09/2026 Migrate DXVK remote repository to generalsx-project org.
      GIT_REPOSITORY    https://github.com/generalsx-project/dxvk.git
      GIT_TAG           ${DXVK_REMOTE_REF}
      # GeneralsX @build copilot 01/04/2026 Keep pinned commit fetch reliable across clean CI builds.
      GIT_SHALLOW       FALSE
      SOURCE_DIR        ${DXVK_SOURCE_DIR}
      BINARY_DIR        ${DXVK_BUILD_DIR}
      PATCH_COMMAND     ${DXVK_RETAIL_PATCH_COMMAND}
      CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env CC=clang CXX=clang++ "CFLAGS=-arch ${DXVK_HOST_ARCH} -mcpu=apple-m1" "CXXFLAGS=-arch ${DXVK_HOST_ARCH} -mcpu=apple-m1" "LDFLAGS=-arch ${DXVK_HOST_ARCH}" ${VULKAN_SDK_ENV_VAR} ${MESON_EXECUTABLE} setup ${DXVK_BUILD_DIR} ${DXVK_SOURCE_DIR} --native-file ${CMAKE_SOURCE_DIR}/cmake/meson-arm64-native.ini -Ddxvk_native_wsi=sdl3 --buildtype=release --reconfigure
      BUILD_COMMAND     ${NINJA_EXECUTABLE} -C ${DXVK_BUILD_DIR} src/d3d9/libdxvk_d3d9.0.dylib src/d3d8/libdxvk_d3d8.0.dylib
      BUILD_BYPRODUCTS  "${DXVK_D3D8_LIB}" "${DXVK_D3D9_LIB}"
      INSTALL_COMMAND   ""
      UPDATE_DISCONNECTED FALSE
    )
  endif()

  # Copy libdxvk_d3d9 + libdxvk_d3d8 to build dir and create unversioned symlinks.
  # d3d8 links against d3d9 via @rpath, so both must be present at runtime.
  add_custom_command(
    OUTPUT  "${CMAKE_BINARY_DIR}/libdxvk_d3d9.0.dylib"
            "${CMAKE_BINARY_DIR}/libdxvk_d3d8.0.dylib"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
              ${DXVK_D3D9_LIB} "${CMAKE_BINARY_DIR}/libdxvk_d3d9.0.dylib"
    COMMAND ${CMAKE_COMMAND} -E create_symlink
              libdxvk_d3d9.0.dylib "${CMAKE_BINARY_DIR}/libdxvk_d3d9.dylib"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
              ${DXVK_D3D8_LIB} "${CMAKE_BINARY_DIR}/libdxvk_d3d8.0.dylib"
    COMMAND ${CMAKE_COMMAND} -E create_symlink
              libdxvk_d3d8.0.dylib "${CMAKE_BINARY_DIR}/libdxvk_d3d8.dylib"
    DEPENDS dxvk_macos_build "${DXVK_D3D8_LIB}" "${DXVK_D3D9_LIB}"
    COMMENT "Installing libdxvk_d3d8 + libdxvk_d3d9 to build directory"
  )
  add_custom_target(dxvk_d3d8_install ALL
    DEPENDS "${CMAKE_BINARY_DIR}/libdxvk_d3d8.0.dylib"
            "${CMAKE_BINARY_DIR}/libdxvk_d3d9.0.dylib"
  )

  # Export path so other cmake files know where the headers are
  set(DXVK_INCLUDE_DIR "${DXVK_SOURCE_DIR}/include/native" CACHE PATH "DXVK native headers")
  # GeneralsX @build felipebraz 10/06/2025 Mirror lowercase dxvk_SOURCE_DIR that FetchContent sets on Linux
  # so CompatLib/CMakeLists.txt check works on macOS as well (CACHE PATH survives auto-regeneration)
  set(dxvk_SOURCE_DIR "${DXVK_SOURCE_DIR}" CACHE PATH "DXVK source directory (macOS)")
  message(STATUS "DXVK source directory: ${DXVK_SOURCE_DIR}")
  message(STATUS "DXVK d3d8 library:     ${DXVK_D3D8_LIB}")

elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND
       (CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64" OR SAGE_DXVK_LINUX_SOURCE))
  find_program(DXVK_MESON meson REQUIRED)
  find_program(DXVK_NINJA ninja REQUIRED)
  find_program(DXVK_GLSLANG NAMES glslang glslangValidator REQUIRED)
  set(DXVK_LINUX_SOURCE TRUE)

  FetchContent_Declare(dxvk_linux_arm64
    GIT_REPOSITORY https://github.com/generalsx-project/dxvk.git
    GIT_TAG ${DXVK_RETAIL_BASE}
  )
  FetchContent_MakeAvailable(dxvk_linux_arm64)
  set(dxvk_SOURCE_DIR "${dxvk_linux_arm64_SOURCE_DIR}")
  set(DXVK_INCLUDE_DIR "${dxvk_SOURCE_DIR}/include/native")

  function(configure_dxvk_linux_arm64)
    include(ExternalProject)
    set(build_dir "${CMAKE_BINARY_DIR}/_deps/dxvk-build-linux-arm64")
    set(pkg_path "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
    if(TARGET SDL3-shared)
      set(pc_dir "${CMAKE_CURRENT_BINARY_DIR}/dxvk-pkgconfig")
      file(GENERATE OUTPUT "${pc_dir}/SDL3.pc" CONTENT
"Name: sdl3
Description: SDL3 built by GeneralsX
Version: ${SDL3_VERSION}
Libs: -L$<TARGET_FILE_DIR:SDL3-shared> -lSDL3
Cflags: -I${SDL3_SOURCE_DIR}/include
")
      set(pkg_path "${pc_dir}:${pkg_path}")
    endif()

    set(d3d8 "src/d3d8/libdxvk_d3d8.so.0.20600")
    set(d3d9 "src/d3d9/libdxvk_d3d9.so.0.20600")
    ExternalProject_Add(dxvk_linux_arm64_build
      SOURCE_DIR "${dxvk_SOURCE_DIR}"
      BINARY_DIR "${build_dir}"
      DOWNLOAD_COMMAND ""
      PATCH_COMMAND ${DXVK_RETAIL_PATCH_COMMAND}
      CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env
        "CC=${CMAKE_C_COMPILER}" "CXX=${CMAKE_CXX_COMPILER}"
        "PKG_CONFIG_PATH=${pkg_path}" "LDFLAGS=-Wl,-rpath,'$ORIGIN'"
        ${DXVK_MESON} setup <BINARY_DIR> <SOURCE_DIR> --reconfigure
        --buildtype=release -Ddxvk_native_wsi=sdl3
        -Dcpp_args=-DVK_ENABLE_BETA_EXTENSIONS
        -Denable_dxgi=false -Denable_d3d10=false -Denable_d3d11=false
      BUILD_COMMAND ${DXVK_NINJA} -C <BINARY_DIR> "${d3d8}" "${d3d9}"
      BUILD_BYPRODUCTS "${build_dir}/${d3d8}" "${build_dir}/${d3d9}"
      INSTALL_COMMAND ""
    )
    if(TARGET SDL3-shared)
      add_dependencies(dxvk_linux_arm64_build SDL3-shared)
    endif()

    foreach(game IN ITEMS g_generals z_generals)
      if(TARGET ${game})
        set(game_dir "$<TARGET_FILE_DIR:${game}>")
        add_custom_target(${game}_dxvk
          COMMAND ${CMAKE_COMMAND} -E make_directory "${game_dir}"
          COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${build_dir}/${d3d8}" "${build_dir}/${d3d9}" "${game_dir}"
          COMMAND ${CMAKE_COMMAND} -E create_symlink libdxvk_d3d8.so.0.20600 "${game_dir}/libdxvk_d3d8.so.0"
          COMMAND ${CMAKE_COMMAND} -E create_symlink libdxvk_d3d9.so.0.20600 "${game_dir}/libdxvk_d3d9.so.0"
          COMMAND ${CMAKE_COMMAND} -E create_symlink libdxvk_d3d8.so.0 "${game_dir}/libdxvk_d3d8.so"
          COMMAND ${CMAKE_COMMAND} -E create_symlink libdxvk_d3d9.so.0 "${game_dir}/libdxvk_d3d9.so"
          DEPENDS "${build_dir}/${d3d8}" "${build_dir}/${d3d9}"
          VERBATIM
        )
        add_dependencies(${game}_dxvk dxvk_linux_arm64_build)
        add_dependencies(${game} ${game}_dxvk)
        set_property(TARGET ${game} APPEND PROPERTY BUILD_RPATH "$ORIGIN")
      endif()
    endforeach()
  endfunction()

else()
  # Linux: Fetch pre-built DXVK native binary for DirectX→Vulkan translation
  # Native 32-bit and 64-bit Linux binaries (.so)
  FetchContent_Declare(
    dxvk
    URL        https://github.com/doitsujin/dxvk/releases/download/v2.6/dxvk-native-2.6-steamrt-sniper.tar.gz
  )
  FetchContent_MakeAvailable(dxvk)
  message(STATUS "Using DXVK native (Linux DirectX→Vulkan)")
  message(STATUS "DXVK source directory: ${dxvk_SOURCE_DIR}")
endif()
