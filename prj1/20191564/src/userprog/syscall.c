#include "userprog/syscall.h"
#include "pagedir.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "lib/kernel/console.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "process.h"
#include "threads/synch.h"

static void syscall_handler (struct intr_frame *);

/* project1 system call 구현, file 관련된 건 read, write 만 */

#define SYS_CALL_NUM 30 
typedef int pid_t;

static int argNums[SYS_CALL_NUM];
static void argNumInit(void); // system call 의 arg 개수 체크
static bool checkUserMemoryAccess(uint32_t *offset); // user가 준 pointer값 맞는지 체크

/* Project1 System Call */
void halt(void);
pid_t exec(struct intr_frame* f);
int wait(struct intr_frame* f);
int read(struct intr_frame* f);
int write(struct intr_frame* f);
/* Project1 Additional System Call */
static int fibonacci(struct intr_frame* f);
static int max_of_four_int(struct intr_frame *f);

void argNumInit(void){
  argNums[SYS_HALT] = 0;
  argNums[SYS_EXEC] = argNums[SYS_WAIT] = argNums[SYS_EXIT] = 1;
  argNums[SYS_READ] = argNums[SYS_WRITE] = 3;
  argNums[SYS_MAX_OF_FOUR_INT] = 4;
  argNums[SYS_FIBONACCI] = 1;
}

bool checkUserMemoryAccess(uint32_t* offset){
  return !is_user_vaddr((void*)offset) || offset == NULL || pagedir_get_page(thread_current()->pagedir, (void*)offset) == NULL;
}

void
syscall_init (void) 
{
  argNumInit();
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/* project1, system call implementation */
static void
syscall_handler (struct intr_frame *f UNUSED) 
{
 
  if(checkUserMemoryAccess((uint32_t*)(f->esp))) exit(-1);
	
  int syscall_number = (int)*((uint32_t*)(f->esp));
  // user 메모리 참조 검토
  for(int argnum=1; argnum <= argNums[syscall_number]; argnum++){
    if(checkUserMemoryAccess((uint32_t*)(f->esp)+argnum)){
      exit(-1);
    }
  }

  switch(syscall_number){
    case SYS_HALT:
      halt();
      break;
    
    case SYS_EXIT:
      exit(*((uint32_t*)(f->esp)+1));
      break;

    case SYS_EXEC:
      f->eax = exec(f);
      break;
      
    case SYS_WAIT:
      f->eax = wait(f);
      break;

    case SYS_READ:
      f->eax = read(f);
      break;
    
    case SYS_WRITE:
      f->eax = write(f); // intr frame 받아서, putbuf를 이용해 화면에 출력
      break;

	case SYS_FIBONACCI:
	  f->eax = fibonacci(f);
	  break;
	
	case SYS_MAX_OF_FOUR_INT:
	  f->eax = max_of_four_int(f);
	  break;

	  }
}

void halt(void){  
  shutdown_power_off();
  return;
}

void exit(int status){
  printf("%s: exit(%d)\n", thread_name(), status);
  thread_current()->exit_status = status;
  thread_exit();
  return;
} 

pid_t exec(struct intr_frame* f){
  return process_execute((void*)*((uint32_t *)(f->esp)+1));
}

int wait(struct intr_frame* f){
  return process_wait(*((uint32_t *)(f->esp)+1));
}

int write(struct intr_frame *f){;
  int fd = (int)*((uint32_t *)(f->esp)+1);
  void* buffer = (void *)*((uint32_t *)(f->esp)+2);
  unsigned size = (int)*((uint32_t *)(f->esp)+3);
  
  if(fd == STDOUT_FILENO){
    putbuf(buffer, size);
    return size;
  }

  return -1;
}


int read(struct intr_frame* f){
  int fd = (int)*((uint32_t *)(f->esp)+1);
  char* buffer = (char *)*((uint32_t *)(f->esp)+2);
  unsigned size = (int)*((uint32_t *)(f->esp)+3);
  //hex_dump(f->esp, f->esp, 100, 1);
  
  if(fd == 0){
	int cnt=0;
	char c;
		
	while(cnt++ < size){
	  c = input_getc();
	  if(c == '\0') break;
	}
	
	return cnt;
  }

  return -1;
}

int fibonacci(struct intr_frame* f){
  int n = (int)*((uint32_t *)(f->esp) + 1);
  
  if(n==1 || n==2) return 1;

  int p1=1, p2=1;
  int res;

  for(int i=3; i<=n; i++){
	res = p1+p2;
	p1 = p2;
	p2 = res;
  }

  return res;
}

int max_of_four_int(struct intr_frame* f){
  int arg[5];

  for(int i=1; i<=4; i++) arg[i] = (int)*((uint32_t *)(f->esp) + i);
  
  if(arg[1] < arg[2]) arg[1] = arg[2];
  if(arg[3] < arg[4]) arg[3] = arg[4];

  return (arg[1] > arg[3]) ? arg[1] : arg[3];
}

