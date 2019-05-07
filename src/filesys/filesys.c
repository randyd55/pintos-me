#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "threads/thread.h"
#define READDIR_MAX_LEN 14
/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes t+he file system module.
   If FORMAT is +true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();

  if (format) 
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  free_map_close ();
}


/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size) 
{
  //add a directory parameter?
  block_sector_t inode_sector = 0; //need to change to the proper dir
  struct dir *dir = dir_open_root ();
  struct inode* inode=fetch_from_path(name);
  
  if(inode!=NULL && inode_is_dir(inode)){
    //printf("Pure relative directory\n\n");
    dir=dir_open(inode);

  }
  //printf("filename: '%s'\n", fetch_filename(name));


  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size)
                  && dir_add (dir, fetch_filename(name), inode_sector));
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  dir_close (dir);
  
  

  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *path)
{
  struct inode* inode=fetch_from_path(path);
  if(inode==NULL)
    return NULL;
  return file_open(inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  struct dir *dir = dir_open_root ();
  bool success = dir != NULL && dir_remove (dir, name);
  dir_close (dir); 

  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}

bool
is_absolute_path(const char* file){
  if((char)file[0]=="/")
    return true;
  return false;
}


struct inode *
fetch_from_path(const char* path){
  char* name=malloc(strlen(path)*sizeof(char)+1);
  char* expected_file = malloc(strlen(fetch_filename(path))+1);
  //printf("filename length: %d\n",strlen(fetch_filename(path))+1);
  struct dir* dir;
  struct inode* inode=NULL;
  strlcpy(expected_file, fetch_filename(path),strlen(fetch_filename(path))+1);

  strlcpy(name,path,strlen(path)-strlen(expected_file)+1);
  //printf("Made it here: %s, %s, %s'\n", path, name, expected_file);
  if(thread_current()->working_dir==NULL){
    thread_current()->working_dir=dir_open_root();
  }
  if(is_absolute_path(name)){
    dir = dir_open_root();
  }
  else{
    dir = dir_reopen(thread_current()->working_dir);
  }
  //char s[] = " /String/to/tokenize. ";
  char *token, *save_ptr;

  for (token = strtok_r (name, "/", &save_ptr); token != NULL;
        token = strtok_r (NULL, "/", &save_ptr)){
    //printf ("'%s'\n", token);
    //printf("lookup: %x, %x, %x\n\n", dir,token, &inode);
    dir_lookup(dir, token, &inode);

    //File or directory not found, return highest existing directory
    if(inode==NULL){
      dir_close(dir);
      return NULL;
    }
    //Set dir to next directory existin in tree
    if(inode_is_dir(inode)){
      dir=dir_open(inode);
    }
    //Found but not a directory, return regular file
    else{
      dir_close(dir);
      return NULL; //Return immediately or break?
    }
  }

  dir_lookup(dir,expected_file,&inode);
  dir_close(dir);
  free(expected_file);
  free(name);
  return inode;
}

char* fetch_filename(const char* path){
  char* name=malloc(strlen(path)+1);
  char *token, *save_ptr, *result;
  result=malloc(strlen(path)+1);
  *result="";
  strlcpy(name,path,strlen(path)+1);
  for (token = strtok_r (name, "/", &save_ptr); token != NULL;
        token = strtok_r (NULL, "/", &save_ptr)){
      strlcpy(result,token,strlen(token)+1);
    } 
  free(name);
  return result;
}
