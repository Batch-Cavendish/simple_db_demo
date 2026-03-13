#!/usr/bin/env python3
import argparse
import os
import subprocess
import sys
import time


def run_commands(db_exe, db_file, commands):
    """Run a sequence of SQL commands and return the output."""
    process = subprocess.Popen(
        [db_exe, db_file],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    # Always end with .exit
    stdout, stderr = process.communicate(input="\n".join(commands) + "\n.exit\n")
    if process.returncode != 0:
        print(f"Error: Database exited with code {process.returncode}")
        print(f"Stderr: {stderr}")
    return stdout


def test_performance(db_exe, db_file, num_rows):
    """Test INSERT, Point Lookup, and Full Table Scan performance."""
    if os.path.exists(db_file):
        try:
            os.remove(db_file)
        except OSError:
            pass

    print(f"Testing performance with {num_rows} rows...")

    # 1. CREATE TABLE
    run_commands(db_exe, db_file, ["CREATE TABLE users (id INT, username TEXT);"])

    # 2. INSERT
    insert_cmds = [
        f"INSERT INTO users VALUES ({i}, 'user{i}');" for i in range(num_rows)
    ]
    start_time = time.time()
    run_commands(db_exe, db_file, insert_cmds)
    insert_time = time.time() - start_time
    print(f"  Inserted {num_rows} rows in {insert_time:.4f} seconds.")

    # 3. Point lookup (indexed on PK)
    import random

    lookup_count = 100
    lookup_cmds = [
        f"SELECT * FROM users WHERE id = {random.randint(0, num_rows - 1)};"
        for _ in range(lookup_count)
    ]
    start_time = time.time()
    run_commands(db_exe, db_file, lookup_cmds)
    lookup_time = (time.time() - start_time) / lookup_count
    print(f"  Point lookup (O(log n)): {lookup_time:.6f} seconds per query.")

    # 4. Full scan (SELECT * FROM users;)
    start_time = time.time()
    run_commands(db_exe, db_file, ["SELECT * FROM users;"])
    scan_time = time.time() - start_time
    print(f"  Full table scan (O(n)): {scan_time:.4f} seconds.")

    if os.path.exists(db_file):
        try:
            os.remove(db_file)
        except OSError:
            pass


def main():
    parser = argparse.ArgumentParser(
        description="SimpleDB Performance Comparison Script"
    )
    parser.add_argument(
        "db_exe",
        help="Path to the database executable",
        nargs="?",
        default="./build/db",
    )
    parser.add_argument(
        "--rows",
        type=int,
        default=1000,
        help="Number of rows for the first test (default: 1000)",
    )
    args = parser.parse_args()

    db_exe = args.db_exe
    if not os.path.exists(db_exe):
        # Handle Windows .exe extension
        if os.path.exists(db_exe + ".exe"):
            db_exe += ".exe"
        else:
            print(f"Error: Executable {db_exe} not found. Build the project first.")
            sys.exit(1)

    db_file = "perf_temp.db"

    test_performance(db_exe, db_file, args.rows)
    print("-" * 30)
    test_performance(db_exe, db_file, args.rows * 10)


if __name__ == "__main__":
    main()
