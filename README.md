# SimpleDB: An Educational C Database

SimpleDB is a lightweight, disk-based database engine written in C23. It is designed as an educational resource to demonstrate the core components of a database system, including a balanced B-Tree storage engine, a page-based memory manager (Pager), and dynamic schema management.

## Project Goals

The primary objective of SimpleDB is to provide a transparent and understandable implementation of database internals. Key goals include:
- **Educational Clarity**: Every component is implemented with readability and conceptual honesty in mind.
- **Cross-Platform Integrity**: Full support for both **Linux** (GCC/Clang) and **Windows** (LLVM/Clang) through a robust portability layer.
- **Disk Persistence**: Implementing a Pager to manage data movement between memory and disk using fixed-size 4KB pages.
- **Dynamic Schema**: Allowing users to define custom tables with varying field types (`int`, `text`) at runtime.
- **CRUD Operations**: Supporting Create, Read, Update, and Delete operations.
- **Reliability & Stability**: Ensuring balanced B-Tree growth, leak-free memory management, and safe page-splitting logic.

## Architecture

SimpleDB follows a classic, modular database architecture:

1.  **Compiler (Parser)**: Tokenizes and parses SQL-like input into internal `Statement` objects.
2.  **Virtual Machine (VM)**: Executes statements against the B-Tree engine.
3.  **Catalog Manager**: Manages multiple table definitions stored in Page 0.
4.  **B-Tree Storage Engine**: Balanced tree structure for $O(\log n)$ data access.
5.  **Buffer Pool (Pager)**: Manages a cache of pages in memory with **O(1) tracking** and LRU eviction.
6.  **Serialization Layer**: Centralized binary conversion logic in `schema.c`.
7.  **Portability Layer**: Abstracted system calls (I/O, Terminal, Strings) in `os_portability.h` for OS-agnostic development.

## Implementation Technologies

-   **Language**: Modern C23 (using `nullptr`, `static_assert`, `constexpr`, fixed-underlying-type enums).
-   **Platform Support**: Linux and Windows (using LLVM toolchain).
-   **Build System**: Meson + Ninja.
-   **Testing**: Unified Python-based test runner for cross-platform validation.

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

## Educational Insights

-   **Cross-Platform Portability**: Learn how to abstract POSIX and Win32 APIs into a single clean interface.
-   **Buffer Management**: The Pager uses O(1) state tracking to manage the buffer pool efficiently.
-   **Safe B-Tree Growth**: Node splits use a temporary buffer to ensure consistency even during tree structural changes.
