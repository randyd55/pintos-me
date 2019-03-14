#include <stdio.h>
#include "userprog/syscall.h"
#include <syscall-nr.h>
//#include "threads/thread.h"
#include "filesys/filesys.h"
#include "userprog/process.h"
//#include "vaddr.h"
#include "devices/input.h"


static void syscall_handler (struct intr_frame *);


int *PHYS_BASE = (int *)0xC0000000;
int fd_count = 1;
struct lock filesys_lock;

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&filesys_lock);
}

static void
syscall_handler (struct intr_frame *f UNUSED)
{
  switch(*((uint32_t *) (f->esp))){
    case SYS_EXEC :
        f->eax=exec(((char *) (f->esp + 4) ));
        break;    
    case SYS_WRITE :
        f->eax=write(*((uint32_t *) (f->esp + 4) ),((void*) (f->esp + 8) ),*((unsigned *) (f->esp + 12) ));
        break;
    case SYS_READ :
        f->eax=read(*((uint32_t *) (f->esp + 4) ),((void*) (f->esp + 8) ),*((unsigned *) (f->esp + 12) ));
        break;
    case SYS_OPEN :
        f->eax=open(((char *) (f->esp + 4) ));
        break;
    case SYS_CLOSE :
        close(*((uint32_t *) (f->esp + 4) ));
        break;
    case SYS_HALT :
        halt();
        break;
    case SYS_EXIT :
        exit(*((uint32_t *) (f->esp + 4) ));
        break;

        //f->eax = use this for any methods that have a return value

        //add write case and all other cases

  }
  //hex_dump(*((uint32_t *) f->esp), *((uint32_t *) f->esp), PHYS_BASE - *((uint32_t *) f->esp),  1);


  printf ("system call!\n");
}

/*  */
void halt(void){
  shutdown_power_off();
}

void exit(int status){
  struct thread *t = thread_current();
  t->called_exit = true; //this thread exited properly
  t->exit_status = status;

  
  sema_up(t->child_exit_sema);
  sema_down(t->parent_wait_sema);

  thread_exit();
}


int wait(pid_t pid){

  //struct thread *t = thread_current();


  return process_wait(pid);
}

pid_t exec(const char *cmd_line){
  return process_execute(cmd_line);

}

int open(const char *file){

    struct file* f_open = NULL;
    int open_spot;
    lock_acquire(&filesys_lock);
    thread_current()->fd++;
    open_spot = getFd();
    if(open_spot != -1){

      f_open = filesys_open(file); //this is just wrong i think?
      thread_current()->files[open_spot] = f_open; //fix her
    }
   // const char* temp_file = thread_current()->files[fd];
   lock_release(&filesys_lock);

    if(f_open == NULL){
      return -1;
    } else {
      return open_spot+2;
    }
}

/*helper method*/
int getFd(){
  int i;
  for(i = 0; i < 128; i++){
    if(thread_current()->files[i] == NULL)
      return i;
  }
  return -1;
}
int read(int fd, const void *buffer, unsigned size){

  int bytes_read = 0, i = 0; 
  lock_acquire(&filesys_lock);
  struct thread* t = thread_current();

  if(fd == 0){

    
    for(; i < size; i++){
      *(uint8_t *) buffer = input_getc();
      buffer++;
    }
    bytes_read = i;

  } else if(t->files[fd - 2] == NULL) {
    return -1; //file cannot be read, because it doesn't exist

  } else { //file exists
    bytes_read = file_read(t->files[fd - 2], buffer, size);
  }
  
  lock_release(&filesys_lock);
  return bytes_read;
}


bool create(const char* file, unsigned initial_size){
  return filesys_create(file, initial_size);
}

bool remove(const char* file){
  return filesys_remove(file);
}


int write(int fd, const void *buffer, unsigned size){

  int written = 0;
  printf("Yolo\n\n\n");
  printf("%s",buffer);
  lock_acquire(&filesys_lock);
  struct thread* t = thread_current();
  if(fd == 0)
    exit(-1);
  else if(fd == 1)
  {
    printf("%s\n",buffer);
    //putbuf(buffer, size);
    //written = size;

  } else if(t->files[fd-2] != NULL) 

  {
    written=file_write(t->files[fd-2], buffer,size);
  }

  lock_release(&filesys_lock);
  return written;
}

void close(int fd){
  struct thread* t = thread_current();

  if(fd < 2){
    printf("invald file descriptor");
  }

  lock_acquire(&filesys_lock);

  if(t->files[fd-2] != NULL){ //if the file exists
    file_close (t->files[fd -2]); //close file
    t->files[fd-2] = NULL; //free up spot
  }

  lock_release(&filesys_lock);
}
