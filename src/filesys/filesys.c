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
  //printf("creating %s\n\n", name);
  //add a directory parameter?
  if(strlen(name) == 0)
   return false;
  block_sector_t inode_sector = 0; //need to change to the proper dir
  struct dir *dir = dir_open_root ();
  struct inode* inode = fetch_from_path(name);
  //printf("HERE %d\n", inode==NULL);
  if(inode!=NULL && inode_is_dir(inode)){
    //printf("Directoruy\n\n");
    dir=dir_open(inode);
  } else{
    //printf("here\n\n");
    return false;
  }
  //printf("filename: '%s'\n", fetch_filename(name));
  bool allocate = free_map_allocate(1, &inode_sector);
  bool inode_c = inode_create(inode_sector, initial_size);
  //printf("inode_create %d\n\n", inode_c);
  bool dir_addd = dir_add(dir, fetch_filename(name), inode_sector);
  //printf("dir_add: %d name: %s\n\n", dir_addd, fetch_filename(name));
  bool success = (dir != NULL
                  && allocate
                  && inode_c
                  && dir_addd);
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  //dir_close (dir);
  
  
  //printf("name: %s, success?: %d\n\n",name, success);
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
  if(strlen(path)==0)
    return NULL;
  struct inode* inode=fetch_from_path(path);
  if(inode==NULL){
    return NULL;
  }
  dir_lookup(dir_open(inode), fetch_filename(path), &inode);
  if(inode==NULL){
    return NULL;
  }
  return file_open(inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  bool success=false;
  struct inode* inode;
  struct dir *dir = dir_open(fetch_from_path(name));
  //printf("filename: %s\n", fetch_filename(name));
  if(thread_current()->working_dir==NULL)
    thread_current()->working_dir=dir_open_root();
  if(dir!=NULL){
    dir_lookup(dir,fetch_filename(name),&inode);
    if(inode==NULL || (inode_is_dir(inode) && (!dir_empty(dir_open(inode)) || dir_is_equal(dir_open(inode),thread_current()->working_dir)))) {

      return false;
    }
    //printf("here: %d\n", dir_empty(dir_open(inode)));

    success = dir_remove (dir, fetch_filename(name));
  }
  //printf("there %d\n", success);
  //dir_close (dir); 

  return success;
}


/* Formats the file system. */
static void
do_format (void)
{
  //printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16,ROOT_DIR_SECTOR))
    PANIC ("root directory creation failed");
  free_map_close ();
  //printf ("done.\n");
}

bool
is_absolute_path(const char* file){
  if(file[0]=='/'){
    return true;
  }
  return false;
}


struct inode *
fetch_from_path(const char* path){
  char* name=malloc(strlen(path)*sizeof(char)+1);
  char* expected_file = malloc(strlen(fetch_filename(path))+1);
  //printf("filename length: %d\n",strlen(fetch_filename(path))+1);
  struct dir* dir;
  struct inode* inode=NULL;
  struct inode* dir_nav=NULL;


  //Get filename of file/directory at end of path
  strlcpy(expected_file, fetch_filename(path),strlen(fetch_filename(path))+1);

  //Determine path leading up to end file/directory
  strlcpy(name,path,strlen(path)-strlen(expected_file)+1);
  
  //printf("Made it here: %s, %s, %s'\n", path, name, expected_file);

  //Determine start of path, absolute or relative
  if(thread_current()->working_dir==NULL){
    thread_current()->working_dir=dir_open_root();
  }
  if(is_absolute_path(name)){
    dir = dir_open_root();
  } 
  else{
    //printf("relative path\n\n");
    dir = dir_reopen(thread_current()->working_dir);
  }
  inode=dir_get_inode(dir);
  //printf("current dir: %x\n\n", dir);
  //Check for empty paths absolute and relative
  if(strlen(name)==0 || (strlen(name)==1 && name[0]=='.')){
    return dir_get_inode(thread_current()->working_dir);
  }
  if(strlen(name)==1 && name[0]=='/'){
    dir = dir_open_root();
    struct inode* temp =  dir_get_inode(dir);
    //dir_close(dir);
    return temp;
  }
  //char s[] = " /String/to/tokenize. ";
  char *token, *save_ptr;
  //Parse path
  for (token = strtok_r (name, "/", &save_ptr); token != NULL;
        token = strtok_r (NULL, "/", &save_ptr)){
    //printf ("token: %s\n", token);
    if(strcmp(token,"..")==0){
      inode=dir_get_parent_inode(dir);

    } else if(strcmp(token,".")==0){
      //Do Nothing
    } 
    else {

      //Check if next directory in path exists
      dir_lookup(dir, token, &inode);
      //printf("lookup: %x, %x, %x\n\n", dir,token, inode);
      //File or directory not found, return NULL
      if(inode==NULL){
        //dir_close(dir);
        //printf("NULL Dir\n");
        return NULL;
      }
      //Directory found, navigate into directory for next search
      else if(inode_is_dir(inode)){
        dir=dir_open(inode);
      }
      //Found but not a directory, return NULL
      else{
        //dir_close(dir);
        //printf("NULL Dor\n\n\n\n\n\n\n");
        return NULL; //Return immediately or break?
      }
    }
  }
  return inode;
}

char* fetch_filename(const char* path){
  char* name=malloc(strlen(path)+1);
  char *token, *save_ptr, *result;
  if(strlen(path)==1 && path[0]=='/' || strlen(path)==0){
    free(name);
    return "";
  }
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
