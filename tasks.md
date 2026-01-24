# Undergraduate Practical Assignments: SimpleDB Enhancement

This document outlines a series of tasks designed to help students understand database internals by extending the existing SimpleDB codebase.

---

## Level 1: Beginner (Schema & REPL)

### Task 1.1: Support for `FLOAT` and `BOOL` Types
**Goal**: Add support for floating-point numbers and boolean values in the database schema.
- **Principle**: Serialization and Data Alignment. Databases must know exactly how many bytes each type occupies to calculate memory offsets correctly.
- **Technical Hints**:
    - Update the `FieldType` enum in `src/main.c`.
    - Modify `serialize_field` and `deserialize_field` to handle `float` and `uint8_t` (for bool).
    - Update the `create` command parser to recognize these new keywords.

### Task 1.2: The `describe` Meta-command
**Goal**: Implement a `.describe` command that prints the current table's schema (field names, types, and offsets).
- **Principle**: Metadata Inspection. Users need to be able to query the "data about the data."
- **Technical Hints**:
    - Add a check in the REPL loop for `strcmp(line, ".describe")`.
    - Iterate through `t->schema.fields` and print the `name`, `type`, and `size`.

---

## Level 2: Intermediate (B-Tree & Query Logic)

### Task 2.1: Implement Internal Node Splitting (COMPLETED)
**Goal**: Currently, the database prints "Internal split not implemented." Implement the logic to split internal nodes when they become full.
- **Status**: **Done**. The B-Tree now supports recursive splitting from leaf to root, allowing it to grow indefinitely.

### Task 2.2: Basic `WHERE` Clause for `SELECT` (COMPLETED)
**Goal**: Modify the `select` command to support a single equality filter, e.g., `select where id 5`.
- **Status**: **Done**. Implemented as `select <key>`, utilizing the B-Tree's `find_node` for efficient $O(\log n)$ lookups.

### Task 2.3: The `update` Command
**Goal**: Implement an `update` command to modify existing records, e.g., `update 1 new_name`.
- **Principle**: In-place modification vs. Copy-on-Write.
- **Technical Hints**:
    - Use `find_node` to locate the `Cursor` for the specific ID.
    - Once the leaf cell is found, use `serialize_field` to overwrite the specific bytes in the page buffer.
    - **Remember to call `mark_page_dirty`!**

---

## Level 3: Advanced (System Architecture)

### Task 3.1: The System Catalog (Multiple Tables)
**Goal**: Allow the database to store multiple tables in a single file.
- **Principle**: Database Bootstrapping. The database needs a "Table of Tables" to know where each B-Tree starts.
- **Technical Hints**:
    - Modify Page 0 to store a `SystemCatalog` structure instead of a single `Schema`.
    - The catalog should map `table_name` strings to `root_page_num` integers.
    - Update `create` and `insert` to require a table name (e.g., `insert users 1 loris`).

### Task 3.2: Secondary Indexes
**Goal**: Create a separate B-Tree index for a non-primary key field (e.g., an index on `username`).
- **Principle**: Redundancy for Performance. Indexes trade disk space for search speed.
- **Technical Hints**:
    - Create a new B-Tree where the "value" stored in the leaf node is the Primary Key of the main table.
    - Every time a row is inserted into the main table, you must also insert an entry into the index B-Tree.
    - Implement a "Fast Select" that uses this index to find records without a full table scan.

### Task 3.3: Durability (Write-Ahead Log)
**Goal**: Ensure data is not lost if the program crashes before `pager_flush` is called.
- **Principle**: Atomicity and Durability (ACID).
- **Technical Hints**:
    - Before modifying a page in memory, write the change to a separate `.log` file.
    - On startup (`db_open`), check if a log file exists. If so, replay the changes to restore the database state.
