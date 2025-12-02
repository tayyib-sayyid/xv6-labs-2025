// mlfqtest.c - Combined test for MLFQ scheduler
// Runs both CPU-bound and I/O-bound children to demonstrate
// MLFQ priority differentiation.

#include "kernel/types.h"
#include "user/user.h"

void
cpu_work(int id)
{
  printf("  CPU worker %d: starting (should demote to Q3)\n", id);
  for(int iter = 0; iter < 100; iter++) {
    // Burn CPU
    volatile int x = 0;
    for(int i = 0; i < 5000000; i++) {
      x = x + 1;
    }
    if(iter % 20 == 0) {
      printf("  CPU worker %d: iter %d\n", id, iter);
    }
  }
  printf("  CPU worker %d: done\n", id);
  exit(0);
}

void
io_work(int id)
{
  printf("  I/O worker %d: starting (should stay at Q0)\n", id);
  for(int iter = 0; iter < 100; iter++) {
    // Small work then sleep
    volatile int x = 0;
    for(int i = 0; i < 1000; i++) {
      x = x + 1;
    }
    pause(1);  // Simulates I/O wait
    if(iter % 20 == 0) {
      printf("  I/O worker %d: iter %d\n", id, iter);
    }
  }
  printf("  I/O worker %d: done\n", id);
  exit(0);
}

int
main(int argc, char *argv[])
{
  int pid;
  
  printf("=== MLFQ Scheduler Test ===\n");
  printf("Starting 2 CPU-bound + 2 I/O-bound processes\n");
  printf("Run 'procinfo' to see queue levels\n\n");
  
  // Fork CPU-bound children
  pid = fork();
  if(pid == 0) {
    cpu_work(1);
  }
  
  pid = fork();
  if(pid == 0) {
    cpu_work(2);
  }
  
  // Fork I/O-bound children
  pid = fork();
  if(pid == 0) {
    io_work(1);
  }
  
  pid = fork();
  if(pid == 0) {
    io_work(2);
  }
  
  // Parent waits for all children
  for(int i = 0; i < 4; i++) {
    wait(0);
  }
  
  printf("\n=== All workers completed ===\n");
  printf("Expected behavior:\n");
  printf("  - CPU workers should have been at Q3 (demoted)\n");
  printf("  - I/O workers should have stayed at Q0 (not demoted)\n");
  
  return 0;
}
