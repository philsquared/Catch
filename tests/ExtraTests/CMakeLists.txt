cmake_minimum_required(VERSION 3.16)
project(Catch2ExtraTests LANGUAGES CXX)

message(STATUS "Extra tests included")

add_test(
  NAME TestShardingIntegration
  COMMAND Python3::Interpreter ${CATCH_DIR}/tests/TestScripts/testSharding.py $<TARGET_FILE:SelfTest>
)
set_tests_properties(TestShardingIntegration
  PROPERTIES
    LABELS "uses-python"
)

add_test(
  NAME TestSharding::OverlyLargeShardIndex
  COMMAND $<TARGET_FILE:SelfTest>  --shard-index 5 --shard-count 5
)
set_tests_properties(TestSharding::OverlyLargeShardIndex
  PROPERTIES
    PASS_REGULAR_EXPRESSION "The shard count \\(5\\) must be greater than the shard index \\(5\\)"
)

set(CATCH_SHARDING_WARNING_REGEX "Warning: using sharding .* with random order")

add_test(
  NAME TestSharding::WarningOnRandomOrderWithoutFixedSeed
  COMMAND $<TARGET_FILE:SelfTest> --shard-index 0 --shard-count 2 --list-tests
)
set_tests_properties(TestSharding::WarningOnRandomOrderWithoutFixedSeed
  PROPERTIES
    PASS_REGULAR_EXPRESSION "${CATCH_SHARDING_WARNING_REGEX}"
)

add_test(
  NAME TestSharding::NoWarningOnRandomOrderWithFixedSeed
  COMMAND $<TARGET_FILE:SelfTest> --shard-index 0 --shard-count 2 --rng-seed 12345 --list-tests
)
set_tests_properties(TestSharding::NoWarningOnRandomOrderWithFixedSeed
  PROPERTIES
    FAIL_REGULAR_EXPRESSION "${CATCH_SHARDING_WARNING_REGEX}"
)

foreach(shardOrder decl lex)
  add_test(
    NAME TestSharding::NoWarningOn${shardOrder}OrderWithoutFixedSeed
    COMMAND $<TARGET_FILE:SelfTest> --shard-index 0 --shard-count 2 --order ${shardOrder} --list-tests
  )
  set_tests_properties(TestSharding::NoWarningOn${shardOrder}OrderWithoutFixedSeed
    PROPERTIES
      FAIL_REGULAR_EXPRESSION "${CATCH_SHARDING_WARNING_REGEX}"
  )
endforeach()

# The MinDuration reporting tests do not need separate compilation, but
# they have non-trivial execution time, so they are categorized as
# extra tests, so that they are run less.
add_test(NAME MinDuration::SimpleThreshold COMMAND $<TARGET_FILE:SelfTest> --min-duration 0.950 [min_duration_test])
set_tests_properties(MinDuration::SimpleThreshold
  PROPERTIES
    PASS_REGULAR_EXPRESSION "s: sleep_for_1000ms"
    FAIL_REGULAR_EXPRESSION "sleep_for_100ms"
    RUN_SERIAL ON  # The test is timing sensitive, so we want to run it
    # serially to avoid false positives on oversubscribed machines
)

# -d yes overrides the threshold, so we should see the faster test even
# with a ridiculous high min duration threshold
add_test(NAME MinDuration::DurationOverrideYes COMMAND $<TARGET_FILE:SelfTest> --min-duration 1.0 -d yes [min_duration_test])
set_tests_properties(MinDuration::DurationOverrideYes
  PROPERTIES
    PASS_REGULAR_EXPRESSION "s: sleep_for_100ms"
)

# -d no overrides the threshold, so we should never see any tests even
# with ridiculously low min duration threshold
add_test(NAME MinDuration::DurationOverrideNo COMMAND $<TARGET_FILE:SelfTest> --min-duration 0.0001 -d no [min_duration_test])
set_tests_properties(MinDuration::DurationOverrideNo
  PROPERTIES
    FAIL_REGULAR_EXPRESSION "sleep_for_250ms"
)

# ------------ end of duration reporting tests

# define folders used:
set(TESTS_DIR ${CATCH_DIR}/tests/ExtraTests)

add_executable(PrefixedMacros ${TESTS_DIR}/X01-PrefixedMacros.cpp)
target_compile_definitions(PrefixedMacros PRIVATE CATCH_CONFIG_PREFIX_ALL CATCH_CONFIG_RUNTIME_STATIC_REQUIRE)
# We want to verify that the main library can also be built with prefixed
# macros, regression test for #3087.
target_link_libraries(PrefixedMacros Catch2_buildall_interface)

add_test(NAME CATCH_CONFIG_PREFIX_ALL COMMAND PrefixedMacros -s)
set_tests_properties(CATCH_CONFIG_PREFIX_ALL
  PROPERTIES
    PASS_REGULAR_EXPRESSION "CATCH_"
    FAIL_REGULAR_EXPRESSION
    # The spaces are important -> They disambiguate between CATCH_REQUIRE
    # and REQUIRE without prefix.
    " REQUIRE; REQUIRE_FALSE; REQUIRE_THROWS; REQUIRE_THROWS_AS; REQUIRE_THROWS_WITH; REQUIRE_THROWS_MATCHES; REQUIRE_NOTHROW; CHECK; CHECK_FALSE; CHECKED_IF; CHECKED_ELSE; CHECK_NOFAIL; CHECK_THROWS; CHECK_THROWS_AS; CHECK_THROWS_WITH; CHECK_THROWS_MATCHES; CHECK_NOTHROW; REQUIRE_THAT; CHECK_THAT"
)

add_executable(DisabledMacros ${TESTS_DIR}/X02-DisabledMacros.cpp)
target_compile_definitions(DisabledMacros PRIVATE CATCH_CONFIG_DISABLE)
# Macro configuration does not touch the compiled parts, so we can link
# it against the main library
target_link_libraries(DisabledMacros Catch2WithMain)

add_test(NAME CATCH_CONFIG_DISABLE-1 COMMAND DisabledMacros -s)
set_tests_properties(CATCH_CONFIG_DISABLE-1
  PROPERTIES
    PASS_REGULAR_EXPRESSION "No tests ran"
    FAIL_REGULAR_EXPRESSION "This should not happen"
)
add_test(NAME CATCH_CONFIG_DISABLE-2 COMMAND DisabledMacros --list-tests)
set_tests_properties(CATCH_CONFIG_DISABLE-2
  PROPERTIES
    PASS_REGULAR_EXPRESSION "0 test cases"
)

add_executable(DisabledExceptions-DefaultHandler ${TESTS_DIR}/X03-DisabledExceptions-DefaultHandler.cpp)
add_executable(DisabledExceptions-CustomHandler ${TESTS_DIR}/X04-DisabledExceptions-CustomHandler.cpp)

foreach(target DisabledExceptions-DefaultHandler DisabledExceptions-CustomHandler)
  target_compile_options(${target}
    PUBLIC
      $<$<CXX_COMPILER_ID:MSVC>:/EHs-c-;/D_HAS_EXCEPTIONS=0>
      $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:QCC>,$<CXX_COMPILER_ID:AppleClang>>:-fno-exceptions>
  )
  target_link_libraries(${target} Catch2_buildall_interface)
endforeach()
target_compile_definitions(DisabledExceptions-CustomHandler PUBLIC CATCH_CONFIG_DISABLE_EXCEPTIONS_CUSTOM_HANDLER)


add_test(NAME CATCH_CONFIG_DISABLE_EXCEPTIONS-1 COMMAND DisabledExceptions-DefaultHandler "Tests that run")
set_tests_properties(CATCH_CONFIG_DISABLE_EXCEPTIONS-1
  PROPERTIES
    PASS_REGULAR_EXPRESSION "assertions: 4 \| 2 passed \| 2 failed"
    FAIL_REGULAR_EXPRESSION "abort;terminate;fatal"
)

# We must now test this works without the build flag.
add_executable(BazelReporterNoCatchConfig ${TESTS_DIR}/X30-BazelReporter.cpp)
target_link_libraries(BazelReporterNoCatchConfig Catch2WithMain)
add_test(NAME NO_CATCH_CONFIG_BAZEL_REPORTER-1
  COMMAND
  Python3::Interpreter "${CATCH_DIR}/tests/TestScripts/testBazelReporter.py" $<TARGET_FILE:BazelReporterNoCatchConfig> "${CMAKE_CURRENT_BINARY_DIR}"
)
set_tests_properties(NO_CATCH_CONFIG_BAZEL_REPORTER-1
  PROPERTIES
    LABELS "uses-python"
    ENVIRONMENT "BAZEL_TEST=1"
)

add_test(NAME BazelEnv::TESTBRIDGE_TEST_ONLY
  COMMAND
      $<TARGET_FILE:BazelReporterNoCatchConfig>
)
set_tests_properties(BazelEnv::TESTBRIDGE_TEST_ONLY
  PROPERTIES
    ENVIRONMENT "BAZEL_TEST=1;TESTBRIDGE_TEST_ONLY=Passing test case"
    PASS_REGULAR_EXPRESSION "All tests passed \\(1 assertion in 1 test case\\)"
)

add_test(NAME BazelEnv::Sharding
  COMMAND
    Python3::Interpreter "${CATCH_DIR}/tests/TestScripts/testBazelSharding.py"
      $<TARGET_FILE:BazelReporterNoCatchConfig>
      "${CMAKE_CURRENT_BINARY_DIR}"
)
set_tests_properties(BazelEnv::Sharding
  PROPERTIES
    LABELS "uses-python"
)

# The default handler on Windows leads to the just-in-time debugger firing,
# which makes this test unsuitable for CI and headless runs, as it opens
# up an interactive dialog.
if(NOT WIN32)
  add_test(NAME CATCH_CONFIG_DISABLE_EXCEPTIONS-2 COMMAND DisabledExceptions-DefaultHandler "Tests that abort")
  set_tests_properties(CATCH_CONFIG_DISABLE_EXCEPTIONS-2
    PROPERTIES
      PASS_REGULAR_EXPRESSION "Catch will terminate"
  )
endif()

add_test(NAME CATCH_CONFIG_DISABLE_EXCEPTIONS-3 COMMAND DisabledExceptions-CustomHandler "Tests that run")
set_tests_properties(CATCH_CONFIG_DISABLE_EXCEPTIONS-3
  PROPERTIES
    PASS_REGULAR_EXPRESSION "assertions: 4 \| 2 passed \| 2 failed"
    FAIL_REGULAR_EXPRESSION "====== CUSTOM HANDLER ======"
)

add_test(NAME CATCH_CONFIG_DISABLE_EXCEPTIONS-4 COMMAND DisabledExceptions-CustomHandler "Tests that abort")
set_tests_properties(CATCH_CONFIG_DISABLE_EXCEPTIONS-4
  PROPERTIES
    PASS_REGULAR_EXPRESSION "====== CUSTOM HANDLER ======"
)

add_executable(DeferredStaticChecks ${TESTS_DIR}/X05-DeferredStaticChecks.cpp)
target_link_libraries(DeferredStaticChecks PRIVATE Catch2WithMain)
target_compile_definitions(DeferredStaticChecks PRIVATE "CATCH_CONFIG_RUNTIME_STATIC_REQUIRE")

add_test(NAME DeferredStaticChecks COMMAND DeferredStaticChecks -r compact)
set_tests_properties(DeferredStaticChecks
  PROPERTIES
    PASS_REGULAR_EXPRESSION "test cases: 1 \\| 1 failed\nassertions: 3 \\| 3 failed;test cases: 1 \\| 1 failed\nassertions: 4 \\| 4 failed"
)

add_executable(MixingClearedAndUnclearedMessages ${TESTS_DIR}/X06-MixingClearedAndUnclearedMessages.cpp)
target_link_libraries(MixingClearedAndUnclearedMessages PRIVATE Catch2WithMain)

add_test(NAME MixingClearedAndUnclearedMessages
  COMMAND
    MixingClearedAndUnclearedMessages
    -r compact)
set_tests_properties(MixingClearedAndUnclearedMessages
  PROPERTIES
    PASS_REGULAR_EXPRESSION ": false with 1 message: 'b'"
)

add_executable(FallbackStringifier ${TESTS_DIR}/X10-FallbackStringifier.cpp)
target_compile_definitions(FallbackStringifier PRIVATE CATCH_CONFIG_FALLBACK_STRINGIFIER=fallbackStringifier)
target_link_libraries(FallbackStringifier Catch2WithMain)

add_test(NAME FallbackStringifier COMMAND FallbackStringifier -r compact -s)
set_tests_properties(FallbackStringifier
  PROPERTIES
    PASS_REGULAR_EXPRESSION "foo{} for: { !!! }"
)

add_executable(DisableStringification ${TESTS_DIR}/X11-DisableStringification.cpp)
target_compile_definitions(DisableStringification PRIVATE CATCH_CONFIG_DISABLE_STRINGIFICATION)
target_link_libraries(DisableStringification Catch2WithMain)
add_test(NAME CATCH_CONFIG_DISABLE_STRINGIFICATION COMMAND DisableStringification -r compact -s)
set_tests_properties(CATCH_CONFIG_DISABLE_STRINGIFICATION
  PROPERTIES
    PASS_REGULAR_EXPRESSION "Disabled by CATCH_CONFIG_DISABLE_STRINGIFICATION"
    FAIL_REGULAR_EXPRESSION  "Hidden{} == Hidden{}"
)

# This test touches windows.h, so it should only be compiled under msvc
if(MSVC)
  # This test fails if it does not compile and succeeds otherwise
  add_executable(WindowsHeader ${TESTS_DIR}/X90-WindowsHeaderInclusion.cpp)
  target_link_libraries(WindowsHeader Catch2WithMain)
  add_test(NAME WindowsHeader COMMAND WindowsHeader -r compact)
  list(APPEND CATCH_TEST_TARGETS ${EXTRA_TEST_BINARIES} WindowsHeader)
endif()

add_executable(PartialTestCaseEvents ${TESTS_DIR}/X21-PartialTestCaseEvents.cpp)
target_link_libraries(PartialTestCaseEvents PRIVATE Catch2WithMain)
add_test(
  NAME PartialTestCaseEvents
  COMMAND Python3::Interpreter ${CATCH_DIR}/tests/TestScripts/testPartialTestCaseEvent.py $<TARGET_FILE:PartialTestCaseEvents>
)
set_tests_properties(PartialTestCaseEvents
  PROPERTIES
    LABELS "uses-python"
)

add_executable(BenchmarksInCumulativeReporter ${TESTS_DIR}/X22-BenchmarksInCumulativeReporter.cpp)
target_link_libraries(BenchmarksInCumulativeReporter PRIVATE Catch2::Catch2WithMain)
add_test(
  NAME BenchmarksInCumulativeReporter
  COMMAND BenchmarksInCumulativeReporter --reporter testReporter
)
set_tests_properties(BenchmarksInCumulativeReporter
  PROPERTIES
    PASS_REGULAR_EXPRESSION "1\n2\n3\n4\n5\n"
    COST 30
)

add_executable(CasingInReporterNames ${TESTS_DIR}/X23-CasingInReporterNames.cpp)
target_link_libraries(CasingInReporterNames PRIVATE Catch2::Catch2WithMain)
add_test(
  NAME Reporters::registration-is-case-preserving
  COMMAND CasingInReporterNames --list-reporters
)
set_tests_properties(Reporters::registration-is-case-preserving
  PROPERTIES
    PASS_REGULAR_EXPRESSION "testReporterCASED"
)
add_test(
  NAME Reporters::selection-is-case-insensitive
  COMMAND  CasingInReporterNames -r testReportercased
)
set_tests_properties(Reporters::selection-is-case-insensitive
  PROPERTIES
    PASS_REGULAR_EXPRESSION "TestReporter constructed"
)

add_executable(CapturedStdoutInTestCaseEvents ${TESTS_DIR}/X27-CapturedStdoutInTestCaseEvents.cpp)
target_link_libraries(CapturedStdoutInTestCaseEvents PRIVATE Catch2::Catch2WithMain)
add_test(
  NAME Reporters::CapturedStdOutInEvents
  COMMAND CapturedStdoutInTestCaseEvents
    --reporter test-reporter
)
set_tests_properties(Reporters::CapturedStdOutInEvents
  PROPERTIES
    PASS_REGULAR_EXPRESSION "X27 - TestReporter constructed"
    FAIL_REGULAR_EXPRESSION "X27 ERROR"
)

if(WIN32)
  set(_NullFile "NUL")
else()
  set(_NullFile "/dev/null")
endif()

add_executable(ListenerStdoutCaptureInMultireporter ${TESTS_DIR}/X24-ListenerStdoutCaptureInMultireporter.cpp)
target_link_libraries(ListenerStdoutCaptureInMultireporter PRIVATE Catch2::Catch2WithMain)

# This test checks that there is nothing written out from the process,
# but if CMake is running the tests under Valgrind or similar tool, then
# that will write its own output to stdout and the test would fail.
if(NOT MEMORYCHECK_COMMAND)
  add_test(
    NAME MultiReporter::NoncapturingListenerDoesntCauseStdoutPassThrough
    COMMAND ListenerStdoutCaptureInMultireporter
      --reporter xml::out=${_NullFile}
      --reporter junit::out=${_NullFile}
  )

  set_tests_properties(MultiReporter::NoncapturingListenerDoesntCauseStdoutPassThrough
    PROPERTIES
      PASS_REGULAR_EXPRESSION "X24 - NonCapturingListener initialized"
      FAIL_REGULAR_EXPRESSION "X24 - FooBarBaz"
  )
endif()

add_executable(ListenerCanAskForCapturedStdout ${TESTS_DIR}/X25-ListenerCanAskForCapturedStdout.cpp)
target_link_libraries(ListenerCanAskForCapturedStdout PRIVATE Catch2::Catch2WithMain)
add_test(
  NAME MultiReporter::CapturingListenerCausesStdoutCapture
  COMMAND ListenerCanAskForCapturedStdout
    --reporter compact::out=${_NullFile}
    --reporter console::out=${_NullFile}
)
set_tests_properties(MultiReporter::CapturingListenerCausesStdoutCapture
  PROPERTIES
    PASS_REGULAR_EXPRESSION "CapturingListener initialized"
    FAIL_REGULAR_EXPRESSION "X25 - ERROR"
)

add_executable(ReporterPreferencesForPassingAssertionsIsRespected ${TESTS_DIR}/X26-ReporterPreferencesForPassingAssertionsIsRespected.cpp)
target_link_libraries(ReporterPreferencesForPassingAssertionsIsRespected PRIVATE Catch2::Catch2WithMain)
add_test(
  NAME Reporters::PreferencesForPassingAssertionsIsRespected
  COMMAND ReporterPreferencesForPassingAssertionsIsRespected
    --reporter test-reporter
)
set_tests_properties(Reporters::PreferencesForPassingAssertionsIsRespected
  PROPERTIES
    PASS_REGULAR_EXPRESSION "X26 - TestReporter constructed"
    FAIL_REGULAR_EXPRESSION "X26 - assertionEnded"
)
add_test(
  NAME MultiReporter::PreferencesForPassingAssertionsIsRespected
  COMMAND ReporterPreferencesForPassingAssertionsIsRespected
    --reporter test-reporter
    --reporter console::out=${_NullFile}
)
set_tests_properties(MultiReporter::PreferencesForPassingAssertionsIsRespected
  PROPERTIES
    PASS_REGULAR_EXPRESSION "X26 - TestReporter constructed"
    FAIL_REGULAR_EXPRESSION "X26 - assertionEnded"
)

add_executable(ListenersGetEventsBeforeReporters ${TESTS_DIR}/X28-ListenersGetEventsBeforeReporters.cpp)
target_link_libraries(ListenersGetEventsBeforeReporters PRIVATE Catch2::Catch2WithMain)
add_test(
  NAME ListenersGetEventsBeforeReporters
  COMMAND ListenersGetEventsBeforeReporters --reporter test-reporter
)
set_tests_properties(ListenersGetEventsBeforeReporters
  PROPERTIES
    PASS_REGULAR_EXPRESSION "X28 - TestReporter constructed"
    FAIL_REGULAR_EXPRESSION "X28 - ERROR"
)

add_executable(CustomArgumentsForReporters ${TESTS_DIR}/X29-CustomArgumentsForReporters.cpp)
target_link_libraries(CustomArgumentsForReporters PRIVATE Catch2::Catch2WithMain)
add_test(
  NAME CustomArgumentsForReporters
  COMMAND CustomArgumentsForReporters
    --reporter "test-reporter::Xa b=c 1::Xz:e = 1234"
)
set_tests_properties(CustomArgumentsForReporters
  PROPERTIES
    PASS_REGULAR_EXPRESSION "Xa b=c 1::Xz:e = 1234"
)

add_executable(TestRunInfoTestCaseCount ${TESTS_DIR}/X39-TestRunInfoTestCaseCount.cpp)
target_link_libraries(TestRunInfoTestCaseCount PRIVATE Catch2::Catch2WithMain)
add_test(
  NAME Listeners::TestRunInfoExposesVisibleTestCount
  COMMAND TestRunInfoTestCaseCount
)
set_tests_properties(Listeners::TestRunInfoExposesVisibleTestCount
  PROPERTIES
    PASS_REGULAR_EXPRESSION "X39 - testCaseCount: 3"
)

add_test(
  NAME Listeners::TestRunInfoExposesFilteredDeduplicatedTestCount
  COMMAND TestRunInfoTestCaseCount [x39a],[x39b]
)
set_tests_properties(Listeners::TestRunInfoExposesFilteredDeduplicatedTestCount
  PROPERTIES
    PASS_REGULAR_EXPRESSION "X39 - testCaseCount: 3"
)

add_executable(DuplicatedTestCases-SameNameAndTags ${TESTS_DIR}/X31-DuplicatedTestCases.cpp)
target_link_libraries(DuplicatedTestCases-SameNameAndTags PRIVATE Catch2::Catch2WithMain)
add_test(
  NAME DuplicatedTestCases::SameNameAndTags
  COMMAND $<TARGET_FILE:DuplicatedTestCases-SameNameAndTags>
)
set_tests_properties(DuplicatedTestCases::SameNameAndTags
  PROPERTIES
     PASS_REGULAR_EXPRESSION "error: .* already defined\\."
)

add_executable(DuplicatedTestCases-SameNameDifferentTags ${TESTS_DIR}/X32-DuplicatedTestCasesDifferentTags.cpp)
target_link_libraries(DuplicatedTestCases-SameNameDifferentTags PRIVATE Catch2::Catch2WithMain)
add_test(
  NAME DuplicatedTestCases::SameNameDifferentTags
  COMMAND $<TARGET_FILE:DuplicatedTestCases-SameNameDifferentTags>
)
set_tests_properties(DuplicatedTestCases::SameNameDifferentTags
  PROPERTIES
     FAIL_REGULAR_EXPRESSION "error: .* already defined\\."
)

add_executable(DuplicatedTestCases-DuplicatedTestCaseMethods ${TESTS_DIR}/X33-DuplicatedTestCaseMethods.cpp)
target_link_libraries(DuplicatedTestCases-DuplicatedTestCaseMethods PRIVATE Catch2::Catch2WithMain)
add_test(
  NAME DuplicatedTestCases::DuplicatedTestCaseMethods
  COMMAND $<TARGET_FILE:DuplicatedTestCases-DuplicatedTestCaseMethods>
)
set_tests_properties(DuplicatedTestCases::DuplicatedTestCaseMethods
  PROPERTIES
     PASS_REGULAR_EXPRESSION "error: .* already defined\\."
)

add_executable(DuplicatedTestCases-DifferentFixtures ${TESTS_DIR}/X34-DuplicatedTestCaseMethodsDifferentFixtures.cpp)
target_link_libraries(DuplicatedTestCases-DifferentFixtures PRIVATE Catch2::Catch2WithMain)
add_test(
  NAME DuplicatedTestCases::DuplicatedTestCaseMethodsDifferentFixtures
  COMMAND $<TARGET_FILE:DuplicatedTestCases-DifferentFixtures>
)
set_tests_properties(DuplicatedTestCases::DuplicatedTestCaseMethodsDifferentFixtures
  PROPERTIES
    FAIL_REGULAR_EXPRESSION "error: .* already defined\\."
)

add_executable(DuplicatedReporters ${TESTS_DIR}/X35-DuplicatedReporterNames.cpp)
target_link_libraries(DuplicatedReporters PRIVATE Catch2::Catch2WithMain)
add_test(
  NAME Reporters::RegistrationErrorsAreCaught
  COMMAND $<TARGET_FILE:DuplicatedReporters>
)
set_tests_properties(Reporters::RegistrationErrorsAreCaught
  PROPERTIES
    PASS_REGULAR_EXPRESSION "Errors occurred during startup!"
)

add_executable(ReportingCrashWithJunitReporter ${TESTS_DIR}/X36-ReportingCrashWithJunitReporter.cpp)
target_link_libraries(ReportingCrashWithJunitReporter PRIVATE Catch2::Catch2WithMain)
add_test(
  NAME Reporters::CrashInJunitReporter
  COMMAND ${CMAKE_COMMAND} -E env $<TARGET_FILE:ReportingCrashWithJunitReporter> --reporter JUnit
)
set_tests_properties(Reporters::CrashInJunitReporter
  PROPERTIES
    PASS_REGULAR_EXPRESSION "</testsuites>"
    LABELS "uses-signals"
)


add_executable(ReportingInvalidTestCaseTags ${TESTS_DIR}/X37-ReportingInvalidTestCaseTags.cpp)
target_link_libraries(ReportingInvalidTestCaseTags PRIVATE Catch2::Catch2WithMain)
add_test(
  NAME Validation::TestCaseTags
  COMMAND $<TARGET_FILE:ReportingInvalidTestCaseTags>
)
# Passing multiple PASS_REGULAR_EXPRESSION does not work, so we join multiple ones with '.*'
set(regexp
  "Found an empty tag while registering test case 'Empty tag is not allowed'"
  "Tag name: \\[!abc\\] is not allowed."
  "Tag names starting with non-alphanumeric characters are reserved."
  "Tag name: \\[@def\\] is not allowed."
  "Tag names starting with non-alphanumeric characters are reserved."
)
list(JOIN regexp ".*" regexp)
set_tests_properties(Validation::TestCaseTags
  PROPERTIES
    PASS_REGULAR_EXPRESSION ${regexp}
)


add_executable(ThrowingReporter ${TESTS_DIR}/X38-ThrowingReporter.cpp)
target_link_libraries(ThrowingReporter PRIVATE Catch2::Catch2WithMain)
add_test(
  NAME Reporters::ThrowInConstructor
  COMMAND
    $<TARGET_FILE:ThrowingReporter>
      --reporter ThrowingReporter::Xthrow-in-constructor=true
)
set_tests_properties(Reporters::ThrowInConstructor
  PROPERTIES
    PASS_REGULAR_EXPRESSION "Error when creating reporter: Exception in constructor"
)

foreach(reporterName   "Automake"
                       "compact"
                       "console"
                       "JUnit"
                       "SonarQube"
                       "TAP"
                       "TeamCity"
                       "XML"
                       "JSON")
  add_test(NAME "Reporters::RejectUnknownArgs::${reporterName}"
    COMMAND
      $<TARGET_FILE:SelfTest>
        --reporter ${reporterName}::Xfoo=bar
        "[comparisons]" # Limit selection to avoid running full test binary if the error check doesn't trigger
  )
  set_tests_properties("Reporters::RejectUnknownArgs::${reporterName}"
    PROPERTIES
      PASS_REGULAR_EXPRESSION "Error when creating reporter: Unknown reporter argument: Xfoo"
  )
endforeach()


add_executable(QuickExitInTest ${TESTS_DIR}/X40-QuickExit.cpp)
target_link_libraries(QuickExitInTest PRIVATE Catch2::Catch2WithMain)
add_test(
  NAME BazelEnv::TEST_PREMATURE_EXIT_FILE
  COMMAND
    Python3::Interpreter
    "${CATCH_DIR}/tests/TestScripts/testBazelExitGuardFile.py"
    $<TARGET_FILE:QuickExitInTest>
    "${CMAKE_CURRENT_BINARY_DIR}"
    "bazel"
)
set_tests_properties(BazelEnv::TEST_PREMATURE_EXIT_FILE
  PROPERTIES
    LABELS "uses-python"
)
add_test(
  NAME PrematureExitGuardFileCanBeUsedFromCLI::CheckAfterCrash
  COMMAND
    Python3::Interpreter
    "${CATCH_DIR}/tests/TestScripts/testBazelExitGuardFile.py"
    $<TARGET_FILE:QuickExitInTest>
    "${CMAKE_CURRENT_BINARY_DIR}"
    "cli"
)
set_tests_properties(PrematureExitGuardFileCanBeUsedFromCLI::CheckAfterCrash
  PROPERTIES
    LABELS "uses-python"
)
add_test(
  NAME PrematureExitGuardFileCanBeUsedFromCLI::CheckWithoutCrash
  COMMAND
    Python3::Interpreter
    "${CATCH_DIR}/tests/TestScripts/testBazelExitGuardFile.py"
    $<TARGET_FILE:QuickExitInTest>
    "${CMAKE_CURRENT_BINARY_DIR}"
    "no-crash"
)
set_tests_properties(PrematureExitGuardFileCanBeUsedFromCLI::CheckWithoutCrash
  PROPERTIES
    LABELS "uses-python"
)


add_executable(AssertionStartingEventGoesBeforeAssertionIsEvaluated
  X20-AssertionStartingEventGoesBeforeAssertionIsEvaluated.cpp
)
target_link_libraries(AssertionStartingEventGoesBeforeAssertionIsEvaluated
  PRIVATE Catch2::Catch2WithMain
)
add_test(
  NAME ReporterEvents::AssertionStartingHappensBeforeAssertionIsEvaluated
  COMMAND $<TARGET_FILE:AssertionStartingEventGoesBeforeAssertionIsEvaluated>
)

#add_executable(DebugBreakMacros ${TESTS_DIR}/X12-CustomDebugBreakMacro.cpp)
#target_link_libraries(DebugBreakMacros Catch2)
#add_test(NAME DebugBreakMacros COMMAND DebugBreakMacros --break)
#set_tests_properties(DebugBreakMacros
#  PROPERTIES
#    PASS_REGULAR_EXPRESSION "Pretty please, break into debugger"
#)

add_executable(NoTests ${TESTS_DIR}/X92-NoTests.cpp)
target_link_libraries(NoTests PRIVATE Catch2::Catch2WithMain)

add_test(
  NAME TestSpecs::EmptySpecWithNoTestsFails
  COMMAND $<TARGET_FILE:NoTests>
)
set_tests_properties(TestSpecs::EmptySpecWithNoTestsFails
  PROPERTIES
    WILL_FAIL ON
)

add_test(
  NAME TestSpecs::OverrideFailureWithEmptySpec
  COMMAND $<TARGET_FILE:NoTests> --allow-running-no-tests
)

add_test(
  NAME List::Listeners::WorksWithoutRegisteredListeners
  COMMAND $<TARGET_FILE:NoTests> --list-listeners
)

add_executable(AllSkipped ${TESTS_DIR}/X93-AllSkipped.cpp)
target_link_libraries(AllSkipped PRIVATE Catch2::Catch2WithMain)

add_test(
  NAME TestSpecs::SkippingAllTestsFails
  COMMAND $<TARGET_FILE:AllSkipped>
)
set_tests_properties(TestSpecs::SkippingAllTestsFails
  PROPERTIES
    WILL_FAIL ON
)

add_executable(FastCompileMacros ${TESTS_DIR}/X07-FastCompileMacros.cpp)
target_link_libraries(FastCompileMacros PRIVATE Catch2_buildall_interface)
target_compile_definitions(FastCompileMacros PRIVATE CATCH_CONFIG_FAST_COMPILE)

add_test(
  NAME CompileConfiguration::FastCompile
  COMMAND $<TARGET_FILE:FastCompileMacros>
)
set_tests_properties(CompileConfiguration::FastCompile
  PROPERTIES
    PASS_REGULAR_EXPRESSION "test cases:  6 \\| 1 passed \\| 1 failed \\| 4 failed as expected\nassertions: 13 \\| 6 passed \\| 2 failed \\| 5 failed as expected"
)

# This sets up a one-off executable that compiles against the amalgamated
# files, and then runs it for a super simple check that the amalgamated
# files are usable.
add_executable(AmalgamatedTestCompilation
  ${TESTS_DIR}/X91-AmalgamatedCatch.cpp
  ${CATCH_DIR}/extras/catch_amalgamated.hpp
  ${CATCH_DIR}/extras/catch_amalgamated.cpp
)
target_include_directories(AmalgamatedTestCompilation PRIVATE ${CATCH_DIR}/extras)
target_compile_features(AmalgamatedTestCompilation PRIVATE cxx_std_14)

add_test(NAME AmalgamatedFileTest COMMAND AmalgamatedTestCompilation)
set_tests_properties(AmalgamatedFileTest
  PROPERTIES
    PASS_REGULAR_EXPRESSION "All tests passed \\(14 assertions in 3 test cases\\)"
)

add_executable(ThreadSafetyTests
  ${TESTS_DIR}/X94-ThreadSafetyTests.cpp
)
target_link_libraries(ThreadSafetyTests Catch2_buildall_interface)
target_compile_definitions(ThreadSafetyTests PUBLIC CATCH_CONFIG_THREAD_SAFE_ASSERTIONS)

add_test(NAME ThreadSafetyTests::ScopedMessagesAndAssertions
  COMMAND ThreadSafetyTests -r compact "Failed REQUIRE in the main thread is fine"
)
set_tests_properties(ThreadSafetyTests::ScopedMessagesAndAssertions
  PROPERTIES
    PASS_REGULAR_EXPRESSION "assertions: 801 \\| 400 passed \\| 401 failed as expected"
    RUN_SERIAL ON
)
add_test(NAME ThreadSafetyTests::UnscopedMessagesAndAssertions
  COMMAND ThreadSafetyTests -r compact "Using unscoped messages in sibling threads"
)
set_tests_properties(ThreadSafetyTests::UnscopedMessagesAndAssertions
  PROPERTIES
    PASS_REGULAR_EXPRESSION "assertions: 401 \\| 401 failed as expected"
    RUN_SERIAL ON
)

add_executable(InfiniteGenerators ${TESTS_DIR}/X95-InfiniteGenerators.cpp)
target_link_libraries(InfiniteGenerators PRIVATE Catch2::Catch2WithMain)

add_test(
  NAME Warnings::InfiniteGenerators::NoFilterWarns
  COMMAND $<TARGET_FILE:InfiniteGenerators> --warn InfiniteGenerators
)
set_tests_properties(Warnings::InfiniteGenerators::NoFilterWarns
  PROPERTIES
    # One test case fails with infinite generator, but the other one runs
    PASS_REGULAR_EXPRESSION "test cases: 2 \\| 1 passed \\| 1 failed"
    TIMEOUT 5
)

add_test(
  NAME Warnings::InfiniteGenerators::MatchAllFilterWarns
  COMMAND $<TARGET_FILE:InfiniteGenerators>
    --warn InfiniteGenerators
    --path-filter g:*
)
set_tests_properties(Warnings::InfiniteGenerators::MatchAllFilterWarns
  PROPERTIES
    # One test case fails with infinite generator, but the other one runs
    PASS_REGULAR_EXPRESSION "test cases: 2 \\| 1 passed \\| 1 failed"
    TIMEOUT 5
)

add_test(
  NAME Warnings::InfiniteGenerators::MatchOneFilterDoesntWarn
  COMMAND $<TARGET_FILE:InfiniteGenerators>
    --warn InfiniteGenerators
    --path-filter g:1
)
set_tests_properties(Warnings::InfiniteGenerators::MatchOneFilterDoesntWarn
  PROPERTIES
    # One test case fails with infinite generator, but the other one runs
    PASS_REGULAR_EXPRESSION "All tests passed \\(1 assertion in 2 test cases\\)"
    TIMEOUT 5
)

add_test(
  NAME Warnings::InfiniteGenerators::SectionFilterWarns
  COMMAND $<TARGET_FILE:InfiniteGenerators>
    --warn InfiniteGenerators
    --section FooBarBaz
)
set_tests_properties(Warnings::InfiniteGenerators::SectionFilterWarns
  PROPERTIES
    # One test case fails with infinite generator, but the other one runs
    PASS_REGULAR_EXPRESSION "test cases: 2 \\| 1 passed \\| 1 failed"
    TIMEOUT 5
)


set(EXTRA_TEST_BINARIES
  AllSkipped
  PrefixedMacros
  DisabledMacros
  DisabledExceptions-DefaultHandler
  DisabledExceptions-CustomHandler
  FallbackStringifier
  DisableStringification
  PartialTestCaseEvents
  DuplicatedTestCases-SameNameAndTags
  DuplicatedTestCases-SameNameDifferentTags
  DuplicatedTestCases-DuplicatedTestCaseMethods
  NoTests
  ListenersGetEventsBeforeReporters
  MixingClearedAndUnclearedMessages
  FastCompileMacros
  InfiniteGenerators
  ThreadSafetyTests
#  DebugBreakMacros
)

# Notice that we are modifying EXTRA_TEST_BINARIES destructively, do not
# use it after this point!
list(FILTER EXTRA_TEST_BINARIES EXCLUDE REGEX "DisabledExceptions.*")
list(APPEND CATCH_TEST_TARGETS ${EXTRA_TEST_BINARIES})
set(CATCH_TEST_TARGETS ${CATCH_TEST_TARGETS} PARENT_SCOPE)
