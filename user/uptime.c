#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
  printf("%d\n", uptime());  // ticks
  exit(0);
}
