#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/mmu.h"
#include "threads/vaddr.h"
#include "intrinsic.h"
#include "userprog/syscall.h"
#ifdef VM
#include "vm/vm.h"
#include "userprog/syscall.h"
#endif

static void process_cleanup (void);
static bool load (const char *file_name, struct intr_frame *if_);
static void initd (void *f_name);
static void __do_fork (void *);
static void argument_stack(char *argv[], int argc, struct intr_frame *if_);//추가부분



/* General process initializer for initd and other process. */
static void
process_init (void) {
	struct thread *current = thread_current ();
}

/* Starts the first userland program, called "initd", loaded from FILE_NAME.
 * The new thread may be scheduled (and may even exit)
 * before process_create_initd() returns. Returns the initd's
 * thread id, or TID_ERROR if the thread cannot be created.
 * Notice that THIS SHOULD BE CALLED ONCE. */
/**
 * file_name을 인자로 받음
 * 새로운 스레드를 생성(tid)
 * tid 반환
*/
tid_t
process_create_initd (const char *file_name) {
	char *fn_copy;
	tid_t tid;

	/* Make a copy of FILE_NAME.
	 * Otherwise there's a race between the caller and load(). */
	//kernel공간에서 사용할 4KB 크기의 단일 페이지를 할당
	fn_copy = palloc_get_page(0);
	if (fn_copy == NULL)
		return TID_ERROR;
	strlcpy(fn_copy, file_name, PGSIZE);//fn_copy에 file_name을 복사. PGSIZE: 4KB

	char *save_ptr;//strtok_r을 사용하기 위한 변수
	// " "이전의 문자열(1st token)를 file_name에 저장
	strtok_r(file_name, " ", &save_ptr);

	/* Create a new thread to execute FILE_NAME. */
	tid = thread_create(file_name, PRI_DEFAULT, initd, fn_copy);
	//수정. error 반환
	if (tid == TID_ERROR){
		palloc_free_page(fn_copy);
		return TID_ERROR;
	}
	return tid;
}

/* A thread function that launches first user process. */
static void
initd (void *f_name) {
#ifdef VM
	supplemental_page_table_init (&thread_current ()->spt);
#endif

	process_init ();

	if (process_exec (f_name) < 0)
		PANIC("Fail to launch initd\n");
	NOT_REACHED ();
}

/* Clones the current process as `name`. Returns the new process's thread id, or
 * TID_ERROR if the thread cannot be created. 현재 스레드를 복사, 자식 스레드를 생성 */
tid_t
process_fork (const char *name, struct intr_frame *if_ UNUSED) {
	/* Clone current thread to new thread.*/
	// 현재 스레드의 parent_if에 복제해야 하는 if를 복사한다.
	struct thread *cur = thread_current();
	memcpy(&cur->parent_if, if_, sizeof(struct intr_frame));

	// 현재 스레드를 fork한 new 스레드를 생성한다.
	tid_t pid = thread_create(name, PRI_DEFAULT, __do_fork, cur);
	if (pid == TID_ERROR)
		return TID_ERROR;

	// 자식이 로드될 때까지 대기하기 위해서 방금 생성한 자식 스레드를 찾는다.
	struct thread *child = get_child_process(pid);

	// 현재 스레드는 생성만 완료된 상태이다. 생성되어서 ready_list에 들어가고 실행될 때 __do_fork 함수가 실행된다.
	// __do_fork 함수가 실행되어 자식load가 완료될 때까지 부모는 대기
	sema_down(&child->load_sema);

	// 자식이 로드되다가 오류로 exit한 경우
	if (child->exit_status == TID_ERROR)
	{
		return TID_ERROR;
	}

	// 자식 프로세스의 pid를 반환한다.
	return pid;
}

#ifndef VM
/* Duplicate the parent's address space by passing this function to the
 * pml4_for_each. This is only for the project 2. */
static bool
duplicate_pte (uint64_t *pte, void *va, void *aux) {//부모의 VM 공간을 복사
	struct thread *current = thread_current();
	struct thread *parent = (struct thread *)aux;//부모 스레드
	void *parent_page;
	void *newpage;
	bool writable;

	/* 1. TODO: If the parent_page is kernel page, then return immediately. */
	if (is_kernel_vaddr(va))
		return true;

	/* 2. Resolve VA from the parent's page map level 4. */
	parent_page = pml4_get_page(parent->pml4, va);//
	if (parent_page == NULL)
		return false;

	/* 3. TODO: Allocate new PAL_USER page for the child and set result to
	 *    TODO: NEWPAGE. */
	newpage = palloc_get_page(PAL_USER | PAL_ZERO);//자식에게 새로운 페이지 할당
	if (newpage == NULL)
		return false;

	/* 4. TODO: Duplicate parent's page to the new page and
	 *    TODO: check whether parent's page is writable or not (set WRITABLE
	 *    TODO: according to the result). */
	memcpy(newpage, parent_page, PGSIZE);
	writable = is_writable(pte);

	/* 5. Add new page to child's page table at address VA with WRITABLE
	 *    permission. */
	if (!pml4_set_page(current->pml4, va, newpage, writable))
	{
		/* 6. TODO: if fail to insert page, do error handling. */
		return false;
	}
	return true;
}
#endif

/* A thread function that copies parent's execution context.
 * Hint) parent->tf does not hold the userland context of the process.
 *       That is, you are required to pass second argument of process_fork to
 *       this function. */
static void
__do_fork (void *aux) {
	struct intr_frame if_;
	struct thread *parent = (struct thread *) aux;// 부모 프로세스의 정보
	struct thread *current = thread_current ();// 현재 실행 중인 자식 프로세스
	/* TODO: somehow pass the parent_if. (i.e. process_fork()'s if_) */
	struct intr_frame *parent_if = &parent->parent_if;//부모thread의 if를 가져옴
	bool success = true;

	/* 1. Read the cpu context to local stack. */
	memcpy (&if_, parent_if, sizeof (struct intr_frame));// 부모의 if를 자식의 로컬 스택에 복사
	if_.R.rax = 0;//자식 프로세스의 return값은 0

	/* 2. Duplicate PT */
	current->pml4 = pml4_create();
	if (current->pml4 == NULL)
		goto error;

	process_activate (current);//자식 프로세스의 pml4를 활성화
/**
 * VM을 사용하는 경우 보조 페이지 테이블을 초기화하고 복사.
 * 그렇지 않은 경우 페이지 테이블 항목을 복사.
 */	
#ifdef VM
	supplemental_page_table_init (&current->spt);
	if (!supplemental_page_table_copy (&current->spt, &parent->spt))
		goto error;
#else
	if (!pml4_for_each (parent->pml4, duplicate_pte, parent))
		goto error;
#endif

	/* TODO: Your code goes here.
	 * TODO: Hint) To duplicate the file object, use `file_duplicate`
	 * TODO:       in include/filesys/file.h. Note that parent should not return
	 * TODO:       from the fork() until this function successfully duplicates
	 * TODO:       the resources of parent
	 * */
	 //부모의fdt를 자식에 복사
	for (int i = 0; i < 128; i++)
	{
		struct file *file = parent->fdt[i];
		if (file == NULL)
			continue;
		if (file > 2)//stdin, sdtout, stderr 제외
			file = file_duplicate(file);
		current->fdt[i] = file;
	}
	current->next_fd = parent->next_fd;

	// 로드가 완료될 때까지 기다리고 있던 부모 대기 해제
	sema_up(&current->load_sema);
	process_init();//자식 프로세스 초기화

	/* Finally, switch to the newly created process. */
	if (success)
		do_iret (&if_);//성공 시 자식의 if 사용, 실행을 시작
error:
	// 에러가 발생하면 부모 프로세스를 해제하고 자식 프로세스를 종료
	sema_up(&current ->load_sema);//로드가 완료될 때까지 기다리고 있던 부모 대기 해제
	exit(TID_ERROR);//fork 실패 시 exit
	// thread_exit ();
}

/* Switch the current execution context to the f_name.
 * Returns -1 on fail. 
 * 인자로 들어오는 f_name을 parsing. user stack에 매개변수를 push.
 */

int process_exec(void *f_name)
{ 
	char *file_name = f_name;
	bool success;

	/* We cannot use the intr_frame in the thread structure.
	 * This is because when current thread rescheduled,
	 * it stores the execution information to the member. */
	struct intr_frame _if;
	_if.ds = _if.es = _if.ss = SEL_UDSEG;
	_if.cs = SEL_UCSEG;
	_if.eflags = FLAG_IF | FLAG_MBS;

	/* We first kill the current context */
	process_cleanup();
	char *token, *save_ptr;
    char *argv[128];
    int argc = 0;
    for (token = strtok_r(file_name, " ", &save_ptr); token != NULL; token = strtok_r(NULL, " ", &save_ptr)) {
        argv[argc++] = token;
    }
	/* And then load the binary */
	lock_acquire(&filesys_lock);
	success = load(file_name, &_if);
	lock_release(&filesys_lock);
	// 이진 파일을 디스크에서 메모리로 로드한다.
	// 이진 파일에서 실행하려는 명령의 위치를 얻고 (if_.rip)
	// user stack의 top 포인터를 얻는다. (if_.rsp)
	// 위 과정을 성공하면 실행을 계속하고, 실패하면 스레드가 종료된다.

	/* If load failed, quit. */
	if (!success)
	{
		palloc_free_page(file_name);
		return -1;
	}

	// 함수 내부에서 parse와 rsp의 값을 직접 변경하기 위해 주소 전달
	argument_stack(&argv, argc, &_if);

	// hex_dump(_if.rsp, _if.rsp, USER_STACK - (uint64_t)_if.rsp, true); // user stack을 16진수로 프린트

	palloc_free_page(file_name);

	/* Start switched process. */
	do_iret(&_if);
	NOT_REACHED();
}


/* Waits for thread TID to die and returns its exit status.  If
 * it was terminated by the kernel (i.e. killed due to an
 * exception), returns -1.  If TID is invalid or if it was not a
 * child of the calling process, or if process_wait() has already
 * been successfully called for the given TID, returns -1
 * immediately, without waiting.
 *
 * This function will be implemented in problem 2-2.  For now, it
 * does nothing. */
int
process_wait (tid_t child_tid UNUSED) {
	/* XXX: Hint) The pintos exit if process_wait (initd), we recommend you
	 * XXX:       to add infinite loop here before
	 * XXX:       implementing the process_wait. */
	struct thread *child = get_child_process(child_tid);
    if (child == NULL) // 1) 자식이 아니면 -1을 반환한다.
        return -1;

    // 2) 자식이 종료될 때까지 대기한다. (process_exit에서 자식이 종료될 때 sema_up 해줄 것이다.)
    sema_down(&child->wait_sema);
    // 3) 자식이 종료됨을 알리는 `wait_sema` signal을 받으면 현재 스레드(부모)의 자식 리스트에서 제거한다.
    list_remove(&child->child_elem);
    // 4) 자식이 완전히 종료되고 스케줄링이 이어질 수 있도록 자식에게 signal을 보낸다.
    sema_up(&child->exit_sema);

    return child->exit_status; // 5) 자식의 exit_status를 반환한다.
}

/* Exit the process. This function is called by thread_exit (). 
 * 자원 해제 순서: 파일 닫기 -> fdt 해제 -> 현재 실행 파일 닫기
*/
void
process_exit (void) {
	 struct thread *curr = thread_current();
	 //fdt의 모든 파일을 닫음.
		for(int i = 2; i< 128; i++){
		if(curr->fdt[i] != NULL)close(i);//파일 닫기
	 }
	 if(curr->fdt != NULL) palloc_free_page(curr->fdt);//fdt 해제. multi-oom?
	 if(curr->running != NULL) file_close(curr->running);//현재 실행 중인 파일도 닫음.

	process_cleanup ();
	//종료 중인 스레드가 자신의 종료를 기다리고 있는 부모 스레드에게 sig를 보냄. 부모가 대기에서 깨어남.
	sema_up(&curr->wait_sema);
	// 부모의 sign 기다림. 대기가 풀리고 나서 do_schedule(THREAD_DYING)이 이어져 다른 스레드가 실행된다.
	sema_down(&curr->exit_sema);

}

/* Free the current process's resources. */
static void
process_cleanup (void) {
	struct thread *curr = thread_current ();

#ifdef VM
	supplemental_page_table_kill (&curr->spt);
#endif

	uint64_t *pml4;
	/* Destroy the current process's page directory and switch back
	 * to the kernel-only page directory. */
	pml4 = curr->pml4;
	if (pml4 != NULL) {
		/* Correct ordering here is crucial.  We must set
		 * cur->pagedir to NULL before switching page directories,
		 * so that a timer interrupt can't switch back to the
		 * process page directory.  We must activate the base page
		 * directory before destroying the process's page
		 * directory, or our active page directory will be one
		 * that's been freed (and cleared). */
		curr->pml4 = NULL;
		pml4_activate (NULL);
		pml4_destroy (pml4);
	}
}

/* Sets up the CPU for running user code in the nest thread.
 * This function is called on every context switch. */
void
process_activate (struct thread *next) {
	/* Activate thread's page tables. */
	pml4_activate (next->pml4);

	/* Set thread's kernel stack for use in processing interrupts. */
	tss_update (next);
}

//추가한 부분. parse: parsing된 문자열, count: parsing된 문자열의 개수, rsp: user stack의 top 포인터
// void argument_stack(char **parse, int count, void **rsp) // 주소를 전달받았으므로 이중 포인터 사용
static void argument_stack(char *argv[], int argc, struct intr_frame *if_)
{
	
    uintptr_t argv_addr[argc];
    // // USER_STACK;
    // USER_STACK = 0x47480000 따라서 여기서부터 빼주면 되겠네
    // Push the address of each string plus a null pointer sentinel, on the stack, in right-to-left order.
    // 위의 제약조건 때문에 i를 0부터 하지 않고, 끝에서부터 (argc-1) 부터 시작.
    for(int i = argc-1; i >= 0; i--) {
        size_t len = strlen(argv[i]) + 1;  // Null 종단자 포함해야함. 따라서 +1
        if_->rsp -= len;
        memcpy(if_->rsp, argv[i], len);
        argv_addr[i] = if_->rsp;
    }
    /* uint8_t 타입은 워드 정렬과 관련된 용도로 일반적으로 사용되는 타입입니다.
    따라서 코드 가독성을 위해 uint8_t 타입을 사용하는 것이 좋습니다. */
    /* Word-aligned accesses are faster than unaligned accesses,
    so for best performance round the stack pointer down to a multiple of 8 before the first push. */
    /* | Name       | Data |  Type      | */
    /* | word-align |   0  |  uint8_t[] | */
    while ( (if_->rsp % 8) != 0  ) { // 8배수 패딩
        if_->rsp--;
        memset(if_->rsp, 0, sizeof(uint8_t)); // 그렇다면, 패딩 부분도 모두 0으로 만들어야 푸쉬가 제대로 되는건가..?
    }
    for (int i=argc; i>=0; i--) {
        if_->rsp -= sizeof(uintptr_t);
        if (i == argc) {
            memset(if_->rsp,0, sizeof( uintptr_t) );
        }
        else {
            memcpy(if_->rsp, &argv_addr[i], sizeof( uintptr_t));
        }
    }
    /* 4 번 단계 */
    if_->R.rsi = if_->rsp;
    if_->R.rdi = argc;
    /* 5번 단계 */
    if_->rsp -= sizeof( uintptr_t);
    memset(if_->rsp, 0, sizeof( uintptr_t));
}

/* We load ELF binaries.  The following definitions are taken
 * from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
#define EI_NIDENT 16

#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
 * This appears at the very beginning of an ELF binary. */
struct ELF64_hdr {
	unsigned char e_ident[EI_NIDENT];
	uint16_t e_type;
	uint16_t e_machine;
	uint32_t e_version;
	uint64_t e_entry;
	uint64_t e_phoff;
	uint64_t e_shoff;
	uint32_t e_flags;
	uint16_t e_ehsize;
	uint16_t e_phentsize;
	uint16_t e_phnum;
	uint16_t e_shentsize;
	uint16_t e_shnum;
	uint16_t e_shstrndx;
};

struct ELF64_PHDR {
	uint32_t p_type;
	uint32_t p_flags;
	uint64_t p_offset;
	uint64_t p_vaddr;
	uint64_t p_paddr;
	uint64_t p_filesz;
	uint64_t p_memsz;
	uint64_t p_align;
};

/* Abbreviations */
#define ELF ELF64_hdr
#define Phdr ELF64_PHDR

static bool setup_stack (struct intr_frame *if_);
static bool validate_segment (const struct Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes,
		bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
 * Stores the executable's entry point into *RIP
 * and its initial stack pointer into *RSP.
 * Returns true if successful, false otherwise. */
static bool
load (const char *file_name, struct intr_frame *if_) {
	struct thread *t = thread_current ();
	struct ELF ehdr;
	struct file *file = NULL;
	off_t file_ofs;
	bool success = false;
	int i;

	/* Allocate and activate page directory. */
	t->pml4 = pml4_create ();//pml4테이블: 64비트 주소 공간을 512개의 64비트 엔트리로 나누어 관리
	if (t->pml4 == NULL)
		goto done;
	process_activate (thread_current ());

	/* Open executable file. */
	file = filesys_open (file_name);
	if (file == NULL) {
		printf ("load: %s: open failed\n", file_name);
		goto done;
	}
	/* 추가한 부분.
	 * load()를 한 후 실행한 파일의 쓰기 권한을 deny_write해야 함.
	 * rox
	 */
    t->running = file;
    file_deny_write(file);

	/* Read and verify executable header. */
	if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
			|| memcmp (ehdr.e_ident, "\177ELF\2\1\1", 7)
			|| ehdr.e_type != 2
			|| ehdr.e_machine != 0x3E // amd64
			|| ehdr.e_version != 1
			|| ehdr.e_phentsize != sizeof (struct Phdr)
			|| ehdr.e_phnum > 1024) {
		printf ("load: %s: error loading executable\n", file_name);
		goto done;
	}

	/* Read program headers. */
	file_ofs = ehdr.e_phoff;
	for (i = 0; i < ehdr.e_phnum; i++) {
		struct Phdr phdr;

		if (file_ofs < 0 || file_ofs > file_length (file))
			goto done;
		file_seek (file, file_ofs);

		if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
			goto done;
		file_ofs += sizeof phdr;
		switch (phdr.p_type) {
			case PT_NULL:
			case PT_NOTE:
			case PT_PHDR:
			case PT_STACK:
			default:
				/* Ignore this segment. */
				break;
			case PT_DYNAMIC:
			case PT_INTERP:
			case PT_SHLIB:
				goto done;
			case PT_LOAD:
				if (validate_segment (&phdr, file)) {
					bool writable = (phdr.p_flags & PF_W) != 0;
					uint64_t file_page = phdr.p_offset & ~PGMASK;
					uint64_t mem_page = phdr.p_vaddr & ~PGMASK;
					uint64_t page_offset = phdr.p_vaddr & PGMASK;
					uint32_t read_bytes, zero_bytes;
					if (phdr.p_filesz > 0) {
						/* Normal segment.
						 * Read initial part from disk and zero the rest. */
						read_bytes = page_offset + phdr.p_filesz;
						zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
								- read_bytes);
					} else {
						/* Entirely zero.
						 * Don't read anything from disk. */
						read_bytes = 0;
						zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
					}
					if (!load_segment (file, file_page, (void *) mem_page,
								read_bytes, zero_bytes, writable))
						goto done;
				}
				else
					goto done;
				break;
		}
	}

	/* Set up stack. */
	if (!setup_stack (if_))
		goto done;

	/* Start address. */
	if_->rip = ehdr.e_entry;

	/* TODO: Your code goes here.
	 * TODO: Implement argument passing (see project2/argument_passing.html). */

	success = true;

done:
	/* We arrive here whether the load is successful or not. */
	//여기에서 file_close()를 해주면 file이 닫히면서 lock이 풀린다.
	// file_close (file);
	return success;
}


/* Checks whether PHDR describes a valid, loadable segment in
 * FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Phdr *phdr, struct file *file) {
	/* p_offset and p_vaddr must have the same page offset. */
	if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK))
		return false;

	/* p_offset must point within FILE. */
	if (phdr->p_offset > (uint64_t) file_length (file))
		return false;

	/* p_memsz must be at least as big as p_filesz. */
	if (phdr->p_memsz < phdr->p_filesz)
		return false;

	/* The segment must not be empty. */
	if (phdr->p_memsz == 0)
		return false;

	/* The virtual memory region must both start and end within the
	   user address space range. */
	if (!is_user_vaddr ((void *) phdr->p_vaddr))
		return false;
	if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
		return false;

	/* The region cannot "wrap around" across the kernel virtual
	   address space. */
	if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
		return false;

	/* Disallow mapping page 0.
	   Not only is it a bad idea to map page 0, but if we allowed
	   it then user code that passed a null pointer to system calls
	   could quite likely panic the kernel by way of null pointer
	   assertions in memcpy(), etc. */
	if (phdr->p_vaddr < PGSIZE)
		return false;

	/* It's okay. */
	return true;
}

#ifndef VM
/* Codes of this block will be ONLY USED DURING project 2.
 * If you want to implement the function for whole project 2, implement it
 * outside of #ifndef macro. */

/* load() helpers. */
static bool install_page (void *upage, void *kpage, bool writable);

/* Loads a segment starting at offset OFS in FILE at address
 * UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
 * memory are initialized, as follows:
 *
 * - READ_BYTES bytes at UPAGE must be read from FILE
 * starting at offset OFS.
 *
 * - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.
 *
 * The pages initialized by this function must be writable by the
 * user process if WRITABLE is true, read-only otherwise.
 *
 * Return true if successful, false if a memory allocation error
 * or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes, bool writable) {
	ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
	ASSERT (pg_ofs (upage) == 0);
	ASSERT (ofs % PGSIZE == 0);

	file_seek (file, ofs);
	while (read_bytes > 0 || zero_bytes > 0) {
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* Get a page of memory. */
		uint8_t *kpage = palloc_get_page (PAL_USER);
		if (kpage == NULL)
			return false;

		/* Load this page. */
		if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes) {
			palloc_free_page (kpage);
			return false;
		}
		memset (kpage + page_read_bytes, 0, page_zero_bytes);

		/* Add the page to the process's address space. */
		if (!install_page (upage, kpage, writable)) {
			printf("fail\n");
			palloc_free_page (kpage);
			return false;
		}

		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		upage += PGSIZE;
	}
	return true;
}

/* Create a minimal stack by mapping a zeroed page at the USER_STACK */
static bool
setup_stack (struct intr_frame *if_) {
	uint8_t *kpage;
	bool success = false;

	kpage = palloc_get_page (PAL_USER | PAL_ZERO);
	if (kpage != NULL) {
		success = install_page (((uint8_t *) USER_STACK) - PGSIZE, kpage, true);
		if (success)
			if_->rsp = USER_STACK;
		else
			palloc_free_page (kpage);
	}
	return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
 * virtual address KPAGE to the page table.
 * If WRITABLE is true, the user process may modify the page;
 * otherwise, it is read-only.
 * UPAGE must not already be mapped.
 * KPAGE should probably be a page obtained from the user pool
 * with palloc_get_page().
 * Returns true on success, false if UPAGE is already mapped or
 * if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable) {
	struct thread *t = thread_current ();

	/* Verify that there's not already a page at that virtual
	 * address, then map our page there. */
	return (pml4_get_page (t->pml4, upage) == NULL
			&& pml4_set_page (t->pml4, upage, kpage, writable));
}
#else
/* From here, codes will be used after project 3.
 * If you want to implement the function for only project 2, implement it on the
 * upper block. */

// page fault가 발생했을 때 lazy load하는 함수
// 디스크에서 메모리로 데이터를 load한다
bool
lazy_load_segment (struct page *page, void *aux) {
	/* TODO: Load the segment from the file */
	/* TODO: This called when the first page fault occurs on address VA. */
	/* TODO: VA is available when calling this function. */
	// VA에서 첫 page fault가 발생할 때 호출된다.
	// VA와 매핑된 프레임에서 세그먼트(데이터) 로드하는 함수
	// load를 지연시키다가 page fault가 발생했을 때 로드하므로 lazy_load_segment 인 것이다.

	// aux로 받은 읽어올 파일정보 가져오기
	struct lazyLoadArg *lazyLoadArg = (struct lazyLoadArg *)aux;

	// 파일 position을 offset으로 지정한다 (offset부터 읽는다)
	file_seek(lazyLoadArg->file, lazyLoadArg->offset);

	// 파일을 read_bytes만큼 프레임에 읽어온다
	if(file_read(lazyLoadArg->file, page->frame->kva, lazyLoadArg->readBytes) != (int)lazyLoadArg->readBytes) {

		// 제대로 읽어오지 못했다면 frame을 free 해준다
		// free라는 건 프레임과의 매핑정보 해제와, 프레임을 free상태로 전환하는 것을 의미한다.
		// 쓰여진 데이터를 지우진 않는다.
		palloc_free_page(page->frame->kva);
		return false;
	}

	// 읽어온 데이터 바로 뒷 부분부터 zerobytes 만큼 0으로 채운다
	memset(page->frame->kva + lazyLoadArg->readBytes, 0, lazyLoadArg->zeroBytes);

	return true;
}

/* Loads a segment starting at offset OFS in FILE at address
 * UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
 * memory are initialized, as follows:
 *
 * - READ_BYTES bytes at UPAGE must be read from FILE
 * starting at offset OFS.
 *
 * - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.
 *
 * The pages initialized by this function must be writable by the
 * user process if WRITABLE is true, read-only otherwise.
 *
 * Return true if successful, false if a memory allocation error
 * or disk read error occurs. */
// 파일의 특정 offset에서 시작하는 세그먼트를 메모리에 로드한다.
// 주어진 offset부터 시작해서, read_bytes 만큼 데이터를 읽고, zero_bytes 만큼의 메모리를 0으로 초기화한다.
// 얘는 lazy loading 아니고, 프로그램 로드 초기에 필요한 데이터를 메모리에 로드할 때 사용된다.
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes, bool writable) {
	ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
	ASSERT (pg_ofs (upage) == 0);
	ASSERT (ofs % PGSIZE == 0);

	// 페이지 단위로, 파일을 읽어올 메타데이터를 첨부한다
	// read_byte 또는 zero_byte가 남아있다면 루프를 돈다
	while (read_bytes > 0 || zero_bytes > 0) {
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */

		// 읽어야 할 바이트 수, 0으로 초기화할 바이트 수 계산
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* TODO: Set up aux to pass information to the lazy_load_segment. */
		// lazy_load_segment 함수 인자로 전달할 구조체 만들기
		struct lazyLoadArg *lazyLoadArg = (struct lazyLoadArg *)malloc(sizeof(struct lazyLoadArg));
		lazyLoadArg->file = file;
		lazyLoadArg->offset = ofs;
		lazyLoadArg->readBytes = page_read_bytes;
		lazyLoadArg->zeroBytes = page_zero_bytes;


		// 실행 파일은 수정할 필요가 없기 때문에 ANON으로 한다
		// 프로그램의 기능을 이용하고자 하는 것이지 수정하고자 여는 것이 아니기 때문이다
		// file-backed로 하면 실행파일이 변경/수정될 수도 있고, 프로그램 기능을 이용하려는 취지에 맞지 않기 때문에 ANON으로 하는게 나은 것 같다.
		// file-backed는 mmap으로 매핑된 파일들에 대해서 할당하면 된다
		if (!vm_alloc_page_with_initializer (VM_ANON, upage,
					writable, lazy_load_segment, lazyLoadArg))
			return false;

		/* Advance. */
		// 읽은 바이트 수와 0으로 초기화한 바이트 수를 감소시킨다.
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;

		// 읽거나 0으로 만든 byte만큼 offset을 뒤로 민다
		ofs += page_read_bytes;
		// 다음 페이지로 이동한다.
		upage += PGSIZE;

	}
	return true;
}

/* Create a PAGE of stack at the USER_STACK. Return true on success. */
// 사용자 스택 페이지 생성 및 초기화
static bool
setup_stack (struct intr_frame *if_) {

	bool success = false;

	// 스택은 아래로 성장하므로, 스택 시작주소에서 PGSIZE를 뺀 부분을 stack_bottom으로 설정한다.
	// stack_bottom은 첫 번째 페이지가 할당될 시작 주소를 의미한다.
	void *stack_bottom = (void *) (((uint8_t *) USER_STACK) - PGSIZE);
	
	/* TODO: Map the stack on stack_bottom and claim the page immediately.
	 * TODO: If success, set the rsp accordingly.
	 * TODO: You should mark the page is stack. */
	/* TODO: Your code goes here */

	// 스택 페이지 식별자 추가해서 stack_bottom에 페이지 하나 할당받기.
	// 성공하면 true, 실패하면 false 리턴한다
	if(vm_alloc_page(VM_ANON | VM_MARKER_0, stack_bottom, 1)) {
		
		// 스택 페이지 할당 성공했으면 즉시 물리 프레임 매핑해준다.
		// 프레임과 연결해서, page table에 넣고, swap_in 호출해서 메모리에 올린다.
		if(success = vm_claim_page(stack_bottom)) {
	
			// rsp 변경해주기
			// 사용할 수 있는 페이지+메모리 할당했으니, USER_STACK부분부터 최대 stack_bottom 위치까지 rsp를 줄이면서 메모리에 값을 쓸 수 있다.
			if_->rsp = USER_STACK;
			thread_current()->stack_bottom = stack_bottom;
		}
	}

	return success;
}
#endif /* VM */

/**
 * 현재 스레드의 fdt에 현재 파일을 추가한다.
 * 스레드에 저장되어 있는 next_fd부터 탐색을 시작해 LIMIT 전까지 탐색해서 빈 자리에 할당한다.
 * 할당을 성공했으면 fd를, 실패했으면 -1을 리턴한다.
*/
int process_add_file(struct file *f)
{
	struct thread *curr = thread_current();
	struct file **fdt = curr->fdt;

	// limit을 넘지 않는 범위 안에서 빈 자리 탐색
	while (curr->next_fd < 128 && fdt[curr->next_fd]){
		curr->next_fd++;
	}
	if (curr->next_fd >= 128)
		return -1;
	fdt[curr->next_fd] = f;

	return curr->next_fd;
}

//스레드가 가진 파일 디스크립터 테이블 fdt에서 fd에 해당하는 파일을 리턴하는 함수
struct file *process_get_file(int fd)
{
	struct thread *curr = thread_current();
	struct file **fdt = curr->fdt;
	/* 파일 디스크립터에 해당하는 파일 객체를 리턴 */
	/* 없을 시 NULL 리턴 */
	if (fd < 2 || fd >= 128)
		return NULL;
	return fdt[fd];
}

// // 파일 디스크립터 테이블에서 파일 객체를 제거하는 함수
void process_close_file(int fd)
{
	struct thread *curr = thread_current();
	struct file **fdt = curr->fdt;
	if (fd < 2 || fd >= 128)
		return NULL;
	fdt[fd] = NULL;
}

// 자식 리스트에서 원하는 프로세스를 검색하는 함수
struct thread *get_child_process(int pid)
{
	/* 자식 리스트에 접근하여 프로세스 디스크립터 검색 */
	struct thread *cur = thread_current();
	struct list *child_list = &cur->child_list;
	for (struct list_elem *e = list_begin(child_list); e != list_end(child_list); e = list_next(e))
	{
		struct thread *t = list_entry(e, struct thread, child_elem);
		/* 해당 pid가 존재하면 프로세스 디스크립터 반환 */
		if (t->tid == pid)
			return t;
	}
	/* 리스트에 존재하지 않으면 NULL 리턴 */
	return NULL;
}