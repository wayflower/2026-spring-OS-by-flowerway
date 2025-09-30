// init: The initial user-level program

#include "kernel/include/types.h"
#include "kernel/include/stat.h"
#include "kernel/include/file.h"
#include "kernel/include/fcntl.h"
#include "xv6-user/user.h"

#define MAX_OUTPUT_SIZE (1<<10)
#define MAX_CASES 1
#define STDOUT 1
#define MAX_READ_BYTES 100

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

char *argv[] = { 0 };

char test_outputs[MAX_OUTPUT_SIZE];
int output_lengths = 0;
char* order = "0";

void print_test_program(const char* program_name) {
  printf("Starting test program: %s\n", program_name);
  printf("Scheduler type: ");

#ifdef TYPE_PRODUCER
    printf("MPMC");
    order = "1";
#elif defined(TYPE_PHILOSOPHER)  
    printf("Philosopher Dining");
    order = "2";
#else
    printf("Unknown");
#endif
    printf("\n\n");
}

int
main(void)
{
  int pid, wpid;
  int status;

  // if(open("console", O_RDWR) < 0){
  //   mknod("console", CONSOLE, 0);
  //   open("console", O_RDWR);
  // }
  dev(O_RDWR, CONSOLE, 0);
  dup(0);  // stdout
  dup(0);  // stderr

  char* program_name = TEST_PROGRAM;
  print_test_program(program_name);
  if (order[0]=='0') {
    exit(1);
  }

    printf("init: starting %s\n", program_name);
    int pipefd[2] = {0, 0};
    if(pipe(pipefd) == -1) {
      printf("init: pipe failed\n");
      exit(1);
    }
    pid = fork();
    if(pid < 0){
      printf("init: fork failed\n");
      close(pipefd[0]);
      close(pipefd[1]);
      exit(1);
    }
    if(pid == 0){
      close(pipefd[0]);
      dup2(pipefd[1], STDOUT);
      close(pipefd[1]);

      exec(program_name, argv);
      printf("init: exec %s failed\n", program_name);
      exit(1);
    }
    
    close(pipefd[1]);
    int bytes_read = 0;
    int total_bytes = 0;
    int max_read_bytes = MAX(MAX_READ_BYTES, MAX_OUTPUT_SIZE-1-total_bytes);
    while((bytes_read = read(pipefd[0], test_outputs+total_bytes, max_read_bytes)) > 0) {
      total_bytes+=bytes_read;
      max_read_bytes = MAX(MAX_READ_BYTES, MAX_OUTPUT_SIZE-1-total_bytes);
    }
    test_outputs[total_bytes] = '\0';
    output_lengths = total_bytes;
    printf("testing output size:%d, contents:\n%s", total_bytes, test_outputs);
    close(pipefd[0]);

    wpid = wait(&status);
    if(wpid == -1) {
      printf("init: no more child processes, break\n");
    } else if(wpid > 0) {
      printf("init: process pid=%d exited\n", wpid); 
    }

  printf("init: test execution completed, starting judger\n");
  char *judger_argv[4];
  judger_argv[0] = "judger";
  judger_argv[1] = order;
  judger_argv[2] = test_outputs;
  judger_argv[3] = 0;

  pid = fork();
  if(pid==0) {
    exec("judger", judger_argv);
    printf("exec judger failed\n");
    exit(1);
  }
  wpid = wait(&status);
  printf("init: judger completed\n");

  shutdown();
  return 0;
}
