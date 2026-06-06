#ifndef __SPINLOCK_H
#define __SPINLOCK_H
#define NSEM 128
struct cpu;

// Mutual exclusion lock.
struct spinlock
{
  uint locked; // Is the lock held?

  // For debugging:
  char *name;      // Name of lock.
  struct cpu *cpu; // The cpu holding the lock.
};

struct sem
{
  int value;            // 信号量的值
  struct spinlock lock; // 保护信号量的自旋锁
  int valid;            // 信号量是否正在被使用
};

// Initialize a spinlock
void initlock(struct spinlock *, char *);

void seminit();

// Acquire the spinlock
// Must be used with release()
void acquire(struct spinlock *);

// Release the spinlock
// Must be used with acquire()
void release(struct spinlock *);

// Check whether this cpu is holding the lock
// Interrupts must be off
int holding(struct spinlock *);

int sem_create(int initial_value);

int sem_destroy(int sem_id);

int sem_p(int sem_id);

int sem_v(int sem_id);

#endif
