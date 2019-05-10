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

  if(strlen(name) == 0)
    return false;
  block_sector_t inode_sector = 0;
  struct dir *dir = dir_open_root ();
  struct inode* inode = fetch_from_path(name);
  //Determine directory to create file in
  if(inode != NULL && inode_is_dir(inode)){
    dir = dir_open(inode);
  } else {
    return false;
  }

  //Allocate resources for file and add
  bool allocate = free_map_allocate(1, &inode_sector);
  bool inode_c = inode_create(inode_sector, initial_size);
  bool added = dir_add(dir, fetch_filename(name), inode_sector);
  bool success = (dir != NULL
                  && allocate
                  && inode_c
                  && added);
  if (!success && inode_sector != 0)
    free_map_release (inode_sector, 1);

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
  if(strlen(path) == 0)
    return NULL;
  struct inode* inode = fetch_from_path(path);

  if(inode == NULL)
    return NULL;

  dir_lookup(dir_open(inode), fetch_filename(path), &inode);

  if(inode == NULL)
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
  bool success = false;
  struct inode* inode;
  //Get the parent directory of the file you want to remove
  struct dir *dir = dir_open(fetch_from_path(name));

  //Ensure thread has a working directory
  if(thread_current()->working_dir == NULL)
    thread_current()->working_dir = dir_open_root();

  if(dir!=NULL){
    //Find file to remove in directory 
    dir_lookup(dir,fetch_filename(name),&inode);
    //Dont remove if inode is null, or if its a directory and not empty or
    //its the current working directory
    if(inode == NULL || (inode_is_dir(inode) && (!dir_empty(dir_open(inode))
     || dir_is_equal(dir_open(inode),thread_current()->working_dir)))) {

        return false;
    }

    success = dir_remove (dir, fetch_filename(name));
  }

  return success;
}


/* Formats the file system. */
static void
do_format (void)
{
  free_map_create ();

  if (!dir_create (ROOT_DIR_SECTOR, 16,ROOT_DIR_SECTOR))
    PANIC ("root directory creation failed");

  free_map_close ();
}

/*
is_absolute_path, determines if the file path given is an abolute file path

Parameters:
- const char* file

Return:
- true, if it is an absolute path
- false, if it is a relative path, file is NULL, or no file path given
*/
bool
is_absolute_path(const char* file)
{

    if(file == NULL){
      return false;
    } else if (strlen(file) == 0){
      return false;
    } else if (file[0] == '/')
      return true;

  return false;
}


/*
fetch_from_path, fetches the file name given,
sets the working_directory of the current thread, and parses the path given

Allows functions to receive file and determine course of action given
the parent directory of their expected file/directory

Parameters:
- const char* path

Return:
- NULL, if the path leads to no existing directory
- inode of the second lowest directory in the path 
*/

struct inode *
fetch_from_path (const char* path) {

  char* name = malloc(strlen( path ) * sizeof(char) + 1 );
  char* expected_file = malloc( strlen ( fetch_filename ( path ) ) + 1 );

  struct dir* dir;
  struct inode* inode = NULL;

  //Get filename of file/directory at end of path
  strlcpy(expected_file,fetch_filename(path),
    strlen(fetch_filename(path)) + 1);

  //Determine path leading up to end file/directory
  strlcpy(name, path, strlen(path) - strlen(expected_file) + 1);

  //Ensure current thread has a working directory
  if(thread_current()->working_dir == NULL){
    thread_current()->working_dir = dir_open_root();
  }

  //Determine start of path, absolute or relative
  if(is_absolute_path(name)){
    dir = dir_open_root();
  } else {
    dir = dir_reopen(thread_current()->working_dir);
  }

  if(dir == NULL)
    return NULL;

  inode = dir_get_inode(dir);

  //Check for empty paths absolute and relative
  if(strlen(name) == 0 || (strlen(name) == 1 && name[0] == '.'))
    return dir_get_inode(thread_current()->working_dir);


  if(strlen(name) == 1 && name[0] == '/'){
    dir = dir_open_root();
    if(dir == NULL)
      return NULL;
    inode = dir_get_inode(dir);
    dir_close(dir);
    return inode;
  }

  //Path parsing
  char *token, *save_ptr;
  for (token = strtok_r (name, "/", &save_ptr); token != NULL;
        token = strtok_r (NULL, "/", &save_ptr)){
    //Check for special directorys '.' and '..'
    //Set to parent directory of current
    if(strcmp(token,"..") == 0){
      inode = dir_get_parent_inode(dir);

    } else if(strcmp(token,".") == 0){
      //Do Nothing, continue parsing
    } else {

      //Check if next directory in path exists
      dir_lookup(dir, token, &inode);

      //File or directory not found, return NULL
      if(inode == NULL){
        dir_close(dir);
        free(name);
        free(expected_file);
        return NULL;
      }

      //Directory found, navigate into directory for next search
      else if(inode_is_dir(inode)){
        dir = dir_open(inode);
        if(dir == NULL)
          return NULL;
      }

      //Found but not a directory, return NULL
      else{
        dir_close(dir);
        free(name);
        free(expected_file);
        return NULL;
      }
    }
  }
  //Once reached end of shaved path, return the inode
  dir_close(dir);
  free(name);
  free(expected_file);
  return inode;
}


/*
fetch_filename, fetches the filename of the path passed in

Parameters:
- const char* path, the path for the associated file or directory

Return:
- char* result, the name of the file at the end of a path,
  i.e. '/a/c/b/d/filename.txt' would return 'filename.txt'

*/
char* 
fetch_filename(const char* path){
  char* name = malloc(strlen(path) + 1);
  char *token, *save_ptr, *result;

  if (path == NULL)
    return NULL;

  if(strlen(path) == 1 && path[0] == '/' || strlen(path) == 0) {
    free(name);
    return "";
  }
  result = malloc(strlen(path) + 1);
  *result = "";

  strlcpy(name,path,strlen(path) + 1);
  for (token = strtok_r (name, "/", &save_ptr); token != NULL;
        token = strtok_r (NULL, "/", &save_ptr)){

      strlcpy(result,token,strlen(token) + 1);
  }
  
  free(name);
  return result;
}
