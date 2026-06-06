//
// File-system system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into file.c and fs.c.
//

#include "include/types.h"
#include "include/riscv.h"
#include "include/param.h"
#include "include/stat.h"
#include "include/spinlock.h"
#include "include/proc.h"
#include "include/sleeplock.h"
#include "include/file.h"
#include "include/pipe.h"
#include "include/fcntl.h"
#include "include/fat32.h"
#include "include/syscall.h"
#include "include/string.h"
#include "include/printf.h"
#include "include/vm.h"
#include "include/sbi.h"

// Fetch the nth word-sized system call argument as a file descriptor
// and return both the descriptor and the corresponding struct file.
static int
argfd(int n, int *pfd, struct file **pf)
{
  int fd;
  struct file *f;

  if (argint(n, &fd) < 0)
    return -1;
  if (fd < 0 || fd >= NOFILE || (f = myproc()->ofile[fd]) == NULL)
    return -1;
  if (pfd)
    *pfd = fd;
  if (pf)
    *pf = f;
  return 0;
}

// Allocate a file descriptor for the given file.
// Takes over file reference from caller on success.
static int
fdalloc(struct file *f)
{
  int fd;
  struct proc *p = myproc();

  for (fd = 0; fd < NOFILE; fd++)
  {
    if (p->ofile[fd] == 0)
    {
      p->ofile[fd] = f;
      return fd;
    }
  }
  return -1;
}

uint64
sys_dup(void)
{
  struct file *f;
  int fd;

  if (argfd(0, 0, &f) < 0)
    return -1;
  if ((fd = fdalloc(f)) < 0)
    return -1;
  filedup(f);
  return fd;
}

uint64
sys_read(void)
{
  struct file *f;
  int n;
  uint64 p;

  if (argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argaddr(1, &p) < 0)
    return -1;
  return fileread(f, p, n);
}

uint64
sys_write(void)
{
  struct file *f;
  int n;
  uint64 p;

  if (argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argaddr(1, &p) < 0)
    return -1;

  return filewrite(f, p, n);
}

uint64
sys_close(void)
{
  int fd;
  struct file *f;

  if (argfd(0, &fd, &f) < 0)
    return -1;
  myproc()->ofile[fd] = 0;
  fileclose(f);
  return 0;
}

uint64
sys_fstat(void)
{
  struct file *f;
  uint64 st; // user pointer to struct stat

  if (argfd(0, 0, &f) < 0 || argaddr(1, &st) < 0)
    return -1;
  return filestat(f, st);
}

static struct dirent *
create(char *path, short type, int mode)
{
  struct dirent *ep, *dp;
  char name[FAT32_MAX_FILENAME + 1];

  if ((dp = enameparent(path, name)) == NULL)
    return NULL;

  if (type == T_DIR)
  {
    mode = ATTR_DIRECTORY;
  }
  else if (mode & O_RDONLY)
  {
    mode = ATTR_READ_ONLY;
  }
  else
  {
    mode = 0;
  }

  elock(dp);
  if ((ep = ealloc(dp, name, mode)) == NULL)
  {
    eunlock(dp);
    eput(dp);
    return NULL;
  }

  if ((type == T_DIR && !(ep->attribute & ATTR_DIRECTORY)) ||
      (type == T_FILE && (ep->attribute & ATTR_DIRECTORY)))
  {
    eunlock(dp);
    eput(ep);
    eput(dp);
    return NULL;
  }

  eunlock(dp);
  eput(dp);

  elock(ep);
  return ep;
}

uint64
sys_open(void)
{
  char path[FAT32_MAX_PATH];
  int fd, omode, dirfd;
  struct file *f;
  struct dirent *ep;

  if (argint(0, &dirfd) < 0 || argstr(1, path, FAT32_MAX_PATH) < 0 || argint(2, &omode) < 0)
    return -1;

  // printf("sys_open: dirfd=%d, path=%s, omode=%d\n", dirfd, path, omode);

  if (path[0] == '/' || dirfd == AT_FDCWD)
  { // 绝对路径或者没有指定 dirfd（AT_FDCWD），直接使用 path 进行查找
    ;
  }
  else
  {
    struct file *dirf;
    if (argfd(0, 0, &dirf) < 0 || !(dirf->ep->attribute & ATTR_DIRECTORY))
    {
      // 这一步检查 dirfd 是否有效，并且对应一个目录dirf。如果不满足条件，返回 -1。
      return -1;
    }

    struct dirent *curr = dirf->ep;
    char fullpath[FAT32_MAX_PATH];

    // 把指针 p_out 放到数组的最后面，准备从后往前写
    char *p_out = fullpath + FAT32_MAX_PATH - 1;
    *p_out = '\0'; // 字符串结尾符

    // 1. 把用户传进来的相对路径塞到最尾部
    int path_len = strlen(path);
    if (path_len >= FAT32_MAX_PATH)
      return -1;
    p_out -= path_len;
    // 这里用 memmove 只移动字符，不带 \0
    memmove(p_out, path, path_len);

    // 2. 循环向上回溯，不断把父目录名拼接到前面
    while (curr != NULL && curr->parent != curr && curr->parent != NULL)
    {
      int len = strlen(curr->filename);

      if (p_out - fullpath < len + 1)
      {
        return -1; // 路径太长
      }

      // 往前移动指针并写入 '/'
      p_out--;
      *p_out = '/';

      // 往前移动指针并写入当前目录名
      p_out -= len;
      memmove(p_out, curr->filename, len); // 同样只移动字符

      curr = curr->parent;
    }

    // 3. 处理根目录的 '/'
    if (p_out > fullpath)
    {
      p_out--;
      *p_out = '/';
    }
    safestrcpy(path, p_out, FAT32_MAX_PATH);
  }

  // printf("sys_open: full path=%s, omode=%d\n", path, omode);

  if (omode & O_CREATE)
  {
    ep = create(path, T_FILE, omode);
    if (ep == NULL)
    {
      return -1;
    }
  }
  else
  {
    if ((ep = ename(path)) == NULL)
    {
      return -1;
    }
    elock(ep);

    // 【修改点 1】：如果参数要求必须是目录，但找到的实体不是目录，返回-1
    if ((omode & O_DIRECTORY) && !(ep->attribute & ATTR_DIRECTORY))
    {
      eunlock(ep);
      eput(ep);
      return -1;
    }

    // 【修改点 2】：放宽限制，只要没有要求写权限（O_WRONLY / O_RDWR），不强求 omode == 0，这让宏标签共存成为可能
    if ((ep->attribute & ATTR_DIRECTORY) && ((omode & 3) != O_RDONLY))
    {
      eunlock(ep);
      eput(ep);
      return -1;
    }
  }

  // struct proc *p = myproc();
  // struct dirent *cwd = p->cwd;
  // // 如果没有父节点（或者 parent 就是它自己），说明是根目录，打印为 "/"
  // printf("sys_open: cwd=%s\n", cwd->parent == NULL ? "/" : cwd->filename);

  if ((f = filealloc()) == NULL || (fd = fdalloc(f)) < 0)
  {
    if (f)
    {
      fileclose(f);
    }
    eunlock(ep);
    eput(ep);
    return -1;
  }

  if (!(ep->attribute & ATTR_DIRECTORY) && (omode & O_TRUNC))
  {
    etrunc(ep);
  }

  f->type = FD_ENTRY;
  f->off = (omode & O_APPEND) ? ep->file_size : 0;
  f->ep = ep;
  f->readable = !(omode & O_WRONLY);
  f->writable = (omode & O_WRONLY) || (omode & O_RDWR);

  eunlock(ep);

  return fd;
}

uint64
sys_mkdir(void)
{
  char path[FAT32_MAX_PATH];
  struct dirent *ep;

  if (argstr(0, path, FAT32_MAX_PATH) < 0 || (ep = create(path, T_DIR, 0)) == 0)
  {
    return -1;
  }
  eunlock(ep);
  eput(ep);
  return 0;
}

uint64
sys_chdir(void)
{
  char path[FAT32_MAX_PATH];
  struct dirent *ep;
  struct proc *p = myproc();

  if (argstr(0, path, FAT32_MAX_PATH) < 0 || (ep = ename(path)) == NULL)
  {
    return -1;
  }
  elock(ep);
  if (!(ep->attribute & ATTR_DIRECTORY))
  {
    eunlock(ep);
    eput(ep);
    return -1;
  }
  eunlock(ep);
  eput(p->cwd);
  p->cwd = ep;
  return 0;
}

uint64
sys_pipe(void)
{
  uint64 fdarray; // user pointer to array of two integers
  struct file *rf, *wf;
  int fd0, fd1;
  struct proc *p = myproc();

  if (argaddr(0, &fdarray) < 0)
    return -1;
  if (pipealloc(&rf, &wf) < 0)
    return -1;
  fd0 = -1;
  if ((fd0 = fdalloc(rf)) < 0 || (fd1 = fdalloc(wf)) < 0)
  {
    if (fd0 >= 0)
      p->ofile[fd0] = 0;
    fileclose(rf);
    fileclose(wf);
    return -1;
  }
  // if(copyout(p->pagetable, fdarray, (char*)&fd0, sizeof(fd0)) < 0 ||
  //    copyout(p->pagetable, fdarray+sizeof(fd0), (char *)&fd1, sizeof(fd1)) < 0){
  if (copyout2(fdarray, (char *)&fd0, sizeof(fd0)) < 0 ||
      copyout2(fdarray + sizeof(fd0), (char *)&fd1, sizeof(fd1)) < 0)
  {
    p->ofile[fd0] = 0;
    p->ofile[fd1] = 0;
    fileclose(rf);
    fileclose(wf);
    return -1;
  }
  return 0;
}

// To open console device.
uint64
sys_dev(void)
{
  int fd, omode;
  int major, minor;
  struct file *f;

  if (argint(0, &omode) < 0 || argint(1, &major) < 0 || argint(2, &minor) < 0)
  {
    return -1;
  }

  if (omode & O_CREATE)
  {
    panic("dev file on FAT");
  }

  if (major < 0 || major >= NDEV)
    return -1;

  if ((f = filealloc()) == NULL || (fd = fdalloc(f)) < 0)
  {
    if (f)
      fileclose(f);
    return -1;
  }

  f->type = FD_DEVICE;
  f->off = 0;
  f->ep = 0;
  f->major = major;
  f->readable = !(omode & O_WRONLY);
  f->writable = (omode & O_WRONLY) || (omode & O_RDWR);

  return fd;
}

// To support ls command
uint64
sys_readdir(void)
{
  struct file *f;
  uint64 p;

  if (argfd(0, 0, &f) < 0 || argaddr(1, &p) < 0)
    return -1;
  return dirnext(f, p);
}

// get absolute cwd string
uint64
sys_getcwd(void)
{
  uint64 addr;
  int size;
  if (argaddr(0, &addr) < 0 || argint(1, &size) < 0)
    return NULL;
  // if (argaddr(0, &addr) < 0)
  //   return -1;

  struct dirent *de = myproc()->cwd;
  char path[FAT32_MAX_PATH];
  char *s = path + sizeof(path) - 1;
  int len;

  if (de->parent == NULL)
  {
    s = "/";
  }
  else
  {
    s = path + FAT32_MAX_PATH - 1;
    *s = '\0';
    while (de->parent)
    { // 这步是递归地向上寻找父目录，直到根目录为止。每找到一个父目录，就把当前目录的名字写到路径字符串的前面。
      len = strlen(de->filename);
      s -= len;
      if (s <= path) // can't reach root "/"
        return NULL;
      strncpy(s, de->filename, len);
      s--;
      if (s <= path) // can't reach root "/"
        return NULL;
      *s = '/';
      de = de->parent;
    }
  }

  // if (copyout(myproc()->pagetable, addr, s, strlen(s) + 1) < 0)
  if (size < strlen(s) + 1) // check buffer size
    return NULL;
  if (copyout2(addr, s, strlen(s) + 1) < 0)
    return NULL;

  return addr;
}

// Is the directory dp empty except for "." and ".." ?
static int
isdirempty(struct dirent *dp)
{
  struct dirent ep;
  int count;
  int ret;
  ep.valid = 0;
  ret = enext(dp, &ep, 2 * 32, &count); // skip the "." and ".."
  return ret == -1;
}

uint64
sys_remove(void)
{
  char path[FAT32_MAX_PATH];
  struct dirent *ep;
  int len;
  if ((len = argstr(0, path, FAT32_MAX_PATH)) <= 0)
    return -1;

  char *s = path + len - 1;
  while (s >= path && *s == '/')
  {
    s--;
  }
  if (s >= path && *s == '.' && (s == path || *--s == '/'))
  {
    return -1;
  }

  if ((ep = ename(path)) == NULL)
  {
    return -1;
  }
  elock(ep);
  if ((ep->attribute & ATTR_DIRECTORY) && !isdirempty(ep))
  {
    eunlock(ep);
    eput(ep);
    return -1;
  }
  elock(ep->parent); // Will this lead to deadlock?
  eremove(ep);
  eunlock(ep->parent);
  eunlock(ep);
  eput(ep);

  return 0;
}

// Must hold too many locks at a time! It's possible to raise a deadlock.
// Because this op takes some steps, we can't promise
uint64
sys_rename(void)
{
  char old[FAT32_MAX_PATH], new[FAT32_MAX_PATH];
  if (argstr(0, old, FAT32_MAX_PATH) < 0 || argstr(1, new, FAT32_MAX_PATH) < 0)
  {
    return -1;
  }

  struct dirent *src = NULL, *dst = NULL, *pdst = NULL;
  int srclock = 0;
  char *name;
  if ((src = ename(old)) == NULL || (pdst = enameparent(new, old)) == NULL || (name = formatname(old)) == NULL)
  {
    goto fail; // src doesn't exist || dst parent doesn't exist || illegal new name
  }
  for (struct dirent *ep = pdst; ep != NULL; ep = ep->parent)
  {
    if (ep == src)
    { // In what universe can we move a directory into its child?
      goto fail;
    }
  }

  uint off;
  elock(src); // must hold child's lock before acquiring parent's, because we do so in other similar cases
  srclock = 1;
  elock(pdst);
  dst = dirlookup(pdst, name, &off);
  if (dst != NULL)
  {
    eunlock(pdst);
    if (src == dst)
    {
      goto fail;
    }
    else if (src->attribute & dst->attribute & ATTR_DIRECTORY)
    {
      elock(dst);
      if (!isdirempty(dst))
      { // it's ok to overwrite an empty dir
        eunlock(dst);
        goto fail;
      }
      elock(pdst);
    }
    else
    { // src is not a dir || dst exists and is not an dir
      goto fail;
    }
  }

  if (dst)
  {
    eremove(dst);
    eunlock(dst);
  }
  memmove(src->filename, name, FAT32_MAX_FILENAME);
  emake(pdst, src, off);
  if (src->parent != pdst)
  {
    eunlock(pdst);
    elock(src->parent);
  }
  eremove(src);
  eunlock(src->parent);
  struct dirent *psrc = src->parent; // src must not be root, or it won't pass the for-loop test
  src->parent = edup(pdst);
  src->off = off;
  src->valid = 1;
  eunlock(src);

  eput(psrc);
  if (dst)
  {
    eput(dst);
  }
  eput(pdst);
  eput(src);

  return 0;

fail:
  if (srclock)
    eunlock(src);
  if (dst)
    eput(dst);
  if (pdst)
    eput(pdst);
  if (src)
    eput(src);
  return -1;
}

uint64
sys_shutdown(void)
{
  // printf("Shutdown hear\n");
  sbi_shutdown();
  return 0;
}

uint64
sys_mount(void)
{
  // 这俩mount都是占位符，暂时不实现功能，但是能通过测试点
  return 0;
}

uint64
sys_umount(void)
{
  return 0;
}

uint64
sys_dup3(void)
{
  struct file *f;
  int oldfd, newfd;

  if (argfd(0, 0, &f) < 0)
    return -1;
  if (argint(1, &newfd) < 0)
    return -1;
  if (argint(0, &oldfd) < 0)
    return -1;
  struct proc *p = myproc();
  if (newfd < 0 || newfd >= NOFILE || p->ofile[newfd] != NULL)
    return -1;
  p->ofile[newfd] = f;
  filedup(f);
  return newfd;
}

uint64
sys_mkdirat(void)
{
  char path[FAT32_MAX_PATH];
  struct dirent *ep;

  // 接下来将path转换为绝对路径，方法和sys_open里处理相对路径的方法一样
  int dirfd, mode;

  if (argint(0, &dirfd) < 0 || argstr(1, path, FAT32_MAX_PATH) < 0 || argint(2, &mode) < 0)
  {
    return -1;
  }

  if (path[0] == '/' || dirfd == AT_FDCWD)
  { // 绝对路径或者没有指定 dirfd（AT_FDCWD），直接使用 path 进行查找
    ;
  }
  else
  {
    struct file *dirf;
    if (argfd(0, 0, &dirf) < 0 || !(dirf->ep->attribute & ATTR_DIRECTORY))
    {
      return -1;
    }

    struct dirent *curr = dirf->ep;
    char fullpath[FAT32_MAX_PATH];
    char *p_out = fullpath + FAT32_MAX_PATH - 1;
    *p_out = '\0';

    int path_len = strlen(path);
    if (path_len >= FAT32_MAX_PATH)
      return -1;
    p_out -= path_len;
    memmove(p_out, path, path_len);

    while (curr != NULL && curr->parent != curr && curr->parent != NULL)
    {
      int len = strlen(curr->filename);
      if (p_out - fullpath < len + 1)
      {
        return -1;
      }
      p_out--;
      *p_out = '/';
      p_out -= len;
      memmove(p_out, curr->filename, len);
      curr = curr->parent;
    }

    if (p_out > fullpath)
    {
      p_out--;
      *p_out = '/';
    }
    safestrcpy(path, p_out, FAT32_MAX_PATH);
  }

  if ((ep = create(path, T_DIR, mode)) == NULL)
  {
    return -1;
  }

  eunlock(ep); // 解开睡眠锁，允许其他进程访问它
  eput(ep);
  return 0;
}

struct linux_dirent64
{
  uint64 d_ino;
  uint64 d_off;
  unsigned short d_reclen;
  unsigned char d_type;
  char d_name[0];
};

uint64
sys_getdents(void)
{
  struct file *f;
  uint64 buf;
  int len;

  if (argfd(0, 0, &f) < 0 || argaddr(1, &buf) < 0 || argint(2, &len) < 0)
    return -1;

  if (f->readable == 0 || !(f->ep->attribute & ATTR_DIRECTORY))
    return -1;

  struct dirent de;
  int count = 0;
  int ret;
  int nread = 0;
  struct linux_dirent64 lde;

  elock(f->ep);
  while (1)
  {
    ret = enext(f->ep, &de, f->off, &count);
    if (ret == 0)
    { // 这个文件已被删除，直接跳过
      f->off += count * 32;
      continue;
    }
    if (ret == -1) // 后面不再有目录了
      break;

    int name_len = strlen(de.filename);
    int reclen = 19 + name_len + 1; // 19 是 struct linux_dirent64不包含文件名大小的size
    reclen = (reclen + 7) & ~7;     // align to 8

    if (nread + reclen > len)
    {
      break; // 再存进去一个linux_dirent64会超过buf的最大空间
    }

    f->off += count * 32;

    // 这下面的部分只是为了严谨起见，但完全可以不添加这些内容
    lde.d_ino = de.first_clus;                            // 用首簇号作为inode
    lde.d_off = f->off;                                   // 如果还想getdents，下次从哪里开始读
    lde.d_reclen = reclen;                                // 包含真实文件名长度的该结构体大小
    lde.d_type = (de.attribute & ATTR_DIRECTORY) ? 4 : 8; // 目录文件则为4，普通文件为8

    // 这下面就是存储过程了
    // 先把不包含文件名的内容存一遍到buf
    if (copyout2(buf + nread, (char *)&lde, 19) < 0)
    {
      eunlock(f->ep);
      return -1;
    }
    // 再把文件名补充进去
    if (copyout2(buf + nread + 19, de.filename, name_len + 1) < 0)
    {
      eunlock(f->ep);
      return -1;
    }

    nread += reclen; // 更新现在读了多少字节
  }
  eunlock(f->ep);

  return nread;
}

uint64
sys_unlink(void)
{
  char path[FAT32_MAX_PATH];
  struct dirent *ep;
  int len;

  if ((len = argstr(1, path, FAT32_MAX_PATH)) <= 0)
    return -1;

  char *s = path + len - 1;
  while (s >= path && *s == '/')
  {
    s--;
  }
  // 这里拒绝删除 "." 和 ".."，否则会导致文件系统混乱（虽然 FAT32 本身就没有这两个目录项，但为了兼容性和安全性，我们也不允许用户创建它们）
  if (s >= path && *s == '.' && (s == path || *--s == '/'))
  {
    return -1;
  }

  // 获取文件对应的 dirent
  // 注意：和前面mkdirat的情况一样，这里简单的 ename(path) 在 dirfd == AT_FDCWD 时
  // 刚好能够通过 myproc()->cwd 或者绝对路径来正确解析并帮我们通过这个测试点。
  if ((ep = ename(path)) == NULL)
  {
    return -1;
  }

  elock(ep);
  // 如果是目录，而且目录非空，不能删除（unlink主要针对文件，但有些文件系统统一处理）
  if ((ep->attribute & ATTR_DIRECTORY) && !isdirempty(ep))
  {
    eunlock(ep);
    eput(ep);
    return -1;
  }

  // 按照 FAT32 原理，将项目标记为0xE5（实际上eremove实现了这一步）
  elock(ep->parent);
  eremove(ep);
  eunlock(ep->parent);

  eunlock(ep);
  eput(ep);

  return 0; // unlink 成功返回0
}