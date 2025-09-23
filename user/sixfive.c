#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

static void process_fd(int fd) {
  char buf[512];
  char token[32];
  int tlen = 0;

  for (;;) {
    int n = read(fd, buf, sizeof buf);
    if (n < 0) return;
    if (n == 0) break;

    for (int i = 0; i < n; i++) {
      char c = buf[i];
      if ((c >= '0' && c <= '9') || (tlen == 0 && c == '-')) {
        if (tlen < (int)sizeof(token) - 1) token[tlen++] = c;
      } else {
        if (tlen > 0) {
          token[tlen] = 0;
          int v = atoi(token);
          if (v % 5 == 0 || v % 6 == 0) printf("%d\n", v);
          tlen = 0;
        }
      }
    }
  }

  if (tlen > 0) {
    token[tlen] = 0;
    int v = atoi(token);
    if (v % 5 == 0 || v % 6 == 0) printf("%d\n", v);
  }
}

int main(int argc, char *argv[]) {
  if (argc == 1) {
    process_fd(0);
  } else {
    for (int i = 1; i < argc; i++) {
      int fd = open(argv[i], O_RDONLY);
      if (fd < 0) {
        fprintf(2, "sixfive: cannot open %s\n", argv[i]);
        continue;
      }
      process_fd(fd);
      close(fd);
    }
  }
  exit(0);
}
