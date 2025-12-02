// iosim.c - I/O-bound simulation for MLFQ scheduler
// This process frequently sleeps, simulating I/O-bound behavior.
// It should stay at queue 0 (high priority) because it never
// uses its full time quantum before sleeping.

#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int pid = getpid();
  int iterations = 0;
  
  printf("iosim[%d]: Starting I/O-bound simulation\n", pid);
  printf("iosim[%d]: Should stay at Q0 (sleeps frequently)\n", pid);
  printf("iosim[%d]: Run 'procinfo' to observe\n\n", pid);
  
  for(;;) {
    // Small amount of work
    volatile int x = 0;
    for(int i = 0; i < 1000; i++) {
      x = x + 1;
    }
    
    // Sleep (simulates waiting for I/O)
    // This resets ticks_at_level, preventing demotion
    pause(1);  // Sleep for 1 tick
    
    iterations++;
    if(iterations % 50 == 0) {
      printf("iosim[%d]: iteration %d (still at high priority)\n", pid, iterations);
    }
  }
  
  return 0;
}
