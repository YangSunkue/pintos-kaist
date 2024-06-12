#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_create_initd (const char *file_name);
tid_t process_fork (const char *name, struct intr_frame *if_);
int process_exec (void *f_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (struct thread *next);

/*추가한 부분
* parse: 파싱된 인자들, count: 인자의 개수, rsp: 스택 포인터를 가리키는 주소
*/
// void argument_stack(char **parse, int count, void **rsp);
static void argument_stack(char *argv[], int argc, struct intr_frame *if_);
int process_add_file(struct file *f);
struct file *process_get_file(int fd);
void process_close_fid(int fd);
struct thread *get_child_process(int pid);

//추가한 부분
int process_add_file(struct file *f);
struct file *process_get_file(int fd);
void process_close_file(int fd);
struct thread *get_child_process(int pid);

bool lazy_load_segment (struct page *page, void *aux);

// load_segment에서 lazy_load_segment로 전달할 데이터들
struct lazyLoadArg {
    struct file *file;  // 읽어올 파일
    off_t offset;       // 파일 position (읽기 시작할 부분)
    uint32_t readBytes; // 읽어올 바이트
    uint32_t zeroBytes; // 0으로 채울 바이트 ( 읽어와서 페이지 단위로 메모리에 할당하고 남은 부분을 0으로 채움 )
};

#define STDIN 1
#define STDOUT 2
#define STDERR 3

#endif /* userprog/process.h */
