/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/mmu.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "userprog/process.h"
#include "hash.h"
#include "threads/vaddr.h"


// 가상 메모리의 기본적인 인터페이스들 제공
// 가상 메모리의 기본적인 인터페이스들 제공
// 가상 메모리의 기본적인 인터페이스들 제공
// 가상 메모리의 기본적인 인터페이스들 제공
// 가상 메모리의 기본적인 인터페이스들 제공

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
// 커널이 페이지 요청을 받으면 이 함수가 호출된다!!
// 페이지 구조체 할당
// uninit으로 만들고, 후에 초기화될 수 있도록 type을 포함한다
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {
	
	// vm_initializer *init : 페이지를 초기화할 때 사용할 초기화 함수 포인터. type에 따라 다르게 동작하며 page fault시에 호출된다.
	// aux라는 추가 인자를 받아서 초기화 작업을 수행한다.
	// swap_in 핸들러가 초기화 함수를 호출하게 된다.
	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	// 입력받은 VA에 대한 페이지가 spt에 없다면 만들어서 넣어준다
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */

		/* TODO: Insert the page into the spt. */
		
		// page 구조체 가상 메모리 할당받기
		struct page *page = (struct page *)malloc(sizeof(struct page));
		if(page == NULL) {
			goto err;
		}

		// 함수 포인터 선언
		bool(*page_initializer)(struct page *, enum vm_type, void *);

		// page type에 따라서 초기화 함수를 설정한다.
		switch(VM_TYPE(type))
		{
		case VM_ANON:
			page_initializer = anon_initializer;
			break;
		case VM_FILE:
			page_initializer = file_backed_initializer;
			break;
		default:
			goto err;
		}

		// 위 정보들로 uninit page 형태로 초기화한다.

		// init : 기본적인 페이지 구조 초기화
		// aux : init에서 페이지 초기화 시 필요한 추가정보들
		// page_initializer : anon, file-backed에 따른 부분들 초기화할 함수
		uninit_new(page, upage, init, type, aux, page_initializer);
		// 쓰기가능여부도 설정해 준다.
		page->writable = writable;

		// 만들어진 uninit page를 spt에 추가한다.
		if(spt_insert_page(spt, page)) {
			return true;
		}
		else {
			goto err;
		}
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
// spt에서 va에 해당하는 page를 찾아서 반환
// 미완성이다 구현해야 한다!!!!!!!!!! -> 구현했따
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {

	// hash_find로 전달할 page
	struct page page;
	page.va = pg_round_down(va);

	// hash_find로 전달할 hash_elem
	struct hash_elem *e;

	// va에 해당하는 페이지를 찾았다면 page 변수에 할당하고 return
	if((e = hash_find(&spt->sptHash, &page.hash_elem)) != NULL) {
		return hash_entry(e, struct page, hash_elem);
	}
	else {
		return NULL;
	}
}


/* Insert PAGE into spt with validation. */
// spt에 page를 삽입하는 함수
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED, struct page *page UNUSED) {

	int succ = false;

	// insert 성공했으면 NULL, 이미 있는거였다면 hash_elem을 그대로 반환하게 된다.
	struct hash_elem *e = hash_insert(&spt->sptHash, &page->hash_elem);
	
	// 성공했으면 true 반환
	if(e == NULL) {
		succ = true;
	}

	return succ;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	vm_dealloc_page (page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. */

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
// 사용 가능한 프레임을 얻는 함수
static struct frame *
vm_get_frame (void) {
	struct frame *frame = NULL;
	/* TODO: Fill this function. */

	// 1페이지 만큼 유저 영역 프레임 할당
	void *kva = palloc_get_page(PAL_USER);

	// 프레임 할당받지 못했으면 커널 패닉
	if(kva == NULL) {
		PANIC("vm_get_frame : palloc_get_page result is NULL");
	}

	// frame 구조체에 가상 메모리 할당 + 가상 주소 반환
	frame = (struct frame *)malloc(sizeof(struct frame));

	// kva, page 초기화 작업
	frame->kva = kva;
	frame->page = NULL;


	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);

	// 사용준비 완료된 frame 반환
	return frame;
}

/* Growing the stack. */
// 하나 이상의 anonymous 페이지를 할당하여 스택 크기 늘리기
// addr은 faulted주소에서 유효한 주소가 된다
static void
vm_stack_growth (void *addr UNUSED) {

	bool success = false;

	// offset을 제외한 페이지 주소 얻기
	addr = pg_round_down(addr);

	// 페이지 할당받기 + spt에 넣기
	if(vm_alloc_page(VM_ANON | VM_MARKER_0, addr, true)) {
		// 물리메모리와 매핑하기 + page table에 추가
		success = vm_claim_page(addr);
	}
	if(success) {
		thread_current()->stack_bottom -=  PGSIZE;
	}

}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
// exception.c의 page_fault()에 의해 호출되는 함수
// page fault를 처리하려고 시도한다
// 인터럽트프레임, fault 일으킨 주소, 사용자모드여부, 쓰기접근여부, 페이지가 메모리에 올라와있는지 여부
// bool
// vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
// 		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {

// 	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
// 	struct page *page = NULL;
// 	/* TODO: Validate the fault */
// 	/* TODO: Your code goes here */
// 	// bogus fault라면 false를 return하나?

// 	// fault 일으킨 주소가 NULL이라면 false 리턴
// 	if(addr == NULL) {
// 		return false;
// 	}

// 	// fault 일으킨 주소가 커널주소라면 false 리턴
// 	if(is_kernel_vaddr(addr)) {
// 		return false;
// 	}
// 	// 여기서 걸린다1

// 	// fault 일으킨 가상 주소의 프레임이 존재하지 않는다면 claim으로 할당해주기
// 	if(not_present) {
		
// 		//rsp 담을 변수
// 		void *rsp;

// 		// 유저 모드에서 발생한 page fault일 경우
// 		if(user) {
// 			rsp = f->rsp;	
// 		}
// 		// 커널 모드에서 발생한 page fault일 경우
// 		else {
// 			rsp = thread_current()->rsp;
// 		}

// 		// 스택 확장으로 처리할 수 있는 경우
// 		// 즉 스택이 확장되어야 하는 상황을 의미한다
// 		if ((USER_STACK - (1 << 20) <= rsp - 8 && rsp - 8 == addr && addr <= USER_STACK) || (USER_STACK - (1 << 20) <= rsp && rsp <= addr && addr <= USER_STACK)) {

// 			// 조건문 이해 못했다. 이해해야 함.
// 			vm_stack_growth(addr);
// 		}


// 		// spt에서 addr에 해당하는 페이지 찾기
// 		page = spt_find_page(spt, addr);
// 		// 여기서 걸린다2
// 		if(page == NULL) {

// 			// 못찾았으면 false 리턴
// 			return false;
// 		}
// 		if(write == 1 && page->writable == 0) {

// 			// write 불가능한 페이지에 write 요청한 경우 false 리턴
// 			return false;
// 		}
// 		return vm_do_claim_page (page);
// 	}

// 	// fault 일으킨 가상 주소에 이미 프레임이 존재한다면 false 리턴
// 	return false;
// }

bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
			
    struct supplemental_page_table *spt UNUSED = &thread_current()->spt;
	/** Project 3-Copy On Write */
    struct page *page = spt_find_page(&thread_current()->spt, addr);

    if (addr == NULL || is_kernel_vaddr(addr))
        return false;

    if (!not_present && write)
        return vm_handle_wp(page);

    if (!page) {
        void *rsp = user ? f->rsp : thread_current()->rsp;
		if (STACK_LIMIT <= rsp - 8 && rsp - 8 == addr && addr <= USER_STACK){
			vm_stack_growth(thread_current()->stack_bottom - PGSIZE);
			return true;
		}
		else if (STACK_LIMIT <= rsp && rsp <= addr && addr <= USER_STACK){
			vm_stack_growth(thread_current()->stack_bottom - PGSIZE);
			return true;
		}
        return false;
    }

    return vm_do_claim_page(page);  
}


/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
// 인자로 주어진 va에 해당하는 페이지를 spt에서 찾는다.
// 그 페이지를 프레임에 할당한다.
bool
vm_claim_page (void *va UNUSED) {
	
	struct page *page = NULL;
	struct thread *cur = thread_current();

	// 현재 프로세스 spt에서 va에 해당하는 페이지 반환
	page = spt_find_page(&cur->spt, va);

	// 해당하는 페이지가 없다면 return false
	if(page == NULL) {
		return false;
	}

	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
// 페이지를 프레임에 할당하는 함수
static bool
vm_do_claim_page (struct page *page) {

	// 사용 가능한 프레임 얻기
	struct frame *frame = vm_get_frame ();
	if(frame == NULL) {
		return false;
	}

	/* Set links */
	// 프레임과 페이지를 서로 연결한다.
	frame->page = page;
	page->frame = frame;
	
	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	// 페이지의 VA를 프레임의 PA로 매핑하는 PTE를 삽입해야 한다.
	// VA를 PA로 매핑하는 로직을 추가해야 함.
	// 페이지 테이블(pml4)에 PTE(page)를 넣으라는 얘기인가?

	// 현재 스레드 가져오기
	struct thread *cur = thread_current();

	// 현재 스레드 페이지 테이블에 page 주소, frame 주소, 쓰기가능여부와 함께 PTE 추가하기
	pml4_set_page(cur->pml4, page->va, frame->kva, page->writable);

	// 실제로 페이지를 메모리에 로드한다.
	return swap_in (page, frame->kva);
}


/** Project 3-Copy On Write */
static bool 
vm_copy_claim_page(struct supplemental_page_table *dst, void *va, void *kva, bool writable) {
    struct page *page = spt_find_page(dst, va);

    if (page == NULL)
        return false;

    struct frame *frame = (struct frame *)malloc(sizeof(struct frame));

    if (!frame)
        return false;

    page->accessible = writable; 
    frame->page = page;
    page->frame = frame;
    frame->kva = kva;

    if (!pml4_set_page(thread_current()->pml4, page->va, frame->kva, false)) {
        free(frame);
        return false;
    }

    list_push_back(&frame_table, &frame->frame_elem); 

    return swap_in(page, frame->kva);
}


/* Initialize new supplemental page table */
// 새로운 SPT 테이블을 초기화하는 함수
// initd에서 새로운 프로세스가 시작하거나, __do_fork로 자식 프로세스가 생성될 때 호출된다.
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {


	// spt, 해시함수, 페이지 VA 비교함수, 보조값 
	// spt 해시 테이블 초기화하기 ( spt->sptHash가 hash 테이블 타입이다 )
	hash_init(&spt->sptHash, page_hash, page_less, NULL);
}

/* Copy supplemental page table from src to dst */
// src에서 dst로 spt를 복사한다.
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
		
		// 해시 테이블 순회를 위한 반복자
		struct hash_iterator iter;
		hash_first(&iter, &src->sptHash);  // iter가 sptHash의 첫 번째 요소를 가리키도록 한다 ( 리스트 head라서 더미데이터임 )

		// 다음 요소부터 while문 반복한다
		while(hash_next(&iter)) {

			// page 가져오기
			struct page *src_page = hash_entry(hash_cur(&iter), struct page, hash_elem);

			enum vm_type type = src_page->operations->type;  // type 가져오기
			void *upage = src_page->va;  					 // va 가져오기
			bool writable = src_page->writable;				 // writable 가져오기

			// uninit page일 경우
			if(type == VM_UNINIT) {
				void *aux = src_page->uninit.aux;     		 // 초기화를 위한 보조 데이터 가져오기
				// src spt에서 가져온 정보들로 페이지 만들기
				vm_alloc_page_with_initializer(page_get_type(src_page), upage, writable, src_page->uninit.init, aux);
			}

			// File-backed page 일 경우
			else if(type == VM_FILE) {

				// 인자로 전달할 aux 제작(복사)하기
				struct lazyLoadArg *aux = (struct lazyLoadArg *)malloc(sizeof(struct lazyLoadArg));
				aux->file = src_page->file.file;
				aux->offset = src_page->file.offset;
				aux->readBytes = src_page->file.page_read_bytes;

				// 여기서 페이지 할당하고 spt에 들어간다
				if(!vm_alloc_page_with_initializer(type, upage, writable, NULL, aux)) {
					return false;
				}

				// spt에서 찾아서 file-backed로 초기화하기.
				struct page *dst_page = spt_find_page(dst, upage);
				file_backed_initializer(dst_page, type, NULL);    // kva를 NULL로 준 후 frame을 통째로 복사한다.
				dst_page->frame = src_page->frame;
				// 현재 스레드 page table에 넣기
				pml4_set_page(thread_current()->pml4, dst_page->va, src_page->frame->kva, src_page->writable);
			}

			// Anonymous page일 경우
			else {
				if(!vm_alloc_page(type, upage, writable)) {
					return false;
				}
				if(!vm_copy_claim_page(dst, upage, src_page->frame->kva, writable)) {
					return false;
				}
			}
		}
		return true;
}

/* Free the resource hold by the supplemental page table */
// spt의 모든 자원들을 free한다 -> process가 exit할 때 process_exit()에서 호출된다.
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */

	// 해시테이블 요소를 모두 지우고, 메모리를 해제한다.
	hash_clear(&spt->sptHash, hash_page_destroy);
}

/** Project 3-Anonymous Page */
// hash table 요소에 대해 메모리 해제하는 함수
void 
hash_page_destroy(struct hash_elem *e, void *aux)
{
    struct page *page = hash_entry(e, struct page, hash_elem);
	// 페이지 구조체에 대한 추가적인 정리 작업들
    destroy(page);
	// 페이지 구조체 메모리 해제
    free(page);
}