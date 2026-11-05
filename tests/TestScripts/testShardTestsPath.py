# SPDX-License-Identifier: BSL-1.0

import json
import pathlib
import shutil
import subprocess
import sys
import tempfile


with tempfile.TemporaryDirectory(prefix='catch shard tests ') as directory:
    workdir = pathlib.Path(directory)
    binary_dir = workdir / 'binary ]==] ${NOT_A_VARIABLE}'
    binary_dir.mkdir()
    binary = binary_dir / pathlib.Path(sys.argv[2]).name
    shutil.copy2(sys.argv[2], binary)
    subprocess.run([
        'cmake', '-DTEST_BINARY=' + binary.as_posix(),
        '-DCTEST_FILE=' + (workdir / 'CTestTestfile.cmake').as_posix(),
        '-DTARGET_NAME=path-test', '-DSHARD_COUNT=1',
        '-DTEST_SPEC=Factorials*', '-P', sys.argv[1],
    ], check=True)
    listing = subprocess.run([
        'ctest', '--test-dir', directory, '--show-only=json-v1',
    ], check=True, capture_output=True, text=True)
    tests = json.loads(listing.stdout)['tests']
    assert len(tests) == 1, tests
    assert tests[0].get('command', [None])[0] == str(binary), tests
