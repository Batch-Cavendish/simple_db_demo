# Gemini Project Context: SimpleDB

## Core Architecture
- **Parser (`src/statement.c`):** Handwritten tokenizer and recursive-descent style parser. Supports case-insensitive SQL keywords.
- **VM (`src/statement.c`):** Executes `Statement` objects against the B-Tree engine.
- **Storage (`src/btree.c`, `src/pager.c`):** B-Tree on 4KB pages with **O(1) tracking** in the Pager for efficient buffer pool management.
- **Catalog & Schema (`src/database.c`, `src/schema.c`):** Persistent Catalog on Page 0. Centralized row-level serialization logic.
- **Portability (`include/os_portability.h`, `src/os_portability.c`):** Centralized abstraction layer for cross-platform (Linux/Windows) support. Handles file I/O, terminal raw mode, and string functions.

## Implementation Details (C23)
- **Standardized Attributes**: Critical functions use `[[nodiscard]]` for strict error-code handling.
- **Type Safety**: Enums use fixed underlying types (e.g., `enum : uint8_t`) for binary stability.
- **Invariants**: `static_assert` enforces architectural constraints like `PAGE_SIZE` at compile time.
- **Modern C**: Uses `nullptr`, `constexpr`, and `stdckdint.h`-style safety patterns.

## REPL & Terminal
- **Raw Mode**: Platform-specific terminal handling moved to `os_portability.c`.
- **Features**: Supports up to 100-entry command history, arrow-key navigation (Up/Down for history, Left/Right for cursor), and ANSI box-mode formatting.
- **Portability**: Maps POSIX functions like `isatty` and constants like `STDIN_FILENO` to Win32 equivalents on Windows.

## Current Constraints & Logic
- **B-Tree Safety**: Leaf-node splits use a temporary buffer to prevent data corruption during tree growth.
- **Pager Efficiency**: Victim selection and page counting utilize optimized $O(1)$ and $O(M)$ patterns.
- **Display Modes**: Supports `.mode box` (ANSI-formatted tables) and `.mode plain`.
- **Primary Key**: The first column is the primary key. Text PKs are hashed to `uint32_t`.
- **File Permissions**: Uses explicit `S_IRUSR` and `S_IWUSR` mapping to ensure consistent file access across OSs.

## Verification Workflow
- **Meson:** Use `meson setup build`, `meson compile -C build`, and `meson test -v -C build`.
- **Automated Tests:** 14 golden tests cover all core features including multi-table catalog, range scans, meta-commands, and formatted output modes.
- **Cross-Platform Consistency**: Unified Python-based test runner ensures identical behavior on Linux and Windows.
- **Performance:** Run `python3 tests/performance_test.py` to verify $O(\log n)$ vs $O(n)$ behavior.
