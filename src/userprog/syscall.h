#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/thread.h"
#include "threads/interrupt.h"
#include "threads/synch.h"
#include "devices/shutdown.h"

//Chineye Driving
struct lock filesys_lock;
struct lock create_lock;
/*lock for extending the file*/
struct lock filesys_extending_lock;

int wait(pid_t pid);

void syscall_init (void);
void halt(void);

void exit(int status);

pid_t exec(const char *cmd_line);

bool create(const char *file, unsigned intial_size);

bool remove(const char *file);

int open(const char *file);

int filesize(int fd);

int read(int fd, const void *buffer, unsigned size);

int write(int fd, const void *buffer, unsigned size);

void seek(int fd, unsigned position);

unsigned tell(int fd);

void close(int fd);

int getFd(void);

bool check_pointer(uint32_t * stack_ptr);

//filesys commands
bool chdir (const char *dir);
bool mkdir (const char *dir);
bool readdir (int fd, char *name);
bool isdir (int fd);
int inumber (int fd);


//Chineye Done
#endif /* userprog/syscall.h */
