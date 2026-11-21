cmake_minimum_required(VERSION 3.28)
find_package(Git REQUIRED)

execute_process(
  COMMAND "${GIT_EXECUTABLE}" rev-parse --local-env-vars
  OUTPUT_VARIABLE local_env
  COMMAND_ERROR_IS_FATAL ANY
)
string(REGEX MATCHALL "GIT_[A-Z0-9_]+" local_git_vars "${local_env}")
foreach(variable IN LISTS local_git_vars ITEMS
    GIT_NAMESPACE GIT_QUARANTINE_PATH GIT_DEFAULT_HASH GIT_DEFAULT_REF_FORMAT)
  unset(ENV{${variable}})
endforeach()

file(GLOB existing LIST_DIRECTORIES TRUE "${source}/*")
if(existing OR EXISTS "${source}/.git")
  message(FATAL_ERROR "Refusing to patch existing DXVK source at ${source}. Configure in a fresh build directory.")
endif()

set(staged "${source}.fetching")
if(EXISTS "${staged}")
  message(FATAL_ERROR "Incomplete DXVK checkout at ${staged}. Configure in a fresh build directory.")
endif()
file(MAKE_DIRECTORY "${staged}")

function(dxvk_git operation)
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" -C "${staged}" ${ARGN}
    RESULT_VARIABLE result
    ERROR_VARIABLE error
  )
  if(NOT result STREQUAL "0")
    message(FATAL_ERROR "DXVK ${operation} failed for ${revision} at ${staged}:\n${error}")
  endif()
endfunction()

dxvk_git("initialization" init --quiet)
dxvk_git("remote setup" remote add origin "${repository}")
dxvk_git("fetch" fetch --quiet --depth=1 origin "${revision}")
dxvk_git("checkout" checkout --quiet --detach "${revision}")
dxvk_git("retail effects patch check" apply --check --whitespace=error-all "${patch}")
dxvk_git("retail effects patch application" apply --whitespace=error-all "${patch}")
dxvk_git("submodule checkout" submodule update --quiet --init --recursive --depth=1)
file(RENAME "${staged}" "${source}")
