#!/bin/sh
# cells.py: change Cells, check the result, change them back
set -e
HEX=$1
T=build/tools_test
mkdir -p $T
python3 tools/cells.py set "$HEX" $T/a.hex 3=12 11=0 12=3 > /dev/null
python3 tools/normalize_hex.py --check $T/a.hex > /dev/null
python3 tools/cells.py check $T/a.hex > /dev/null
python3 tools/cells.py show $T/a.hex | grep -q "^ 3  0x12   12"
python3 tools/cells.py show $T/a.hex | grep -q "^11  0x00    0"
# only the two Cells records differ
test "$(diff "$HEX" $T/a.hex | grep -c '^>')" -eq 2
python3 tools/cells.py set $T/a.hex $T/b.hex 3=7 11=5 12=2 > /dev/null
cmp "$HEX" $T/b.hex
# out of range values are refused
! python3 tools/cells.py set "$HEX" $T/c.hex 12=4 2> /dev/null
echo "test_tools: OK"
