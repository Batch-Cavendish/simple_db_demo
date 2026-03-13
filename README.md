# SimpleDB: An Educational C Database

SimpleDB is a lightweight, disk-based database engine written in C23. It is designed as an educational resource to demonstrate the core components of a database system, including a balanced B-Tree storage engine, a page-based memory manager (Pager), and dynamic schema management.

## Project Goals

The primary objective of SimpleDB is to provide a transparent and understandable implementation of database internals. Key goals include:
- **Educational Clarity**: Every component is implemented with readability and conceptual honesty in mind, showing *why* certain architectural choices are made (e.g., using B-Trees for $O(\log n)$ lookups).
- **Disk Persistence**: Implementing a Pager to manage data movement between memory and disk using fixed-size 4KB pages.
- **Dynamic Schema**: Allowing users to define custom tables with varying field types (`int`, `text`) at runtime.
- **CRUD Operations**: Supporting Create, Read, Update, and Delete operations.
- **Reliability & Stability**: Recent updates have focused on ensuring balanced B-Tree growth, leak-free memory management, and safe page-splitting logic.

## Architecture

SimpleDB follows a classic, modular database architecture:

1.  **Compiler (Parser)**: The `prepare_statement` function tokenizes and parses raw text input (SQL-like) into an internal `Statement` object.
2.  **Virtual Machine (VM)**: The `execute_statement` function executes the `Statement` on the database engine.
3.  **Catalog Manager**: Manages multiple table definitions (name, root page, schema) stored in Page 0.
4.  **B-Tree Storage Engine**: Data is organized in a B-Tree structure for $O(\log n)$ access.
    -   **Leaf Nodes**: Store data rows.
    -   **Internal Nodes**: Facilitate navigation and scaling.
    -   **Safe Splitting**: Automatically handles node splits using a temporary-buffer strategy to ensure data integrity during tree growth.
5.  **Buffer Pool (Pager)**: Manages a cache of pages in memory with **O(1) tracking** and **LRU (Least Recently Used)** eviction.
6.  **Serialization Layer**: Centralized `serialize_row` and `deserialize_row` logic in `schema.c` for clean data conversion.
7.  **Resource Management**: Centralized cleanup ensures no memory leaks during the execution lifecycle.

## File Structure

The codebase is organized into modular components:

- `include/common.h`: Shared constants and core type definitions.
- `include/os_portability.h` / `src/os_portability.c`: Cross-platform abstraction for file I/O and terminal handling.
- `include/pager.h` / `src/pager.c`: Efficient disk I/O management and buffer pool.
- `include/btree.h` / `src/btree.c`: B-Tree implementation with unified cell-moving logic.
- `include/database.h` / `src/database.c`: High-level database, catalog, and cursor management.
- `include/statement.h` / `src/statement.c`: Statement preparation and execution logic.
- `include/schema.h` / `src/schema.c`: Row-level serialization and schema utility functions.
- `src/main.c`: Clean REPL loop utilizing portable terminal abstractions.

## Implementation Technologies

-   **Language**: C23 (ISO/IEC 9899:2024).
-   **Platform Support**: Linux (GCC/Clang) and Windows (LLVM 21).
-   **Build System**: Meson + Ninja.
-   **Testing**: Python-based unified test runner for cross-platform consistency.

## Usage

### Build & Run

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

### Supported SQL Commands

#### 1. Table Operations
```sql
db > CREATE TABLE users (id INT, username TEXT);
db > .tables -- List tables
db > .table  -- Alias for .tables
```

#### 2. Data Manipulation
```sql
db > INSERT INTO users VALUES (1, 'Alice');
db > SELECT * FROM users;
db > UPDATE users SET username = 'Bob' WHERE id = 1;
db > DELETE FROM users WHERE id = 1;
```

#### 3. Display Modes
```sql
db > .mode box   -- Enable formatted box output
db > .mode plain -- Default row output
```

#### 4. REPL Features
- **Arrow Keys**: Use Up/Down for history and Left/Right for cursor adjustment.
- **History**: Up to 100 previous commands are remembered.

## Educational Insights

-   **Cross-Platform Portability**: Demonstrates how to abstract OS-specific terminal and file APIs into a single portable interface.
-   **Efficient Buffer Management**: The Pager uses O(1) state tracking to manage the buffer pool, teaching students about I/O optimization and eviction strategies.
-   **Safe B-Tree Growth**: Node splits use a temporary buffer to ensure the tree remains consistent even during complex structural changes.
-   **Row Serialization**: Centralized schema logic shows how high-level C structures are mapped to stable binary formats on disk.
