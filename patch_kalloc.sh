sed -i '/if(((uint64)pa % PGSIZE) != 0 ||/!b;n;a\  acquire(\&refr_lock);\n  if(--ref_cnt[(uint64)pa / PGSIZE] > 0) {\n    release(\&refr_lock);\n    return;\n  }\n  release(\&refr_lock);' kernel/kalloc.c
sed -i '/memset((char\*)r, 5, PGSIZE); \/\/ fill with junk/c\    memset((char*)r, 5, PGSIZE); \/\/ fill with junk\n    acquire(\&refr_lock);\n    ref_cnt[(uint64)r \/ PGSIZE] = 1;\n    release(\&refr_lock);' kernel/kalloc.c
echo "void ref_add(uint64 pa) { acquire(&refr_lock); ref_cnt[pa / PGSIZE]++; release(&refr_lock); }" >> kernel/kalloc.c
