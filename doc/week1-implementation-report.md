# Week 1 Implementation Report: MLFQ Scheduler Setup

**Project:** Multi-Level Feedback Queue (MLFQ) Scheduler for xv6-RISC-V  
**Date:** Week 1  
**Branch:** `project`

---

## Overview

This document summarizes all code changes made during Week 1 to set up the foundation for the MLFQ scheduler, including:
1. Implementation of the `getprocinfo` system call
2. MLFQ data structure scaffolding
3. Design documentation

---

## Test Output

After running `make CPUS=1 qemu` and executing `procinfo` in the xv6 shell:

```
$ procinfo
=== Process Information ===
PID     STATE           QLEVEL  TICKS   TICKS@LVL       SIZE    NAME
---     -----           ------  -----   ---------       ----    ----
1       sleep           0       0       0               20480   init
2       sleep           0       0       0               24576   sh
3       running         0       0       0               20480   procinfo

Total processes: 3
```

**Explanation:**
- **PID 1 (init):** The first user process, sleeping while waiting for children
- **PID 2 (sh):** The shell, sleeping while waiting for `procinfo` to complete
- **PID 3 (procinfo):** Currently running (executing the syscall)
- **QLEVEL = 0:** All processes start at highest priority queue (MLFQ not active yet)
- **TICKS = 0:** Tick counting will be implemented in Week 2
- **SIZE:** Memory footprint in bytes (~20KB per process)

---

## File Changes Summary

### Kernel Files Modified

| File | Changes |
|------|---------|
| `kernel/syscall.h` | Added syscall number 22 |
| `kernel/syscall.c` | Registered new syscall handler |
| `kernel/sysproc.c` | Implemented `sys_getprocinfo()` |
| `kernel/proc.h` | Added `struct procinfo` and MLFQ fields to `struct proc` |
| `kernel/proc.c` | Added `getprocinfo()` helper, MLFQ scaffolding, TODO comments |
| `kernel/defs.h` | Added function prototype |
| `kernel/param.h` | Added MLFQ constants |

### User Files Modified

| File | Changes |
|------|---------|
| `user/user.h` | Added `struct procinfo` and syscall declaration |
| `user/usys.pl` | Added syscall stub generation |
| `user/procinfo.c` | **New file** - test program |
| `Makefile` | Added `_procinfo` to UPROGS |

### Documentation Created

| File | Purpose |
|------|---------|
| `doc/mlfq-design-week1.md` | MLFQ scheduler design document |
| `doc/week1-implementation-report.md` | This file |

---

## Detailed Code Changes

### 1. System Call Number (`kernel/syscall.h`)

```c
// Added at the end of the file:
#define SYS_getprocinfo 22
```

**Purpose:** Assigns a unique number (22) to identify the new syscall.

---

### 2. Syscall Dispatch Table (`kernel/syscall.c`)

```c
// Added extern declaration:
extern uint64 sys_getprocinfo(void);

// Added to syscalls[] array:
[SYS_getprocinfo] sys_getprocinfo,
```

**Purpose:** Tells the kernel which function to call when syscall 22 is invoked.

---

### 3. Process Info Structure (`kernel/proc.h`)

```c
// New struct for returning process info to user space:
struct procinfo {
  int pid;                     // Process ID
  int state;                   // Process state (enum procstate)
  char name[16];               // Process name
  int priority;                // Priority level (placeholder for MLFQ)
  int queue_level;             // Current MLFQ queue level (placeholder)
  uint ticks_used;             // Total CPU ticks used by this process
  uint ticks_at_level;         // Ticks used at current queue level
  uint64 sz;                   // Memory size in bytes
};

// Added to struct proc (MLFQ fields):
  int queue_level;             // Current queue level (0 = highest priority)
  uint ticks_at_level;         // Ticks used at current queue level
  uint total_ticks;            // Total CPU ticks consumed by this process
```

**Purpose:** 
- `struct procinfo` is a user-visible structure for the syscall return data
- The new fields in `struct proc` will track MLFQ state per-process

---

### 4. Syscall Implementation (`kernel/sysproc.c`)

```c
// Fill a procinfo array with information about all processes.
// Returns the number of valid entries written.
uint64
sys_getprocinfo(void)
{
  uint64 addr;  // user pointer to struct procinfo array
  int max;      // max number of entries in the array

  argaddr(0, &addr);
  argint(1, &max);

  return getprocinfo(addr, max);
}
```

**Purpose:** Extracts arguments from user space and calls the helper function.

---

### 5. Helper Function (`kernel/proc.c`)

```c
// Fill a user-space array with information about all non-UNUSED processes.
int
getprocinfo(uint64 addr, int max)
{
  struct proc *p;
  struct procinfo info;
  int count = 0;
  struct proc *curproc = myproc();

  for(p = proc; p < &proc[NPROC] && count < max; p++) {
    acquire(&p->lock);
    if(p->state != UNUSED) {
      info.pid = p->pid;
      info.state = p->state;
      safestrcpy(info.name, p->name, sizeof(info.name));
      info.priority = p->queue_level;
      info.queue_level = p->queue_level;
      info.ticks_used = p->total_ticks;
      info.ticks_at_level = p->ticks_at_level;
      info.sz = p->sz;

      release(&p->lock);

      if(copyout(curproc->pagetable, addr + count * sizeof(info),
                 (char *)&info, sizeof(info)) < 0) {
        return -1;
      }
      count++;
    } else {
      release(&p->lock);
    }
  }
  return count;
}
```

**Purpose:** Iterates through the process table, copies info to user space safely.

---

### 6. MLFQ Constants (`kernel/param.h`)

```c
// MLFQ (Multi-Level Feedback Queue) Scheduler Parameters
#define NQUEUE              4    // Number of priority queues (0 = highest)
#define MLFQ_QUANTUM_Q0     1    // Time quantum for queue 0 (ticks)
#define MLFQ_QUANTUM_Q1     2    // Time quantum for queue 1 (ticks)
#define MLFQ_QUANTUM_Q2     4    // Time quantum for queue 2 (ticks)
#define MLFQ_QUANTUM_Q3     8    // Time quantum for queue 3 (ticks)
#define MLFQ_BOOST_INTERVAL 100  // Ticks between priority boosts
```

**Purpose:** Configurable parameters for the MLFQ scheduler.

---

### 7. MLFQ Scaffolding (`kernel/proc.c`)

```c
// Time quantum for each priority level (in ticks)
int mlfq_quantum[NQUEUE] = {
  MLFQ_QUANTUM_Q0,  // Queue 0: highest priority, shortest quantum
  MLFQ_QUANTUM_Q1,  // Queue 1
  MLFQ_QUANTUM_Q2,  // Queue 2
  MLFQ_QUANTUM_Q3   // Queue 3: lowest priority, longest quantum
};

// Last time a priority boost was performed
uint mlfq_last_boost = 0;
```

**Purpose:** Data structures ready for Week 2 implementation.

---

### 8. Process Initialization (`kernel/proc.c` - `allocproc()`)

```c
  // Initialize MLFQ scheduler fields (Week 1 scaffolding)
  p->queue_level = 0;      // Start at highest priority queue
  p->ticks_at_level = 0;   // No ticks used yet at this level
  p->total_ticks = 0;      // No total ticks consumed yet
```

**Purpose:** New processes start at priority queue 0 with zero tick counts.

---

### 9. User-Space Interface (`user/user.h`)

```c
// Process information structure
struct procinfo {
  int pid;
  int state;
  char name[16];
  int priority;
  int queue_level;
  unsigned int ticks_used;
  unsigned int ticks_at_level;
  unsigned long sz;
};

// Process state constants
#define PROC_UNUSED   0
#define PROC_USED     1
#define PROC_SLEEPING 2
#define PROC_RUNNABLE 3
#define PROC_RUNNING  4
#define PROC_ZOMBIE   5

// Syscall declaration
int getprocinfo(struct procinfo*, int);
```

---

### 10. Syscall Stub (`user/usys.pl`)

```perl
entry("getprocinfo");
```

**Purpose:** Generates assembly code to invoke syscall 22 via `ecall`.

---

### 11. Test Program (`user/procinfo.c`)

A user program that:
1. Calls `getprocinfo()` to retrieve all process information
2. Formats and displays the data in a readable table
3. Shows PID, state, queue level, tick counts, memory size, and name

---

## TODO Comments Added (for Week 2)

The following locations have TODO comments marking where MLFQ logic will be added:

1. **`scheduler()`** - Replace round-robin with queue-based selection
2. **`yield()`** - Update tick counts and check for demotion
3. **`sleep()`** - Reset ticks_at_level for I/O-bound behavior

---

## How to Build and Test

```bash
# Clean and build
make clean
make qemu 

# In xv6 shell:
$ procinfo

# Exit QEMU
```

---

## Next Steps (Week 2)

1. Implement queue management functions
2. Replace round-robin scheduler with MLFQ
3. Add tick tracking on timer interrupts
4. Implement demotion logic
5. Test with CPU-bound vs I/O-bound processes
