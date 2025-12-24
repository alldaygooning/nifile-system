#!/bin/bash

MOUNT="/mnt/ni"

echo "=== NIFS Filesystem Test ==="

# Test 1: Create file
echo ""
echo "1. Create file with touch"
if touch "$MOUNT/file1.txt"; then
    echo "PASS: Created file1.txt"
else
    echo "FAIL: touch returned $?"
    exit 1
fi

# Test 2: Write data to file
echo ""
echo "2. Write data to file"
if echo "Hello NIFS!" > "$MOUNT/file1.txt"; then
    echo "PASS: Wrote to file1.txt"
else
    echo "FAIL: write returned $?"
    exit 1
fi

# Test 3: Read data from file
echo ""
echo "3. Read data from file"
content=$(cat "$MOUNT/file1.txt" 2>/dev/null)
if [ $? -eq 0 ]; then
    if [ "$content" = "Hello NIFS!" ]; then
        echo "PASS: Read correct data"
    else
        echo "FAIL: Wrong data: '$content'"
        exit 1
    fi
else
    echo "FAIL: cat returned $?"
    exit 1
fi

# Test 4: Append data to file
echo ""
echo "4. Append data to file"
if echo "More data" >> "$MOUNT/file1.txt"; then
    content=$(cat "$MOUNT/file1.txt")
    expected=$'Hello NIFS!\nMore data'
    if [ "$content" = "$expected" ]; then
        echo "PASS: Appended correctly"
    else
        echo "FAIL: Append mismatch"
        exit 1
    fi
else
    echo "FAIL: append returned $?"
    exit 1
fi

# Test 5: Create and write to second file
echo ""
echo "5. Multiple files with different data"
if echo "File2 content" > "$MOUNT/file2.txt"; then
    if [ "$(cat "$MOUNT/file2.txt")" = "File2 content" ]; then
        echo "PASS: Second file works"
    else
        echo "FAIL: Second file read error"
        exit 1
    fi
else
    echo "FAIL: create file2 returned $?"
    exit 1
fi

# Test 6: Verify files are separate
echo ""
echo "6. Verify file separation"
file1=$(cat "$MOUNT/file1.txt")
file2=$(cat "$MOUNT/file2.txt")
if [ "$file1" != "$file2" ]; then
    echo "PASS: Files are separate"
else
    echo "FAIL: Files mixed up"
    exit 1
fi

# Test 7: Overwrite file
echo ""
echo "7. Overwrite file"
if echo "New content" > "$MOUNT/file1.txt"; then
    if [ "$(cat "$MOUNT/file1.txt")" = "New content" ]; then
        echo "PASS: Overwrite works"
    else
        echo "FAIL: Overwrite read error"
        exit 1
    fi
else
    echo "FAIL: overwrite returned $?"
    exit 1
fi

# Test 8: Directory listing
echo ""
echo "8. Directory listing"
if ls -la "$MOUNT/"; then
    echo "PASS: ls succeeded"
else
    echo "FAIL: ls returned $?"
fi

# Test 9: Remove files
echo ""
echo "9. Cleanup - remove files"
if rm "$MOUNT/file1.txt" "$MOUNT/file2.txt"; then
    echo "PASS: Files removed"
else
    echo "FAIL: rm returned $?"
    exit 1
fi

# Test 10: Verify removal
echo ""
echo "10. Verify empty directory"
count=$(ls -1 "$MOUNT/" 2>/dev/null | wc -l)
if [ $? -eq 0 ]; then
    if [ "$count" -eq 0 ]; then
        echo "PASS: Directory empty"
    else
        echo "FAIL: Directory has $count files"
        exit 1
    fi
else
    echo "FAIL: ls for count returned $?"
    exit 1
fi

echo "All tests passed!"
