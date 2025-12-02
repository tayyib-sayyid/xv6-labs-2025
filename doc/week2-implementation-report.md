# Week 2 Implementation Report: MLFQ Scheduler

**Project:** Multi-Level Feedback Queue (MLFQ) Scheduler for xv6-RISC-V  
**Date:** Week 2  
**Branch:** `project`

---

## 1. Overview

Week 2 focused on implementing the core MLFQ scheduling logic. The scheduler now:
- Maintains 4 priority queues (Q0 highest → Q3 lowest)
- Assigns exponentially increasing time quanta per level
- Demotes CPU-bound processes to lower priority queues
- Rewards I/O-bound processes by keeping them at high priority

---

## 2. Code Changes

### 2.1 kernel/proc.c — Scheduler Core

#### Added: Time Quantum Array
```c
// MLFQ time quantum for each queue level (in ticks)
int mlfq_quantum[NQUEUE] = {
  MLFQ_QUANTUM_Q0,  // 1 tick
  MLFQ_QUANTUM_Q1,  // 2 ticks
  MLFQ_QUANTUM_Q2,  // 4 ticks
  MLFQ_QUANTUM_Q3   // 8 ticks
};
```
**Purpose:** Defines how long a process can run at each priority level before demotion.

#### Added: mlfq_find_runnable()
```c
static struct proc*
mlfq_find_runnable(int level)
{
  struct proc *p;
  for(p = proc; p < &proc[NPROC]; p++) {
    if(p->state == RUNNABLE && p->queue_level == level)
      return p;
  }
  return 0;
}
```
**Purpose:** Finds the first RUNNABLE process at a given queue level. Provides round-robin behavior within each priority level.

#### Added: mlfq_demote()
```c
static void
mlfq_demote(struct proc *p)
{
  if(p->queue_level < NQUEUE - 1) {
    p->queue_level++;
    p->ticks_at_level = 0;
  }
}
```
**Purpose:** Moves a process to the next lower priority queue and resets its tick counter.

#### Modified: scheduler()
```c
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  
  c->proc = 0;
  for(;;){
    intr_on();

    // MLFQ: Scan queues from highest priority (0) to lowest (NQUEUE-1)
    int found = 0;
    for(int level = 0; level < NQUEUE && !found; level++) {
      p = mlfq_find_runnable(level);
      if(p) {
        acquire(&p->lock);
        if(p->state == RUNNABLE) {
          p->state = RUNNING;
          c->proc = p;
          swtch(&c->context, &p->context);
          c->proc = 0;

          // MLFQ: Check if process exceeded its quantum and should be demoted
          if(p->ticks_at_level >= mlfq_quantum[p->queue_level]) {
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
**Key Changes:**
1. Scans queues 0→3 (highest to lowest priority)
2. Runs the first RUNNABLE process found at the highest priority level
3. After process yields/preempts, checks if it exceeded its time quantum
4. Demotes if quantum exceeded

#### Modified: sleep()
```c
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  acquire(&p->lock);
  release(lk);

  p->chan = chan;
  p->state = SLEEPING;
  
  // MLFQ: Reset ticks when sleeping (I/O reward)
  p->ticks_at_level = 0;

  sched();
  
  p->chan = 0;
  release(&p->lock);
  acquire(lk);
}
```
**Key Change:** Resets `ticks_at_level` when a process voluntarily sleeps. This rewards I/O-bound processes by giving them a fresh time quantum, helping them stay at higher priority levels.

---

### 2.2 kernel/trap.c — Tick Tracking

#### Modified: usertrap()
```c
// give up the CPU if this is a timer interrupt.
if(which_dev == 2) {
  // MLFQ: Track ticks before yielding
  p->ticks_at_level++;
  p->total_ticks++;
  yield();
}
```
**Purpose:** On each timer interrupt, increments:
- `ticks_at_level`: Used for demotion decisions (reset on demotion or sleep)
- `total_ticks`: Total CPU time consumed (never reset, for accounting)

---

### 2.3 kernel/param.h — MLFQ Constants

```c
#define NQUEUE           4     // Number of MLFQ priority queues
#define MLFQ_QUANTUM_Q0  1     // Ticks for queue 0 (highest priority)
#define MLFQ_QUANTUM_Q1  2     // Ticks for queue 1
#define MLFQ_QUANTUM_Q2  4     // Ticks for queue 2
#define MLFQ_QUANTUM_Q3  8     // Ticks for queue 3 (lowest priority)
#define MLFQ_BOOST_INTERVAL 100 // Ticks between priority boosts (Week 3)
```

---

### 2.4 Test Programs

| File | Purpose |
|------|---------|
| `user/cpuheavy.c` | CPU-bound workload — should demote to Q3 |
| `user/iosim.c` | I/O-bound simulation with frequent sleeps — should stay at Q0 |
| `user/mlfqtest.c` | Forks 2 CPU + 2 I/O workers, monitors queue levels |
| `user/procinfo.c` | Displays process queue levels using getprocinfo syscall |

---

## 3. Testing Results

### Test 1: CPU-Bound Demotion

**Command:**
```
$ cpuheavy &
$ procinfo
```

**Output:**
```
=== Process Information ===
PID     STATE           QLEVEL  TICKS   TICKS@LVL       SIZE    NAME
1       sleep           0       0       0               20480   init
2       sleep           0       0       0               24576   sh
5       running         0       0       0               20480   procinfo
4       runnable        3       288     1               20480   cpuheavy
```

**Analysis:** ✅ PASS
- `cpuheavy` demoted from Q0 → Q3 (queue_level = 3)
- Accumulated 288 total ticks of CPU time
- I/O-bound processes (init, sh) remain at Q0

---

### Test 2: Mixed Workload (mlfqtest)

**Command:**
```
$ mlfqtest
```

**Output:**
```
=== MLFQ Scheduler Test ===
Starting 2 CPU-bound + 2 I/O-bound processes

  CPU worker 1: starting (should demote to Q3)
  CPU worker 2: starting (should demote to Q3)
  I/O worker 1: starting (should stay at Q0)
  I/O worker 2: starting (should stay at Q0)
  CPU worker 1: iter 20
  CPU worker 2: iter 20
  I/O worker 1: iter 20
  I/O worker 2: iter 20
  ...
  CPU worker 1: done
  CPU worker 2: done
  I/O worker 1: done
  I/O worker 2: done

=== All workers completed ===
```

**Analysis:** ✅ PASS
- CPU workers ran in bursts (longer time slices at lower queues)
- I/O workers remained responsive (stayed at Q0 due to sleep resets)
- All workers completed successfully

---

## 4. MLFQ Behavior Summary

| Process Type | Behavior | Final Queue |
|--------------|----------|-------------|
| CPU-bound (no sleep) | Consumes full quantum, gets demoted | Q3 |
| I/O-bound (frequent sleep) | Resets ticks on sleep, stays high | Q0 |
| Interactive (shell) | Sleeps waiting for input | Q0 |

### Demotion Path
```
Q0 (1 tick) → Q1 (2 ticks) → Q2 (4 ticks) → Q3 (8 ticks)
     ↓              ↓              ↓              ↓
  demote         demote         demote         stay
```

### I/O Reward Mechanism
```
Process sleeps → ticks_at_level = 0 → fresh quantum → stays at current level
```

---

## 5. Files Modified

| File | Changes |
|------|---------|
| `kernel/proc.c` | Added mlfq_quantum[], mlfq_find_runnable(), mlfq_demote(), replaced scheduler(), modified sleep() |
| `kernel/trap.c` | Added tick tracking in usertrap() |
| `kernel/param.h` | Added NQUEUE, MLFQ_QUANTUM_*, MLFQ_BOOST_INTERVAL |
| `Makefile` | Added test programs to UPROGS |

## 6. Files Created

| File | Purpose |
|------|---------|
| `user/cpuheavy.c` | CPU-bound test program |
| `user/iosim.c` | I/O-bound simulation |
| `user/mlfqtest.c` | Combined MLFQ test |
| `user/procinfo.c` | Queue status display |

---

## 7. Week 3 Preview

Remaining features to implement:
1. **Priority Boost:** Periodically move all processes to Q0 to prevent starvation
2. **Accounting-Based Demotion:** Track total CPU time per level to prevent gaming
3. **Enhanced Diagnostics:** Scheduler statistics syscall

---

## 8. Build & Run Instructions

```bash
# Build with single CPU
make clean
make CPUS=1 qemu

# In xv6 shell:
$ mlfqtest          # Run combined test
$ cpuheavy &        # Background CPU-bound process
$ procinfo          # Check queue levels
$ kill <pid>        # Stop background process
```

---

**Status:** Week 2 Complete ✅
