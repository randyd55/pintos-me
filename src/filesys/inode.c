#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "devices/block.h"
#include "threads/thread.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44
#define DIRECT_BLOCKS 122
#define SINGLE_BLOCKS 128
#define DOUBLE_BLOCKS 128
#define MAX_FILE_SIZE 16384


/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    off_t length;                         				/* File size in bytes. */
    bool is_directory;
    int entry_cnt;                                /* Number of entries in 
                                                     directory */
    unsigned magic;                       				/* Magic number. */
    block_sector_t direct_blocks[DIRECT_BLOCKS];	/*first 122 direct blocks*/
    block_sector_t singleIB;              				/*Sector location of single 
                                                    indirection block */
    block_sector_t doubleIB;              				/*Sector location of double 
                                                    indirection block */
  };

/*On disk single Indirect block, 
contains 128 direct pointers to file data.*/
struct singleIB
  {
    block_sector_t data_blocks[SINGLE_BLOCKS]; /*location of each data block*/

  };

/*On disk doubly indirect block, contains 128 sector locations of
single index blocks*/
struct doubleIB 
  {
    block_sector_t single_blocks[DOUBLE_BLOCKS]; /* location of each 
                                                        Index Block */               
  }; 

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */
	  
  };

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos)
{
  ASSERT (inode != NULL);
  block_sector_t val;
  val = -1;
  if (pos < inode->data.length){
    //pos is within the first 122 direct blocks
	  if(pos < DIRECT_BLOCKS * BLOCK_SECTOR_SIZE){
      val = inode->data.direct_blocks[pos / BLOCK_SECTOR_SIZE];
	  }
    //pos is within the blocks pointed to by the Single Indirect block
	  else if(pos <= (DIRECT_BLOCKS + SINGLE_BLOCKS) * BLOCK_SECTOR_SIZE){
	  	struct singleIB *single = malloc(sizeof(struct singleIB));
	  	block_read(fs_device, inode->data.singleIB, single);
	  	val = single->data_blocks[pos/BLOCK_SECTOR_SIZE - DIRECT_BLOCKS];
      free(single);
	  }
    //pos is within the double indirect blocks
	  else if(pos <= MAX_FILE_SIZE * BLOCK_SECTOR_SIZE){
	  	struct doubleIB *doubly = malloc(sizeof(struct doubleIB));
    	block_read(fs_device, inode->data.doubleIB, doubly);
    	struct singleIB *single = malloc(sizeof(struct singleIB));
      int num_sectors = pos / BLOCK_SECTOR_SIZE;
      int double_block = num_sectors - (DIRECT_BLOCKS + SINGLE_BLOCKS);
    	int singleNum = (double_block) / DOUBLE_BLOCKS;
    	block_read(fs_device, doubly->single_blocks[singleNum], single);
    	val = single->data_blocks[(double_block) % DOUBLE_BLOCKS];
      free(doubly);
      free(single);
	  }

  }
  return val;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void)
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;
  int currLength = 0;
  ASSERT (length >= 0);
  size_t i;
  int location;
  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);
  disk_inode = calloc (1, sizeof *disk_inode);

  //allocate direct blocks
  int inode_location = 0;
  disk_inode->entry_cnt=0;
  disk_inode->is_directory=false;
  disk_inode->length = length;
  disk_inode->magic = INODE_MAGIC;
 
  //Allocates the direct blocks
  for(i = 0; i < DIRECT_BLOCKS; i++){
    //If we surpass the given length, stop allocating blocks
  	if(currLength >= length){
  		success = true;
      break;
    }
  	else{
  		if(free_map_allocate(1, &location)){
  			disk_inode->direct_blocks[i] = location;
  			currLength += BLOCK_SECTOR_SIZE;
  		}
      //not enough room for file, free the allocated sectors
  		else{
  			inode_create_failure(disk_inode, currLength);
  			return false;
  		}
  		
  	}
  	  
  }
  //allocate sectors on disk for the singleIB and doubleIB
  if(free_map_allocate(1, &location))
  	disk_inode->singleIB = location;
  if(free_map_allocate(1, &location))
  	disk_inode->doubleIB = location;
  block_write (fs_device, sector, disk_inode);
  static char zeros[BLOCK_SECTOR_SIZE];
  //Writes zeros to the spaces allocated for direct blocks
  for(i = 0; i < currLength/BLOCK_SECTOR_SIZE; i++){
  	block_write(fs_device, disk_inode->direct_blocks[i], zeros);
  }
  //If we have reached the files needed size, return
  if(success){
  	return success;
  }
  //build singleIB struct and allocate the blocks it points to
  struct singleIB *singly = malloc(sizeof(struct singleIB));  
  for(i = 0; i < SINGLE_BLOCKS; i++){
      //Enough space has been allocated
  		if(currLength >= length){
  			success = true;
  			break;
  		}
      //allocate blocks and write zeros to them
  		else if(free_map_allocate(1, &location)){
           singly->data_blocks[i] = location;
           currLength += BLOCK_SECTOR_SIZE;
           block_write(fs_device, location, zeros);
  		}
      //not enough room pm disk for file, free the allocated sectors
  		else{
  			inode_create_failure(disk_inode, currLength);
        free(singly);
  			return false;
  		}   
  }
  //write singleIB to disk
  block_write(fs_device, disk_inode->singleIB, singly);
  free(singly);

  if(success){
  	return success;
  }
  struct doubleIB *doubly = malloc(sizeof(struct doubleIB));
  //Build the doubleIB struct and allocate its single Index Blocks
  //and the blocks those point to.
  for(i = 0; i < DOUBLE_BLOCKS; i++){
  	size_t j;
  	struct singleIB *doubly_singly = malloc(sizeof(struct singleIB));  
  	if(!free_map_allocate(1, &location) && !success){
  	 	inode_create_failure(disk_inode, currLength);
  	 	return false;
  	}
  	else if(success)
  	  break;
  	doubly->single_blocks[i] = location;
    //Builds each singleIB struct and allocates its data on disk
  	for(j = 0; j < SINGLE_BLOCKS; j++){  
  		if(currLength >= length){
  		  success = true;
  		}
  		else if(free_map_allocate(1, &location)){
        doubly_singly->data_blocks[i] = location;
        currLength += BLOCK_SECTOR_SIZE;
        block_write(fs_device, location, zeros);
  		}
  		else{
  			inode_create_failure(disk_inode, currLength);
  			return false;
  		}   
  	}
  	block_write(fs_device, doubly->single_blocks[i], doubly_singly);
    free(doubly_singly);
  }
  block_write(fs_device, disk_inode->doubleIB, doubly);
  free(doubly);
  free(disk_inode);
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;
  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e))
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector)
        {
          inode_reopen (inode);
          return inode;
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL){
    return NULL;

  }

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  block_read (fs_device, inode->sector, &inode->data);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk. (Does it?  Check code.)
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode)
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;
  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {

      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);

      /* Deallocate blocks if removed. */
      if (inode->removed)
        {
          //Write inode data back to disk
          block_write(fs_device, inode->sector, &inode->data);
          free_map_release (inode->sector, 1);
          free_map_release (inode->data.direct_blocks[0],
                            bytes_to_sectors (inode->data.length));
        }

      free (inode);
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode)
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset)
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;
  
  while (size > 0)
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Read full sector directly into caller's buffer. */
          block_read (fs_device, sector_idx, buffer + bytes_read);
        }
      else
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          if (bounce == NULL)
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          block_read (fs_device, sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset)
{

  int i;
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;
  if (is_denied(inode))
    return 0;
  int sectors_to_allocate;
  int current_sectors = 0;
  int sectors_after_write = 0;
  //Need to allocate sectors
  if(offset + size > inode->data.length){
    //determines how many sectors the file currently has allocated and the 
    //number of sectors it needs to complete the write
    //if (offset + size) or the file's current length lies on a sector boundary
    //the number of sectors is adjusted accordingly
  	current_sectors = inode->data.length % BLOCK_SECTOR_SIZE != 0 ? 
                      inode->data.length/BLOCK_SECTOR_SIZE + 1 : 
                      inode->data.length/BLOCK_SECTOR_SIZE;
  	sectors_after_write = (offset + size) % BLOCK_SECTOR_SIZE != 0 ? 
                          (offset + size)/BLOCK_SECTOR_SIZE + 1 : 
                          (offset + size)/BLOCK_SECTOR_SIZE; 
  }
  //allocates the sectors needed for the write
  for(i = current_sectors; i < sectors_after_write; i++){
    int size_temp = size; 
  	allocate_sector(i, inode);
  	size_temp -= BLOCK_SECTOR_SIZE;
  }

  inode->data.length = offset + size > inode->data.length ? offset + size : 
                                       inode->data.length;
  block_write(fs_device, inode->sector, &inode->data);

  while (size > 0)
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;
      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0){
        break;
      }

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Write full sector directly to disk. */
          block_write (fs_device, sector_idx, buffer + bytes_written);
        }
      else
        {
          /* We need a bounce buffer. */
          if (bounce == NULL)
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL){
                break;
              }
            }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          if (sector_ofs > 0 || chunk_size < sector_left)
            block_read (fs_device, sector_idx, bounce);
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          block_write (fs_device, sector_idx, bounce);
        }

      /* Advance. */

      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
	}
  free (bounce);
  
  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode)
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode)
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}

/*if inode_create fails due to lack of disk space, this will free the sectors
that have been allocated*/
void 
inode_create_failure(struct inode_disk *d_inode, int length){
	int i = 0;
	//free direct blocks
    while(length > 0 && i < DIRECT_BLOCKS){
    	free_map_release(d_inode->direct_blocks[i], 1);
    	length -= BLOCK_SECTOR_SIZE;
    	i++;
    } 
    if(length <= 0){
    	return;
    }
    //free singleIB
    i = 0;
    struct singleIB *singly = malloc(sizeof(struct singleIB));
    block_read(fs_device, d_inode->singleIB, singly); 
    while(length > 0 && i < SINGLE_BLOCKS){
    	free_map_release(singly->data_blocks[i], 1);
    	length -= BLOCK_SECTOR_SIZE;
    	i++;
    }
    free(singly);
    if(length <= 0){
    	return;
    }
    //free doubleIB
    i = 0;
    struct doubleIB *doubly = malloc(sizeof(struct doubleIB));
    block_read(fs_device, d_inode->doubleIB, doubly);
    while(length > 0 && i < DOUBLE_BLOCKS){
    	int j = 0;
    	struct singleIB *doubly_singly = malloc(sizeof(struct singleIB));
    	block_read(fs_device, doubly->single_blocks[i], doubly_singly); 
    	while(length > 0 && j < SINGLE_BLOCKS){
    		free_map_release(doubly_singly->data_blocks[j], 1);
    		length -= BLOCK_SECTOR_SIZE;
    		i++;
    	}
    	free(doubly_singly);
    }

}

//Allocates a new sector for INODE, SECTOR_IDX is used to determine whether it
//should be a direct block, part of the SingleIB, or part of the doubleIB 
void allocate_sector(int sector_idx, struct inode *inode){
	int location;
	
	if(!free_map_allocate(1, &location)){
		return;
	}
	static char zeros[BLOCK_SECTOR_SIZE];
	if(sector_idx < DIRECT_BLOCKS){
	  	inode->data.direct_blocks[sector_idx] = location;
	  	block_write(fs_device, location, zeros);
	  }
	  else if(sector_idx <= (DIRECT_BLOCKS + SINGLE_BLOCKS)){
	  	 struct singleIB *singly = malloc(sizeof(struct singleIB));
	  	 block_read(fs_device, inode->data.singleIB, singly);
	  	 singly->data_blocks[sector_idx - DIRECT_BLOCKS] = location;
	  	 block_write(fs_device, location, zeros);
	  	 block_write(fs_device, inode->data.singleIB, singly);
       free(singly);
	  }
	  else if(sector_idx <= MAX_FILE_SIZE){
	  	 struct doubleIB *doubly = malloc(sizeof(struct doubleIB));
	  	 block_read(fs_device, inode->data.doubleIB, doubly);
	  	 struct singleIB *singly = malloc(sizeof(struct singleIB));
       int double_block_num = (sector_idx - (DIRECT_BLOCKS + SINGLE_BLOCKS));
	  	 block_read(fs_device, 
                doubly->single_blocks[double_block_num/DOUBLE_BLOCKS], zeros);
	  	 singly->data_blocks[double_block_num % DOUBLE_BLOCKS] = location;
	  	 block_write(fs_device, location, zeros);
	  	 block_write(fs_device, 
                doubly->single_blocks[double_block_num/DOUBLE_BLOCKS], singly);
	  	 block_write(fs_device, inode->data.doubleIB, doubly);
       free(singly);
       free(doubly);
	  }
	  else{
	  	return;
	  }

	return;
}
//sets the INODE to be a directory
void inode_set_dir(struct inode* inode){
  inode->data.is_directory=true;
}
//returns true if INODE represents a directory
bool inode_is_dir(struct inode* inode){
  return inode->data.is_directory;
}

//Returns true if INODE is not writeable
bool is_denied(struct inode* inode){
  return inode->deny_write_cnt > 0;
}

//Returns the deny_cnt of INODE
int deny_cnt(struct inode* inode){
  return inode->deny_write_cnt;
}
//increments INODE's entry count
void add_entry(struct inode* inode){
  inode->data.entry_cnt++;
}
//decrements INODE's entry count
void remove_entry(struct inode* inode){
  inode->data.entry_cnt--;
}
//returns INODE's entry count
int entry_cnt(struct inode* inode){
  return inode->data.entry_cnt;
}

