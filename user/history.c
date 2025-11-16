#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
  int fd = open("sh_history", O_RDONLY);
  if (fd < 0) { fprintf(2, "history: no history\n"); exit(1); }
  char buf[512]; int n;
  while ((n = read(fd, buf, sizeof(buf))) > 0) write(1, buf, n);
  close(fd); exit(0);
}
