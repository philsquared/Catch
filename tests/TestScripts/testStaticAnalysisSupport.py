#!/usr/bin/env python3

#              Copyright Catch2 Authors
# Distributed under the Boost Software License, Version 1.0.
#   (See accompanying file LICENSE.txt or copy at
#        https://www.boost.org/LICENSE_1_0.txt)

# SPDX-License-Identifier: BSL-1.0

"""
Tests Catch2's static analysis support, by building the test project in
`tests/TestProjects/StaticAnalysis` under clang-tidy and comparing the warnings
against the `// expect-warning: <check>` markers in its sources.

Requires 2 arguments, path to folder where the Catch2's main CMakeLists.txt
exists, and path to where the output files should be stored. Optionally takes
the clang-tidy binary as a third argument, and the C++ compiler as a fourth one.
"""

import os
import re
import shutil
import subprocess
import sys
from typing import Dict, Set

# Catch2 models the early termination with a [[noreturn]] call, which
# bugprone-unchecked-optional-access only started honouring in clang-tidy 17.
MINIMAL_CLANG_TIDY_VERSION = 17

# Reported as a skip by CTest, see the test's SKIP_RETURN_CODE property.
SKIP_RETURN_CODE = 77

EXPECTED_WARNING_MARKER = '// expect-warning: <check>'

# Anchored, so that prose mentioning the marker is not mistaken for one.
expected_warning_regex = re.compile(r'//\s*expect-warning:\s*(?P<check>[\w.-]+)\s*$')
clang_tidy_version_regex = re.compile(r'LLVM version (\d+)\.')
# `<path>:<line>:<column>: warning: <message> [<check>]`
warning_regex = re.compile(
    r'^(?P<path>.+?):(?P<line>\d+):\d+:\s+warning:\s+.*\[(?P<check>[\w.-]+)\]$')


def get_clang_tidy_version(clang_tidy: str) -> int:
    try:
        result = subprocess.run([clang_tidy, '--version'],
                                capture_output = True,
                                check = True,
                                text = True)
    except OSError as err:
        print(f"Could not run '{clang_tidy} --version': {err}")
        exit(5)
    except subprocess.CalledProcessError as err:
        print(f"'{clang_tidy} --version' failed with {err.returncode}")
        print(f'stderr: {err.stderr}')
        print(f'stdout: {err.stdout}')
        exit(5)

    version_match = clang_tidy_version_regex.search(result.stdout)
    if not version_match:
        print('Could not find clang-tidy version in output')
        print(f"output: '{result.stdout}'")
        exit(5)
    return int(version_match.group(1))


def configure_and_build(sources_dir: str, build_dir: str, catch2_path: str,
                        clang_tidy: str, cxx_compiler: str) -> str:
    # An incremental build would only re-analyze what changed.
    shutil.rmtree(build_dir, ignore_errors = True)

    config_cmd = ['cmake',
                  '-B', build_dir,
                  '-S', sources_dir,
                  f'-DCATCH2_PATH={catch2_path}',
                  '-DCMAKE_BUILD_TYPE=Debug',
                  f'-DSTATIC_ANALYSIS_CLANG_TIDY={clang_tidy}']

    if cxx_compiler:
        config_cmd.append(f'-DCMAKE_CXX_COMPILER={cxx_compiler}')

    build_cmd = ['cmake',
                 '--build', build_dir,
                 '--config', 'Debug',
                 '--parallel', str(os.cpu_count() or 1)]

    try:
        subprocess.run(config_cmd,
                       capture_output = True,
                       check = True,
                       text = True)
    except subprocess.CalledProcessError as err:
        print('Error when configuring the test project')
        print(f'cmd: {err.cmd}')
        print(f'stderr: {err.stderr}')
        print(f'stdout: {err.stdout}')
        exit(3)

    result = subprocess.run(build_cmd,
                            stdout = subprocess.PIPE,
                            stderr = subprocess.STDOUT,
                            check = False,
                            text = True)
    if result.returncode != 0:
        print('Error when building the test project')
        print(f'cmd: {build_cmd}')
        print(f'exit code: {result.returncode}')
        print(f'output: {result.stdout}')
        exit(6)
    return result.stdout


# Expectations and warnings are both `{file name: {line number: check name}}`,
# keyed by the bare file name, so the sources have to stay in a flat directory.
def parse_expectations(sources_dir: str) -> Dict[str, Dict[int, str]]:
    expectations = {}
    for entry in sorted(os.listdir(sources_dir)):
        if not entry.endswith('.cpp'):
            continue
        marked = {}
        with open(os.path.join(sources_dir, entry), mode = 'r', encoding = 'utf-8') as file:
            for number, line in enumerate(file, start = 1):
                match = expected_warning_regex.search(line.rstrip())
                if match:
                    marked[number] = match.group('check')
        expectations[entry] = marked
    return expectations


def parse_warnings(build_output: str,
                   known_files: Set[str]) -> Dict[str, Dict[int, str]]:
    warnings = {file_name: {} for file_name in known_files}
    for line in build_output.splitlines():
        match = warning_regex.match(line.strip())
        if not match:
            continue
        # The compiler's own diagnostics look just like clang-tidy's, except
        # that they are tagged with a `-Wflag` instead of a check name.
        check = match.group('check')
        if check.startswith('-W'):
            continue
        # clang-tidy also reports through Catch2's own headers.
        file_name = os.path.basename(match.group('path'))
        if file_name not in warnings:
            continue
        warnings[file_name][int(match.group('line'))] = check
    return warnings


if __name__ == '__main__':
    if len(sys.argv) not in (3, 4, 5):
        print(f'Wrong number of arguments: {len(sys.argv)}')
        print(f'Usage: {sys.argv[0]} catch2-top-level-dir base-build-output-dir '
              '[clang-tidy-binary [cxx-compiler]]')
        exit(2)

    catch2_path = os.path.abspath(sys.argv[1])
    build_dir = os.path.join(os.path.abspath(sys.argv[2]), 'StaticAnalysisTests')
    # CMake passes an empty string for a cache variable that was blanked out.
    clang_tidy = sys.argv[3] if len(sys.argv) >= 4 and sys.argv[3] else 'clang-tidy'
    cxx_compiler = sys.argv[4] if len(sys.argv) == 5 else ''
    sources_dir = os.path.join(catch2_path, 'tests', 'TestProjects', 'StaticAnalysis')

    version = get_clang_tidy_version(clang_tidy)
    if version < MINIMAL_CLANG_TIDY_VERSION:
        print(f"'{clang_tidy}' is version {version}, but these tests need "
              f'{MINIMAL_CLANG_TIDY_VERSION} or newer.')
        print('bugprone-unchecked-optional-access ignores [[noreturn]] before '
              'clang-tidy 17, so the REQUIRE cases would warn just like the '
              'CHECK ones. Skipping.')
        exit(SKIP_RETURN_CODE)

    expectations = parse_expectations(sources_dir)
    if not expectations:
        print(f"Found no sources in '{sources_dir}'")
        exit(4)

    build_output = configure_and_build(sources_dir, build_dir, catch2_path,
                                       clang_tidy, cxx_compiler)
    warnings = parse_warnings(build_output, set(expectations))

    total_warnings = sum(len(lines) for lines in warnings.values())
    if total_warnings == 0:
        # Every source is expected to warn, so this would pass silently.
        print('clang-tidy reported no warnings at all, it likely did not run.')
        print(f'build output: "{build_output}"')
        exit(7)

    mismatched = 0
    for file_name in sorted(expectations):
        expected = expectations[file_name]
        actual = warnings[file_name]
        for line in sorted(set(actual) - set(expected)):
            print(f'{file_name}:{line}: unexpected [{actual[line]}] warning, '
                  f'the line is not marked with "{EXPECTED_WARNING_MARKER}"')
            mismatched += 1
        for line in sorted(set(expected) - set(actual)):
            print(f'{file_name}:{line}: missing [{expected[line]}] warning, '
                  f'the line is marked with "{EXPECTED_WARNING_MARKER}"')
            mismatched += 1
        for line in sorted(set(expected) & set(actual)):
            if expected[line] != actual[line]:
                print(f'{file_name}:{line}: expected a [{expected[line]}] warning, '
                      f'but got a [{actual[line]}] one')
                mismatched += 1

    if mismatched:
        print(f'Found {mismatched} mismatched warnings!')
        print(f'build output: "{build_output}"')
        exit(1)

    print(f'{total_warnings} warnings matched the expectations')
