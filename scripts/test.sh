#!/bin/bash

MOUNT="/mnt/ni"

# Test 1: Create file
echo "Test 1: Create file with touch"
touch "$MOUNT/test1.txt"
if [ -f "$MOUNT/test1.txt" ]; then
    echo "✓ Created test1.txt"
else
    echo "✗ Failed to create test1.txt"
    exit 1
fi

# Test 2: List directory
echo -e "\nTest 2: List directory"
ls -la "$MOUNT/"

# Test 3: Remove file
echo -e "\nTest 3: Remove file"
rm "$MOUNT/test1.txt"
if [ ! -f "$MOUNT/test1.txt" ]; then
    echo "✓ Removed test1.txt"
else
    echo "✗ Failed to remove test1.txt"
    exit 1
fi

echo -e "\nAll tests passed!"
