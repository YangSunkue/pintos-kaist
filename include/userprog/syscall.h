#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include "threads/synch.h"

void syscall_init (void);

#include "include/filesys/off_t.h";
#include "stddef.h";

void check_valid_buffer(void *buffer, size_t size, bool writable);
void *mmap(void *addr, size_t length, int writable, int fd, off_t offset);
void munmap(void *addr);
struct lock filesys_lock;//file system을 위한 lock
#endif /* userprog/syscall.h */
