
#include "include/types.h"
#include "include/riscv.h"
#include "include/param.h"
#include "include/memlayout.h"
#include "include/spinlock.h"
#include "include/proc.h"
#include "include/syscall.h"
#include "include/timer.h"
#include "include/kalloc.h"
#include "include/string.h"
#include "include/printf.h"
#include "include/syscall.h"
#include "include/vm.h"

extern int exec(char *path, char **argv);

uint64
sys_exec(void)
{
  char path[FAT32_MAX_PATH], *argv[MAXARG];
  int i;
  uint64 uargv, uarg;

  if (argstr(0, path, FAT32_MAX_PATH) < 0 || argaddr(1, &uargv) < 0)
  {
    return -1;
  }
  memset(argv, 0, sizeof(argv));
  for (i = 0;; i++)
  {
    if (i >= NELEM(argv))
    {
      goto bad;
    }
    if (fetchaddr(uargv + sizeof(uint64) * i, (uint64 *)&uarg) < 0)
    {
      goto bad;
    }
    if (uarg == 0)
    {
      argv[i] = 0;
      break;
    }
    argv[i] = kalloc();
    if (argv[i] == 0)
      goto bad;
    if (fetchstr(uarg, argv[i], PGSIZE) < 0)
      goto bad;
  }

  int ret = exec(path, argv);

  for (i = 0; i < NELEM(argv) && argv[i] != 0; i++)
    kfree(argv[i]);

  return ret;

bad:
  for (i = 0; i < NELEM(argv) && argv[i] != 0; i++)
    kfree(argv[i]);
  return -1;
}

uint64
sys_exit(void)
{
  int n;
  if (argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0; // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if (argaddr(0, &p) < 0)
    return -1;
  return wait(p, -1);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if (argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if (growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if (argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n)
  {
    if (myproc()->killed)
    {
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if (argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64
sys_trace(void)
{
  int mask;
  if (argint(0, &mask) < 0)
  {
    return -1;
  }
  myproc()->tmask = mask;
  return 0;
}

uint64
sys_times(void)
{
  uint64 addr;
  if (argaddr(0, &addr) < 0)
    return -1;

  struct tms t;
  acquire(&tickslock);
  t.utime = ticks;
  t.stime = ticks;
  t.cutime = ticks;
  t.cstime = ticks;
  release(&tickslock);
  if (copyout2(addr, (char *)&t, sizeof(t)) < 0)
    return -1;
  return 0;
}

struct utsname
{
  char sysname[65];
  char nodename[65];
  char release[65];
  char version[65];
  char machine[65];
  char domainname[65];
};

uint64
sys_uname(void)
{
  uint64 addr;
  if (argaddr(0, &addr) < 0)
    return -1;
  struct utsname un;
  strncpy(un.sysname, "Miss", sizeof(un.sysname));
  strncpy(un.nodename, "DSY", sizeof(un.nodename));
  strncpy(un.release, ",", sizeof(un.release));
  strncpy(un.version, "I", sizeof(un.version));
  strncpy(un.machine, "love", sizeof(un.machine));
  strncpy(un.domainname, "you!", sizeof(un.domainname));
  if (copyout2(addr, (char *)&un, sizeof(un)) < 0)
    return -1;
  return 0;
}

uint64
sys_wait4(void)
{
  uint64 addr;
  int wait_pid;
  if (argint(0, &wait_pid) < 0 || argaddr(1, &addr) < 0)
    return -1;
  return wait(addr, wait_pid);
}

uint64
sys_gettimeofday(void)
{
  uint64 addr;
  if (argaddr(0, &addr) < 0)
    return -1;

  uint64 t = r_time();
  struct
  {
    uint64 sec;
    uint64 usec;
  } tv;

  tv.sec = t / 12500000;
  tv.usec = (t % 12500000) * 1000000 / 12500000;

  if (copyout2(addr, (char *)&tv, sizeof(tv)) < 0)
    return -1;
  return 0;
}

uint64
sys_getppid(void)
{
  return myproc()->parent ? myproc()->parent->pid : 0;
}

uint64
sys_clone(void)
{
  uint64 stack;
  if (argaddr(1, &stack) < 0)
    return -1;
  return clone(stack);
}

uint64
sys_yield(void)
{
  yield();
  return 0;
}

uint64
sys_nanosleep(void)
{
  uint64 tvaddr;
  uint ticks0;

  if (argaddr(0, &tvaddr) < 0)
    return -1;

  struct TimeVal
  {
    uint64 sec;
    uint64 usec;
  } tv;

  if (copyin2((char *)&tv, tvaddr, sizeof(tv)) < 0)
    return -1;

  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < tv.sec * 200 + tv.usec * 200 / 1000000)
  {
    if (myproc()->killed)
    {
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_brk(void)
{
  int addr;
  int endaddr;

  if (argint(0, &endaddr) < 0)
    return -1;
  if (endaddr == 0)
    return myproc()->sz;
  addr = myproc()->sz;
  if (growproc(endaddr - addr) < 0)
    return -1;
  return endaddr;
}

uint64
sys_mmap(void)
{
  uint64 addr, length, offset;
  int prot, flags, fd;

  if (argaddr(0, &addr) < 0 || argaddr(1, &length) < 0 ||
      argint(2, &prot) < 0 || argint(3, &flags) < 0 ||
      argint(4, &fd) < 0 || argaddr(5, &offset) < 0)
  {
    return -1;
  }

  // mmap 的地址如果用户指定了，必须是页对齐的（虽然我们是向下寻址忽略了传入的addr）
  // 但对于某些系统调用测试，会要求检查 addr 必须按页对齐
  if (addr != 0 && (addr % PGSIZE) != 0)
  {
    return -1;
  }

  struct proc *p = myproc();
  struct file *f = NULL;

  if (fd >= 0 && fd < NOFILE && p->ofile[fd])
  {
    f = p->ofile[fd];
  }
  else
  {
    // fd < 0 (通常为 -1) 可能是匿名映射，但如果存在文件映射则需要合法的 fd
    if (fd != -1)
      return -1;
  }

  if (f)
  {
    if ((prot & PROT_READ) && !f->readable)
      return -1;
    if ((prot & PROT_WRITE) && (flags & MAP_SHARED) && !f->writable)
      return -1;
  }

  struct vma *v = NULL;
  for (int i = 0; i < NVMA; i++)
  {
    if (p->vmas[i].valid == 0)
    {
      v = &p->vmas[i];
      break;
    }
  }

  if (v == NULL)
    return -1;

  if (addr == 0)
  {
    // 向下寻找空闲的虚拟地址空间
    addr = TRAPFRAME;
    for (int i = 0; i < NVMA; i++)
    {
      if (p->vmas[i].valid && p->vmas[i].addr < addr)
      {
        addr = p->vmas[i].addr;
      }
    }
    // 向下分配并且对齐
    addr -= PGROUNDUP(length);
    addr &= ~(PGSIZE - 1);
  }

  // 简单的内存碰撞检测
  if (addr < PGROUNDUP(p->sz))
  {
    return -1;
  }

  v->valid = 1;
  v->addr = addr;
  v->length = length;
  v->prot = prot;
  v->flags = flags;
  v->offset = offset;
  v->f = f ? filedup(f) : NULL;

  return v->addr;
}

uint64
sys_munmap(void)
{
  uint64 addr, length;
  if (argaddr(0, &addr) < 0 || argaddr(1, &length) < 0)
  {
    return -1;
  }

  // munmap 时起始地址必须严格页对齐
  if (addr % PGSIZE != 0)
  {
    return -1;
  }

  // 释放范围向上按页对齐
  uint64 align_len = PGROUNDUP(length);

  struct proc *p = myproc();
  struct vma *v = NULL;

  for (int i = 0; i < NVMA; i++)
  {
    // 允许部分或完整 unmap，只要 addr 匹配就行
    if (p->vmas[i].valid && addr >= p->vmas[i].addr && addr < p->vmas[i].addr + p->vmas[i].length)
    {
      v = &p->vmas[i];
      break;
    }
  }

  if (v == NULL)
    return -1;

  if (v->flags & MAP_SHARED)
  {
    vma_writeback(p->pagetable, addr, length, v); // 写回时保持原有实际长度
  }

  // 这里调用 vmunmap 会将我们之前按需分配好的物理页面彻底还给内核
  vmunmap(p->pagetable, addr, align_len / PGSIZE, 1);

  if (addr == v->addr && length >= v->length)
  {
    // 全退还
    if (v->f)
    {
      fileclose(v->f);
      v->f = NULL;
    }
    v->valid = 0;
  }
  else
  {
    // 处理 OSComp 中可能出现的部分截断 munmap (例如头部的 partial unmap)
    // 根据需要做更精确的设计，但最简单的是整体释放，这里只做简单的偏移
    v->addr = addr + length;
    v->length -= length;
    v->offset += length;
  }

  return 0;
}