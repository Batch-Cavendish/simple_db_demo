# Roadmap: Windows Port for SimpleDB

This document outlines the strategy for bringing SimpleDB to the Windows ecosystem while maintaining its educational clarity and C23 standards.

## Phase 1: Tooling & Build System
Since we have already migrated to **Meson + Ninja**, the build system foundation is ready. This stack is highly compatible with Windows.

- [x] **Primary Compiler**: Standardized on **LLVM 21** (`clang`) for Windows. LLVM 21 provides robust support for C23 features.
- [x] **Build Tools**: Use **Meson** and **Ninja** to ensure fast, incremental builds consistent with Linux.
- [x] **Version Control**: Use **Git** for local development.
- [ ] **CI Integration**: (Future) Implement **GitHub Actions** workflows.
- [x] **Dependency Check**: Verified no hard-coded Linux paths or shell assumptions in `meson.build`.

## Phase 2: Portability Abstractions (System Layer)
- [x] **File I/O Abstraction**:
    - Ported `src/pager.c` and `src/database.c` to use a portable abstraction layer (`include/os_portability.h`).
    - Handled Win32 mode flags (`_S_IREAD`, `_S_IWRITE`) to prevent permission errors.
    - Ensured all files are opened in **Binary Mode** (`O_BINARY`) to prevent line-ending corruption.
- [x] **Terminal/REPL**:
    - Centralized terminal raw-mode handling in `os_portability.c`.
    - Mapped `isatty` and `STDIN_FILENO` for Windows support.
- [x] **String Portability**:
    - Mapped `strcasecmp` to `_stricmp` and `strncasecmp` to `_strnicmp` in `include/os_portability.h`.

## Phase 3: Unified Python Testing Framework
- [x] **Python Test Runner**: Fully implemented `tests/golden_tests/run_golden_tests.py`.
    - Uses `subprocess` to drive the binary.
    - Uses `difflib` for output comparison.
    - Successfully handles cross-platform path separators.
- [x] **Integration**: Meson calls the Python runner directly.
- [x] **Test Verification**: All 14 golden tests and all unit tests pass on Windows.

## Phase 4: Distribution & Future Steps
- [ ] **Static Linking**: Provide a standalone `.exe`.
- [ ] **Developer Environment Documentation**: Guide for using VS Code on Windows.

## Resolved Technical Debt
- [x] **`src/pager.c`**: Now uses `os_portability.h`.
- [x] **`src/database.c`**: Removed `unistd.h` dependency.
- [x] **`src/statement.c`**: Removed `strings.h` dependency.
- [x] **`tests/unit_tests.c`**: Successfully compiles and runs on Windows using portable abstractions.
