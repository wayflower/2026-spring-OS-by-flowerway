#include "kernel/include/types.h"
#include "kernel/include/stat.h"
#include "xv6-user/user.h"

uint64 mmap(uint64 addr, int length, int prot, int flags, int fd, int offset);
int munmap(uint64 addr, int length);
int set_max_page_in_mem(int);
int get_swap_count(void);
int lru_access_notify(uint64 addr);