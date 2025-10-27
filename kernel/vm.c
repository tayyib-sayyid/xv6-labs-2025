#include "param.h"
#include "types.h"
#include "memlayout.h"
#include "elf.h"
#include "riscv.h"
#include "defs.h"
#include "spinlock.h"
#include "proc.h"
#include "fs.h"
#include "vm.h"

// I walk a lonely road
// The only one that I have ever known
// Don't know where it goes
// But it's home to me and I walk alone 
// I walk alone 
// I walk this empty page
// On the boulevard of broken codes
// Dont know where it goes
// But it's home to me and I walk alone

// Special thanks to Juhair ameerali merchant for being there in the hard days 

/*
 * the kernel's page table.
 */
pagetable_t kernel_pagetable;

extern char etext[];  // kernel.ld sets this to end of kernel code.
extern char trampoline[]; // trampoline.S

extern void *superalloc(void); // ADDED BY SAFEGUARD
extern void superfree(void *); // ADDED BY SAFEGUARD

int mappages_super(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm); // ADDED BY SAFEGUARD

// Make a direct-map page table for the kernel.
pagetable_t
kvmmake(void)
{
  pagetable_t kpgtbl;

  kpgtbl = (pagetable_t) kalloc();
  memset(kpgtbl, 0, PGSIZE);

  // uart registers
  kvmmap(kpgtbl, UART0, UART0, PGSIZE, PTE_R | PTE_W);

  // virtio mmio disk interface
  kvmmap(kpgtbl, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);

#ifdef LAB_NET
  // PCI-E ECAM (configuration space), for pci.c
  kvmmap(kpgtbl, 0x30000000L, 0x30000000L, 0x10000000, PTE_R | PTE_W);

  // pci.c maps the e1000's registers here.
  kvmmap(kpgtbl, 0x40000000L, 0x40000000L, 0x20000, PTE_R | PTE_W);
#endif  

  // PLIC
  kvmmap(kpgtbl, PLIC, PLIC, 0x4000000, PTE_R | PTE_W);

  // map kernel text executable and read-only.
  kvmmap(kpgtbl, KERNBASE, KERNBASE, (uint64)etext - KERNBASE, PTE_R | PTE_X);

  // map kernel data and physical RAM.
  kvmmap(kpgtbl, (uint64)etext, (uint64)etext, PHYSTOP - (uint64)etext, PTE_R | PTE_W);

  // map the trampoline.
  kvmmap(kpgtbl, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);

  // allocate and map kernel stacks.
  proc_mapstacks(kpgtbl);
  
  return kpgtbl;
}

void
kvminit(void)
{
  kernel_pagetable = kvmmake();
}

void
kvminithart()
{
  sfence_vma();
  w_satp(MAKE_SATP(kernel_pagetable));
  sfence_vma();
}

pte_t *
walk(pagetable_t pagetable, uint64 va, int alloc)
{
  if (va >= MAXVA)
    panic("walk");

  for (int level = 2; level > 0; level--) {
    pte_t *pte = &pagetable[PX(level, va)];
    if (*pte & PTE_V) {
      pagetable = (pagetable_t)PTE2PA(*pte);
#ifdef LAB_PGTBL
      if (PTE_LEAF(*pte)) {
        return pte;
      }
#endif
    } else {
      if (!alloc || (pagetable = (pde_t*)kalloc()) == 0)
        return 0;
      memset(pagetable, 0, PGSIZE);
      *pte = PA2PTE(pagetable) | PTE_V;
    }
  }
  return &pagetable[PX(0, va)];
}

// ADDED BY SAFEGUARD
static pte_t *
walk_for_level(pagetable_t pagetable, uint64 va, int alloc, int want_level)
{
  for (int level = 2; level > want_level; level--) {
    pte_t *pte = &pagetable[PX(level, va)];
    if (*pte & PTE_V) {
      pagetable = (pagetable_t)PTE2PA(*pte);
    } else {
      if (!alloc)
        return 0;

      pagetable_t pgtab = (pagetable_t)kalloc();

      if (pgtab == 0)
        return 0;

      memset(pgtab, 0, PGSIZE);

      *pte = PA2PTE(pgtab) | PTE_V;
      pagetable = pgtab;
    }
  }
  return &pagetable[PX(want_level, va)];
}

uint64
walkaddr(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  uint64 pa;

  if (va >= MAXVA)
    return 0;

  pte = walk(pagetable, va, 0);
  if (pte == 0 || (*pte & PTE_V) == 0 || (*pte & PTE_U) == 0)
    return 0;

  pa = PTE2PA(*pte);
  return pa;
}

#if defined(LAB_PGTBL) || defined(SOL_MMAP) || defined(SOL_COW)

// ADDED BY SAFEGUARD
static void __attribute__((unused))
_vmprint(pagetable_t pagetable, int level, uint64 va, int depth)
{
  for (int i = 0; i < 512; i++) {
    pte_t pte = pagetable[i];

    if ((pte & PTE_V) == 0)
      continue;

    uint64 entry_va = va | ((uint64)i << PXSHIFT(level));
    uint64 pa = PTE2PA(pte);

    for (int d = 0; d < depth; d++)
      printf(" ..");

    printf("%p: pte %p pa %p\n", (void *)entry_va, (void *)pte, (void *)pa);

    if ((pte & (PTE_R | PTE_W | PTE_X)) == 0)
      _vmprint((pagetable_t)pa, level - 1, entry_va, depth + 1);
  }
}

void
vmprint(pagetable_t pagetable)
{
  printf("page table %p\n", (void *)pagetable);
  _vmprint(pagetable, 2, 0, 1);
}

#endif

void
kvmmap(pagetable_t kpgtbl, uint64 va, uint64 pa, uint64 sz, int perm)
{
  if (mappages(kpgtbl, va, sz, pa, perm) != 0)
    panic("kvmmap");
}

int
mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm)
{
  uint64 a, last;
  pte_t *pte;

  if ((va % PGSIZE) != 0)
    panic("mappages: va not aligned");

  if ((size % PGSIZE) != 0)
    panic("mappages: size not aligned");

  if (size == 0)
    panic("mappages: size");

  a = va;
  last = va + size - PGSIZE;
  for (;;) {
    if ((pte = walk(pagetable, a, 1)) == 0)
      return -1;
    if (*pte & PTE_V)
      panic("mappages: remap");
    *pte = PA2PTE(pa) | perm | PTE_V;
    if (a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

pagetable_t
uvmcreate()
{
  pagetable_t pagetable;
  pagetable = (pagetable_t)kalloc();
  if (pagetable == 0)
    return 0;
  memset(pagetable, 0, PGSIZE);
  return pagetable;
}

// ADDED BY SAFEGUARD
static int
demote_superpage(pagetable_t pagetable, uint64 va)
{
  pte_t *l1pte = walk_for_level(pagetable, va, 0, 1);
  if (l1pte == 0 || (*l1pte & PTE_V) == 0)
    return -1;
  if (((*l1pte) & (PTE_R|PTE_W|PTE_X)) == 0)
    return -1;

  uint64 parent_pa = PTE2PA(*l1pte);
  int flags = PTE_FLAGS(*l1pte);

  pagetable_t new_pgtab = (pagetable_t)kalloc();
  if (new_pgtab == 0)
    return -1;
  memset(new_pgtab, 0, PGSIZE);

  for (int i = 0; i < SUPERPAGE_NPAGES; i++) {
    uint64 sub_pa = parent_pa + (uint64)i * PGSIZE;
    new_pgtab[i] = PA2PTE(sub_pa) | flags | PTE_V;
  }

  *l1pte = PA2PTE(new_pgtab) | PTE_V;
  return 0;
}

int
mappages_super(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm)
{
  if ((va & (SUPERPAGE_SIZE - 1)) || (pa & (SUPERPAGE_SIZE - 1)) || size != SUPERPAGE_SIZE)
    panic("mappages_super: bad align/size");

  pte_t *pte = walk_for_level(pagetable, va, 1, 1); 

  if (pte == 0)
    return -1;

  if (*pte & PTE_V)
    return -1; 

  *pte = PA2PTE(pa) | perm | PTE_V; 
  return 0;
}

int
ismapped(pagetable_t pagetable, uint64 va)
{
  pte_t *pte = walk(pagetable, va, 0);
  if (pte == 0)
    return 0;
  if (*pte & PTE_V)
    return 1;
  return 0;
}

// EDITED BY SAFEGUARD
void
uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
  uint64 a;
  pte_t *pte;

  if ((va % PGSIZE) != 0)
    panic("uvmunmap: not aligned");

  for (a = va; a < va + npages * PGSIZE; a += PGSIZE) {

    pte_t *l1pte = walk_for_level(pagetable, a, 0, 1);
    if (l1pte && (*l1pte & PTE_V) && ((*l1pte & (PTE_R|PTE_W|PTE_X)) != 0)) {

      if ((a % SUPERPAGE_SIZE) == 0 && (va + npages * PGSIZE) - a >= SUPERPAGE_SIZE) {

        if (do_free) {
          superfree((void*)PTE2PA(*l1pte));
        }
        *l1pte = 0;
        a += SUPERPAGE_SIZE - PGSIZE; 
        continue;
      } else {
        if (demote_superpage(pagetable, a) != 0)
          panic("uvmunmap: demote_superpage failed");
      }
    }

    if ((pte = walk(pagetable, a, 0)) == 0)
      continue;
    if ((*pte & PTE_V) == 0)
      continue;

    if (do_free) {
      uint64 pa = PTE2PA(*pte);
      kfree((void*)pa);
    }
    *pte = 0;
  }
}

uint64
uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz, int xperm)
{
  char *mem;
  uint64 a;
  int sz;

  if (newsz < oldsz)
    return oldsz;

  oldsz = PGROUNDUP(oldsz);

  for (a = oldsz; a < newsz; a += sz) {
    sz = PGSIZE;
    if ((a % SUPERPAGE_SIZE == 0) && (newsz - a >= SUPERPAGE_SIZE)) {
      mem = superalloc();
      if (mem) {
        if (mappages_super(pagetable, a, SUPERPAGE_SIZE, (uint64)mem,
                           PTE_R | PTE_W | PTE_U | xperm) == 0) {
          sz = SUPERPAGE_SIZE;
          continue; 
        } else {
          superfree(mem);
        }
      }
    }

    mem = kalloc();
    if (mem == 0) {
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
#ifndef LAB_SYSCALL
    memset(mem, 0, sz);
#endif
    if (mappages(pagetable, a, sz, (uint64)mem, PTE_R | PTE_U | xperm) != 0) {
      kfree(mem);
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
  }

  return newsz;
}

uint64
uvmdealloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  if (newsz >= oldsz)
    return oldsz;

  if (PGROUNDUP(newsz) < PGROUNDUP(oldsz)) {
    int npages = (PGROUNDUP(oldsz) - PGROUNDUP(newsz)) / PGSIZE;
    uvmunmap(pagetable, PGROUNDUP(newsz), npages, 1);
  }
  return newsz;
}

void
freewalk(pagetable_t pagetable)
{
  for (int i = 0; i < 512; i++) {
    pte_t pte = pagetable[i];
    if ((pte & PTE_V) && (pte & (PTE_R | PTE_W | PTE_X)) == 0) {
      uint64 child = PTE2PA(pte);
      freewalk((pagetable_t)child);
      pagetable[i] = 0;
    } else if (pte & PTE_V) {
      panic("freewalk: leaf");
    }
  }
  kfree((void *)pagetable);
}

void
uvmfree(pagetable_t pagetable, uint64 sz)
{
  if (sz > 0)
    uvmunmap(pagetable, 0, PGROUNDUP(sz) / PGSIZE, 1);
  freewalk(pagetable);
}

int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;
  char *mem;

  for (i = 0; i < sz; i += PGSIZE) {
    pte_t *l1pte = walk_for_level(old, i, 0, 1);
    if (l1pte && (*l1pte & PTE_V) && ((*l1pte & (PTE_R|PTE_W|PTE_X)) != 0)) {
      if ((i % SUPERPAGE_SIZE) == 0 && sz - i >= SUPERPAGE_SIZE) {
        pa = PTE2PA(*l1pte);
        flags = PTE_FLAGS(*l1pte);

        void *child_block = superalloc();
        if (child_block) {
          memmove(child_block, (void*)pa, SUPERPAGE_SIZE);
          if (mappages_super(new, i, SUPERPAGE_SIZE, (uint64)child_block, flags) != 0) {
            superfree(child_block);
            goto err;
          }
          i += SUPERPAGE_SIZE - PGSIZE; 
          continue;
        }
      }
    }

    // Handle normal 4KB pages
    pte = walk(old, i, 0);
    if (pte == 0 || (*pte & PTE_V) == 0)
      continue;

    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);

    if ((mem = kalloc()) == 0)
      goto err;
    memmove(mem, (char*)pa, PGSIZE);
    if (mappages(new, i, PGSIZE, (uint64)mem, flags) != 0) {
      kfree(mem);
      goto err;
    }
  }
  return 0;

err:
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
}


void
uvmclear(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  pte = walk(pagetable, va, 0);
  if (pte == 0)
    panic("uvmclear");
  *pte &= ~PTE_U;
}

int
copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
  uint64 n, va0, pa0;
  pte_t *pte;

  while (len > 0) {
    va0 = PGROUNDDOWN(dstva);
    if (va0 >= MAXVA)
      return -1;

    pa0 = walkaddr(pagetable, va0);
    if (pa0 == 0) {
      if ((pa0 = vmfault(pagetable, va0, 0)) == 0)
        return -1;
    }

    if ((pte = walk(pagetable, va0, 0)) == 0)
      return -1;

    if ((*pte & PTE_W) == 0)
      return -1;

    n = PGSIZE - (dstva - va0);
    if (n > len)
      n = len;
    memmove((void *)(pa0 + (dstva - va0)), src, n);

    len -= n;
    src += n;
    dstva = va0 + PGSIZE;
  }
  return 0;
}

int
copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
{
  uint64 n, va0, pa0;
  
  while (len > 0) {
    va0 = PGROUNDDOWN(srcva);
    pa0 = walkaddr(pagetable, va0);
    if (pa0 == 0) {
      if ((pa0 = vmfault(pagetable, va0, 0)) == 0)
        return -1;
    }
    n = PGSIZE - (srcva - va0);
    if (n > len)
      n = len;
    memmove(dst, (void *)(pa0 + (srcva - va0)), n);

    len -= n;
    dst += n;
    srcva = va0 + PGSIZE;
  }
  return 0;
}

int
copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max)
{
  uint64 n, va0, pa0;
  int got_null = 0;

  while (got_null == 0 && max > 0) {
    va0 = PGROUNDDOWN(srcva);
    pa0 = walkaddr(pagetable, va0);
    if (pa0 == 0)
      return -1;
    n = PGSIZE - (srcva - va0);
    if (n > max)
      n = max;

    char *p = (char *)(pa0 + (srcva - va0));
    while (n > 0) {
      if (*p == '\0') {
        *dst = '\0';
        got_null = 1;
        break;
      } else {
        *dst = *p;
      }
      --n;
      --max;
      p++;
      dst++;
    }

    srcva = va0 + PGSIZE;
  }
  return got_null ? 0 : -1;
}

uint64
vmfault(pagetable_t pagetable, uint64 va, int read)
{
  uint64 mem;
  struct proc *p = myproc();
  
  if (va >= p->sz)
    return 0;
  va = PGROUNDDOWN(va);
  if (ismapped(pagetable, va))
    return 0;

  mem = (uint64)kalloc();
  if (mem == 0)
    return 0;
  memset((void *)mem, 0, PGSIZE);
  if (mappages(p->pagetable, va, PGSIZE, mem, PTE_W | PTE_U | PTE_R) != 0) {
    kfree((void *)mem);
    return 0;
  }
  return mem;
}

#ifdef LAB_PGTBL
pte_t*
pgpte(pagetable_t pagetable, uint64 va)
{
  return walk(pagetable, va, 0);
}
#endif
