# SPDX-License-Identifier: BSL-1.0

# Unit tests for the `DISCOVERY_ENVIRONMENT` option of `catch_discover_tests`.
#
# Run as
#     cmake -DCATCH_ADD_TESTS_SCRIPT=/path/to/extras/CatchAddTests.cmake \
#           -P TestDiscoveryEnvironment.cmake

cmake_minimum_required(VERSION 3.19)

if(NOT DEFINED CATCH_ADD_TESTS_SCRIPT)
  message(FATAL_ERROR "Missing argument `CATCH_ADD_TESTS_SCRIPT`")
endif()

if(NOT EXISTS "${CATCH_ADD_TESTS_SCRIPT}")
  message(FATAL_ERROR "Cannot find CatchAddTests.cmake at '${CATCH_ADD_TESTS_SCRIPT}'")
endif()

# Pull in the helper functions. Without `TEST_EXECUTABLE` being defined,
# `catch_discover_tests_impl` is not called.
include("${CATCH_ADD_TESTS_SCRIPT}")

set(_failures 0)

# The test asserts on the state of CATCH_TEST_DISCOVERY_SENTINEL, so stash and
# clear whatever the caller happened to have in the environment. Otherwise a
# caller who already has it set would see spurious failures, and running this
# script standalone would clobber their value.
if(DEFINED ENV{CATCH_TEST_DISCOVERY_SENTINEL})
  set(_caller_sentinel_was_set TRUE)
  set(_caller_sentinel "$ENV{CATCH_TEST_DISCOVERY_SENTINEL}")
else()
  set(_caller_sentinel_was_set FALSE)
endif()
unset(ENV{CATCH_TEST_DISCOVERY_SENTINEL})

macro(restore_caller_sentinel)
  if(_caller_sentinel_was_set)
    set(ENV{CATCH_TEST_DISCOVERY_SENTINEL} "${_caller_sentinel}")
  else()
    unset(ENV{CATCH_TEST_DISCOVERY_SENTINEL})
  endif()
endmacro()

function(expect_contains description haystack needle)
  string(FIND "${haystack}" "${needle}" _pos)
  if(_pos GREATER_EQUAL 0)
    message("  [PASS] ${description}")
  else()
    message("  [FAIL] ${description}")
    message("         expected to find: [${needle}]")
    message("         in:               [${haystack}]")
    math(EXPR _n "${_failures} + 1")
    set(_failures "${_n}" PARENT_SCOPE)
  endif()
endfunction()

set(_fake_executable "${CMAKE_CURRENT_LIST_DIR}/FakeListingExecutable.cmake")
if(NOT EXISTS "${_fake_executable}")
  message(FATAL_ERROR "Cannot find fake executable at '${_fake_executable}'")
endif()

set(_work_dir "${CMAKE_CURRENT_BINARY_DIR}/discovery-environment-test")
file(MAKE_DIRECTORY "${_work_dir}")

# `cmake -P <script>` stands in for a test executable, so that this test does
# not need a compiler and runs the same way on every platform.
function(run_discovery out_ctest_file)
  cmake_parse_arguments(_arg "" "" "DISCOVERY_ENVIRONMENT" ${ARGN})
  catch_discover_tests_impl(
    TEST_EXECUTABLE "${_fake_executable}"
    TEST_EXECUTOR "${CMAKE_COMMAND}" -P
    TEST_WORKING_DIR "${_work_dir}"
    TEST_PREFIX [==['']==]
    TEST_SUFFIX [==['']==]
    TEST_LIST fake_TESTS
    CTEST_FILE "${out_ctest_file}"
    TEST_DISCOVERY_ENVIRONMENT ${_arg_DISCOVERY_ENVIRONMENT}
    ADD_TAGS_AS_LABELS 0
  )
endfunction()

# 1) Without the option, the executable sees no sentinel.
set(_ctest_file "${_work_dir}/without-env.cmake")
run_discovery("${_ctest_file}")
file(READ "${_ctest_file}" _without_env)
expect_contains("sentinel is unset when DISCOVERY_ENVIRONMENT is not given"
  "${_without_env}" "sentinel: <unset>")

# 2) With the option, the value reaches the discovery process.
set(_ctest_file "${_work_dir}/with-env.cmake")
run_discovery("${_ctest_file}"
  DISCOVERY_ENVIRONMENT "CATCH_TEST_DISCOVERY_SENTINEL=from-discovery-env")
file(READ "${_ctest_file}" _with_env)
expect_contains("DISCOVERY_ENVIRONMENT value reaches the listing process"
  "${_with_env}" "sentinel: from-discovery-env")

# 3) The variable does not leak past discovery. This matters in PRE_TEST mode,
#    where discovery runs inside the CTest process itself.
if(DEFINED ENV{CATCH_TEST_DISCOVERY_SENTINEL})
  message("  [FAIL] variable leaked past discovery: '$ENV{CATCH_TEST_DISCOVERY_SENTINEL}'")
  math(EXPR _failures "${_failures} + 1")
else()
  message("  [PASS] variable does not leak past discovery")
endif()

# 4) A pre-existing value is restored rather than clobbered.
set(ENV{CATCH_TEST_DISCOVERY_SENTINEL} "pre-existing")
set(_ctest_file "${_work_dir}/override-env.cmake")
run_discovery("${_ctest_file}"
  DISCOVERY_ENVIRONMENT "CATCH_TEST_DISCOVERY_SENTINEL=overridden")
file(READ "${_ctest_file}" _override_env)
expect_contains("DISCOVERY_ENVIRONMENT overrides a pre-existing value"
  "${_override_env}" "sentinel: overridden")
if("$ENV{CATCH_TEST_DISCOVERY_SENTINEL}" STREQUAL "pre-existing")
  message("  [PASS] pre-existing value is restored after discovery")
else()
  message("  [FAIL] pre-existing value not restored, got '$ENV{CATCH_TEST_DISCOVERY_SENTINEL}'")
  math(EXPR _failures "${_failures} + 1")
endif()
unset(ENV{CATCH_TEST_DISCOVERY_SENTINEL})

# 5) A repeated entry uses the last value during discovery, and still restores
#    the value from before discovery rather than the one set by an earlier
#    iteration.
set(ENV{CATCH_TEST_DISCOVERY_SENTINEL} "pre-existing")
set(_ctest_file "${_work_dir}/duplicate-env.cmake")
run_discovery("${_ctest_file}"
  DISCOVERY_ENVIRONMENT
    "CATCH_TEST_DISCOVERY_SENTINEL=first"
    "CATCH_TEST_DISCOVERY_SENTINEL=second")
file(READ "${_ctest_file}" _duplicate_env)
expect_contains("a repeated entry uses the last value during discovery"
  "${_duplicate_env}" "sentinel: second")
if("$ENV{CATCH_TEST_DISCOVERY_SENTINEL}" STREQUAL "pre-existing")
  message("  [PASS] a repeated entry still restores the pre-discovery value")
else()
  message("  [FAIL] a repeated entry restored '$ENV{CATCH_TEST_DISCOVERY_SENTINEL}' instead of 'pre-existing'")
  math(EXPR _failures "${_failures} + 1")
endif()

restore_caller_sentinel()

if(_failures GREATER 0)
  message(FATAL_ERROR "${_failures} DISCOVERY_ENVIRONMENT test(s) failed")
else()
  message(STATUS "All DISCOVERY_ENVIRONMENT tests passed")
endif()
