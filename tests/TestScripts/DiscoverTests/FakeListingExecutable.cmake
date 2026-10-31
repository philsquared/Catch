# SPDX-License-Identifier: BSL-1.0

# A stand-in for a Catch2 test executable, used by
# `TestDiscoveryEnvironment.cmake`.
#
# It writes a minimal `--list-tests` JSON listing in which the name of the
# single test case is the value of the `CATCH_TEST_DISCOVERY_SENTINEL`
# environment variable. That lets the caller prove whether an environment
# variable reached the process that performs test discovery.
#
# CMake is used as the fake executable so that the test works on every
# platform the real CI runs on, rather than relying on a shell script.

cmake_minimum_required(VERSION 3.19)

# Find the `--out <path>` argument that catch_discover_tests_impl passes.
set(_out_path)
set(_prev)
math(EXPR _last "${CMAKE_ARGC} - 1")
foreach(_i RANGE ${_last})
  set(_arg "${CMAKE_ARGV${_i}}")
  if(_prev STREQUAL "--out")
    set(_out_path "${_arg}")
  endif()
  if(_arg STREQUAL "--list-reporters")
    # The reporter probe only cares about the exit code.
    return()
  endif()
  set(_prev "${_arg}")
endforeach()

if(NOT _out_path)
  message(FATAL_ERROR "FakeListingExecutable: no --out argument was provided")
endif()

if(DEFINED ENV{CATCH_TEST_DISCOVERY_SENTINEL})
  set(_test_name "sentinel: $ENV{CATCH_TEST_DISCOVERY_SENTINEL}")
else()
  set(_test_name "sentinel: <unset>")
endif()

file(WRITE "${_out_path}"
"{ \"version\": 2
, \"metadata\": { \"name\": \"fake\", \"rng-seed\": 0, \"catch2-version\": \"3.0.0\" }
, \"listings\": { \"tests\": [ { \"name\": \"${_test_name}\", \"class-name\": \"\", \"tags\": [], \"source-location\": { \"filename\": \"fake.cpp\", \"line\": 1 } } ] } }
")
