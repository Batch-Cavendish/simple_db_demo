#ifndef OS_PORTABILITY_H
#define OS_PORTABILITY_H

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <direct.h>

#define open _open
#define read _read
#define write _write
#define lseek _lseek
#define close _close
#define unlink _unlink

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

// Mode flags (simplified for educational database)
#define S_IWUSR 0
#define S_IRUSR 0

// Use O_BINARY on Windows to prevent line-ending conversion
#define DB_OPEN_FLAGS (O_RDWR | O_CREAT | O_BINARY)

#else
// POSIX systems
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#define DB_OPEN_FLAGS (O_RDWR | O_CREAT)
#endif

#endif // OS_PORTABILITY_H
