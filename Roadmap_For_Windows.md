# Roadmap: Windows Port for SimpleDB

This document outlines the strategy for bringing SimpleDB to the Windows ecosystem while maintaining its educational clarity and C23 standards.

## Phase 1: Tooling & Build System
Since we have already migrated to **Meson + Ninja**, the build system foundation is ready. This stack is highly compatible with Windows.

- [x] **Primary Compiler**: Standardize on **LLVM 21** (`clang-cl` or `clang`) for Windows. LLVM 21 provides robust support for C23 features like `nullptr`, standardized attributes, and fixed-underlying-type enums.
- [x] **Build Tools**: Use **Meson** and **Ninja** to ensure fast, incremental builds that are consistent with the Linux environment.
- [ ] **Version Control**: Use **Git** for local development and **GitHub** for project hosting.
- [ ] **CI Integration**: Implement **GitHub Actions** workflows using `windows-2022` or later runners with the LLVM 21 toolchain pre-installed or via `llvm/actions/setup-llvm`.
- [ ] **Dependency Check**: Ensure no hard-coded Linux paths or shell assumptions exist in `meson.build`.

## Phase 2: Portability Abstractions (System Layer)
The current codebase relies on several POSIX-specific headers and functions (e.g., `unistd.h`, `open`, `lseek`).

- [x] **File I/O Abstraction**:
    - Port `src/pager.c` to use a portable abstraction layer (`include/os_portability.h`).
    - Replace POSIX `unlink()` with a portable wrapper.
    - Ensure all files are opened in **Binary Mode** (`O_BINARY`) to prevent line-ending corruption (LF vs CRLF).
- [ ] **Terminal/REPL**:
    - Windows console handling for colors (if added) or line editing.
    - Ensure `fgets` handles CRLF correctly in the REPL loop.
- [ ] **Path Handling**: Use `\` as the separator or ensure the logic handles both `/` and `\`.

## Phase 3: Unified Python Testing Framework
To ensure 100% consistency across Linux and Windows, we will move away from shell-based testing entirely.

- [x] **Python Test Runner**: Replace `tests/golden_tests/run_golden_tests.sh` with a comprehensive `tests/test_runner.py` (implemented as `run_golden_tests.py`).
    - Use `subprocess` to drive the `db` (Linux) or `db.exe` (Windows) binary.
    - Use `difflib` for "Golden" output comparison.
    - Automate the cleanup of `.db` files across different filesystems.
- [x] **Integration**: Configure Meson to call the Python runner directly.
- [ ] **Binary Compatibility**: Use the Python runner to verify that database files created on one platform can be read on another (checking for padding/endianness issues).

## Phase 4: Distribution
- [ ] **Static Linking**: Provide a standalone `.exe` for students using Windows without a full C environment.
- [ ] **Developer Environment Documentation**: Provide a guide for using **VS Code + MSVC** or **Visual Studio** to build and debug the project.

## Technical Debt to Address
- **`src/pager.c`**: Needs a `common_io.h` to abstract `_open` (Windows) vs `open` (Linux).
- **`tests/unit_tests.c`**: Remove `unistd.h` and use portable alternatives for `unlink`.
