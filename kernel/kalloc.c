// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "include/types.h"
#include "include/param.h"
#include "include/memlayout.h"
#include "include/riscv.h"
#include "include/spinlock.h"
#include "include/kalloc.h"
#include "include/string.h"
#include "include/printf.h"

void freerange(void *pa_start, void *pa_end);

extern char kernel_end[]; // first address after kernel.

struct run
{
  struct run *next;
};

struct
{
  struct spinlock lock;
  struct run *freelist;
  uint64 npage;
} kmem;

int ref_cnt[PHYSTOP / PGSIZE];
uint64 total_pages = 0;
struct spinlock refr_lock;

void kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&refr_lock, "refr");
  kmem.freelist = 0;
  kmem.npage = 0;
  freerange(kernel_end, (void *)PHYSTOP);
#ifdef DEBUG
  printf("kernel_end: %p, phystop: %p\n", kernel_end, (void *)PHYSTOP);
  printf("kinit\n");
#endif
}

void freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char *)PGROUNDUP((uint64)pa_start);
  for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE)
  {
    acquire(&refr_lock);
    ref_cnt[(uint64)p / PGSIZE] = 1;
    total_pages++;
    release(&refr_lock);
    kfree(p);
  }
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void kfree(void *pa)
{
  struct run *r;

  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < kernel_end || (uint64)pa >= PHYSTOP)
    panic("kfree");
  acquire(&refr_lock);
  if (--ref_cnt[(uint64)pa / PGSIZE] > 0)
  {
    release(&refr_lock);
    return;
  }
  release(&refr_lock);

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run *)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  kmem.npage++;
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
  if (r)
  {
    kmem.freelist = r->next;
    kmem.npage--;
  }
  release(&kmem.lock);

  if (r)
  {
    memset((char *)r, 5, PGSIZE); // fill with junk
    acquire(&refr_lock);
    ref_cnt[(uint64)r / PGSIZE] = 1;
    release(&refr_lock);
  }
  return (void *)r;
}

uint64
freemem_amount(void)
{
  return kmem.npage << PGSHIFT;
}
void ref_add(uint64 pa)
{
  acquire(&refr_lock);
  ref_cnt[pa / PGSIZE]++;
  release(&refr_lock);
}

uint64
allocated_pages(void)
{
  acquire(&kmem.lock);
  uint64 pages = total_pages - kmem.npage;
  release(&kmem.lock);
  return pages;
}
