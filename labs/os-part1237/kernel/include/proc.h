#ifndef __PROC_H
#define __PROC_H

#include "param.h"
#include "riscv.h"
#include "types.h"
#include "spinlock.h"
#include "file.h"
#include "fat32.h"
#include "trap.h"

// Saved registers for kernel context switches.
struct context
{
  uint64 ra;
  uint64 sp;

  // callee-saved
  uint64 s0;
  uint64 s1;
  uint64 s2;
  uint64 s3;
  uint64 s4;
  uint64 s5;
  uint64 s6;
  uint64 s7;
  uint64 s8;
  uint64 s9;
  uint64 s10;
  uint64 s11;
};

// Per-CPU state.
struct cpu
{
  struct proc *proc;      // The process running on this cpu, or null.
  struct context context; // swtch() here to enter scheduler().
  int noff;               // Depth of push_off() nesting.
  int intena;             // Were interrupts enabled before push_off()?
};

extern struct cpu cpus[NCPU];

enum procstate
{
  UNUSED,
  SLEEPING,
  RUNNABLE,
  RUNNING,
  ZOMBIE
};

// 仿照用户态的宏定义，确保内核也能看懂这些 flag
// for mmap
#define PROT_NONE 0
#define PROT_READ 1
#define PROT_WRITE 2
#define PROT_EXEC 4
#define PROT_GROWSDOWN 0X01000000
#define PROT_GROWSUP 0X02000000

#define MAP_FILE 0
#define MAP_SHARED 0x01
#define MAP_PRIVATE 0X02
#define MAP_FAILED ((void *)-1)

struct vma
{
  int valid;      // 槽位是否可用：0代表空闲，1代表正在使用
  uint64 addr;    // 分配给用户的虚拟地址起始点 (账本记录的地址)
  int length;     // 映射的长度
  int prot;       // 权限 (PROT_READ, PROT_WRITE)
  int flags;      // 标志 (MAP_SHARED 等)
  struct file *f; // 指向被映射的打开文件
  int offset;     // 文件的偏移量
};

// Per-process state
struct proc
{
  struct spinlock lock;

  // p->lock must be held when using these:
  enum procstate state; // Process state
  struct proc *parent;  // Parent process
  void *chan;           // If non-zero, sleeping on chan
  int killed;           // If non-zero, have been killed
  int xstate;           // Exit status to be returned to parent's wait
  int pid;              // Process ID

  // these are private to the process, so p->lock need not be held.
  uint64 kstack;               // Virtual address of kernel stack
  uint64 sz;                   // Size of process memory (bytes)
  pagetable_t pagetable;       // User page table
  pagetable_t kpagetable;      // Kernel page table
  struct trapframe *trapframe; // data page for trampoline.S
  struct context context;      // swtch() here to run process
  struct file *ofile[NOFILE];  // Open files
  struct dirent *cwd;          // Current directory
  char name[16];               // Process name (debugging)
  int tmask;                   // trace mask
  struct vma vmas[NVMA];       // 进程的虚拟内存区域表
};

void reg_info(void);
int cpuid(void);
void exit(int);
int fork(void);
int growproc(int);
pagetable_t proc_pagetable(struct proc *);
void proc_freepagetable(pagetable_t, uint64);
int kill(int);
struct cpu *mycpu(void);
struct cpu *getmycpu(void);
struct proc *myproc();
void procinit(void);
void scheduler(void) __attribute__((noreturn));
void sched(void);
void setproc(struct proc *);
void sleep(void *, struct spinlock *);
void userinit(void);
int wait(uint64, int);
void wakeup(void *);
void yield(void);
int either_copyout(int user_dst, uint64 dst, void *src, uint64 len);
int either_copyin(void *dst, int user_src, uint64 src, uint64 len);
void procdump(void);
uint64 procnum(void);
void test_proc_init(int);
int clone(uint64 stack);
void vma_writeback(pagetable_t pagetable, uint64 addr, uint64 length, struct vma *v);

#endif
