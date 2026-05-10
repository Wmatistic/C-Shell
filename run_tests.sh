#!/usr/bin/env bash
# tests/run_tests.sh — smoke tests for mysh
set -euo pipefail

SH=./mysh
PASS=0
FAIL=0

run() {
    local desc="$1"; local input="$2"; local expected="$3"
    local actual
    actual=$(echo "$input" | $SH 2>/dev/null | tail -1)
    if [[ "$actual" == "$expected" ]]; then
        echo "  PASS  $desc"
        ((++PASS))
    else
        echo "  FAIL  $desc"
        echo "        expected: '$expected'"
        echo "        actual:   '$actual'"
        ((++FAIL))
    fi
}

echo "=== mysh test suite ==="

run "echo"              'echo hello'           'hello'
run "pipe"              'echo hello | cat'     'hello'
run "multi-pipe"        'echo foo | cat | cat' 'foo'
run "exit status 0"     'true; echo $?'        '0'
run "exit status 1"     'false; echo $?'       '1'
run "variable expand"   'export X=42; echo $X' '42'
run "redirect out"      'echo hi > /tmp/mysh_test.txt; cat /tmp/mysh_test.txt' 'hi'
run "redirect append"   'echo a > /tmp/mysh_app.txt; echo b >> /tmp/mysh_app.txt; tail -1 /tmp/mysh_app.txt' 'b'
run "cd and pwd"        'cd /tmp; pwd'         '/tmp'
run "glob expansion"    'echo /tmp/*.txt | grep -q txt && echo ok' 'ok'

rm -f /tmp/mysh_test.txt /tmp/mysh_app.txt

echo ""
echo "Results: $PASS passed, $FAIL failed"
[[ $FAIL -eq 0 ]]
