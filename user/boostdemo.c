// boostdemo.c - Demonstrates MLFQ priority boosting (Week 3)
//
// This program spawns CPU-bound and I/O-bound workers, then demonstrates
// how periodic boosting (automatic or manual) prevents starvation.
//
// Usage:
//   boostdemo          - Run demo with automatic periodic boosts only
//   boostdemo manual   - Run demo and trigger manual boosts
//
// Expected behavior:
//   - CPU-bound workers demote to Q3 over time
//   - After a boost (automatic every 100 ticks, or manual), all move to Q0
//   - I/O-bound workers tend to stay at Q0 between boosts

#include "kernel/types.h"
#include "user/user.h"

#define NUM_CPU_WORKERS 2
#define NUM_IO_WORKERS  2

// Print process queue information
void print_queue_status(const char *label)
{
  struct procinfo info[64];
  int n = getprocinfo(info, 64);
  
  printf("\n=== %s ===\n", label);
  printf("PID\tQueue\tTicks\tName\n");
  
  for(int i = 0; i < n; i++) {
    // Only show active processes (RUNNABLE, RUNNING, SLEEPING)
    if(info[i].state >= 2 && info[i].state <= 4) {
      printf("%d\t%d\t%d\t%s\n",
             info[i].pid,
             info[i].queue_level,
             info[i].ticks_at_level,
             info[i].name);
    }
  }
}

// CPU-bound worker: tight loop, no sleeping
void cpu_worker(int id)
{
  volatile unsigned long counter = 0;
  
  printf("  CPU worker %d (PID %d): started\n", id, getpid());
  
  // Run for a while, counting
  for(int iter = 0; iter < 100; iter++) {
    // Burn CPU cycles
    for(int i = 0; i < 5000000; i++) {
      counter++;
    }
  }
  
  printf("  CPU worker %d: done (counter=%lu)\n", id, counter);
  exit(0);
}

// I/O-bound worker: small work + frequent sleeps
void io_worker(int id)
{
  printf("  I/O worker %d (PID %d): started\n", id, getpid());
  
  for(int iter = 0; iter < 50; iter++) {
    // Small amount of work
    volatile int x = 0;
    for(int i = 0; i < 10000; i++) {
      x += i;
    }
    
    // Sleep to simulate I/O wait
    pause(1);
  }
  
  printf("  I/O worker %d: done\n", id);
  exit(0);
}

int main(int argc, char *argv[])
{
  int manual_mode = 0;
  
  if(argc > 1 && strcmp(argv[1], "manual") == 0) {
    manual_mode = 1;
    printf("=== MLFQ Boost Demo (Manual Mode) ===\n");
    printf("Will trigger manual boosts during execution\n\n");
  } else {
    printf("=== MLFQ Boost Demo (Automatic Mode) ===\n");
    printf("Automatic boosts occur every %d ticks\n\n", 100);
  }
  
  // Spawn CPU-bound workers
  printf("Spawning %d CPU-bound workers...\n", NUM_CPU_WORKERS);
  for(int i = 0; i < NUM_CPU_WORKERS; i++) {
    int pid = fork();
    if(pid == 0) {
      cpu_worker(i + 1);
    }
  }
  
  // Spawn I/O-bound workers
  printf("Spawning %d I/O-bound workers...\n", NUM_IO_WORKERS);
  for(int i = 0; i < NUM_IO_WORKERS; i++) {
    int pid = fork();
    if(pid == 0) {
      io_worker(i + 1);
    }
  }
  
  // Wait a bit for workers to start
  pause(5);
  
  // Monitor and optionally boost
  for(int round = 0; round < 5; round++) {
    char label[32];
    
    if(manual_mode && round == 2) {
      // Trigger a manual boost in round 2
      printf("\n*** TRIGGERING MANUAL BOOST (boostproc(-1)) ***\n");
      boostproc(-1);
      print_queue_status("After Manual Boost");
    } else {
      // Just print status
      if(round == 0) {
        strcpy(label, "Initial State");
      } else {
        strcpy(label, "Snapshot ");
        label[9] = '0' + round;
        label[10] = '\0';
      }
      print_queue_status(label);
    }
    
    pause(20);  // Wait between snapshots
  }
  
  // Wait for all children to finish
  printf("\nWaiting for workers to complete...\n");
  for(int i = 0; i < NUM_CPU_WORKERS + NUM_IO_WORKERS; i++) {
    wait(0);
  }
  
  printf("\n=== Demo Complete ===\n");
  printf("Observations:\n");
  printf("  - CPU workers should have spent time at Q3 (demoted)\n");
  printf("  - I/O workers should have stayed mostly at Q0\n");
  printf("  - After boosts, all processes return to Q0\n");
  
  return 0;
}
