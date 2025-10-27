// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
#include "memlayout.h"

// ADDED BY SAFEGUARD
static void *super_free_list[N_SUPERPAGES];
static int super_free_cnt = 0;


// ADDED BY SAFEGUARD, slight name change to avoid conflict
void freerange(void *vstart, void *vend);
// void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

// ADDED BY SAFEGUARD
void
freerange(void *vstart, void *vend)
{
  char *p = (char*)PGROUNDUP((uint64)vstart);
  for (; (uint64)p + PGSIZE <= (uint64)vend; ) {
    if (super_free_cnt < N_SUPERPAGES &&
        ((uint64)p & (SUPERPAGE_SIZE - 1)) == 0 &&
        (uint64)p + SUPERPAGE_SIZE <= (uint64)vend) {
      super_free_list[super_free_cnt++] = p;
      p = (char*)((uint64)p + SUPERPAGE_SIZE);
      continue;
    }
    kfree(p);
    p += PGSIZE;
  }
}

// void
// freerange(void *pa_start, void *pa_end)
// {
//   char *p;
//   p = (char*)PGROUNDUP((uint64)pa_start);
//   for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
//     kfree(p);
// }

// ADDED BY SAFEGUARD
void *
superalloc(void)
{
  if (super_free_cnt == 0)
    return 0;
  return super_free_list[--super_free_cnt];
}

// ADDED BY SAFEGUARD
void
superfree(void *pa)
{
  if (super_free_cnt >= N_SUPERPAGES)
    panic("superfree: overflow");

  super_free_list[super_free_cnt++] = pa;
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
