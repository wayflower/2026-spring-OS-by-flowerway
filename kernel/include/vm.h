#ifndef __VM_H
#define __VM_H

#include "types.h"
#include "riscv.h"

void kvminit(void);
void kvminithart(void);
uint64 kvmpa(uint64);
void kvmmap(uint64, uint64, uint64, int);
int mappages(pagetable_t, uint64, uint64, uint64, int);
pagetable_t uvmcreate(void);
// void            uvminit(pagetable_t, uchar *, uint);
void uvminit(pagetable_t, pagetable_t, uchar *, uint);
uint64 uvmalloc(pagetable_t, pagetable_t, uint64, uint64);
uint64 uvmdealloc(pagetable_t, pagetable_t, uint64, uint64);
// int             uvmcopy(pagetable_t, pagetable_t, uint64);
int uvmcopy(pagetable_t, pagetable_t, pagetable_t, uint64);
void uvmfree(pagetable_t, uint64);
// void            uvmunmap(pagetable_t, uint64, uint64, int);
void vmunmap(pagetable_t, uint64, uint64, int);
void uvmclear(pagetable_t, uint64);
uint64 walkaddr(pagetable_t, uint64);
int copyout(pagetable_t, uint64, char *, uint64);
int copyin(pagetable_t, char *, uint64, uint64);
int copyinstr(pagetable_t, char *, uint64, uint64);
pagetable_t proc_kpagetable(void);
void kvmfreeusr(pagetable_t kpt);
void kvmfree(pagetable_t kpagetable, int stack_free);
uint64 kwalkaddr(pagetable_t pagetable, uint64 va);
int copyout2(uint64 dstva, char *src, uint64 len);
int copyin2(char *dst, uint64 srcva, uint64 len);
int copyinstr2(char *dst, uint64 srcva, uint64 max);
void vmprint(pagetable_t pagetable);
pte_t *walk(pagetable_t pagetable, uint64 va, int alloc);
void mock_swap_init(void);
int alloc_global_swap_slot(int pid, uint64 vaddr); // 从全局交换区里分配一个空闲的slot（用于之后换出页面内容）
void free_global_swap_slot(int idx);               // 将使用完毕的slot归还至全局交换区（换入内容后释放资源）
struct swap_slot
{
    int used;          // 是否有效
    int pid;           // 进程ID
    uint64 vaddr;      // 该页面原本对应的虚拟地址
    char data[PGSIZE]; // 该页面数据的实际缓冲区
};

#define PROT_READ 0x1
#define PROT_WRITE 0x2
#define PROT_EXEC 0x4
#define MAP_PRIVATE 0x1
#define MAP_ANONYMOUS 0x2

#define MAX_SWAP_SLOTS 10

#endif
