#!/bin/bash
# Golden test - Quick validation that all formatters work

LLDB="../../../../../../../build/bin/lldb"

echo "==================================="
echo "GOLDEN FORMATTER TEST"
echo "==================================="
echo ""

# Run LLDB with test commands
$LLDB ./test_comprehensive_formatters -b << 'EOF' 2>&1 | grep -E "= @|= [0-9]+|= YES|= NO|= nil|TestObject"
breakpoint set --line 301
run
frame variable taggedString
frame variable taggedInt
frame variable taggedBool
frame variable simpleArray
frame variable simpleDict
frame variable simpleSet
frame variable customObj
frame variable emptyArray
frame variable emptyDict
quit
EOF

echo ""
echo "==================================="
echo "If you see formatted values above:"
echo "✅ Formatters are working correctly!"
echo ""
echo "Expected outputs:"
echo '  - Strings: @"..."'
echo '  - Numbers: 42, YES, 3.14'
echo '  - Arrays: @[...]'
echo '  - Dictionaries: @{...}'
echo '  - Sets: [NSSet setWithObjects:...]'
echo '  - Custom: TestObject(...)'
echo "===================================" 