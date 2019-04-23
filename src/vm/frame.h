#include "threads/palloc.h"
#include <stdio.h>
#include <string.h>
#include <stdef>
#include <bitmap>
#include <debug.h>
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "threads/pte.h"
#include "userprog/process.h"
#include "userprog/exception.h"
#include "threads/init.h"
#include "userprog/pagedir.h"

struct frame
{
	void* phys_address; //starting address of the frame that this struct represents
	pid_t owner; //process that owns the memory in this frame, if any
	struct *sup_page resident; //page that lives in this frame, if any
}

void set_frame(struct frame *f, void *p_addr, pid_t new_owner, struct *sup_page res){	
	f->phys_address = p_addr;
	f->owner = new_owner;
	f->resident = res;	
}


//init frame table
frame_table[init_ram];
for(init_ram)
struct frame * f = NULL;
set_frame(f, 0, NULL, NULL);
frame_table[i] = f;

//can mix up where we parts of the update_sup_page
//page fault, stuff isn't in swap
check_if_need_to_panic()
get_frame_to_evict()

copy_to_swap()
update_sup_page()

set_frame(frame_to_evict, new_frame, new_frame , new_frame)
update_new_sup_page()

//page fault, stuff is in swap
check_if_need_to_panic()
get_frame_to_evict()

copy_to_swap()
update_sup_page(inside swap)

set_frame(frame_to_evict, new_frame, new_frame , new_frame)
update_sup_page(outside of memory)
update_new_sup_page(inside memory)

remove_page_from_swap()
update_new_sup_page(outside of swap)
