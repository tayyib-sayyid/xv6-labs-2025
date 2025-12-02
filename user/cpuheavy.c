// cpuheavy.c - CPU-bound test program for MLFQ scheduler
// This process should be demoted from queue 0 down to queue 3
// because it never yields voluntarily or sleeps.

#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int pid = getpid();
  int iterations = 0;
  
  printf("cpuheavy[%d]: Starting CPU-bound workload\n", pid);
  printf("cpuheavy[%d]: Should demote from Q0 to Q3 over time\n", pid);
  printf("cpuheavy[%d]: Run 'procinfo' in another shell to observe\n\n", pid);
  
  // Infinite CPU-bound loop
  // This should cause demotion: Q0 -> Q1 -> Q2 -> Q3
  for(;;) {
    // Busy work - just burn CPU cycles
    volatile int x = 0;
    for(int i = 0; i < 10000000; i++) {
      x = x + 1;
    }
    
    iterations++;
    if(iterations % 100 == 0) {  // Print less frequently
      printf("cpuheavy[%d]: iter %d\n", pid, iterations);
    }
  }
  
  return 0;
}
