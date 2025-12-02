# MLFQ Scheduler Design Document — Week 1

**Project:** Multi-Level Feedback Queue (MLFQ) Scheduler for xv6-RISC-V  
**Author:** [Your Name]  
**Date:** Week 1  
**Target:** Single-core xv6 (`CPUS=1`)

---

## 1. Overview and Goals

The goal of this project is to replace the default round-robin scheduler in xv6 with a **Multi-Level Feedback Queue (MLFQ)** scheduler. MLFQ is a sophisticated scheduling algorithm that:

1. **Optimizes turnaround time** by running shorter jobs first (similar to SJF).
2. **Minimizes response time** for interactive processes by keeping them at high priority.
3. **Prevents starvation** through periodic priority boosts.
4. **Learns process behavior** by observing CPU usage patterns and adjusting priorities dynamically.

### Why MLFQ for xv6?

- xv6's default round-robin scheduler treats all processes equally, which is suboptimal for mixed workloads (I/O-bound vs CPU-bound).
- MLFQ provides a more realistic scheduling experience similar to production operating systems.
- The simple xv6 codebase makes it an ideal learning platform for implementing complex schedulers.

---

## 2. MLFQ Design Parameters

### 2.1 Number of Priority Queues

**Decision: 4 priority levels (queues 0–3)**

| Queue Level | Priority | Description |
|-------------|----------|-------------|
| 0 | Highest | New processes, interactive/I/O-bound processes |
| 1 | High | Processes that have used some CPU time |
| 2 | Medium | Processes showing CPU-bound behavior |
| 3 | Lowest | Long-running CPU-bound processes |

**Rationale:** 4 levels provide enough granularity to distinguish process behavior without excessive complexity. This is consistent with many real-world MLFQ implementations.

### 2.2 Time Quantum per Level

**Decision: Exponentially increasing time slices**

| Queue Level | Time Quantum (ticks) | Description |
|-------------|---------------------|-------------|
| 0 | 1 tick | Very short — responsive to I/O |
| 1 | 2 ticks | Short |
| 2 | 4 ticks | Medium |
| 3 | 8 ticks | Long — for CPU-bound processes |

**Rationale:** 
- Short quanta at high priority ensure quick response times for interactive processes.
- Longer quanta at low priority reduce context switch overhead for CPU-bound jobs.
- Exponential growth (2^level) is a common and effective pattern.

**Note:** In xv6, one "tick" corresponds to one timer interrupt (~100ms with default `w_stimecmp(r_time() + 1000000)`). This may need adjustment for finer granularity.

### 2.3 Demotion Rules (Priority Decrease)

**Rule:** If a process uses its **entire time quantum** at a given level without voluntarily yielding (e.g., sleeping for I/O), it is **demoted to the next lower priority queue**.

- This identifies CPU-bound processes that consume their full time slice.
- Demotion happens at the end of the time quantum, not immediately.

**Implementation:**
1. Track `ticks_at_level` for each process.
2. When `ticks_at_level >= quantum_for_level[queue_level]`, demote the process.
3. Reset `ticks_at_level` to 0 after demotion.
4. Processes at level 3 (lowest) remain there — they cannot be demoted further.

### 2.4 Promotion Rules (Priority Boost)

**Rule 1: Voluntary Yield Boost (I/O behavior)**
- If a process voluntarily yields (calls `sleep()`) before using its full quantum, it stays at its current priority level.
- This rewards I/O-bound processes that don't hog the CPU.

**Rule 2: Periodic Priority Boost (Anti-starvation)**
- Every **S ticks** (e.g., S = 100 ticks = ~10 seconds), **all processes** are boosted to the highest priority queue (level 0).
- This prevents starvation of CPU-bound processes that might otherwise never run.
- Reset `ticks_at_level` for all processes after a boost.

**Decision: S = 100 ticks (configurable via `MLFQ_BOOST_INTERVAL`)**

### 2.5 New Process Placement

**Rule:** All new processes start at **queue level 0** (highest priority).

- This ensures new processes get quick initial response.
- Their behavior determines future priority.

---

## 3. Data Structures

### 3.1 Additions to `struct proc` (in `kernel/proc.h`)

```c
// MLFQ scheduler fields
int queue_level;             // Current queue level (0 = highest priority)
uint ticks_at_level;         // Ticks used at current queue level
uint total_ticks;            // Total CPU ticks consumed by this process
```

**Already added in Week 1 scaffolding.**

### 3.2 MLFQ Queue Structure (in `kernel/proc.c`)

**Option A: Array of process pointers per level (simpler)**
```c
#define NQUEUE 4              // Number of priority levels

// Queue arrays: each queue holds pointers to RUNNABLE processes at that level
struct proc *mlfq_queues[NQUEUE][NPROC];
int mlfq_queue_size[NQUEUE];  // Number of processes in each queue
```

**Option B: Linked list per level (more flexible)**
```c
// Add to struct proc:
struct proc *next_in_queue;   // Next process in same queue

// Global queue heads:
struct proc *mlfq_heads[NQUEUE];
struct proc *mlfq_tails[NQUEUE];
```

**Decision for Week 2:** Use **Option A (array-based)** for simplicity. Can be optimized to linked lists later if needed.

### 3.3 MLFQ Constants (in `kernel/param.h`)

```c
// MLFQ Scheduler Parameters
#define NQUEUE           4    // Number of priority queues
#define MLFQ_QUANTUM_Q0  1    // Time quantum for queue 0 (highest priority)
#define MLFQ_QUANTUM_Q1  2    // Time quantum for queue 1
#define MLFQ_QUANTUM_Q2  4    // Time quantum for queue 2
#define MLFQ_QUANTUM_Q3  8    // Time quantum for queue 3 (lowest priority)
#define MLFQ_BOOST_INTERVAL 100  // Ticks between priority boosts
```

---

## 4. Scheduler Algorithm (Pseudocode)

```
scheduler():
    forever:
        // Check for priority boost
        if (ticks - last_boost_time >= MLFQ_BOOST_INTERVAL):
            boost_all_processes_to_level_0()
            last_boost_time = ticks

        // Find highest priority runnable process
        for level = 0 to NQUEUE-1:
            for each process p in queue[level]:
                if p is RUNNABLE:
                    switch to p
                    // When p returns (yields or preempted):
                    p->ticks_at_level++
                    p->total_ticks++
                    
                    // Check for demotion
                    if p->ticks_at_level >= quantum[p->queue_level]:
                        if p->queue_level < NQUEUE-1:
                            p->queue_level++
                        p->ticks_at_level = 0
                    
                    goto restart_scheduler_loop

        // No runnable process found
        wait_for_interrupt()
```

---

## 5. Integration Points in xv6

### 5.1 Files to Modify

| File | Changes |
|------|---------|
| `kernel/param.h` | Add MLFQ constants |
| `kernel/proc.h` | Add fields to `struct proc` (done in Week 1) |
| `kernel/proc.c` | Main scheduler changes, queue management |
| `kernel/trap.c` | May need to track ticks per process on timer interrupt |

### 5.2 Key Functions to Modify

1. **`scheduler()`** — Replace round-robin with MLFQ algorithm
2. **`allocproc()`** — Initialize new process at queue level 0 (done in Week 1)
3. **`yield()`** — May need to update tick counts
4. **`sleep()`** — Reset `ticks_at_level` on voluntary yield (promotes I/O-bound behavior)
5. **`wakeup()`** — Ensure woken process is placed in correct queue

### 5.3 Context Switch Considerations

The current context switch mechanism via `swtch()` remains unchanged. MLFQ changes **which** process is selected, not **how** the switch happens.

---

## 6. Testing Strategy

### 6.1 Using `procinfo` (Week 1)

The `procinfo` user program displays:
- Process queue level
- Ticks used at current level
- Total ticks consumed

Run it to observe scheduler behavior:
```
$ procinfo
```

### 6.2 Week 2+ Test Programs

1. **CPU-bound test:** A process that loops forever should be demoted to level 3.
2. **I/O-bound test:** A process that frequently sleeps should stay at level 0.
3. **Mixed workload:** Run both types simultaneously and verify correct prioritization.
4. **Starvation test:** Verify priority boost prevents starvation.

---

## 7. Week-by-Week Implementation Plan

### Week 1 (Current) — Setup & Design ✓
- [x] Understand xv6 scheduler
- [x] Implement `getprocinfo` syscall
- [x] Add MLFQ fields to `struct proc`
- [x] Write this design document
- [ ] Add MLFQ constants and queue data structures (scaffolding)
- [ ] Add TODO comments in scheduler path

### Week 2 — Core MLFQ Implementation
- [ ] Implement queue management functions (enqueue, dequeue)
- [ ] Replace round-robin scheduler with MLFQ
- [ ] Implement demotion logic
- [ ] Test with CPU-bound processes

### Week 3 — Refinement & Testing
- [ ] Implement priority boost (anti-starvation)
- [ ] Handle edge cases (process exit, fork, exec)
- [ ] Write comprehensive test programs
- [ ] Performance analysis and tuning

---

## 8. Invariants and Correctness

1. **Queue Level Bounds:** `0 <= p->queue_level < NQUEUE` always.
2. **Single Queue Membership:** A RUNNABLE process exists in exactly one queue.
3. **Priority Order:** Higher-priority queues are always checked before lower ones.
4. **No Starvation:** Periodic boosts guarantee all processes eventually run.
5. **Lock Ordering:** Always acquire `p->lock` before modifying queue membership.

---

## 9. References

- Arpaci-Dusseau & Arpaci-Dusseau, *Operating Systems: Three Easy Pieces*, Chapter 8 (MLFQ)
- xv6 book: https://pdos.csail.mit.edu/6.828/2023/xv6/book-riscv-rev3.pdf
- Original MLFQ: Corbató et al., *The Compatible Time-Sharing System* (CTSS), 1962

---

## Appendix A: `getprocinfo` Syscall Reference

```c
// User-space struct (in user/user.h)
struct procinfo {
  int pid;
  int state;
  char name[16];
  int priority;           // Same as queue_level for MLFQ
  int queue_level;
  unsigned int ticks_used;
  unsigned int ticks_at_level;
  unsigned long sz;
};

// Syscall
int getprocinfo(struct procinfo *buf, int max);
// Returns: number of processes, or -1 on error
```

Usage:
```c
struct procinfo procs[64];
int n = getprocinfo(procs, 64);
for (int i = 0; i < n; i++) {
    printf("PID %d at queue level %d\n", procs[i].pid, procs[i].queue_level);
}
```

---

# Week 2 Implementation Details

## 9. MLFQ Scheduler Implementation

### 9.1 Queue Helper Functions

Two helper functions were added to `kernel/proc.c`:

#### mlfq_find_runnable(int level)
```c
static struct proc* mlfq_find_runnable(int level) {
  struct proc *p;
  for (p = proc; p < &proc[NPROC]; p++) {
    if (p->state == RUNNABLE && p->queue_level == level)
      return p;
  }
  return 0;
}
```

Scans the process table for the first RUNNABLE process at the specified queue level. Returns the process or NULL if none found.

**Design Choice:** Rather than maintaining separate queue data structures (which would require complex synchronization), we scan the existing proc[] array filtered by queue_level. This is simpler and sufficient for xv6's small NPROC (64 processes).

#### mlfq_demote(struct proc *p)
```c
static void mlfq_demote(struct proc *p) {
  if (p->queue_level < NQUEUE - 1) {
    p->queue_level++;
    p->ticks_at_level = 0;
  }
}
```

Demotes a process to the next lower priority queue and resets its tick counter. Processes already at the lowest queue (level 3) remain there.

### 9.2 MLFQ Scheduler Loop

The core scheduler was replaced with MLFQ logic:

```c
void scheduler(void) {
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;

  for (;;) {
    intr_on();
    int found = 0;

    // Scan queues from highest priority (0) to lowest (3)
    for (int level = 0; level < NQUEUE && !found; level++) {
      p = mlfq_find_runnable(level);
      if (p) {
        acquire(&p->lock);
        if (p->state == RUNNABLE) {
          p->state = RUNNING;
          c->proc = p;
          swtch(&c->context, &p->context);
          c->proc = 0;

          // After process returns, check for demotion
          if (p->ticks_at_level >= mlfq_quantum[p->queue_level]) {
            mlfq_demote(p);
          }
          found = 1;
        }
        release(&p->lock);
      }
    }
  }
}
```

**Key Behaviors:**
1. **Priority scanning:** Always starts at queue 0 and scans to queue 3, ensuring higher-priority processes run first.
2. **Round-robin within level:** `mlfq_find_runnable()` returns the first matching process, providing simple round-robin when multiple processes are at the same level.
3. **Demotion check:** After a process yields or is preempted, we check if it exceeded its time quantum and demote if necessary.

### 9.3 Timer Tick Tracking

Modified `usertrap()` in `kernel/trap.c` to track CPU usage:

```c
// Give up the CPU if this is a timer interrupt.
if (which_dev == 2) {
  // MLFQ: Track ticks before yielding
  p->ticks_at_level++;
  p->total_ticks++;
  yield();
}
```

Each timer interrupt increments both:
- `ticks_at_level`: Used for demotion decisions (reset on demotion or I/O)
- `total_ticks`: Total CPU time consumed (never reset, for accounting)

### 9.4 Yield and Sleep Updates

#### yield()
```c
void yield(void) {
  struct proc *p = myproc();
  acquire(&p->lock);
  p->state = RUNNABLE;
  // Note: ticks_at_level is NOT reset here — demotion handled in scheduler
  sched();
  release(&p->lock);
}
```

Yield does not reset tick counters, preserving the CPU usage information for demotion decisions.

#### sleep()
```c
void sleep(void *chan, struct spinlock *lk) {
  struct proc *p = myproc();
  acquire(&p->lock);
  release(lk);

  p->chan = chan;
  p->state = SLEEPING;
  // MLFQ: Reset ticks when voluntarily sleeping (I/O reward)
  p->ticks_at_level = 0;

  sched();
  p->chan = 0;
  release(&p->lock);
  acquire(lk);
}
```

**I/O Reward Mechanism:** When a process voluntarily sleeps (typically waiting for I/O), we reset `ticks_at_level`. This rewards I/O-bound processes by giving them a fresh quantum, helping them stay at higher priority levels.

## 10. Test Programs

### 10.1 cpuheavy — CPU-Bound Test

A process that runs an infinite CPU-intensive loop:
```c
int main(void) {
  int pid = getpid();
  printf("cpuheavy (PID %d): Starting CPU-bound workload\n", pid);
  volatile unsigned long counter = 0;
  for (;;) {
    counter++;
    // Every 50 million iterations, print status
    if (counter % 50000000 == 0) {
      printf("cpuheavy (PID %d): counter = %lu\n", pid, counter);
    }
  }
}
```

**Expected Behavior:** Should quickly demote from queue 0 → 1 → 2 → 3 as it consumes its time quantum at each level without sleeping.

### 10.2 iosim — I/O-Bound Simulation

A process that simulates I/O-bound behavior by frequently sleeping:
```c
int main(void) {
  int pid = getpid();
  printf("iosim (PID %d): Starting I/O-bound simulation\n", pid);
  int iterations = 0;
  for (;;) {
    // Do a small amount of work
    volatile int j = 0;
    for (int i = 0; i < 10000; i++) j += i;

    // Sleep to simulate I/O wait
    sleep(1);
    iterations++;
    if (iterations % 50 == 0) {
      printf("iosim (PID %d): completed %d I/O cycles\n", pid, iterations);
    }
  }
}
```

**Expected Behavior:** Should stay at queue 0 because each sleep() resets ticks_at_level, preventing demotion.

### 10.3 mlfqtest — Combined MLFQ Test

Forks multiple CPU-bound and I/O-bound workers, then monitors their queue levels:
```c
int main(int argc, char *argv[]) {
  printf("=== MLFQ Scheduler Test ===\n");

  // Fork CPU-heavy workers
  for (int i = 0; i < 2; i++) {
    if (fork() == 0) {
      volatile unsigned long c = 0;
      for (;;) c++;
    }
  }

  // Fork I/O-bound workers
  for (int i = 0; i < 2; i++) {
    if (fork() == 0) {
      for (;;) {
        volatile int j = 0;
        for (int k = 0; k < 1000; k++) j++;
        sleep(1);
      }
    }
  }

  // Monitor process states
  sleep(10);
  for (int round = 0; round < 5; round++) {
    struct procinfo info[64];
    int n = getprocinfo(info, 64);
    printf("\n--- Snapshot %d ---\n", round + 1);
    for (int i = 0; i < n; i++) {
      if (info[i].state >= 3) { // RUNNABLE or RUNNING
        printf("PID %d: queue=%d ticks=%d total=%d\n",
               info[i].pid, info[i].queue_level,
               info[i].ticks_at_level, info[i].total_ticks);
      }
    }
    sleep(20);
  }
  printf("\nTest complete. Use Ctrl+A X to exit QEMU.\n");
  for (;;) sleep(100);
}
```

**Expected Behavior:**
- CPU-bound workers should be at queue 3 (lowest priority)
- I/O-bound workers should stay at queue 0 or 1 (high priority)
- The procinfo output should show this differentiation

### 10.4 procinfo — Queue Status Display

A simple utility to display current process queue assignments:
```c
int main(void) {
  struct procinfo info[64];
  int n = getprocinfo(info, 64);
  printf("PID\tState\tQueue\tTicks\tTotal\tName\n");
  for (int i = 0; i < n; i++) {
    printf("%d\t%d\t%d\t%d\t%d\t%s\n",
           info[i].pid, info[i].state, info[i].queue_level,
           info[i].ticks_at_level, info[i].total_ticks, info[i].name);
  }
}
```

## 11. Testing Instructions

### 11.1 Building and Running

```bash
# Build with single CPU
make clean
make CPUS=1 qemu
```

### 11.2 Test Scenarios

**Test 1: CPU-Bound Demotion**
```
$ cpuheavy &
$ sleep 5
$ procinfo
```
The cpuheavy process should show queue_level = 3.

**Test 2: I/O-Bound Stays High**
```
$ iosim &
$ sleep 5
$ procinfo
```
The iosim process should show queue_level = 0.

**Test 3: Mixed Workload**
```
$ mlfqtest
```
Watch the snapshots — CPU workers demote to queue 3, I/O workers stay at queue 0.

## 12. Week 3 Preview

The following features are planned for Week 3:

1. **Priority Boost:** Periodically move all processes to queue 0 to prevent starvation (using MLFQ_BOOST_INTERVAL = 100 ticks).
2. **Accounting-Based Demotion:** Track total CPU time at each level rather than per-quantum to prevent gaming.
3. **Enhanced Diagnostics:** Syscall to dump scheduler statistics.

---

**Document Updated:** Week 2
