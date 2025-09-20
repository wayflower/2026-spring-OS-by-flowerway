// init: The initial user-level program

#include "kernel/include/types.h"
#include "kernel/include/stat.h"
#include "kernel/include/file.h"
#include "kernel/include/fcntl.h"
#include "xv6-user/user.h"

#define MAX_OUTPUT_SIZE 1024

char *argv[] = { 0 };
char test_outputs[1][MAX_OUTPUT_SIZE];

int
main(void)
{
  int pid, wpid;

  // if(open("console", O_RDWR) < 0){
  //   mknod("console", CONSOLE, 0);
  //   open("console", O_RDWR);
  // }
  dev(O_RDWR, CONSOLE, 0);
  dup(0);  // stdout
  dup(0);  // stderr
  int pipefd[2];

  for(int i = 0; i < 1; i++){
    printf("init: starting sh\n");
    if(pipe(pipefd) == -1) {
      printf("init: pipe failed\n");
      exit(1);
    }
    pid = fork();
    if(pid < 0){
      printf("init: fork failed\n");
      exit(1);
    }
    if(pid == 0){
      close(pipefd[0]);
      dup2(pipefd[1], 1);
      close(pipefd[1]);
      exec("hello_world", argv);
      printf("init: exec sh failed\n");
      exit(1);
    }
      close(pipefd[1]);
      int bytes_read = read(pipefd[0], test_outputs[i], MAX_OUTPUT_SIZE - 1);
      close(pipefd[0]);
       if (bytes_read > 0) {
            test_outputs[i][bytes_read] = '\0';
       }
       printf("This is my output: %s\n", test_outputs[i]);

    for(;;){
      // this call to wait() returns if the shell exits,
      // or if a parentless process exits.
      wpid = wait((int *) 0);
      if(wpid == pid){
        // the shell exited; restart it.
        break;
      } else if(wpid < 0){
        printf("init: wait returned an error\n");
        exit(1);
      } else {
        // it was a parentless process; do nothing.
      }
    }
  }
  shutdown();
  return 0;
}
