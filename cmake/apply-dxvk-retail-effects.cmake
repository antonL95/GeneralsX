find_package(Git REQUIRED)

execute_process(
  COMMAND "${GIT_EXECUTABLE}" apply --reverse --check "${patch}"
  WORKING_DIRECTORY "${source}"
  RESULT_VARIABLE already_applied
  OUTPUT_QUIET ERROR_QUIET
)
if(NOT already_applied EQUAL 0)
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" restore --source=HEAD --worktree --
      src/d3d8/d3d8_main.cpp src/d3d8/d3d8.sym
      src/d3d9/d3d9_bridge.cpp src/d3d9/d3d9_bridge.h src/d3d9/d3d9_device.cpp src/d3d9/d3d9_device.h
    WORKING_DIRECTORY "${source}"
    COMMAND_ERROR_IS_FATAL ANY
  )
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" apply "${patch}"
    WORKING_DIRECTORY "${source}"
    COMMAND_ERROR_IS_FATAL ANY
  )
endif()
