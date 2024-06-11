/* file.c: Implementation of memory backed file object (mmaped object). */

// File-Backed page를 위한 기능 제공
// File-Backed page를 위한 기능 제공
// File-Backed page를 위한 기능 제공
// File-Backed page를 위한 기능 제공
// File-Backed page를 위한 기능 제공

#include "vm/vm.h"
#include "userprog/syscall.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "threads/mmu.h"

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

/* DO NOT MODIFY this struct */
// file-backed 페이지에 대한 함수 포인터 테이블
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,    // 페이지를 삭제하는 함수
	.type = VM_FILE,
};

/* The initializer of file vm */
void
vm_file_init (void) {
}

/* Initialize the file backed page */
// 예외처리 안해줘도 되나..??????
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	// 핸들러 설정
	page->operations = &file_ops; // swap in/out, destroy 함수가 들어있다
	// 인자로 받은 페이지의 file_page를 가져온다
	struct file_page *file_page = &page->file;

	// 보조 데이터(초기화 정보) 가져온 후 설정하기
	struct lazyLoadArg *aux = (struct lazyLoadArg *)page->uninit.aux;
	file_page->file = aux->file;
	file_page->offset = aux->offset;
	file_page->page_read_bytes = aux->readBytes;

	return true;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
// file-backed page 파괴 함수이다. page 메모리 해제는 caller함수가 한다.
static void
file_backed_destroy (struct page *page) {

	// 인자로 받은 페이지의 file_page 가져오기
	struct file_page *file_page UNUSED = &page->file;
	
	// dirty page인 경우
	if(pml4_is_dirty(thread_current()->pml4, page->va)) {

		// 변경된 파일 데이터를 파일에 덮어쓴다
		file_write_at(file_page->file, page->va, file_page->page_read_bytes, file_page->offset);

		// 변경사항을 적용했으니 dirty bit를 false로 설정한다
		pml4_set_dirty(thread_current()->pml4, page->va, false);
	}

	// 페이지의 물리 프레임 매핑 해제 및 메모리 해제
	if(page->frame) {
		list_remove(&page->frame->frame_elem);
		page->frame->page = NULL;
		page->frame = NULL;
		free(page->frame);
	}

	pml4_clear_page(thread_current()->pml4, page->va);
}

/* Do the mmap */
// fd로 열린 파일의 offset 바이트부터 length 바이트 만큼을 프로세스 va에 매핑한다
// 파일 length가 PGSIZE의 배수가 아닐 경우 마지막 페이지의 나머지 부분은 메모리에 0으로 설정되어 올라가며, stick out 한다고 표현한다.
// 파일이 디스크로 다시 쓰여질 때, 나머지 바이트 0들은 폐기된다.
// 파일이 매핑된 가상 주소의 시작 부분을 return한다.
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {

	// 파일을 length만큼만 읽는다
	
	lock_acquire(&filesys_lock);
	
	// 원본 파일 디스크립터에 독립적인 새로운 파일 디스크립터 만든다.
	struct file *mfile = file_reopen(file);  // 이미 열린 파일을 기반으로 새로운 파일 구조체를 생성

	void *ori_addr = addr; // 파일이 매핑될 가상 주소의 시작점이다.
	size_t read_bytes = (length > file_length(mfile)) ? file_length(mfile) : length;  // 읽어올 크기 지정하기
	size_t zero_bytes = PGSIZE - read_bytes % PGSIZE;								  // 0으로 채울 크기 지정하기

	struct lazyLoadArg *aux;
	// 읽거나 0으로 만들 byte가 남았다면 반복한다
	while(read_bytes > 0 || zero_bytes > 0) {
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		// 보조데이터 구조체 가상메모리 할당
		aux = (struct lazyLoadArg *)malloc(sizeof(struct lazyLoadArg));
		if(!aux) {
			goto err;
		}

		// file-backed page의 lazy_load_segment 함수에 줄 보조 데이터들
		aux->file = mfile;
		aux->offset = offset;
		aux->readBytes = page_read_bytes;

		// File-backed가 될 uninit page 만들기 + spt에 넣기
		if(!vm_alloc_page_with_initializer(VM_FILE, addr, writable, lazy_load_segment, aux)) {
			goto err;
		}

		// 읽거나 0으로 만들 byte 갱신 + 주소와 offset 갱신
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		addr += PGSIZE;					// 여기부터 가상 주소에 할당해라
		offset += page_read_bytes;		// 파일의 이 부분에서부터 읽어와라
	}

	lock_release(&filesys_lock);
	return ori_addr;

err:
	free(aux);
		lock_release(&filesys_lock);
	return NULL;
}

/* Do the munmap */
// addr에 대한 매핑 해제
void
do_munmap (void *addr) {

	struct thread *curr = thread_current();
	struct page *page;

	lock_acquire(&filesys_lock);
	// addr에 해당하는 페이지를 현재 스레드 spt에서 찾는다
	while((page = spt_find_page(&curr->spt, addr))) {

		// 찾았다면 부순다
		destroy(page);
		// 다음 페이지를 확인하러 간다
		addr += PGSIZE;
	}
	lock_release(&filesys_lock);

}
