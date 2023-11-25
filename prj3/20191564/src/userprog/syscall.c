#include "userprog/syscall.h"
#include "pagedir.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "lib/kernel/console.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "process.h"
#include "threads/synch.h"
#include "filesys/filesys.h"
#include "filesys/file.h"

static void syscall_handler (struct intr_frame *);

/* project1 system call 구현, file 관련된 건 read, write 만 */

#define SYS_CALL_NUM 30 
#define FILE_NAME 0
#define FILE_DESC 1
#define FILE_BUFFER 2
typedef int pid_t;
struct lock lock_for_file;
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

/* Project2 System Call */
bool create(struct intr_frame* f);
bool remove(struct intr_frame* f);
int open(struct intr_frame* f);
int filesize(struct intr_frame* f);
void seek(struct intr_frame* f);
unsigned tell(struct intr_frame* f);
void close(struct intr_frame* f);
static bool checkFileValidation(void* param, int flag);


void argNumInit(void){
  argNums[SYS_HALT] = 0;
  argNums[SYS_EXEC] = argNums[SYS_WAIT] = argNums[SYS_EXIT] = argNums[SYS_FIBONACCI] = argNums[SYS_REMOVE] = argNums[SYS_OPEN] = 1;
  argNums[SYS_FILESIZE] = argNums[SYS_TELL] = argNums[SYS_CLOSE] = 1;
  argNums[SYS_CREATE] = argNums[SYS_SEEK] = 2;
  argNums[SYS_READ] = argNums[SYS_WRITE] = 3;
  argNums[SYS_MAX_OF_FOUR_INT] = 4;

  lock_init(&lock_for_file);
}

bool checkUserMemoryAccess(uint32_t* offset){
  return !is_user_vaddr((void*)offset) || pagedir_get_page(thread_current()->pagedir, (void*)offset) == NULL;
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
  //printf("syscall number : %d\n", syscall_number);
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

	// project2 System Call
	
	case SYS_CREATE:
	  f->eax = create(f);
	  break;

	case SYS_REMOVE:
	  f->eax = remove(f);
	  break;

	case SYS_OPEN:
	  f->eax = open(f);
	  break;

	case SYS_FILESIZE:
	  f->eax = filesize(f);
	  break;

	case SYS_SEEK:
	  seek(f);
	  break;

	case SYS_TELL:
	  f->eax = tell(f);
	  break;

	case SYS_CLOSE:
	  close(f);
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
  for(int i=3; i<128; i++){
	if(thread_current()->fd_table[i] == NULL) continue;
	file_close(thread_current()->fd_table[i]);
  }

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
  //printf("write : %s\n", (char*)buffer);

  if(!checkFileValidation((void*)buffer, FILE_BUFFER)) exit(-1);

  if(fd == STDOUT_FILENO){

	lock_acquire(&lock_for_file);
    putbuf(buffer, size);
	lock_release(&lock_for_file);
    return size;
  }

  else{
	if(!checkFileValidation((void*)fd, FILE_DESC)) exit(-1);
	lock_acquire(&lock_for_file);
	int res = file_write(thread_current()->fd_table[fd], buffer, size);
	lock_release(&lock_for_file);
	return res;
  }

  return -1;
}


int read(struct intr_frame* f){
  int fd = (int)*((uint32_t *)(f->esp)+1);
  char* buffer = (char *)*((uint32_t *)(f->esp)+2);
  unsigned size = (int)*((uint32_t *)(f->esp)+3);
  //hex_dump(f->esp, f->esp, 100, 1);

  if(!checkFileValidation((void*)buffer, FILE_BUFFER)) exit(-1);
  

  if(fd == 0){
	int cnt=0;
	char c;
		
	lock_acquire(&lock_for_file);
	while(cnt++ < size){
	  c = input_getc();
	  *buffer = c;
	  buffer++;
	  if(c == '\0') break;
	}
	lock_release(&lock_for_file);
	return cnt;
  }

  else{
	if(!checkFileValidation((void*)fd, FILE_DESC)) exit(-1);
	lock_acquire(&lock_for_file);
	int res = file_read(thread_current()->fd_table[fd], buffer, size);
	lock_release(&lock_for_file);
	return res;
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


/* Project2 System Call */
bool create(struct intr_frame* f){
  const char*file = (const char*)*((uint32_t *)(f->esp)+1);
  unsigned initial_size = (int)*((uint32_t *)(f->esp)+2);

  if(!checkFileValidation((void*)file, FILE_NAME)) exit(-1);

  lock_acquire(&lock_for_file);
  bool success = filesys_create(file, initial_size);
  lock_release(&lock_for_file);

  return success;
}

bool remove(struct intr_frame* f){
  const char* file = (const char*)*((uint32_t *)(f->esp)+1);

  if(!checkFileValidation((void*)file, FILE_NAME)) exit(-1);

  lock_acquire(&lock_for_file);
  bool success = filesys_remove(file);
  lock_release(&lock_for_file);

  return success;
}

int open(struct intr_frame* f){
  
  const char*file = (const char*)*((uint32_t *)(f->esp)+1);
  if(!checkFileValidation((void*)file, FILE_NAME)) exit(-1);
  
  int i;
  int fd = -1;

  struct file* newFile;

  for(i=3; i<128; i++){
	if(thread_current()->fd_table[i] != NULL) continue;

	lock_acquire(&lock_for_file);
	newFile = filesys_open(file);
	lock_release(&lock_for_file);
	
	if(newFile){
	  fd = i;
	  thread_current()->fd_table[i] = newFile;
	  if(strcmp(thread_name(), file) == 0) file_deny_write(newFile);
	}
	  
	break;
  }

  return fd;

}

int filesize(struct intr_frame* f){
  int fd = (int)*((uint32_t*)(f->esp) + 1);
  
  if(!checkFileValidation((void*)fd, FILE_DESC)) exit(-1);
  lock_acquire(&lock_for_file);
  int file_size = file_length(thread_current()->fd_table[fd]);
  lock_release(&lock_for_file);

  return file_size;
}

void seek(struct intr_frame* f){

  int fd = (int)*((uint32_t*)(f->esp) + 1);
  unsigned position = (int)*((uint32_t *)(f->esp)+2);

  if(!checkFileValidation((void*)fd, FILE_DESC)) exit(-1);
  lock_acquire(&lock_for_file);
  file_seek(thread_current()->fd_table[fd], position);
  lock_release(&lock_for_file);
  return;
}

unsigned tell(struct intr_frame* f){

  int fd = (int)*((uint32_t*)(f->esp) + 1);
  
  if(!checkFileValidation((void*)fd, FILE_DESC)) exit(-1);
  lock_acquire(&lock_for_file);
  unsigned next_byte = file_tell(thread_current()->fd_table[fd]);
  lock_release(&lock_for_file);

  return next_byte;
}

void close(struct intr_frame* f){
  int fd = (int)*((uint32_t*)(f->esp) + 1);
  if(!checkFileValidation((void*)fd, FILE_DESC)) exit(-1);

  lock_acquire(&lock_for_file);
  file_close(thread_current()->fd_table[fd]);
  lock_release(&lock_for_file);

  thread_current()->fd_table[fd] = NULL;

  return;
}

bool checkFileValidation(void* param, int flag){
  if(flag == FILE_NAME){
	return param != NULL;
  }

  else if(flag == FILE_BUFFER){
	return is_user_vaddr(param) && param != NULL;
  }

  else{
	int fd = (int)param;
	return thread_current()->fd_table[fd] != NULL && (2 < fd && fd < 128);
  }

}
  
