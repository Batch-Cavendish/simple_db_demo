import subprocess
import time
import os

DB_EXE = "./db"
DB_FILE = "perf.db"

def run_commands(commands):
    process = subprocess.Popen([DB_EXE, DB_FILE], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    stdout, stderr = process.communicate(input="\n".join(commands) + "\n.exit\n")
    return stdout

def test_performance(num_rows):
    if os.path.exists(DB_FILE):
        os.remove(DB_FILE)
    
    # CREATE TABLE
    run_commands(["CREATE TABLE users (id INT, username TEXT);"])
    
    # INSERT
    insert_cmds = [f"INSERT INTO users VALUES ({i}, 'user{i}');" for i in range(num_rows)]
    start_time = time.time()
    run_commands(insert_cmds)
    insert_time = time.time() - start_time
    print(f"Inserted {num_rows} rows in {insert_time:.4f} seconds.")
    
    # Point lookup (indexed)
    start_time = time.time()
    # Perform 100 random lookups
    import random
    lookup_cmds = [f"SELECT * FROM users WHERE id = {random.randint(0, num_rows-1)};" for _ in range(100)]
    run_commands(lookup_cmds)
    lookup_time = (time.time() - start_time) / 100
    print(f"Point lookup (O(log n)): {lookup_time:.6f} seconds per query.")
    
    # Full scan (not really possible to force non-indexed on PK currently without another column)
    # But we can compare SELECT * FROM users;
    start_time = time.time()
    run_commands(["SELECT * FROM users;"])
    scan_time = time.time() - start_time
    print(f"Full table scan (O(n)): {scan_time:.4f} seconds.")

if __name__ == "__main__":
    if not os.path.exists(DB_EXE):
        print("Executable not found. Run 'make' first.")
    else:
        test_performance(1000)
        print("-" * 30)
        test_performance(10000)
        if os.path.exists(DB_FILE):
            os.remove(DB_FILE)
