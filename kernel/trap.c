#include "include/kalloc.h"
#include "include/types.h"
#include "include/param.h"
#include "include/memlayout.h"
#include "include/riscv.h"
#include "include/spinlock.h"
#include "include/proc.h"
#include "include/sbi.h"
#include "include/plic.h"
#include "include/trap.h"
#include "include/syscall.h"
#include "include/printf.h"
#include "include/console.h"
#include "include/timer.h"
#include "include/disk.h"
#include "include/string.h"
#include "include/vm.h"

extern char trampoline[], uservec[], userret[];

// in kernelvec.S, calls kerneltrap().
extern void kernelvec();

int devintr();

// void
// trapinit(void)
// {
//   initlock(&tickslock, "time");
//   #ifdef DEBUG
//   printf("trapinit\n");
//   #endif
// }

// set up to take exceptions and traps while in the kernel.
void trapinithart(void)
{
  w_stvec((uint64)kernelvec);
  w_sstatus(r_sstatus() | SSTATUS_SIE);
  // enable supervisor-mode timer interrupts.
  w_sie(r_sie() | SIE_SEIE | SIE_SSIE | SIE_STIE);
  set_next_timeout();
#ifdef DEBUG
  printf("trapinithart\n");
#endif
}

struct VMA_page *select_victim_page(struct VMA *head)
{
  struct VMA *v;
  struct VMA_page *victim = 0;

  // 遍历进程内部环形双向链表上挂载的所有 VMA 块
  for (v = head->vm_next; v != head; v = v->vm_next)
  {
    int npages = (v->vm_end - v->vm_start + PGSIZE - 1) / PGSIZE;

    // 遍历每一个 VMA 内部对应的物理页面信息
    for (int i = 0; i < npages; i++)
    {
      // 只有真正在内存中的页 (status == 1) 才能作为候选者
      if (v->pages[i].status == 1)
      {
        victim = &v->pages[i]; // 更新当前最老的页面指针
        printf("换出页面: %d\n", i);
        break;
      }
    }
  }

  if (victim == 0)
    panic("select_victim_page: no valid victim found");

  return victim;
};

//
// handle an interrupt, exception, or system call from user space.
// called from trampoline.S
//
void usertrap(void)
{
  // printf("run in usertrap\n");
  int which_dev = 0;

  if ((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  // send interrupts and exceptions to kerneltrap(),
  // since we're now in the kernel.
  w_stvec((uint64)kernelvec);

  struct proc *p = myproc();

  // save user program counter.
  p->trapframe->epc = r_sepc();

  if (r_scause() == 8)
  {
    // system call
    if (p->killed)
      exit(-1);
    // sepc points to the ecall instruction,
    // but we want to return to the next instruction.
    p->trapframe->epc += 4;
    // an interrupt will change sstatus &c registers,
    // so don't enable until done with those registers.
    intr_on();
    syscall();
  }
  else if ((which_dev = devintr()) != 0)
  {
    // ok
  }
  else if (r_scause() == 13 || r_scause() == 15)
  {
    uint64 va = r_stval();
    struct proc *p = myproc();
    va = PGROUNDDOWN(va);
    struct VMA *vma;
    for (vma = p->head.vm_next; vma != &p->head; vma = vma->vm_next)
      if (va >= vma->vm_start && va < vma->vm_end)
        break;
    if (vma == &p->head)
    {
      p->killed = 1;
      exit(-1);
    }
    struct VMA_page *page = addr2page(&p->head, va);
    if (page == 0)
    {
      p->killed = 1;
      exit(-1);
    }
    if (p->cur_page_in_mem >= p->max_page_in_mem)
    {
      struct VMA_page *victim = 0;
      victim = select_victim_page(&p->head);
      if (swap_out(p, victim) < 0)
      {
        p->killed = 1;
        exit(-1);
      }
    }

    if (page->status == 2)
    {
      // 页面在交换区内，需要换入到物理内存
      if (swap_in(p, page) < 0) // 这一步自动完成了cur_page_in_mem++的操作
      {
        p->killed = 1;
        exit(-1);
      }
    }
    else if (page->status == 0)
    {
      // 页面尚未使用过，执行全新分配
      char *mem;
      if ((mem = kalloc()) == 0)
      {
        p->killed = 1;
        exit(-1);
      }
      memset(mem, 0, PGSIZE);
      uint64 pa = (uint64)mem; // 提取kalloc出的mem的物理地址

      int perm = PTE_U;
      if (vma->prot & PROT_READ)
        perm |= PTE_R;
      if (vma->prot & PROT_WRITE)
        perm |= PTE_W;
      if (vma->prot & PROT_EXEC)
        perm |= PTE_X;
      // 在用户页表上添加
      if (mappages(p->pagetable, va, PGSIZE, pa, perm) != 0)
      {
        kfree(mem);
        exit(-1);
      }
      if (mappages(p->kpagetable, va, PGSIZE, pa, perm & ~PTE_U) != 0)
      {
        vmunmap(p->pagetable, va, 1, 0);
        kfree(mem);
        exit(-1);
      }
      p->cur_page_in_mem++;
      page->status = 1; // 标记为在内存中
    }
  }
  else
  {
    printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
    printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
    p->killed = 1;
  }

  if (p->killed)
    exit(-1);

  // give up the CPU if this is a timer interrupt.
  if (which_dev == 2)
    yield();

  usertrapret();
}

//
// return to user space
//
void usertrapret(void)
{
  struct proc *p = myproc();

  // we're about to switch the destination of traps from
  // kerneltrap() to usertrap(), so turn off interrupts until
  // we're back in user space, where usertrap() is correct.
  intr_off();

  // send syscalls, interrupts, and exceptions to trampoline.S
  w_stvec(TRAMPOLINE + (uservec - trampoline));

  // set up trapframe values that uservec will need when
  // the process next re-enters the kernel.
  p->trapframe->kernel_satp = r_satp();         // kernel page table
  p->trapframe->kernel_sp = p->kstack + PGSIZE; // process's kernel stack
  p->trapframe->kernel_trap = (uint64)usertrap;
  p->trapframe->kernel_hartid = r_tp(); // hartid for cpuid()

  // set up the registers that trampoline.S's sret will use
  // to get to user space.

  // set S Previous Privilege mode to User.
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
  x |= SSTATUS_SPIE; // enable interrupts in user mode
  w_sstatus(x);

  // set S Exception Program Counter to the saved user pc.
  w_sepc(p->trapframe->epc);

  // tell trampoline.S the user page table to switch to.
  // printf("[usertrapret]p->pagetable: %p\n", p->pagetable);
  uint64 satp = MAKE_SATP(p->pagetable);

  // jump to trampoline.S at the top of memory, which
  // switches to the user page table, restores user registers,
  // and switches to user mode with sret.
  uint64 fn = TRAMPOLINE + (userret - trampoline);
  ((void (*)(uint64, uint64))fn)(TRAPFRAME, satp);
}

// interrupts and exceptions from kernel code go here via kernelvec,
// on whatever the current kernel stack is.
void kerneltrap()
{
  int which_dev = 0;
  uint64 sepc = r_sepc();
  uint64 sstatus = r_sstatus();
  uint64 scause = r_scause();

  if ((sstatus & SSTATUS_SPP) == 0)
    panic("kerneltrap: not from supervisor mode");
  if (intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  if ((which_dev = devintr()) == 0)
  {
    printf("\nscause %p\n", scause);
    printf("sepc=%p stval=%p hart=%d\n", r_sepc(), r_stval(), r_tp());
    struct proc *p = myproc();
    if (p != 0)
    {
      printf("pid: %d, name: %s\n", p->pid, p->name);
    }
    panic("kerneltrap");
  }
  // printf("which_dev: %d\n", which_dev);

  // give up the CPU if this is a timer interrupt.
  if (which_dev == 2 && myproc() != 0 && myproc()->state == RUNNING)
  {
    yield();
  }
  // the yield() may have caused some traps to occur,
  // so restore trap registers for use by kernelvec.S's sepc instruction.
  w_sepc(sepc);
  w_sstatus(sstatus);
}

// Check if it's an external/software interrupt,
// and handle it.
// returns  2 if timer interrupt,
//          1 if other device,
//          0 if not recognized.
int devintr(void)
{
  uint64 scause = r_scause();

#ifdef QEMU
  // handle external interrupt
  if ((0x8000000000000000L & scause) && 9 == (scause & 0xff))
#else
  // on k210, supervisor software interrupt is used
  // in alternative to supervisor external interrupt,
  // which is not available on k210.
  if (0x8000000000000001L == scause && 9 == r_stval())
#endif
  {
    int irq = plic_claim();
    if (UART_IRQ == irq)
    {
      // keyboard input
      int c = sbi_console_getchar();
      if (-1 != c)
      {
        consoleintr(c);
      }
    }
    else if (DISK_IRQ == irq)
    {
      disk_intr();
    }
    else if (irq)
    {
      printf("unexpected interrupt irq = %d\n", irq);
    }

    if (irq)
    {
      plic_complete(irq);
    }

#ifndef QEMU
    w_sip(r_sip() & ~2); // clear pending bit
    sbi_set_mie();
#endif

    return 1;
  }
  else if (0x8000000000000005L == scause)
  {
    timer_tick();
    return 2;
  }
  else
  {
    return 0;
  }
}

void trapframedump(struct trapframe *tf)
{
  printf("a0: %p\t", tf->a0);
  printf("a1: %p\t", tf->a1);
  printf("a2: %p\t", tf->a2);
  printf("a3: %p\n", tf->a3);
  printf("a4: %p\t", tf->a4);
  printf("a5: %p\t", tf->a5);
  printf("a6: %p\t", tf->a6);
  printf("a7: %p\n", tf->a7);
  printf("t0: %p\t", tf->t0);
  printf("t1: %p\t", tf->t1);
  printf("t2: %p\t", tf->t2);
  printf("t3: %p\n", tf->t3);
  printf("t4: %p\t", tf->t4);
  printf("t5: %p\t", tf->t5);
  printf("t6: %p\t", tf->t6);
  printf("s0: %p\n", tf->s0);
  printf("s1: %p\t", tf->s1);
  printf("s2: %p\t", tf->s2);
  printf("s3: %p\t", tf->s3);
  printf("s4: %p\n", tf->s4);
  printf("s5: %p\t", tf->s5);
  printf("s6: %p\t", tf->s6);
  printf("s7: %p\t", tf->s7);
  printf("s8: %p\n", tf->s8);
  printf("s9: %p\t", tf->s9);
  printf("s10: %p\t", tf->s10);
  printf("s11: %p\t", tf->s11);
  printf("ra: %p\n", tf->ra);
  printf("sp: %p\t", tf->sp);
  printf("gp: %p\t", tf->gp);
  printf("tp: %p\t", tf->tp);
  printf("epc: %p\n", tf->epc);
}
