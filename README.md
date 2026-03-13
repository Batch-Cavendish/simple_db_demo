# SimpleDB: An Educational C Database

SimpleDB is a lightweight, disk-based database engine written in C23. It is designed as an educational resource to demonstrate the core components of a database system, including a balanced B-Tree storage engine, a page-based memory manager (Pager), and dynamic schema management.

## Project Goals

The primary objective of SimpleDB is to provide a transparent and understandable implementation of database internals. Key goals include:
- **Educational Clarity**: Every component is implemented with readability and conceptual honesty in mind, showing *why* certain architectural choices are made (e.g., using B-Trees for $O(\log n)$ lookups).
- **Disk Persistence**: Implementing a Pager to manage data movement between memory and disk using fixed-size 4KB pages.
- **Dynamic Schema**: Allowing users to define custom tables with varying field types (`int`, `text`) at runtime.
- **CRUD Operations**: Supporting Create, Read, Update, and Delete operations.
- **Reliability & Stability**: Recent updates have focused on ensuring balanced B-Tree growth and leak-free memory management even during error conditions.

## Architecture

SimpleDB follows a classic, modular database architecture:

1.  **Compiler (Parser)**: The `prepare_statement` function tokenizes and parses raw text input (SQL-like) into an internal `Statement` object. It handles up to 128 tokens per statement.
2.  **Virtual Machine (VM)**: The `execute_statement` function executes the bytecode-like `Statement` on the database engine.
3.  **Catalog Manager**: Manages multiple table definitions (name, root page, schema) stored in Page 0.
4.  **B-Tree Storage Engine**: Data is organized in a B-Tree structure for $O(\log n)$ access.
    -   **Leaf Nodes**: Store data rows.
    -   **Internal Nodes**: Facilitate navigation and scaling.
    -   **Balanced Splitting**: Automatically handles node splits (both leaf and internal) using a 50/50 split strategy to keep the tree balanced and efficient.
5.  **Buffer Pool (Pager)**: Manages a cache of pages in memory with **LRU (Least Recently Used)** eviction and dirty-page tracking to minimize disk I/O.
6.  **Serialization Layer**: Converts C structures into compact binary formats.
7.  **Resource Management**: Centralized cleanup via `free_statement` ensures no memory leaks during the execution lifecycle.

## File Structure

The codebase is organized into modular components:

- `include/common.h`: Shared constants and core type definitions.
- `include/os_portability.h`: Cross-platform abstraction for file I/O (POSIX/Windows).
- `include/pager.h` / `src/pager.c`: Disk I/O management and page-based buffer pool.
- `include/btree.h` / `src/btree.c`: B-Tree implementation and node manipulation.
- `include/database.h` / `src/database.c`: High-level database, catalog, and cursor management.
- `include/statement.h` / `src/statement.c`: Statement preparation and execution logic.
- `include/schema.h` / `src/schema.c`: Row serialization and schema-related utility functions.
- `src/main.c`: Main REPL loop and CLI interface.

## Implementation Technologies

-   **Language**: C23 (ISO/IEC 9899:2024).
-   **Key Features**: standardized attributes (`[[nodiscard]]`), fixed-underlying-type enums, and `nullptr`.
-   **Platform Support**: Linux (GCC/Clang) and Windows (LLVM 21).
-   **Storage Format**: Custom binary format using 4096-byte pages.
-   **Build System**: Meson + Ninja.
-   **Testing**: Python-based unified test runner for cross-platform consistency.

## Usage

### Quick Start (Meson)

1. **Setup and Build**:
   ```bash
   meson setup build
   meson compile -C build
   ```

2. **Run the database**:
   ```bash
   ./build/db mydb.db
   ```

3. **Run tests**:
   ```bash
   meson test -v -C build
   ```

4. **Run performance benchmarks**:
   ```bash
   python3 tests/performance_test.py
   ```

### Supported SQL Commands

#### 1. Create a Table
SimpleDB supports multiple tables per database file. The first column defined is automatically the primary key.
```sql
db > CREATE TABLE users (id INT, username TEXT);
db > CREATE TABLE products (id INT, name TEXT, price INT);
```

#### 2. Insert Data
```sql
db > INSERT INTO users VALUES (1, 'Alice');
db > INSERT INTO products VALUES (101, 'Laptop', 1200);
```

#### 3. Select Data
```sql
-- Full Table Scan
db > SELECT * FROM users;

-- Index Lookup (O(log n))
db > SELECT * FROM users WHERE id = 1;

-- Range Scans (O(log n) + sequential scan)
db > SELECT * FROM users WHERE id > 10;
db > SELECT * FROM users WHERE id < 5;
```

#### 4. Update Data
Modify existing records in-place.
```sql
db > UPDATE users SET username = 'Charlie' WHERE id = 1;
```

#### 5. Delete Data
```sql
db > DELETE FROM users WHERE id = 2;
```

#### 6. Transactions (Teaching Demos)
SimpleDB supports basic transaction syntax for educational purposes. Currently, every statement is auto-committed.
```sql
db > BEGIN;
db > INSERT INTO users VALUES (3, 'Bob');
db > COMMIT;
db > ROLLBACK; -- Educational placeholder
```

#### 7. Meta-commands
```sql
db > .exit
```

### Examples
You can find more example SQL scripts in the `examples/` directory. To run an example:
```bash
./build/db mydb.db < examples/01_create_and_insert.sql
```

## Educational Insights

-   **Cross-Platform Portability**: The use of `os_portability.h` demonstrates how to abstract operating system differences (like POSIX vs. Win32 file APIs). It ensures consistent database behavior, such as using `O_BINARY` on Windows to prevent data corruption during disk I/O.
-   **Compiler vs. VM**: By separating `prepare_statement` from `execute_statement`, the code demonstrates how databases decouple parsing ("what to do") from execution ("how to do it").
-   **Buffer Pool Management**: The `Pager` implements an LRU eviction policy. It "pins" pages during use to prevent corruption and only writes "dirty" (modified) pages back to disk, teaching the importance of I/O optimization.
-   **B-Tree Internals**: The project now supports full B-Tree growth, including internal node splitting, demonstrating how databases maintain performance ($O(\log n)$) regardless of data size.
-   **Range Scans**: The `SELECT` command supports `>`, `<`, and `=` operators, showing how B-Trees facilitate efficient range queries by traversing to a starting point and following leaf-level pointers.
-   **Multi-Table Catalog**: Implementation of a persistent catalog on Page 0 to manage multiple tables, teaching the concept of database metadata.
-   **Transaction Syntax**: Provides a hook to discuss ACID properties, atomic commits, and the necessity of Write-Ahead Logging (WAL) for true durability.
-   **Hashing**: Text primary keys are hashed to `uint32_t` for B-Tree indexing, showing how databases handle non-numeric keys in index structures.
