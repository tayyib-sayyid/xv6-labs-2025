# Week 3 Implementation Report: Boosting and Polishing


## 1. Overview

Week 3 completes the MLFQ scheduler by implementing:
- **Priority boosting** to prevent starvation of CPU-bound processes
- **`boostproc()` syscall** for manual boost testing
- **`boostdemo` test program** to demonstrate boosting behavior

---

## 2. Starvation Prevention Strategy

### Problem
Without intervention, CPU-bound processes demote to Queue 3 and may receive very little CPU time if higher-priority I/O-bound processes keep arriving. This leads to **starvation**.

### Solution: Periodic Global Priority Boost
Every `MLFQ_BOOST_INTERVAL` ticks (default: 100), **all processes are moved to Queue 0** with their tick counters reset. This ensures:
1. No process stays stuck at low priority indefinitely
2. CPU-bound processes get periodic opportunities to run
3. The scheduler "forgets" past behavior, allowing processes to prove they've changed

---

## 3. Code Changes

### 3.1 kernel/proc.c — Global Tick Counter

```c
// Global tick counter for MLFQ boost timing (Week 3)
// Incremented on every timer interrupt; used to trigger periodic priority boosts
uint mlfq_ticks = 0;
```
**Purpose:** Tracks elapsed time for boost interval detection.

### 3.2 kernel/proc.c — mlfq_boost_all()

```c
// Move all boostable processes to the highest priority queue (Q0).
// This prevents starvation of CPU-bound processes stuck in low queues.
void
mlfq_boost_all(void)
{
  struct proc *p;
  
  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    // Boost RUNNABLE, RUNNING, and SLEEPING processes
    // Skip UNUSED and ZOMBIE (they don't need scheduling)
    if(p->state == RUNNABLE || p->state == RUNNING || p->state == SLEEPING) {
      p->queue_level = 0;       // Move to highest priority queue
      p->ticks_at_level = 0;    // Reset quantum usage
    }
    release(&p->lock);
  }
}
```

**Key Design Decisions:**
1. **Boost all active states:** RUNNABLE, RUNNING, and SLEEPING processes are boosted. UNUSED and ZOMBIE are skipped.
2. **Per-process locking:** We acquire each process's lock individually, avoiding contention and deadlock.
3. **Reset tick counters:** `ticks_at_level` is reset to give each process a fresh quantum at Q0.

### 3.3 kernel/proc.c — mlfq_boost_proc()

```c
// Boost a single process by PID, or all processes if pid == -1.
// Returns 0 on success, -1 if process not found.
int
mlfq_boost_proc(int pid)
{
  struct proc *p;
  
  if(pid == -1) {
    // Boost all processes
    mlfq_boost_all();
    return 0;
  }
  
  // Boost specific process
  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->pid == pid && p->state != UNUSED && p->state != ZOMBIE) {
      p->queue_level = 0;
      p->ticks_at_level = 0;
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  
  return -1;  // Process not found
}
```

**Purpose:** Kernel helper for the `boostproc()` syscall.

### 3.4 kernel/trap.c — Timer Integration

```c
// give up the CPU if this is a timer interrupt.
if(which_dev == 2) {
  // MLFQ: Track timer ticks for quantum enforcement
  p->ticks_at_level++;
  p->total_ticks++;
  
  // MLFQ Week 3: Periodic priority boost for starvation prevention
  mlfq_ticks++;
  if(mlfq_ticks >= MLFQ_BOOST_INTERVAL) {
    mlfq_boost_all();
    mlfq_ticks = 0;
  }
  
  yield();
}
```

**Location:** `usertrap()` in `kernel/trap.c`

**Why in usertrap?**
- Timer interrupts occur predictably on every tick
- Ensures consistent boost timing regardless of scheduler activity
- Safe context: running process is well-defined

### 3.5 kernel/defs.h — Function Declarations

```c
void            mlfq_boost_all(void);
int             mlfq_boost_proc(int);
extern uint     mlfq_ticks;
```

### 3.6 boostproc() Syscall

**Syscall Number:** 23 (in `kernel/syscall.h`)

**Kernel Implementation (kernel/sysproc.c):**
```c
// Boost a process or all processes to the highest priority queue.
// pid > 0: boost specific process
// pid == -1: boost all processes
// Returns 0 on success, -1 on failure.
uint64
sys_boostproc(void)
{
  int pid;
  argint(0, &pid);
  return mlfq_boost_proc(pid);
}
```

**User Interface (user/user.h):**
```c
int boostproc(int);
```

**Usage:**
- `boostproc(-1)` — Boost all processes to Q0
- `boostproc(pid)` — Boost specific process to Q0

---

## 4. Test Program: boostdemo

### Purpose
Demonstrates priority boosting by spawning mixed workloads and observing queue level changes.

### Usage
```
$ boostdemo           # Observe automatic boosts every 100 ticks
$ boostdemo manual    # Trigger manual boost via boostproc(-1)
```

### Code Summary
```c
// Spawns 2 CPU-bound + 2 I/O-bound workers
// Periodically prints queue levels via getprocinfo()
// In manual mode, triggers boostproc(-1) mid-execution
```

---

## 5. Actual Test Results

### 5.1 Test: `boostdemo` (Automatic Mode)

```
$ boostdemo
=== MLFQ Boost Demo (Automatic Mode) ===
Automatic boosts occur every 100 ticks

Spawning 2 CPU-bound workers...
Spawning 2 I/O-bound workers...
  CPU worker 1 (PID 4): started
  CPU worker 2 (PID 5): started
  I/O worker 1 (PID 6): started
  I/O worker 2 (PID 7): started

=== Initial State ===
PID     Queue   Ticks   Name
1       0       0       init
2       0       0       sh
3       0       0       boostdemo
4       2       0       boostdemo
5       1       1       boostdemo
6       0       0       boostdemo
7       0       0       boostdemo

=== Snapshot 1 ===
PID     Queue   Ticks   Name
1       0       0       init
2       0       0       sh
3       0       0       boostdemo
4       3       3       boostdemo
5       3       0       boostdemo
6       0       0       boostdemo
7       0       0       boostdemo
  CPU worker 1: done (counter=500000000)

=== Snapshot 2 ===
PID     Queue   Ticks   Name
1       0       0       init
2       0       0       sh
3       0       0       boostdemo
5       3       6       boostdemo
6       0       0       boostdemo
7       0       0       boostdemo
  CPU worker 2: done (counter=500000000)
  I/O worker 1: done
  I/O worker 2: done

=== Snapshot 3 ===
PID     Queue   Ticks   Name
1       0       0       init
2       0       0       sh
3       0       0       boostdemo

=== Demo Complete ===
Observations:
  - CPU workers should have spent time at Q3 (demoted)
  - I/O workers should have stayed mostly at Q0
  - After boosts, all processes return to Q0
```

#### Analysis of Automatic Mode Results

| Process | Initial Queue | Snapshot 1 Queue | Behavior |
|---------|---------------|------------------|----------|
| CPU worker 1 (PID 4) | Q2 | Q3 | ✅ Demoted to lowest priority |
| CPU worker 2 (PID 5) | Q1 | Q3 | ✅ Demoted to lowest priority |
| I/O worker 1 (PID 6) | Q0 | Q0 | ✅ Stayed at highest priority |
| I/O worker 2 (PID 7) | Q0 | Q0 | ✅ Stayed at highest priority |

**Why No Visible Automatic Boost in This Run?**

The test completed relatively quickly — all workers finished within 2-3 snapshots. With `MLFQ_BOOST_INTERVAL = 100` ticks, an automatic boost would only be visible if:
- Workers ran longer than 100 ticks, OR
- We used `boostdemo manual` to trigger an explicit boost

The workers completed their computation before the 100-tick boost interval elapsed, so no automatic boost was triggered during the test. This is expected behavior — the automatic boost is a safety mechanism that activates only when processes run long enough to potentially starve.

---

### 5.2 Test: `boostdemo manual` (Manual Boost Mode)

```
$ boostdemo manual
=== MLFQ Boost Demo (Manual Mode) ===
Will trigger manual boosts during execution

Spawning 2 CPU-bound workers...
Spawning 2 I/O-bound workers...
  CPU worker 1 (PID 9): started
  CPU worker 2 (PID 10): started
  I/O worker 1 (PID 11): started
  I/O worker 2 (PID 12): started

=== Initial State ===
PID     Queue   Ticks   Name
1       0       0       init
2       0       0       sh
8       0       0       boostdemo
9       2       0       boostdemo
10      1       1       boostdemo
11      0       0       boostdemo
12      0       0       boostdemo

=== Snapshot 1 ===
PID     Queue   Ticks   Name
1       0       0       init
2       0       0       sh
8       0       0       boostdemo
9       3       3       boostdemo
10      3       0       boostdemo
11      0       0       boostdemo
12      0       0       boostdemo
  CPU worker 1: done (counter=500000000)

*** TRIGGERING MANUAL BOOST (boostproc(-1)) ***

=== After Manual Boost ===
PID     Queue   Ticks   Name
1       0       0       init
2       0       0       sh
8       0       0       boostdemo
10      0       0       boostdemo
11      0       0       boostdemo
12      0       0       boostdemo
  CPU worker 2: done (counter=500000000)
  I/O worker 1: done
  I/O worker 2: done

=== Snapshot 3 ===
PID     Queue   Ticks   Name
1       0       0       init
2       0       0       sh
8       0       0       boostdemo

=== Demo Complete ===
Observations:
  - CPU workers should have spent time at Q3 (demoted)
  - I/O workers should have stayed mostly at Q0
  - After boosts, all processes return to Q0
```

#### Analysis of Manual Mode Results

**Before Manual Boost (Snapshot 1):**
| PID | Process | Queue | Status |
|-----|---------|-------|--------|
| 9 | CPU worker 1 | Q3 | Demoted (CPU-bound) |
| 10 | CPU worker 2 | Q3 | Demoted (CPU-bound) |
| 11 | I/O worker 1 | Q0 | Stayed high (I/O-bound) |
| 12 | I/O worker 2 | Q0 | Stayed high (I/O-bound) |

**After Manual Boost:**
| PID | Process | Queue | Status |
|-----|---------|-------|--------|
| 10 | CPU worker 2 | **Q0** | ✅ Boosted from Q3 → Q0 |
| 11 | I/O worker 1 | Q0 | Stayed at Q0 |
| 12 | I/O worker 2 | Q0 | Stayed at Q0 |

**Why is PID 9 Missing After the Boost?**

CPU worker 1 (PID 9) completed its work and exited **before** the manual boost was triggered. The output shows:
```
  CPU worker 1: done (counter=500000000)

*** TRIGGERING MANUAL BOOST (boostproc(-1)) ***
```

This timing is expected — PID 9 finished just before the boost, so it was no longer in the process table when the boost occurred. Only PID 10 (which was still running at Q3) received the boost and moved to Q0.

**Key Evidence of Successful Manual Boost:**
1. CPU worker 2 (PID 10) was at **Q3** before the boost
2. After `boostproc(-1)`, it moved to **Q0**
3. Its `Ticks` counter also reset to **0** (fresh quantum)

---

## 6. MLFQ Final Policy Summary

### Queue Structure
| Queue | Priority | Time Quantum | Typical Processes |
|-------|----------|--------------|-------------------|
| Q0 | Highest | 1 tick | New, I/O-bound, recently boosted |
| Q1 | High | 2 ticks | Transitioning |
| Q2 | Medium | 4 ticks | Moderate CPU usage |
| Q3 | Lowest | 8 ticks | CPU-bound |

### Rules
1. **New processes start at Q0**
2. **Timer exhaustion → Demotion:** If a process uses its full quantum without sleeping, it demotes to the next lower queue
3. **Voluntary sleep → Stay:** Sleeping resets `ticks_at_level`, rewarding I/O-bound behavior
4. **Periodic boost:** Every 100 ticks, all processes move to Q0

### Anti-Starvation Guarantee
With `MLFQ_BOOST_INTERVAL = 100` ticks, a CPU-bound process at Q3 will:
- Run for at most 100 ticks before being boosted
- Get a fresh start at Q0 with a 1-tick quantum
- Have opportunity to run before being demoted again

---

## 7. Files Modified

| File | Changes |
|------|---------|
| `kernel/proc.c` | Added `mlfq_ticks`, `mlfq_boost_all()`, `mlfq_boost_proc()` |
| `kernel/trap.c` | Added boost check in timer interrupt path |
| `kernel/defs.h` | Added boost function declarations |
| `kernel/syscall.h` | Added `SYS_boostproc` (23) |
| `kernel/syscall.c` | Added `sys_boostproc` to syscall table |
| `kernel/sysproc.c` | Added `sys_boostproc()` implementation |
| `user/user.h` | Added `boostproc()` declaration |
| `user/usys.pl` | Added `boostproc` entry |
| `Makefile` | Added `_boostdemo` to UPROGS |

## 8. Files Created

| File | Purpose |
|------|---------|
| `user/boostdemo.c` | Test program for priority boosting |

---

## 9. Testing Instructions

### Build
```bash
make clean
make CPUS=1 qemu
```

### Test Scenarios

**Test 1: Observe Automatic Boosting**
```
$ cpuheavy &
$ procinfo        # Should show cpuheavy at Q3
$ pause 100
$ procinfo        # After boost, cpuheavy should be at Q0
```

**Test 2: Manual Boost via Syscall**
```
$ cpuheavy &
$ procinfo        # cpuheavy at Q3
$ boostdemo manual
```
Watch for "TRIGGERING MANUAL BOOST" message and observe queue reset.

**Test 3: Full Demo**
```
$ boostdemo
```
Observe CPU workers demoting to Q3, then being boosted back to Q0 periodically.

---

## 10. Qualitative Results

### CPU-bound vs I/O-bound Fairness
- **I/O-bound processes** (frequent sleepers) remain at Q0, getting quick response times
- **CPU-bound processes** demote to Q3 but still make progress due to:
  - Longer time quanta at lower queues (8 ticks at Q3)
  - Periodic boosts every 100 ticks

### Starvation Prevention
- Before boosting: CPU-bound processes at Q3 would rarely run if I/O processes dominated Q0
- After boosting: All processes periodically return to Q0, ensuring forward progress

---

## 11. Project Completion Summary

| Week | Deliverable | Status |
|------|-------------|--------|
| Week 1 | `getprocinfo` syscall | ✅ Complete |
| Week 1 | MLFQ design document | ✅ Complete |
| Week 1 | Queue scaffolding | ✅ Complete |
| Week 2 | 4-level priority queues | ✅ Complete |
| Week 2 | Per-level time quanta | ✅ Complete |
| Week 2 | Demotion logic | ✅ Complete |
| Week 2 | I/O reward (sleep reset) | ✅ Complete |
| Week 3 | Priority boosting | ✅ Complete |
| Week 3 | `boostproc()` syscall | ✅ Complete |
| Week 3 | Test programs | ✅ Complete |

---
