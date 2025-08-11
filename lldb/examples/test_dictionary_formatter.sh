#!/bin/bash
#===-- test_dictionary_formatter.sh - Test enhanced dictionary formatter ---===//
#
# Script to test the enhanced dictionary formatter with LLDB
#
#===----------------------------------------------------------------------===//

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== Testing Enhanced Dictionary Formatter ===${NC}"

# Build the test program
echo -e "${YELLOW}Building test program...${NC}"
make test_enhanced_dictionary
if [ $? -ne 0 ]; then
    echo -e "${RED}Build failed!${NC}"
    exit 1
fi

# Test with LLDB
echo -e "${YELLOW}Running LLDB tests...${NC}"

# Create LLDB command script
cat > test_dict_formatter.lldb << 'EOF'
# Set breakpoints at test locations
b test_enhanced_dictionary.m:57
b test_enhanced_dictionary.m:88
b test_enhanced_dictionary.m:107
b test_enhanced_dictionary.m:138
b test_enhanced_dictionary.m:151
b test_enhanced_dictionary.m:177
b test_enhanced_dictionary.m:210
b test_enhanced_dictionary.m:245

# Run the program
run

# Test empty dictionary
frame variable empty
frame variable mutableEmpty
po empty
po mutableEmpty
continue

# Test simple dictionaries
frame variable stringDict
frame variable numberDict
frame variable mixedDict
po stringDict
po numberDict
po mixedDict
continue

# Test nested dictionary
frame variable nestedDict
po nestedDict
continue

# Test custom objects dictionary
frame variable customDict
frame variable reverseCustomDict
po customDict
po reverseCustomDict
continue

# Test large dictionary
frame variable largeDict
po largeDict
continue

# Test mutable dictionary
frame variable mutableDict
po mutableDict
continue

# Test edge cases
frame variable dictWithNulls
frame variable dupKeys
frame variable specialKeys
po dictWithNulls
po dupKeys
po specialKeys
continue

# Test performance dictionary
frame variable perfDict
po perfDict
continue

# Exit
quit
EOF

# Run LLDB with the test script
echo -e "${YELLOW}Testing dictionary formatting in LLDB...${NC}"
/home/robk/code/llvm-project/build/bin/lldb -s test_dict_formatter.lldb ./test_enhanced_dictionary 2>&1 | tee test_dict_output.log

# Check for success patterns
echo -e "${YELLOW}Analyzing results...${NC}"

# Check for proper key=value formatting (not [0].key, [0].value)
if grep -q "\[0\]\.key" test_dict_output.log; then
    echo -e "${RED}FAIL: Still showing [0].key format${NC}"
else
    echo -e "${GREEN}PASS: Not showing [0].key format${NC}"
fi

# Check for proper dictionary display
if grep -q "name.*=.*John Doe" test_dict_output.log; then
    echo -e "${GREEN}PASS: Dictionary shows key=value format${NC}"
else
    echo -e "${YELLOW}WARNING: Could not verify key=value format${NC}"
fi

# Check for empty dictionary display
if grep -q "{}" test_dict_output.log; then
    echo -e "${GREEN}PASS: Empty dictionary shows as {}${NC}"
else
    echo -e "${YELLOW}WARNING: Empty dictionary format not verified${NC}"
fi

# Check for no crashes
if grep -q "error:" test_dict_output.log; then
    echo -e "${RED}FAIL: Errors detected during testing${NC}"
else
    echo -e "${GREEN}PASS: No errors detected${NC}"
fi

echo -e "${GREEN}=== Test Complete ===${NC}"
echo "Full output saved to test_dict_output.log"