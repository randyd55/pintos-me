#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "filesys/filesys.h"
#include "userprog/process.h"
#include "threads/vaddr.h"
#include "devices/input.h"
#include "userprog/pagedir.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "threads/thread.h"
#include "filesys/file.h"


static void syscall_handler (struct intr_frame *);

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&filesys_lock);
  lock_init(&filesys_extending_lock);
  lock_init(&create_lock);
}

/**
 * Determines appropriate action and sends data from stack to
 * the given functions if any system call is made
 */
static void
syscall_handler (struct intr_frame *f UNUSED)
{

  //Randy Driving
  //Check if stack is valid
  if(!check_pointer(f->esp))
    exit(-1);

  //Determine which syscall was made and process the call
  switch(*((uint32_t *) (f->esp))){
    case SYS_EXEC :
      if (check_pointer(*(int*)(f->esp + 4)))
        f->eax = exec(((char *) *(int*)(f->esp + 4)));
      else
      	exit(-1);
      break;

    case SYS_WRITE :
      if (check_pointer(f->esp +4) 
        && check_pointer(*(int*)(f->esp + 8)) 
        && check_pointer(f->esp + 12))
        f->eax = write(*((uint32_t *) (f->esp + 4) ),
          ((void*)*(int*)(f->esp + 8) ),
          *((unsigned *) (f->esp + 12) ));
      else
      	exit(-1);
      break;

    case SYS_READ :
       if (check_pointer(f->esp + 4) 
        && check_pointer(*(int*)(f->esp + 8)) 
        && check_pointer(f->esp + 12))
          f->eax = read(*((uint32_t *) (f->esp + 4) ),
            ((void*)*(int*)(f->esp + 8) ),
            *((unsigned *) (f->esp + 12) ));
      else
      	exit(-1);
      break;

    case SYS_OPEN :
      if (check_pointer(*(int*)(f->esp + 4)))
        f->eax = open(((char *) *(int*)(f->esp + 4) ));
      else
      	exit(-1);
      break;

    case SYS_CLOSE :
      if (check_pointer(f->esp + 4))
        close(*((uint32_t *) (f->esp + 4) ));
      else
      	exit(-1);
      break;

    case SYS_CREATE :
		  if (check_pointer(*(int*)(f->esp +4)))
        f->eax = create((char*)*(int*) (f->esp + 4),(*(int*)(f->esp + 8)));
      else
      	exit(-1);
      break;

    case SYS_REMOVE :
      if (check_pointer(f->esp + 4))
        f->eax = remove((char*)*(int*) (f->esp + 4) );
      else
        exit(-1);
      break;
    case SYS_FILESIZE:
      f->eax = filesize(*(int*)(f->esp +4));
      break;

    case SYS_SEEK:
      seek(*(int*)(f->esp+4),*(unsigned*)(f->esp+8));
      break;

    case SYS_TELL:
      f->eax = tell(*(unsigned*)(f->esp + 4));
      break;

    case SYS_HALT :
      halt();
      break;

    case SYS_EXIT :
      if (check_pointer(f->esp + 4))
        exit(*((uint32_t *) (f->esp + 4) ));
      else
      	exit(-1);
      break;

    case SYS_WAIT :
      f->eax = wait(*(int*)(f->esp +4));
      break;

    case SYS_CHDIR :
      if(check_pointer(f->esp + 4))
        f->eax = chdir((char *)*(int*)(f->esp + 4));
      else
        f->eax = false; //not sure whether to exit or ret. false
      break;

    case SYS_MKDIR :
      if(check_pointer(f->esp + 4))
        f->eax = mkdir((char*)*(int*)(f->esp + 4));
      else
        f->eax = false; //not sure whether to exit or ret. false
      break;

    case SYS_READDIR :
      if(check_pointer(f->esp + 8))
        f->eax = readdir((*(int*)(f->esp + 4)),
        ((char *)*(int*)(f->esp + 8)));
      else
        f->eax = false; //not sure whether to exit or ret. false
      break;

    case SYS_ISDIR :
      if (check_pointer(f->esp + 4))
        f->eax = isdir(*(int*)(f->esp +4));
      else 
        f->eax = false;
      break;

    case SYS_INUMBER :
      if (check_pointer(f->esp + 4))
        f->eax = inumber(*(int*)(f->esp +4));
      else 
        //what if there is no associated file?what do we return?
      break;
  }
      

  
  //Randy Done
}

//Chineye Driving

/*
  Changes the working directory of a thread, returns false if
  Directory doesn't exist
*/
bool 
chdir (const char *dir){
  struct inode* inode;
  struct dir* old_dir;
  struct dir* new_dir;
  lock_acquire(&filesys_lock);
  //Determine parent directory of directory change
  inode=fetch_from_path(dir);
  old_dir=dir_open(inode);
  if(old_dir==NULL){
    lock_release(&filesys_lock);
    return false;
  }
  //Check that end of path isnt a special directory
  if(strcmp(fetch_filename(dir),"..")!=0 && inode!=NULL){
    dir_lookup(old_dir,fetch_filename(dir),&inode);
  } else {
    //If its '..', just set thread working directory to the
    //directory found at fetch_from_path
    new_dir=dir_open(inode);
    if(new_dir==NULL){
      lock_release(&filesys_lock);
      return false;
    }
    dir_close(thread_current()->working_dir);
    thread_current()->working_dir=new_dir;
    lock_release(&filesys_lock);
    return true;
  }
  dir_close(old_dir);

  //If not a special directory, change working directory to the direcotry
  //found at the end of path
  if(inode==NULL){
    lock_release(&filesys_lock);
    return false;
  } else{
    new_dir=dir_open(inode);
    if(new_dir==NULL){
      lock_release(&filesys_lock);
      return false;
    }
    dir_close(thread_current()->working_dir);
    thread_current()->working_dir=new_dir;
    lock_release(&filesys_lock);
    return true;
  }
  

}
/*
  Makes a new directory with the path const char* dir
*/
bool 
mkdir (const char *dir){
  char* name=malloc(strlen(dir)*sizeof(char));
  bool success;
  int location; 

  if(strlen(dir) == 0){
    return false;
  }

  //Determine parent directory to create new directory in
  struct dir* parent_dir = dir_open(fetch_from_path(dir));
  if(parent_dir==NULL){
    lock_release(&filesys_lock);
    return false;
  }
  //Determine name of new directory
  name = fetch_filename(dir);

  //Allocate and add directory
  if(free_map_allocate(1,&location))
    success=dir_create(location,0,
            inode_get_inumber(dir_get_inode(parent_dir)));
  bool added= dir_add(parent_dir,name,location);
  struct inode *inode = NULL;

  //If add failed(name collision, etc), free resources
  if(!added){
    free_map_release(location,1);
  } 

  dir_close(parent_dir);
  free(name);
  return success && added;
}

/*
  Reads entry names in a directory and copies it to char* name
*/
bool 
readdir (int fd, char *name){
  struct file* file;
  struct dir* dir;
  bool success=false;
  file= thread_current()->files[fd];
  //Determine directory to read and read entry
  if(file!=NULL && inode_is_dir(file_get_inode(file))){
    dir=dir_open(file_get_inode(file));
    dir_set_pos(dir,file_tell(file));
    success=dir_readdir(dir,name);
  }
  //Update file pos so that directory can continue reading while its open
  if(success)
    file_seek(file,file_tell(file)+dir_entry_size());
  dir_close(dir);
  return success;
}
/*
  Returns bool on whether or not a file is a directory
*/
bool 
isdir (int fd){
  if(fd<2 || fd>128)
    return false;
  return inode_is_dir(file_get_inode(thread_current()->files[fd]));
}
/*
  Returns free map sector of inode which is unique to that inode
*/
int 
inumber (int fd){
  struct file* file;
  file= thread_current()->files[fd];
  if(file!=NULL){
    return inode_get_inumber(file_get_inode(file));
  }
  return -1;
}


/**
 * Checks that pointer is within valid access space
 */
bool
check_pointer (uint32_t * stack_ptr)
{
  if (stack_ptr == NULL 
    || is_kernel_vaddr(stack_ptr) 
    || !is_user_vaddr(stack_ptr) 
    || pagedir_get_page(thread_current() -> pagedir, stack_ptr) == NULL)
    return false;
  return true;
}
//Chineye Done

//Tim Driving
/**
 * Halts process
 */
void 
halt (void)
{
  shutdown_power_off();
}

/**
 * Exits process with given exit status
 */
void 
exit (int status)
{
  struct thread *t = thread_current();
  //Set exit status
  t->exit_status = status;
  //Output exit statement
  printf("%s: exit(%d)\n",t->name,t->exit_status);
  lock_acquire(&filesys_lock);
  file_close(t->executable);
  lock_release(&filesys_lock);
  thread_exit();
}

/** 
 * Waits until child process with given pid 
 * finishes and returns its exit status 
 */
int
wait (pid_t pid)
{
  return process_wait(pid);
}
/**
 * Executes program given its name and arguments
 */
pid_t 
exec (const char *cmd_line)
{
  int e = process_execute(cmd_line);
  return e;
}

//Randy Done
//Tim Driving
/**
 * Opens file in filesystem
 */
int 
open (const char *file)
{
    struct file* f_open = NULL;
    int open_spot;
    lock_acquire(&filesys_lock);
    thread_current()->fd++;
    //Find spot for thread to store file pointer
    open_spot = getFd();
    //Store file in thread for later use
    if(open_spot != -1)
    {
      f_open = filesys_open(file);
      thread_current()->files[open_spot] = f_open;
    }
    lock_release(&filesys_lock);
    //Return file descriptor of file in thread(open_spot)
    if(f_open == NULL)
      return -1;
    else
      return open_spot;
}


/**
 * Helper method, determines the nearest open position in thread file array
 */
int 
getFd ()
{
  int i;
  //Ignore first 2 file descriptors, reserved for system
  for(i = 2; i < 130; i++)
  {
    if(thread_current()->files[i] == NULL)
      return i;
  }
  return -1;
}

/**
 * Read data from file of given file descriptor
 */
int 
read (int fd, const void *buffer, unsigned size)
{
  int bytes_read = 0, i;
  lock_acquire(&filesys_lock);
  struct thread* t = thread_current();

  //STDIN read
  if(fd == 0)
  {
    for(i=0; i < size; i++)
    {
      *(uint8_t *) buffer = input_getc();
      buffer++;
    }
    bytes_read = i;
  }
  //Invalid file descriptor
  else if(fd < 0 || fd > 130)
  {
    bytes_read = -1;
  }
  //User file read
  else if(t->files[fd] == NULL) 
  {
    //File doesn't exist
    bytes_read = -1;
  } else { 
    //File exists
    bytes_read = file_read(t->files[fd], buffer, size);
  }
  lock_release(&filesys_lock);
  return bytes_read;
}

//Tim Done
//Anthony Driving

/**
 * Returns filesize of file with given file descriptor
 */
int 
filesize (int fd)
{
  return file_length(thread_current()->files[fd]);
}

/**
 * Creates a new file of a given size and name
 */
bool 
create (const char* file, unsigned initial_size)
{
  return filesys_create(file, initial_size);
}

/**
 * Deletes file of a given name
 */
bool 
remove (const char* file)
{
  return filesys_remove(file);
}

/**
 * Writes data to a file of a given file descriptor
 */
int 
write (int fd, const void *buffer, unsigned size)
{
  //Check for invalid write
  if(buffer==NULL)
    exit(-1);
  int written = 0;
  struct thread* t = thread_current();
  //Invalid file descriptor
  if(fd <= 0 || fd > 130)
  {
    exit(-1);
  }
  
  //STDOUT write
  else if(fd == 1)
  {
    //User input
    putbuf((char*)buffer, size ); 
    written = size;
  }
  else if(isdir(fd)){

    written = -1;
  }
  //User file write
  else 
  {
    written = file_write(t->files[fd],buffer,size); 
  }
  return written;
}
//Anthony Done
//Randy Driving

/**
 * Sets next byte to be written to or read from of given file descriptor
 * to position
 */
void 
seek (int fd, unsigned position)
{
  struct thread* t=thread_current();
  file_seek(t->files[fd],position);
}

/**
 * Returns the next byte to be written to or read from of file with given
 * file descriptor
 */
unsigned 
tell (int fd)
{
  struct thread* t=thread_current();
  file_tell(t->files[fd]);
}

/**
 * Removes file from the current thread's file list and closes the file
 */
void 
close (int fd)
{
  struct thread* t = thread_current();
  if(fd < 2 || fd>128)
  {
    exit(-1);
  }
  lock_acquire(&filesys_lock);
  //Check if file exists in thread struct
  if(t->files[fd] != NULL)
  { 
    //Close file and free space
    file_close (t->files[fd]); 
    t->files[fd] = NULL; 
  }
  lock_release(&filesys_lock);
}

//Randy Done