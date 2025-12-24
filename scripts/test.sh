#!/bin/bash

MOUNT="/mnt/ni"

# Test 1: Create file
echo ""
echo "1. Create file with touch"
if touch "$MOUNT/file1.txt"; then
    echo "SUCCESS: Created file1.txt"
else
    echo "FAIL: touch returned $?"
    exit 1
fi

# Test 2: Write data to file
echo ""
echo "2. Write data to file"
if echo "Hello NIFS!" > "$MOUNT/file1.txt"; then
    echo "SUCCESS: Wrote to file1.txt"
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
        echo "SUCCESS: Read correct data"
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
        echo "SUCCESS: Appended correctly"
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
        echo "SUCCESS: Second file works"
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
    echo "SUCCESS: Files are separate"
else
    echo "FAIL: Files mixed up"
    exit 1
fi

# Test 7: Overwrite file
echo ""
echo "7. Overwrite file"
if echo "New content" > "$MOUNT/file1.txt"; then
    if [ "$(cat "$MOUNT/file1.txt")" = "New content" ]; then
        echo "SUCCESS: Overwrite works"
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
    echo "SUCCESS: ls succeeded"
else
    echo "FAIL: ls returned $?"
fi

# Test 9: Remove files
echo ""
echo "9. Cleanup - remove files"
if rm "$MOUNT/file1.txt" "$MOUNT/file2.txt"; then
    echo "SUCCESS: Files removed"
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
        echo "SUCCESS: Directory empty"
    else
        echo "FAIL: Directory has $count files"
        exit 1
    fi
else
    echo "FAIL: ls for count returned $?"
    exit 1
fi

# Test 11: Create a directory
echo ""
echo "11. Create a directory"
if mkdir "$MOUNT/dir1"; then
    echo "SUCCESS: Created dir1"
else
    echo "FAIL: mkdir returned $?"
    exit 1
fi

# Test 12: Create a file inside the directory
echo ""
echo "12. Create a file inside the directory"
if echo "Inside dir1" > "$MOUNT/dir1/file_in_dir.txt"; then
    echo "SUCCESS: Created file inside directory"
else
    echo "FAIL: create in dir returned $?"
    exit 1
fi

# Test 13: Read file from directory
echo ""
echo "13. Read file from directory"
content=$(cat "$MOUNT/dir1/file_in_dir.txt" 2>/dev/null)
if [ $? -eq 0 ]; then
    if [ "$content" = "Inside dir1" ]; then
        echo "SUCCESS: Read from directory"
    else
        echo "FAIL: Wrong data from directory: '$content'"
        exit 1
    fi
else
    echo "FAIL: cat from directory returned $?"
    exit 1
fi

# Test 14: Create nested directories (dir inside dir)
echo ""
echo "14. Create nested directories"
if mkdir -p "$MOUNT/dir1/subdir1/subsubdir1"; then
    echo "SUCCESS: Created nested directories"
else
    echo "FAIL: mkdir -p returned $?"
    exit 1
fi

# Test 15: Create file in deeply nested directory
echo ""
echo "15. Create file in deeply nested directory"
if echo "Very deep content" > "$MOUNT/dir1/subdir1/subsubdir1/deepfile.txt"; then
    echo "SUCCESS: Created file in deeply nested directory"
else
    echo "FAIL: create in deep dir returned $?"
    exit 1
fi

# Test 16: Verify directory listings at different levels
echo ""
echo "16. Verify directory listings at different levels"
# List root
if ls "$MOUNT/" | grep -q "dir1"; then
    echo "SUCCESS: dir1 appears in root listing"
else
    echo "FAIL: dir1 not in root listing"
    exit 1
fi

# List dir1
if ls "$MOUNT/dir1/" | grep -q "subdir1"; then
    echo "SUCCESS: subdir1 appears in dir1 listing"
else
    echo "FAIL: subdir1 not in dir1 listing"
    exit 1
fi

# Test 17: Create multiple directories at same level
echo ""
echo "17. Create multiple directories at same level"
if mkdir "$MOUNT/dir2" && mkdir "$MOUNT/dir3"; then
    count=$(ls -d "$MOUNT/"dir*/ 2>/dev/null | wc -l)
    if [ $count -ge 3 ]; then
        echo "SUCCESS: Created multiple directories ($count found)"
    else
        echo "FAIL: Expected at least 3 directories, found $count"
        exit 1
    fi
else
    echo "FAIL: Failed to create multiple directories"
    exit 1
fi

# Test 18: Create files with same name in different directories
echo ""
echo "18. Create files with same name in different directories"
if echo "In dir2" > "$MOUNT/dir2/same_name.txt" && \
   echo "In dir3" > "$MOUNT/dir3/same_name.txt"; then
    content2=$(cat "$MOUNT/dir2/same_name.txt")
    content3=$(cat "$MOUNT/dir3/same_name.txt")
    if [ "$content2" = "In dir2" ] && [ "$content3" = "In dir3" ]; then
        echo "SUCCESS: Same filename works in different directories"
    else
        echo "FAIL: Files mixed up between directories"
        exit 1
    fi
else
    echo "FAIL: Failed to create same-name files"
    exit 1
fi

# Test 19: Remove empty directory
echo ""
echo "19. Remove empty directory"
if mkdir "$MOUNT/emptydir" && rmdir "$MOUNT/emptydir"; then
    if [ ! -d "$MOUNT/emptydir" ]; then
        echo "SUCCESS: Empty directory removed"
    else
        echo "FAIL: Directory still exists after rmdir"
        exit 1
    fi
else
    echo "FAIL: rmdir returned $?"
    exit 1
fi

# Test 20: Try to remove non-empty directory (should fail)
echo ""
echo "20. Try to remove non-empty directory (should fail)"
if mkdir "$MOUNT/nonempty" && echo "test" > "$MOUNT/nonempty/file.txt"; then
    if rmdir "$MOUNT/nonempty" 2>/dev/null; then
        echo "FAIL: rmdir succeeded on non-empty directory"
        exit 1
    else
        echo "SUCCESS: rmdir correctly failed on non-empty directory"
        # Clean it up properly
        rm "$MOUNT/nonempty/file.txt"
        rmdir "$MOUNT/nonempty"
    fi
else
    echo "FAIL: Setup for non-empty dir test failed"
    exit 1
fi
