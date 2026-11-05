# SPDX-License-Identifier: BSL-1.0

# Unit tests for `prepare_command_fragment` helper in `extras/CatchAddTests.cmake`.
#
# Yes, we are at the stage where the script helpers need unit tests.
#
# Run as
#     cmake -DCATCH_ADD_TESTS_SCRIPT=/path/to/extras/CatchAddTests.cmake \
#           -P TestPrepareCommandFragment.cmake


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

function(expect_equal description actual expected)
  if(actual STREQUAL expected)
    message("  [PASS] ${description}")
  else()
    message("  [FAIL] ${description}")
    message("         expected: [${expected}]")
    message("         actual:   [${actual}]")
    math(EXPR _n "${_failures} + 1")
    set(_failures "${_n}" PARENT_SCOPE)
  endif()
endfunction()

prepare_command_fragment(test_fragment SimpleName /path/to/tests)
expect_equal("Simple arg, no quotes" "${test_fragment}" " SimpleName /path/to/tests")

prepare_command_fragment(test_fragment "Name with spaces")
expect_equal("Spaces in arg, needs quotes" "${test_fragment}" " [==[Name with spaces]==]")

prepare_command_fragment(test_fragment Foo PROPERTIES LABELS "tagA\;tagB\;tagC")
expect_equal("semicolons in argument are kept and quoted"
  "${test_fragment}" " Foo PROPERTIES LABELS [==[tagA\;tagB\;tagC]==]")

set(test_fragment "PRE-EXISTING")
prepare_command_fragment(test_fragment Foo PROPERTIES BAR baz)
expect_equal("out var does no accumulate commands"
  "${test_fragment}" " Foo PROPERTIES BAR baz")



prepare_command_fragment(test_fragment "Name with ]==] inside")
expect_equal("closing bracket delimiter is preserved"
  "${test_fragment}" " [===[Name with ]==] inside]===]")

prepare_command_fragment(test_fragment "Name with ]==] and ]===] inside")
expect_equal("multiple closing bracket delimiters are preserved"
  "${test_fragment}" " [====[Name with ]==] and ]===] inside]====]")

set(roundtrip_name "Name with ]==] inside")
prepare_command_fragment(test_fragment "${roundtrip_name}")
cmake_language(EVAL CODE "set(roundtrip_result ${test_fragment})")
expect_equal("generated fragment roundtrips as one argument"
  "${roundtrip_result}" "${roundtrip_name}")


if(_failures GREATER 0)
  message(FATAL_ERROR "${_failures} prepare_command_fragment test(s) failed")
else()
  message(STATUS "All prepare_command_fragment tests passed")
endif()
