# Gemini Project Context: SimpleDB

## Core Architecture
- **Parser (`src/statement.c`):** Handwritten tokenizer and recursive-descent style parser. Supports case-insensitive SQL keywords. Now handles range operators (`>`, `<`) and transaction keywords (`BEGIN`, `COMMIT`, `ROLLBACK`).
- **VM (`src/statement.c`):** Executes `Statement` objects against the table.
- **Storage (`src/btree.c`, `src/pager.c`):** B-Tree on 4KB pages.
- **Catalog & Schema (`src/database.c`, `src/schema.c`):** Manages multiple tables via a `Catalog` stored on Page 0. Each table has its own root page and `Schema`.

## Implementation Details (C23)
- **Standardized Attributes**: Critical functions use `[[nodiscard]]` to ensure error codes from `prepare_statement` and `execute_statement` are not ignored.
- **Type Safety**: Enums use fixed underlying types (e.g., `enum : uint8_t`) for stable binary layouts.
- **Readability**: Uses `nullptr` for type-safe null pointers.
- **Invariants**: Uses `static_assert` to enforce architectural constraints like `PAGE_SIZE` at compile time.

## SQL Parser
- **Tokenizer:** `consume_token_ctx` handles whitespace, parentheses, commas, equals, `>`, `<`, and semicolons. It manages an internal `PrepareContext` that tracks up to 128 tokens per statement.
- **Case Sensitivity:** Keywords (`SELECT`, `INSERT`, `CREATE`, `BEGIN`, etc.) are case-insensitive.
- **Semicolons:** Supported as optional terminators.

## Current Constraints & Logic
- **Primary Key**: The first column is the primary key. Text PKs are hashed to `uint32_t` for B-Tree indexing.
- **Multiple Tables**: Supports up to 5 tables per database file, managed by the catalog on Page 0.
- **Range Scans**: Efficiently supports `>` and `<` on the primary key column using B-Tree traversal and leaf-node chaining.
- **Balanced B-Tree**: Node splits use a 50/50 strategy to ensure tree balance and optimal search performance.
- **Error Handling**: Standardized `PrepareResult` and `ExecuteResult` provide meaningful feedback for duplicate keys, missing tables, catalog overflow, and string overflows.
- **Memory Management**: Statements use the `{}` empty initializer. Resources are managed via a centralized `free_statement` function to prevent leaks in both preparation and execution stages. `PrepareContext` ensures all consumed tokens are freed.

## Verification Workflow
- Use `make test` to run both unit tests (`assert`-based) and golden tests (shell-based output comparison).
- Run `python3 tests/performance_test.py` to verify $O(\log n)$ vs $O(n)$ performance.
- Always verify with `make` to ensure C23 compatibility.
