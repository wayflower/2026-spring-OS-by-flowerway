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
  uint64 ref_cnt[PHYSTOP / PGSIZE];
} kmem;

void kinit()
{
  initlock(&kmem.lock, "kmem");
  kmem.freelist = 0;
  kmem.npage = 0;
  for (int i = 0; i < NELEM(kmem.ref_cnt); i++)
    kmem.ref_cnt[i] = 0;
  freerange(kernel_end, (void *)PHYSTOP);
#ifdef DEBUG
  printf("kernel_end: %p, phystop: %p\n", kernel_end, (void *)PHYSTOP);
  printf("kinit\n");
#endif
}

uint64 total_pages = 0;

void freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char *)PGROUNDUP((uint64)pa_start);
  for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE)
  {
    // 【修改点】：在 freerange 最初释放页面给系统前，强行将计数设为 1
    // 因为 kfree 内部会执行减 1 操作，这样减完刚好是 0，正常挂入空闲链表
    kmem.ref_cnt[(uint64)p / PGSIZE] = 1;
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

  acquire(&kmem.lock);

  kmem.ref_cnt[(uint64)pa / PGSIZE]--;

  if (kmem.ref_cnt[(uint64)pa / PGSIZE] > 0)
  {
    release(&kmem.lock);
    return;
  }
  release(&kmem.lock); // 减到 0 了，放开锁，继续往下走真正的清理流程

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
    kmem.ref_cnt[(uint64)r / PGSIZE] = 1;
  }
  release(&kmem.lock);

  if (r)
    memset((char *)r, 5, PGSIZE); // fill with junk
  return (void *)r;
}

// 【新增改动】：安全地获取引用计数
int kgetref(void *pa)
{
  int count;
  acquire(&kmem.lock);
  count = kmem.ref_cnt[(uint64)pa / PGSIZE];
  release(&kmem.lock);
  return count;
}

// 【新增改动】：安全地增加引用计数 (fork 时调用)
void kaddref(void *pa)
{
  acquire(&kmem.lock);
  kmem.ref_cnt[(uint64)pa / PGSIZE]++;
  release(&kmem.lock);
}

uint64
freemem_amount(void)
{
  return kmem.npage << PGSHIFT;
}

uint64
allocated_pages(void)
{
  acquire(&kmem.lock);
  uint64 pages = total_pages - kmem.npage;
  release(&kmem.lock);
  return pages;
}
