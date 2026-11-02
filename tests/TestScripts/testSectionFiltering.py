#!/usr/bin/env python3

#              Copyright Catch2 Authors
# Distributed under the Boost Software License, Version 1.0.
#   (See accompanying file LICENSE.txt or copy at
#        https://www.boost.org/LICENSE_1_0.txt)

# SPDX-License-Identifier: BSL-1.0

"""
This test script verifies the behaviour of the legacy section filtering
using `-c`, `--section` CLI parameters.

This is done by having a hardcoded set of test filter + section filter
combinations, together with the expected number of assertions that will
be run inside the test for given filter combo.
"""

import os
import subprocess
import sys
from typing import Tuple, List
import xml.etree.ElementTree as ET

def make_old_filters(section_names: Tuple[str, ...]) -> List[str]:
    final = []
    for name in section_names:
        final.append('--section')
        final.append(name)
    return final

def make_new_filters(path_filters: Tuple[str, ...]) -> List[str]:
    final = []
    for component in path_filters:
        final.append('--path-filter')
        final.append(component)
    return final

def run_one_test(binary_path: str,
                 test_name: str,
                 section_names: Tuple[str, ...],
                 expected_assertions: int,
                 new_filters: bool,
                 expected_captures=None):
    cmd = [
      binary_path,
      '--reporter', 'xml',
      test_name
    ]
    if new_filters:
        cmd.extend(make_new_filters(section_names))
    else:
        cmd.extend(make_old_filters(section_names))
    if expected_captures is not None:
        cmd.append('--success')
    try:
        ret = subprocess.run(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=True,
            universal_newlines=True,
            timeout=10,
        )
        stdout = ret.stdout
    except subprocess.CalledProcessError as ex:
        print('Could not run "{}"'.format(cmd))
        print("Return code: {}".format(ex.returncode))
        print("stdout: {}".format(ex.stdout))
        print("stderr: {}".format(ex.stderr))
        raise

    try:
        tree = ET.fromstring(stdout)
    except ET.ParseError as ex:
        print("Invalid XML: '{}'".format(ex))
        raise

    # Validate that we ran exactly 1 test case, and it passed
    test_case_stats = tree.find('OverallResultsCases')
    expected_testcases = {'successes' : '1', 'failures' : '0', 'expectedFailures': '0', 'skips': '0'}
    assert test_case_stats.attrib == expected_testcases, f'We did not run single passing test case as expected. {test_name}: {test_case_stats.attrib}'

    # Validate that we got exactly the expected number of passing assertions
    expected_assertions = {'successes' : str(expected_assertions), 'failures' : '0', 'expectedFailures': '0', 'skips': '0'}
    assertion_stats = tree.find('OverallResults')
    assert assertion_stats.attrib == expected_assertions, f'"{test_name}": {assertion_stats.attrib} vs {expected_assertions}'
    if expected_captures is not None:
        captures = [info.text.strip() for info in tree.iter('Info')]
        assert captures == expected_captures, f'{cmd}: {captures} vs {expected_captures}'


# Inputs taken from issue #3038
old_section_tests = {
  'foo': (
    ((), 3),
    (('A',), 3),
    (('A', 'B'), 0),
    (('A', 'B1'), 1),
    (('A', 'B2'), 1),
    (('A', 'B1', 'B2'), 1),
    (('A', 'B2', 'XXXX'), 1),
  ),
  'bar': (
    ((), 9),
    (('A',), 9),
    (('A', 'B1'), 3),
    (('XXXX',), 2),
    (('B1',), 2),
    (('A', 'B1', 'B2'), 3),
  ),
  'baz': (
    ((), 4),
    (('A',), 1),
    (('A', 'B'), 1),
    (('A', 'XXXX'), 1),
    (('B',), 3),
    (('XXXX',), 0),
  ),
  'qux': (
    ((), 12),
    (('A',), 7),
    (('B',), 9),
    (('B', 'XXXX'), 9),
    (('XXXX',), 6),
  ),
  'corge': (
    ((), 12),
    (('i=2',), 7),
    (('i=3',), 7),
  ),
  'grault': (
    ((), 12),
    (('A',), 3),
    (('B',), 9),
    (('B', 'i=1'), 7),
    (('B', 'XXXX'), 6),
  ),
}

new_filter_tests = {
  'grault': (
    ((), 12),
    (('c:B', 'g:1'), 3),
    (('c:B', 'g:1', 'c:i=2'), 3),
    # We filter the generator to different element than the dynamic section
    (('c:B', 'g:1', 'c:i=1'), 2),
    (('c:B', 'g:0,2'), 6),
    (('c:B', 'g:0-2', 'c:i=2'), 7),
  ),
  'garply': (
    ((), 3),
    (('g:1',), 1),
    (('g:*',), 3),
    # Followup section filters do not change results (there are no sections underneath)
    (('g:1', 'c:abcdef'), 1),
    (('g:*', 'c:abcdef'), 3),
    (('g:0,2',), 2),
    (('g:1-2',), 2),
    (('g:0,1-2',), 3),
    (('g:1-1',), 1),
    (('g:0,2', 'c:abcdef'), 2),
  ),
  'waldo': (
    ((), 9),
    (('g:1',), 3),
    (('g:*',), 9),
    (('g:1', 'g:2'), 1),
    (('g:*', 'g:1'), 3),
    (('g:0', 'g:*'), 3),
    (('g:*', 'g:*'), 9),
    (('g:*', 'g:1', 'c:abcdef'), 3),
    (('g:0,2', 'g:1-2'), 4),
    (('g:0-1', 'g:*'), 6),
    (('g:*', 'g:0,2'), 6),
  ),
  'fred': (
    ((), 24),
    # No sections entered, but we still hit assertion at the start of test
    (('c:abcdef',), 1),
    (('c:A', ), 12),
    (('c:A', 'g:*', 'c:A1'), 6),
    (('c:A', 'g:1', 'c:A1'), 2),
    (('c:A', 'g:1'), 4),
    # Section 'A3' does not exist
    (('c:A', 'g:*', 'c:A3'), 3),
    (('c:A', 'g:1', 'c:A3'), 1),
    (('c:A', 'g:0,2'), 8),
    (('c:A', 'g:0-2', 'c:A1'), 6),
    (('c:A', 'g:0,2', 'c:A3'), 2),
    # We would like something like 'c:*', 'g:2', but we do not support globs
    # for sections (yet?)
  ),
}


if len(sys.argv) != 2:
    print("Wrong number of arguments, expected just the path to Catch2 SelfTest binary")
    exit(1)

bin_path = os.path.abspath(sys.argv[1])

for test_filter, specs in old_section_tests.items():
    for section_path, expected_assertions in specs:
        run_one_test(bin_path, test_filter, section_path, expected_assertions, new_filters=False)

for test_filter, specs in new_filter_tests.items():
    for section_path, expected_assertions in specs:
        run_one_test(bin_path, test_filter, section_path, expected_assertions, new_filters=True)

# Verify the selected values and their order, not just the assertion counts.
run_one_test(bin_path, 'garply', ('g:0,2',), 2, True,
             ['i := 1', 'i := 100'])
run_one_test(bin_path, 'garply', ('g:1-2',), 2, True,
             ['i := 10', 'i := 100'])
run_one_test(bin_path, 'waldo', ('g:0,2', 'g:1-2'), 4, True,
             ['i := 1', 'j := 20', 'i := 1', 'j := 200',
              'i := 100', 'j := 20', 'i := 100', 'j := 200'])

# A later unavailable index must fail inside the test, including when the
# generator is nested in a section. It must not escape during tracker cleanup.
for test_name, filters, successes in (
    ('garply', ('g:0,3',), 1),
    ('garply', ('g:1-3',), 2),
    ('garply', ('g:0,3,5',), 1),
    ('garply', ('g:0-4294967295',), 3),
    ('fred', ('c:A', 'g:0,3'), 6),
    ('waldo', ('g:0,2', 'g:0,3'), 2),
):
    cmd = [bin_path, test_name, '--reporter', 'xml'] + make_new_filters(filters)
    result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            universal_newlines=True, timeout=10)
    # SelfTest uses a custom exit code for test failures.
    assert result.returncode == 42, f'{cmd}: {result.returncode}\n{result.stdout}\n{result.stderr}'
    tree = ET.fromstring(result.stdout)
    stats = tree.find('OverallResults').attrib
    failures = 2 if test_name == 'waldo' else 1
    assert stats['failures'] == str(failures), f'{cmd}: {stats}'
    assert stats['successes'] == str(successes), f'{cmd}: {stats}'
    assert 'not enough elements' in result.stdout, result.stdout
