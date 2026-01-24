# SimpleDB: An Educational C Database

SimpleDB is a lightweight, disk-based database engine written in C11. It is designed as an educational resource to demonstrate the core components of a database system, including a B-Tree storage engine, a page-based memory manager (Pager), and dynamic schema management.

## Project Goals

The primary objective of SimpleDB is to provide a transparent and understandable implementation of database internals. Key goals include:
- **Educational Clarity**: Every component is implemented with readability and conceptual honesty in mind, showing *why* certain architectural choices are made (e.g., using B-Trees for $O(\log n)$ lookups).
- **Disk Persistence**: Implementing a Pager to manage data movement between memory and disk using fixed-size 4KB pages.
- **Dynamic Schema**: Allowing users to define custom tables with varying field types (`int`, `text`) at runtime.
- **CRUD Operations**: Supporting Create, Read, Update (via re-insert/delete), and Delete operations.

## Architecture

SimpleDB follows a classic database architecture:

1.  **REPL (Read-Eval-Print Loop)**: The front-end interface that parses user input and translates it into database commands.
2.  **Schema Manager**: Handles the definition and persistence of table structures. The schema is stored in "Page 0" of the database file to ensure the database can self-describe upon reopening.
3.  **B-Tree Storage Engine**: Data is organized in a B-Tree structure.
    -   **Leaf Nodes**: Store the actual data rows.
    -   **Internal Nodes**: Store keys and pointers to child nodes to facilitate efficient searching.
    -   **Splitting**: When a leaf node exceeds its 4KB capacity, it splits into two, maintaining the tree's balance.
4.  **Pager**: Manages a cache of 4KB pages. It interacts directly with the filesystem using POSIX system calls (`open`, `lseek`, `read`, `write`).
5.  **Serialization Layer**: Converts C structures and dynamic field values into a compact binary format for disk storage.

## Implementation Technologies

-   **Language**: C11 (ISO/IEC 9899:2011).
-   **Storage Format**: Custom binary format using 4096-byte pages.
-   **System Interface**: POSIX standard library for file descriptor management and memory allocation.
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
db > create id int username text email text
```

#### 2. Insert Data
Insert values corresponding to the defined schema.
```sql
db > insert 1 user1 user1@example.com
db > insert 2 user2 user2@example.com
```

#### 3. Select Data
Retrieve all records from the table.
```sql
db > select
(1, user1, user1@example.com)
(2, user2, user2@example.com)
```

#### 4. Delete Data
Delete a record by its primary key (the first field defined).
```sql
db > delete 1
```

#### 5. Exit
Save changes and exit the REPL.
```sql
db > .exit
```

## Educational Insights

-   **Why Pages?**: Databases use fixed-size pages (usually 4KB or 8KB) to align with hardware sector sizes, minimizing the number of expensive disk I/O operations.
-   **Why B-Trees?**: B-Trees are preferred over binary trees or hash maps because they are "disk-aware." Their high branching factor reduces the tree height, meaning fewer pages need to be loaded from disk to find a specific key.
-   **Why Page 0?**: Storing the schema at a fixed location (Page 0) allows the database to boot without external metadata, making the database file truly portable.
