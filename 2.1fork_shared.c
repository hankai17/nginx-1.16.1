#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
int var = 100;

int main() {
  int *p = (int*)malloc(sizeof(int) * 100);
  p[1] = 99;
  pid_t pid = fork();
  if(pid == -1) {
	perror("fork err:");
	exit(1);
  } else if(pid == 0) {
	sleep(1);
    p[0] = 2;
	printf("child, pid=%u, ppid=%ui, var=%d\n",getpid(),getppid(),p[0]);
	//printf("var_addr:%d\n",(int)(&var));
    printf("child p[1]: %d\n", p[1]);
  } else {
    p[0] = 1;
	printf("parent, pid=%u, ppid=%u, var=%d\n",getpid(),getppid(),p[0]);
	sleep(2);
	printf("after, parent, pid=%u, ppid=%u, var=%d\n",getpid(),getppid(),p[0]);
	//printf("var_addr:%d\n",(int)&var);
    printf("parent p[1]: %d\n", p[1]);
  }
  return 0;
}

