// Mutual exclusion spin locks.

#include "include/types.h"
#include "include/param.h"
#include "include/memlayout.h"
#include "include/spinlock.h"
#include "include/riscv.h"
#include "include/proc.h"
#include "include/intr.h"
#include "include/printf.h"

struct
{
  struct sem sems[NSEM]; // 信号量数组
  struct spinlock lock;  // 保护信号量数组的自旋锁
} sem_pool;

void initlock(struct spinlock *lk, char *name)
{
  lk->name = name;
  lk->locked = 0;
  lk->cpu = 0;
}

void seminit()
{
  initlock(&sem_pool.lock, "sem_pool_lock");
  for (int i = 0; i < NSEM; i++)
  {
    initlock(&sem_pool.sems[i].lock, "semaphore");
    sem_pool.sems[i].value = 0;
    sem_pool.sems[i].valid = 0;
  }
}

// Acquire the lock.
// Loops (spins) until the lock is acquired.
void acquire(struct spinlock *lk)
{
  push_off(); // disable interrupts to avoid deadlock.
  if (holding(lk))
    panic("acquire");

  // On RISC-V, sync_lock_test_and_set turns into an atomic swap:
  //   a5 = 1
  //   s1 = &lk->locked
  //   amoswap.w.aq a5, a5, (s1)
  while (__sync_lock_test_and_set(&lk->locked, 1) != 0)
    ;

  // Tell the C compiler and the processor to not move loads or stores
  // past this point, to ensure that the critical section's memory
  // references happen strictly after the lock is acquired.
  // On RISC-V, this emits a fence instruction.
  __sync_synchronize();

  // Record info about lock acquisition for holding() and debugging.
  lk->cpu = mycpu();
}

// Release the lock.
void release(struct spinlock *lk)
{
  if (!holding(lk))
    panic("release");

  lk->cpu = 0;

  // Tell the C compiler and the CPU to not move loads or stores
  // past this point, to ensure that all the stores in the critical
  // section are visible to other CPUs before the lock is released,
  // and that loads in the critical section occur strictly before
  // the lock is released.
  // On RISC-V, this emits a fence instruction.
  __sync_synchronize();

  // Release the lock, equivalent to lk->locked = 0.
  // This code doesn't use a C assignment, since the C standard
  // implies that an assignment might be implemented with
  // multiple store instructions.
  // On RISC-V, sync_lock_release turns into an atomic swap:
  //   s1 = &lk->locked
  //   amoswap.w zero, zero, (s1)
  __sync_lock_release(&lk->locked);

  pop_off();
}

// Check whether this cpu is holding the lock.
// Interrupts must be off.
int holding(struct spinlock *lk)
{
  int r;
  r = (lk->locked && lk->cpu == mycpu());
  return r;
}

int sem_create(int initial_value)
{
  acquire(&sem_pool.lock);
  for (int i = 0; i < NSEM; i++)
  {
    if (sem_pool.sems[i].valid == 0)
    {
      sem_pool.sems[i].valid = 1;
      sem_pool.sems[i].value = initial_value;
      release(&sem_pool.lock);
      return i;
    }
  }
  release(&sem_pool.lock);
  return -1; // No available semaphore
}

int sem_destroy(int sem_id)
{
  if (sem_id < 0 || sem_id >= NSEM)
  {
    return -1; // Invalid semaphore ID
  }
  acquire(&sem_pool.lock);
  struct sem *s = &sem_pool.sems[sem_id];
  acquire(&s->lock);
  if (s->valid == 0)
  {
    release(&s->lock);
    release(&sem_pool.lock);
    return -1; // Semaphore not in use
  }
  s->valid = 0;
  s->value = 0;
  wakeup(s); // Wake up any waiting processes
  release(&s->lock);
  release(&sem_pool.lock);
  return 0; // Success
}

int sem_p(int sem_id)
{
  if (sem_id < 0 || sem_id >= NSEM)
  {
    return -1; // Invalid semaphore ID
  }
  struct sem *s = &sem_pool.sems[sem_id];
  acquire(&s->lock);
  while (s->value <= 0)
  {
    sleep(s, &s->lock); // Sleep until the semaphore is available
  }
  s->value--;
  release(&s->lock);
  return 0;
}

int sem_v(int sem_id)
{
  if (sem_id < 0 || sem_id >= NSEM)
  {
    return -1; // Invalid semaphore ID
  }
  struct sem *s = &sem_pool.sems[sem_id];
  acquire(&s->lock);
  s->value++;
  wakeup(s);
  release(&s->lock);
  return 0;
}