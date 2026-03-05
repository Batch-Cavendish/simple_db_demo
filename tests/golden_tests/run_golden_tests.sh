#!/bin/bash

# Simple golden test runner

DB_FILE="test.db"
EXE="../../db"
TEST_DIR="."

if [ ! -f "$EXE" ]; then
    echo "Executable $EXE not found. Please run make first."
    exit 1
fi

passed=0
failed=0

for sql_file in "$TEST_DIR"/*.sql; do
    [ -e "$sql_file" ] || continue
    
    base_name=$(basename "$sql_file" .sql)
    expected_file="$TEST_DIR/$base_name.expected"
    actual_file="$TEST_DIR/$base_name.actual"
    
    # Don't delete DB file if it's a persistence test (part 2+)
    if [[ "$base_name" != *"persistence_p2"* ]]; then
        rm -f "$DB_FILE"
    fi
    
    # Run the database with the SQL commands
    cat "$sql_file" | "$EXE" "$DB_FILE" > "$actual_file" 2>&1
    
    # Remove the prompts from actual output for cleaner comparison
    # Use sed to remove "db > " anywhere it appears
    sed -i 's/db > //g' "$actual_file"
    # Also remove trailing whitespace and empty lines at the end
    sed -i 's/[[:space:]]*$//' "$actual_file"
    sed -i '/^$/d' "$actual_file"

    if [ ! -f "$expected_file" ]; then
        echo "No expected file for $sql_file. Creating it from actual output."
        cp "$actual_file" "$expected_file"
    fi
    
    if diff -u "$expected_file" "$actual_file" > /dev/null; then
        echo "PASS: $base_name"
        passed=$((passed + 1))
        rm -f "$actual_file"
    else
        echo "FAIL: $base_name"
        diff -u "$expected_file" "$actual_file"
        failed=$((failed + 1))
    fi
done

rm -f "$DB_FILE"

echo "--------------------------------"
echo "Tests passed: $passed"
echo "Tests failed: $failed"

if [ $failed -gt 0 ]; then
    exit 1
else
    exit 0
fi
