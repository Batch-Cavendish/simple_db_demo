# SimpleDB: An Educational C Database

SimpleDB is a lightweight, disk-based database engine written in C23. It is designed as an educational resource to demonstrate the core components of a database system, including a B-Tree storage engine, a page-based memory manager (Pager), and dynamic schema management.

## Project Goals

The primary objective of SimpleDB is to provide a transparent and understandable implementation of database internals. Key goals include:
- **Educational Clarity**: Every component is implemented with readability and conceptual honesty in mind, showing *why* certain architectural choices are made (e.g., using B-Trees for $O(\log n)$ lookups).
- **Disk Persistence**: Implementing a Pager to manage data movement between memory and disk using fixed-size 4KB pages.
- **Dynamic Schema**: Allowing users to define custom tables with varying field types (`int`, `text`) at runtime.
- **CRUD Operations**: Supporting Create, Read, Update (via re-insert/delete), and Delete operations.

## Architecture

SimpleDB follows a classic, modular database architecture:

1.  **Compiler (Parser)**: The `prepare_statement` function parses raw text input (SQL-like) and converts it into an internal `Statement` object. This separates syntax analysis from execution logic.
2.  **Virtual Machine (VM)**: The `execute_statement` function executes the bytecode-like `Statement` on the database engine.
3.  **Schema Manager**: Handles the definition and persistence of table structures in "Page 0," making the database self-describing.
4.  **B-Tree Storage Engine**: Data is organized in a B-Tree structure for $O(\log n)$ access.
    -   **Leaf Nodes**: Store data rows.
    -   **Internal Nodes**: Faciliate navigation and scaling.
    -   **Splitting**: Automatically handles node splits (both leaf and internal) to keep the tree balanced as it grows.
5.  **Buffer Pool (Pager)**: Manages a cache of pages in memory with **LRU (Least Recently Used)** eviction and dirty-page tracking to minimize disk I/O.
6.  **Serialization Layer**: Converts C structures into compact binary formats.

## File Structure

The codebase is organized into modular components:

- `include/common.h`: Shared constants and core type definitions.
- `include/pager.h` / `src/pager.c`: Disk I/O management and page-based buffer pool.
- `include/btree.h` / `src/btree.c`: B-Tree implementation and node manipulation.
- `include/table.h` / `src/table.c`: High-level database and cursor management.
- `include/statement.h` / `src/statement.c`: Statement preparation and execution logic.
- `include/schema.h` / `src/schema.c`: Row serialization and schema-related utility functions.
- `src/main.c`: Main REPL loop and CLI interface.

## Implementation Technologies

-   **Language**: C11 (ISO/IEC 9899:2011).
-   **Storage Format**: Custom binary format using 4096-byte pages.
-   **System Interface**: POSIX standard library.
-   **Build System**: GNU Makefile.

## Usage

### Building the Project

To compile SimpleDB, simply run:

```bash
make
```

### Running the Database

Start the database by specifying a database file:

```bash
./db mydb.sqlite
```

### Supported Commands

#### 1. Create a Table
Define the fields of your table. Currently, SimpleDB supports one table per file.
```sql
db > CREATE TABLE users (id INT, username TEXT, email TEXT);
```

#### 2. Insert Data
Insert values corresponding to the defined schema.
```sql
db > INSERT INTO users VALUES (1, 'user1', 'user1@example.com');
db > INSERT INTO users VALUES (2, 'user2', 'user2@example.com');
```

#### 3. Select Data
Retrieve all records (Full Table Scan) or a specific record by key (Index Lookup).
```sql
-- Full Table Scan
db > SELECT * FROM users;
(1, user1, user1@example.com)
(2, user2, user2@example.com)

-- Index Lookup (O(log n))
db > SELECT * FROM users WHERE id = 1;
(1, user1, user1@example.com)
```

#### 4. Delete Data
Delete a record by its primary key. Works with both Integer and Text primary keys (via hashing).
```sql
db > DELETE FROM users WHERE id = 1;
```

#### 5. Exit
Save changes and exit the REPL.
```sql
db > .exit
```

## Educational Insights

-   **Compiler vs. VM**: By separating `prepare_statement` from `execute_statement`, the code demonstrates how databases decouple parsing ("what to do") from execution ("how to do it").
-   **Buffer Pool Management**: The `Pager` implements an LRU eviction policy. It "pins" pages during use to prevent corruption and only writes "dirty" (modified) pages back to disk, teaching the importance of I/O optimization.
-   **B-Tree Internals**: The project now supports full B-Tree growth, including internal node splitting, demonstrating how databases maintain performance ($O(\log n)$) regardless of data size.
-   **Table Scan vs. Index Lookup**: The `select` command demonstrates the massive performance difference between scanning every page (default) and traversing the tree to find a specific key (`select <id>`).
-   **Hashing**: Text primary keys are hashed to `uint32_t` for B-Tree indexing, showing how databases handle non-numeric keys in index structures.
