# SimpleDB: Education Guide for Database Principles

This project is a "white-box" implementation of a relational database, designed to teach the internal mechanics of database engines through clear, readable C23 code.

## Course Modules & Learning Objectives

### 1. Storage & Buffer Management (The Pager)
**Files:** `include/pager.h`, `src/pager.c`
- **Concept:** Databases manage blocks called **Pages** (4KB).
- **Learning Objective:** Understand **LRU eviction** and **O(1) state tracking**.
- **Teaching Point:** Discuss why we track `num_pages_in_memory`. How does it improve performance compared to scanning all possible slots?

### 2. Indexing & Data Structures (The B-Tree)
**Files:** `include/btree.h`, `src/btree.c`
- **Concept:** B-Trees provide $O(\log n)$ performance.
- **Learning Objective:** Visualize **Balanced Node Splits** and **Safe Tree Growth**.
- **Teaching Point:** Walk through the split logic in `leaf_node_split_and_insert`. Why do we use a temporary buffer during the split? (To prevent data corruption if the split process is interrupted).

### 3. Row Serialization & Schema
**Files:** `include/schema.h`, `src/schema.c`
- **Concept:** Mapping high-level structures to binary formats.
- **Learning Objective:** Understand the role of a **Serialization Layer**.
- **Teaching Point:** How does `serialize_row` handle different data types (INT vs. TEXT)? Discuss the importance of null-terminating text fields on disk.

### 4. Cross-Platform Portability & REPL
**Files:** `include/os_portability.h`, `src/os_portability.c`
- **Concept:** Abstracting OS-specific APIs.
- **Learning Objective:** Understand how to support multiple platforms (POSIX and Windows).
- **Teaching Point:** Discuss terminal "raw mode". Why is it necessary for features like arrow-key navigation and command history?

### 5. Query Compilation (The SQL Parser)
**Files:** `src/statement.c`
- **Concept:** Translating declarative SQL into imperative instructions.
- **Learning Objective:** Understand tokenization and recursive-descent parsing.
- **Teaching Point:** Contrast `prepare_statement` (Parsing) with `execute_statement` (Execution).

---

## Suggested Student Assignments

### Exercise A: Buffer Pool Optimization
**Goal:** Implement a more advanced eviction policy (e.g., MRU or 2Q).
- **Tasks:** Modify `get_page` in `pager.c` to use a different victim selection strategy and discuss the trade-offs.

### Exercise B: Safe B-Tree Deletion
**Goal:** Implement internal node rebalancing during deletion.
- **Tasks:** Currently, deletion only removes the key from the leaf. Challenge students to implement "underflow" detection and sibling borrowing.

### Exercise C: Terminal Features
**Goal:** Add a search feature to the command history.
- **Tasks:** Modify `src/os_portability.c` to support `Ctrl-R` search through the `History` struct.

### Exercise D: New Meta-Commands
**Goal:** Add a `.stats` command to show Pager statistics.
- **Tasks:** Implement a command that prints `num_pages`, `num_pages_in_memory`, and `timer` value to show the database's internal state.

---

## Debugging & Testing

Teach students to use the included Python-based test runner for 100% consistency across environments.

```bash
# Run the automated test suite
meson test -v -C build

# Check for memory leaks
valgrind --leak-check=full ./build/db test.db
```
