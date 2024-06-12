#ifndef VM_VM_H
#define VM_VM_H
#include <stdbool.h>
#include "threads/palloc.h"
#include <hash.h>

enum vm_type
{
	/* page not initialized */
	VM_UNINIT = 0,
	/* page not related to the file, aka anonymous page */
	VM_ANON = 1,
	/* page that realated to the file */
	VM_FILE = 2,
	/* page that hold the page cache, for project 4 */
	VM_PAGE_CACHE = 3,

	/* Bit flags to store state */

	/* Auxillary bit flag marker for store information. You can add more
	 * markers, until the value is fit in the int. */
	VM_MARKER_0 = (1 << 3), // 스택이 저장된 메모리 페이지 식별, OR 연산으로 사용
	// VM_ANON | VM_MARKER_0 이렇게 type을 주면 두가지 플래그가 모두 적용된다

	VM_MARKER_1 = (1 << 4),

	/* DO NOT EXCEED THIS VALUE. */
	VM_MARKER_END = (1 << 31),
};

#include "vm/uninit.h"
#include "vm/anon.h"
#include "vm/file.h"
#ifdef EFILESYS
#include "filesys/page_cache.h"
#endif

struct page_operations;
struct thread;

// if(VM_TYPE(type) == VM_ANON) {}
#define VM_TYPE(type) ((type)&7)


/* The representation of "page".
 * This is kind of "parent class", which has four "child class"es, which are
 * uninit_page, file_page, anon_page, and page cache (project4).
 * DO NOT REMOVE/MODIFY PREDEFINED MEMBER OF THIS STRUCTURE. */
struct page
{
	const struct page_operations *operations;
	void *va;			 // 유저 영역 VA
	struct frame *frame; /* 매핑된 프레임인듯 하다 Back reference for frame */ 

	/* Your implementation */
	struct hash_elem hash_elem;
	bool writable;
	bool accessible;
	int mapped_page_count; // file_backed_page인 경우, 매핑에 사용한 페이지 개수 (매핑 해제 시 사용)

	/* Per-type data are binded into the union.
	 * Each function automatically detects the current union */
	union
	{
		// 3개의 페이지 형태 중 하나를 가질 수 있다.
		// 초기화되지 않음, anonymous, file-backed
		struct uninit_page uninit;
		struct anon_page anon;
		struct file_page file;
#ifdef EFILESYS
		struct page_cache page_cache;  // 이건 신경 안 써도 될 것 같다?
#endif
	};
};

/* The representation of "frame" */
// 물리 메모리를 뜻하는, frame 구조체
struct frame
{
	void *kva;						// 커널 가상 주소, PA라고 봐도 무방할 듯 하다 ( 어차피 1:1 매핑이니까 )
	struct page *page;				// 프레임에 매핑된 페이지
	struct list_elem frame_elem;	// frame_table을 위한 list_elem
};
struct slot
{
	struct page *page;
	uint32_t slot_no;
	struct list_elem swap_elem;
};

/* The function table for page operations.
 * This is one way of implementing "interface" in C.
 * Put the table of "method" into the struct's member, and
 * call it whenever you needed. */
// 3개의 함수 포인터를 가진, 함수 테이블
struct page_operations
{
	bool (*swap_in)(struct page *, void *);
	bool (*swap_out)(struct page *);
	void (*destroy)(struct page *);
	enum vm_type type;
};

#define swap_in(page, v) (page)->operations->swap_in((page), v)
#define swap_out(page) (page)->operations->swap_out(page)

// 페이지 유형에 맞는 destroy 함수 포인터가 실행된다.
// 함수 포인터는 페이지 유형에 따라 아래와 같은 함수를 호출한다.
// file-backed일 경우 : file_backed_destroy
// anonymous일 경우 : anon_destroy 
// 어떻게 page에 따라 구분되어 실행되는지는 아직 정확히 모르겠다
#define destroy(page)                \
	if ((page)->operations->destroy) \
	(page)->operations->destroy(page)

/* Representation of current process's memory space.
 * We don't want to force you to obey any specific design for this struct.
 * All designs up to you for this. */
// 
struct supplemental_page_table
{
	struct hash sptHash;
};

#include "threads/thread.h"
void supplemental_page_table_init(struct supplemental_page_table *spt);
bool supplemental_page_table_copy(struct supplemental_page_table *dst,
								  struct supplemental_page_table *src);
void supplemental_page_table_kill(struct supplemental_page_table *spt);
struct page *spt_find_page(struct supplemental_page_table *spt,
						   void *va);
bool spt_insert_page(struct supplemental_page_table *spt, struct page *page);
void spt_remove_page(struct supplemental_page_table *spt, struct page *page);

void vm_init(void);
bool vm_try_handle_fault(struct intr_frame *f, void *addr, bool user,
						 bool write, bool not_present);


// 4번째 인자(초기화함수)가 NULL로 들어가지만 initializer에서 type에 따라 anon, file에 따른 initializer 함수를 설정하여 uninit_new를 호출해 준다.
	// vm initializer에 들어가는 4번째 인자는 lazy_load_segment이다.
#define vm_alloc_page(type, upage, writable) \
	vm_alloc_page_with_initializer((type), (upage), (writable), NULL, NULL)


bool vm_alloc_page_with_initializer(enum vm_type type, void *upage,
									bool writable, vm_initializer *init, void *aux);
void vm_dealloc_page(struct page *page);
bool vm_claim_page(void *va);
enum vm_type page_get_type(struct page *page);
void hash_page_destroy(struct hash_elem *e, void *aux);

struct list swap_table;
// struct list frame_table;
struct lock swap_table_lock;
struct lock frame_table_lock;

// 추가
static bool vm_copy_claim_page(struct supplemental_page_table *dst, void *va, void *kva, bool writable);
bool page_less(const struct hash_elem *a, const struct hash_elem *b, void *aux);

// 스택 가장 작은쪽 주소
#define STACK_LIMIT (USER_STACK - (1 << 20))
#endif /* VM_VM_H */