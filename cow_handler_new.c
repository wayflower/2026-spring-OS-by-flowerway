int cow_handler(pagetable_t pagetable, pagetable_t kpagetable, uint64 va) {
  if(va >= MAXUVA) return -1;
  va = PGROUNDDOWN(va);
  pte_t *pte = walk(pagetable, va, 0);
  if(pte == 0) return -1;
  if((*pte & PTE_V) == 0) return -1;
  if((*pte & PTE_U) == 0) return -1;
  if((*pte & PTE_COW) == 0) return -1;

  uint64 pa = PTE2PA(*pte);
  uint flags = PTE_FLAGS(*pte);
  
  extern struct spinlock refr_lock;
  extern int ref_cnt[];
  
  acquire(&refr_lock);
  if (ref_cnt[pa / PGSIZE] == 1) {
      release(&refr_lock);
      flags = (flags & ~PTE_COW) | PTE_W;
      *pte = PA2PTE(pa) | flags;
      
      pte_t *kpte = walk(kpagetable, va, 0);
      if(kpte) {
          *kpte = PA2PTE(pa) | (flags & ~PTE_U);
      }
      sfence_vma();
      return 0;
  }
  release(&refr_lock);

  char *mem = kalloc();
  if(mem == 0) return -1;
  
  memmove(mem, (char*)pa, PGSIZE);
  
  flags = (flags & ~PTE_COW) | PTE_W;
  *pte = PA2PTE(mem) | flags;
  
  pte_t *kpte = walk(kpagetable, va, 0);
  if(kpte) {
      *kpte = PA2PTE(mem) | (flags & ~PTE_U);
  }
  
  kfree((void*)pa);
  
  sfence_vma();
  
  return 0;
}
