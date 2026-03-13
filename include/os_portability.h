#ifndef OS_PORTABILITY_H
#define OS_PORTABILITY_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <direct.h>
#include <string.h>
#include <sys/stat.h>

#define open _open
#define read _read
#define write _write
#define lseek _lseek
#define close _close
#define unlink _unlink
#define isatty _isatty
#define strdup _strdup
#define strcasecmp _stricmp
#define strncasecmp _strnicmp

#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#endif

#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif

#ifndef STDERR_FILENO
#define STDERR_FILENO 2
#endif

// File opening flags
#ifndef O_RDWR
#define O_RDWR _O_RDWR
#endif
#ifndef O_CREAT
#define O_CREAT _O_CREAT
#endif
#ifndef O_BINARY
#define O_BINARY _O_BINARY
#endif

// Mode flags (mapped to Win32 constants)
#ifndef S_IWUSR
#define S_IWUSR _S_IWRITE
#endif
#ifndef S_IRUSR
#define S_IRUSR _S_IREAD
#endif

// Use O_BINARY on Windows to prevent line-ending conversion
#define DB_OPEN_FLAGS (O_RDWR | O_CREAT | O_BINARY)

#else
// POSIX systems
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <strings.h>

#define DB_OPEN_FLAGS (O_RDWR | O_CREAT)
#endif

#include "common.h"

// Terminal / REPL Portability
void terminal_enable_raw_mode();
void terminal_disable_raw_mode();
int terminal_read_line(char *buf, size_t size);
void terminal_history_add(const char *line);

#endif // OS_PORTABILITY_H
