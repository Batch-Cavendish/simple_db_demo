# SimpleDB: Education Guide for Database Principles

This project is designed as a "white-box" implementation of a relational database, making it an ideal tool for teaching the internal mechanics of database engines. Unlike production databases (PostgreSQL, SQLite), SimpleDB prioritizes readability and architectural clarity over performance and feature density.

## Course Modules & Learning Objectives

### 1. Storage & Buffer Management (The Pager)
**Files:** `include/pager.h`, `src/pager.c`
- **Concept:** Databases manage data in fixed-size blocks called **Pages** (4KB).
- **Learning Objective:** Understand the "Buffer Pool" pattern, **LRU (Least Recently Used)** eviction, and dirty-page tracking.
- **Teaching Point:** Discuss `pin_page`/`unpin_page`. Why must we pin a page before use? (To prevent eviction during active I/O).

### 2. Indexing & Data Structures (The B-Tree)
**Files:** `include/btree.h`, `src/btree.c`
- **Concept:** B-Trees provide $O(\log n)$ search, insert, and delete performance.
- **Learning Objective:** Visualize **Leaf Nodes** (data) vs. **Internal Nodes** (roadmap).
- **Teaching Point:** Walk through the **Balanced Node Split** (50/50 strategy) and discuss how it maintains tree depth and performance.

### 3. Resource Management & Memory Safety
**Files:** `src/statement.c`, `src/main.c`, `include/statement.h`
- **Concept:** Managing dynamic memory in long-running C applications.
- **Learning Objective:** Understand the lifecycle of a `Statement` and the importance of centralized cleanup (`free_statement`).
- **Teaching Point:** Demonstrate how a parser error could lead to a memory leak without proper `free` logic. Use `valgrind` to verify leak-free execution.

### 4. Query Compilation (The SQL Parser)
**Files:** `src/statement.c`
- **Concept:** SQL is declarative; the "Compiler" translates it into imperative instructions.
- **Learning Objective:** Understand tokenization, recursive-descent parsing, and **Input Sanitization**.
- **Teaching Point:** Contrast `prepare_statement` (Parsing) with `execute_statement` (Execution). How does `prepare_select` find the correct schema for a table?

### 5. Database Metadata (The Catalog)
**Files:** `include/database.h`, `src/database.c`
- **Concept:** Databases are self-describing systems.
- **Learning Objective:** Understand how a **Catalog** stores table definitions, schemas, and root pointers.
- **Teaching Point:** Why is Page 0 reserved for the Catalog? How do we handle bootstrapping a new database?

### 6. Modern C Engineering (C23)
- **Concept:** Using modern language features to build robust systems.
- **Learning Objective:** 
    - **`[[nodiscard]]`**: Why is it critical for error-handling functions?
    - **`nullptr`**: Type-safe null pointers.
    - **`static_assert`**: Catching architectural mismatches (like page size) at compile time.
    - **Typed Enums**: Ensuring memory-efficient and predictable layouts for disk serialization.

---

## Suggested Student Assignments

### Exercise A: B-Tree Balancing
**Goal:** Understand how B-Trees scale and stay balanced.
- **Tasks:** Insert a large number of records and use `.check [table_name]` to verify integrity. Discuss why a 50/50 split is used instead of a 100/0 split.

### Exercise B: Memory Leak Investigation
**Goal:** Master the use of `valgrind` for memory safety.
- **Tasks:** Intentionally comment out a `free` call in `free_statement` and ask students to find the leak using `valgrind`. Discuss why "definitely lost" is different from "still reachable".

### Exercise C: Catalog Management
**Goal:** Add a `.tables` meta-command to list all tables in the database.
- **Tasks:** Modify `main.c` to iterate through `db->catalog.tables` and print their names and column counts. (Note: This is already partially implemented as a demo).

### Exercise D: Checked Arithmetic
**Goal:** Improve security in `pager.c` using C23 `stdckdint.h`.
- **Tasks:** Replace manual offset calculations with `ckd_mul` and `ckd_add` to prevent integer overflow vulnerabilities when calculating file offsets.

### Exercise E: Secondary Index (Conceptual)
**Goal:** Design a non-unique index on a `TEXT` field.
- **Tasks:** Design a B-tree mapping hashed `TEXT` values to PKs. Discuss atomic updates across multiple trees.

---

## Debugging & Testing

Teach students to use the included test suite and `valgrind`:

```bash
# Run the automated test suite (Unit + Golden)
make test

# Check for memory leaks
valgrind --leak-check=full ./db test.db
```
