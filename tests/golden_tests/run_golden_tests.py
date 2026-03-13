#!/usr/bin/env python3
import subprocess
import os
import sys
import difflib
import argparse

def normalize_output(stdout):
    """Normalize output by removing prompts and trailing whitespace."""
    lines = stdout.splitlines()
    processed_lines = []
    for line in lines:
        # Remove prompt "db > " if present
        if "db > " in line:
            line = line.replace("db > ", "")
        
        line = line.rstrip()
        if line:
            processed_lines.append(line)
    return processed_lines

def run_test(sql_file, db_exe, test_dir):
    """Run a single SQL test file and compare with .expected file."""
    base_name = os.path.splitext(os.path.basename(sql_file))[0]
    expected_file = os.path.join(test_dir, base_name + ".expected")
    
    # Use a unique DB file name per test to avoid conflicts,
    # but handle persistence test pairs correctly.
    # We'll use "test_01_basic.db" etc.
    # For persistence tests, we'll strip the _p1/_p2 suffix for the db filename.
    db_base = base_name.replace("_p1", "").replace("_p2", "")
    db_file = f"temp_{db_base}.db"

    # Part 1 creates/resets the DB, Part 2 expects it to exist.
    is_persistence_part2 = "_p2" in base_name
    is_persistence_part1 = "_p1" in base_name
    
    # If not part of a multi-part test, or if it's the first part, clean up the DB file first
    if not is_persistence_part2:
        if os.path.exists(db_file):
            try:
                os.remove(db_file)
            except OSError as e:
                print(f"Warning: Could not remove {db_file}: {e}")

    try:
        with open(sql_file, 'r', encoding='utf-8') as f:
            sql_commands = f.read()

        # Run the database executable
        process = subprocess.Popen(
            [db_exe, db_file],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding='utf-8',
            errors='replace'
        )
        stdout, _ = process.communicate(input=sql_commands)

        actual_lines = normalize_output(stdout)

        if not os.path.exists(expected_file):
            print(f"No expected file for {sql_file}. Creating it.")
            with open(expected_file, 'w', encoding='utf-8', newline='\n') as f:
                f.write("\n".join(actual_lines) + "\n")
            return True

        with open(expected_file, 'r', encoding='utf-8') as f:
            expected_lines = [line.rstrip() for line in f.readlines() if line.rstrip()]

        if actual_lines == expected_lines:
            print(f"PASS: {base_name}")
            return True
        else:
            print(f"FAIL: {base_name}")
            diff = difflib.unified_diff(
                expected_lines,
                actual_lines,
                fromfile='expected',
                tofile='actual',
                lineterm=''
            )
            for line in diff:
                print(line)
            return False
            
    except Exception as e:
        print(f"Error running test {base_name}: {e}")
        return False
    finally:
        # Clean up DB file unless it's needed for the next part of a persistence test
        if os.path.exists(db_file) and not is_persistence_part1:
             try:
                os.remove(db_file)
             except OSError:
                pass

def main():
    parser = argparse.ArgumentParser(description="SimpleDB Golden Test Runner")
    parser.add_argument("db_exe", help="Path to the database executable")
    parser.add_argument("test_dir", help="Directory containing .sql and .expected files", nargs="?", default=".")
    args = parser.parse_args()

    db_exe = args.db_exe
    test_dir = args.test_dir

    if not os.path.exists(db_exe):
        # Handle Windows .exe extension if omitted
        if os.path.exists(db_exe + ".exe"):
            db_exe += ".exe"
        else:
            print(f"Error: Executable {db_exe} not found.")
            sys.exit(1)

    if not os.path.isdir(test_dir):
        print(f"Error: Test directory {test_dir} not found.")
        sys.exit(1)

    # Find all .sql files in the test directory
    sql_files = [os.path.join(test_dir, f) for f in os.listdir(test_dir) if f.endswith(".sql")]
    sql_files.sort()

    if not sql_files:
        print(f"No .sql files found in {test_dir}")
        sys.exit(0)

    passed = 0
    failed = 0

    # Ensure we run in a clean environment for temp files
    # CWD is usually the build directory when run via Meson
    for sql_file in sql_files:
        if run_test(sql_file, db_exe, test_dir):
            passed += 1
        else:
            failed += 1

    print("--------------------------------")
    print(f"Tests passed: {passed}")
    print(f"Tests failed: {failed}")

    if failed > 0:
        sys.exit(1)

if __name__ == "__main__":
    main()
