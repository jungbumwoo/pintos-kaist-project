#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_create_initd (const char *file_name);
tid_t process_fork (const char *name, struct intr_frame *if_);
int process_exec (void *f_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (struct thread *next);
bool lazy_load_segment (struct page *page, void *aux);

// Project 2-3 Parent child
struct thread *get_child_with_pid(int pid);

// Project 3-1
bool install_page (void *upage, void *kpage, bool writable); // process.c에 구현되어져 있는건데 vm.c에서 쓰려고 옮겨봄

#endif /* userprog/process.h */
