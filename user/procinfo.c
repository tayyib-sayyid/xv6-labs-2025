// procinfo.c - User program to test getprocinfo syscall
// Displays information about all running processes in xv6.
// This will be useful for debugging the MLFQ scheduler.

#include "kernel/types.h"
#include "user/user.h"

// Convert state number to string
static char*
statename(int state)
{
  switch(state) {
    case PROC_UNUSED:   return "unused";
    case PROC_USED:     return "used";
    case PROC_SLEEPING: return "sleep";
    case PROC_RUNNABLE: return "runnable";
    case PROC_RUNNING:  return "running";
    case PROC_ZOMBIE:   return "zombie";
    default:            return "???";
  }
}

int
main(int argc, char *argv[])
{
  struct procinfo procs[64];  // NPROC = 64 in xv6
  int nprocs;

  printf("=== Process Information ===\n");
  printf("PID\tSTATE\t\tQLEVEL\tTICKS\tTICKS@LVL\tSIZE\tNAME\n");
  printf("---\t-----\t\t------\t-----\t---------\t----\t----\n");

  nprocs = getprocinfo(procs, 64);
  if(nprocs < 0) {
    printf("getprocinfo failed\n");
    exit(1);
  }

  for(int i = 0; i < nprocs; i++) {
    struct procinfo *p = &procs[i];
    printf("%d\t%s\t\t%d\t%d\t%d\t\t%d\t%s\n",
           p->pid,
           statename(p->state),
           p->queue_level,
           p->ticks_used,
           p->ticks_at_level,
           (int)p->sz,
           p->name);
  }

  printf("\nTotal processes: %d\n", nprocs);
  return 0;
}
