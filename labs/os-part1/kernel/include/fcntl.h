#define O_RDONLY 0x000
#define O_WRONLY 0x001
#define O_RDWR 0x002
#define O_APPEND 0x004
#define O_CREATE 0x40 // 修改，原本为0x200
#define O_TRUNC 0x400

#define DIR 0x040000
#define FILE 0x100000

#define O_DIRECTORY 0x0200000
#define AT_FDCWD -100
