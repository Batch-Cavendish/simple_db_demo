# SimpleDB Development Roadmap - COMPLETED

This project is a fully functional educational database in C11.

## Phase 1: Foundation & REPL
- [x] Set up project structure.
- [x] Implement InputBuffer.
- [x] Create REPL loop.
- [x] Handle Meta-commands.

## Phase 2: The Compiler Front-End
- [x] Define Statement types.
- [x] Implement Tokenizer/Parser (Dynamic Schema aware).

## Phase 3: The Storage Engine (Pager)
- [x] Implement Pager for file I/O.
- [x] Implement Page caching.

## Phase 4: B-Tree Implementation
- [x] Define Node structures.
- [x] Implement Leaf Node splitting.
- [x] Implement B-Tree Search algorithm.

## Phase 5: Dynamic Schema & Custom Fields
- [x] Implement Schema persistence in Page 0.
- [x] Implement dynamic row serialization based on schema.
- [x] Support `CREATE TABLE` command.

## Phase 6: CRUD Operations
- [x] Finalize `INSERT`.
- [x] Finalize `SELECT`.
- [x] Implement `DELETE`.

## Phase 7: Testing & Refinement
- [x] Verified persistence and splitting.
- [x] Verified dynamic fields.
- [x] Strict C11 compliance.