#include<stdio.h>
#include<syscall.h>
#include<stdlib.h>

int main(int argc, char*argv[]){
  if(argc != 5){
	printf("argc should be 5\n");
	exit(1);
  }

  int num[5];

  for(int i=1; i<=4; i++) num[i] = atoi(argv[i]);

  int fib_result = fibonacci(num[1]);
  int max_result = max_of_four_int(num[1], num[2], num[3], num[4]);

  printf("%d %d\n", fib_result, max_result);

  return 0;
}
