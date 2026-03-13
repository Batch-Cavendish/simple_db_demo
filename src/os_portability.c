#include "os_portability.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <termios.h>
#include <unistd.h>
#include <poll.h>

#define MAX_HISTORY 100
typedef struct {
  char *lines[MAX_HISTORY];
  int count;
  int current;
} History;

static History history = {0};
static struct termios orig_termios;

static void history_add(const char *line) {
  if (line[0] == '\0') return;
  if (history.count > 0 && strcmp(history.lines[history.count - 1], line) == 0) return;

  if (history.count < MAX_HISTORY) {
    history.lines[history.count++] = strdup(line);
  } else {
    free(history.lines[0]);
    for (int i = 0; i < MAX_HISTORY - 1; i++) {
      history.lines[i] = history.lines[i + 1];
    }
    history.lines[MAX_HISTORY - 1] = strdup(line);
  }
}

void terminal_history_add(const char *line) {
    history_add(line);
}

void terminal_disable_raw_mode() {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void terminal_enable_raw_mode() {
  tcgetattr(STDIN_FILENO, &orig_termios);
  atexit(terminal_disable_raw_mode);

  struct termios raw = orig_termios;
  raw.c_iflag &= ~(IXON | ICRNL);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN);
  raw.c_cflag |= (CS8);
  raw.c_cc[VMIN] = 1;
  raw.c_cc[VTIME] = 0;

  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

static void refresh_line(const char *prompt, const char *buf, int len, int pos) {
  printf("\r\x1b[K%s%s", prompt, buf);
  if (pos < len) {
    printf("\r\x1b[%dC", (int)strlen(prompt) + pos);
  }
  fflush(stdout);
}

int terminal_read_line(char *buf, size_t size) {
  const char *prompt = "db > ";
  int len = 0;
  int pos = 0;
  memset(buf, 0, size);
  history.current = history.count;
  
  printf("%s", prompt);
  fflush(stdout);

  while (1) {
    char c;
    if (read(STDIN_FILENO, &c, 1) <= 0) return -1;

    if (c == '\r' || c == '\n') {
      buf[len] = '\0';
      printf("\n");
      return len;
    } else if (c == 127 || c == 8) { // Backspace
      if (pos > 0) {
        memmove(buf + pos - 1, buf + pos, len - pos);
        len--;
        pos--;
        buf[len] = '\0';
        refresh_line(prompt, buf, len, pos);
      }
    } else if (c == 4) { // Ctrl-D
      if (len == 0) return -1;
    } else if (c == '\x1b') { // Escape sequence
      struct pollfd pfd = { STDIN_FILENO, POLLIN, 0 };
      if (poll(&pfd, 1, 50) > 0) {
        char seq[2];
        if (read(STDIN_FILENO, &seq[0], 1) > 0 && read(STDIN_FILENO, &seq[1], 1) > 0) {
          if (seq[0] == '[') {
            if (seq[1] == 'A') { // Up arrow
              if (history.current > 0) {
                history.current--;
                strncpy(buf, history.lines[history.current], size - 1);
                buf[size - 1] = '\0';
                len = pos = (int)strlen(buf);
                refresh_line(prompt, buf, len, pos);
              }
            } else if (seq[1] == 'B') { // Down arrow
              if (history.current < history.count) {
                history.current++;
                if (history.current < history.count) {
                  strncpy(buf, history.lines[history.current], size - 1);
                  buf[size - 1] = '\0';
                } else {
                  buf[0] = '\0';
                }
                len = pos = (int)strlen(buf);
                refresh_line(prompt, buf, len, pos);
              }
            } else if (seq[1] == 'C') { // Right arrow
              if (pos < len) {
                pos++;
                refresh_line(prompt, buf, len, pos);
              }
            } else if (seq[1] == 'D') { // Left arrow
              if (pos > 0) {
                pos--;
                refresh_line(prompt, buf, len, pos);
              }
            }
          }
        }
      }
    } else if ((unsigned char)c >= 32) {
      if (len < (int)size - 1) {
        memmove(buf + pos + 1, buf + pos, len - pos);
        buf[pos] = c;
        len++;
        buf[len] = '\0';
        pos++;
        refresh_line(prompt, buf, len, pos);
      }
    }
  }
}

#else
// Windows stubs (or minimal implementation)
void terminal_enable_raw_mode() {}
void terminal_disable_raw_mode() {}
void terminal_history_add(const char *line) {}
int terminal_read_line(char *buf, size_t size) {
    printf("db > ");
    fflush(stdout);
    if (fgets(buf, size, stdin) == NULL) return -1;
    buf[strcspn(buf, "\n")] = 0;
    buf[strcspn(buf, "\r")] = 0;
    return strlen(buf);
}
#endif
