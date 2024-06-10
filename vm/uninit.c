/* uninit.c: Implementation of uninitialized page.
 *
 * All of the pages are born as uninit page. When the first page fault occurs,
 * the handler chain calls uninit_initialize (page->operations.swap_in).
 * The uninit_initialize function transmutes the page into the specific page
 * object (anon, file, page_cache), by initializing the page object,and calls
 * initialization callback that passed from vm_alloc_page_with_initializer
 * function.
 * */

// 초기화되지 않은 페이지들을 위한 기능 제공
// 모든 페이지는 초기화되지 않은 페이지로 설정된 후, Anonymous / File-Backed로 변환된다
// 초기화되지 않은 페이지들을 위한 기능 제공
// 모든 페이지는 초기화되지 않은 페이지로 설정된 후, Anonymous / File-Backed로 변환된다
// 초기화되지 않은 페이지들을 위한 기능 제공
// 모든 페이지는 초기화되지 않은 페이지로 설정된 후, Anonymous / File-Backed로 변환된다

#include "vm/vm.h"
#include "vm/uninit.h"

static bool uninit_initialize (struct page *page, void *kva);
static void uninit_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations uninit_ops = {
	.swap_in = uninit_initialize,
	.swap_out = NULL,
	.destroy = uninit_destroy,
	.type = VM_UNINIT,
};

/* DO NOT MODIFY this function */
// uninit type의 페이지 구조체 생성
void
uninit_new (struct page *page, void *va, vm_initializer *init,
		enum vm_type type, void *aux,
		bool (*initializer)(struct page *, enum vm_type, void *)) {
	ASSERT (page != NULL);

	// page 구조체를 입력받은 정보들로 초기화해 준다
	*page = (struct page) {
		.operations = &uninit_ops,
		.va = va,
		.frame = NULL, /* no frame for now */

		// uninit_page 초기화
		.uninit = (struct uninit_page) {
			.type = type,
			.init = init,
			.aux = aux,
			.page_initializer = initializer,
		}
	};
}

/* Initalize the page on first fault */
// page fault가 일어나면 호출된다.
// page 구조체에 할당해 두었던 초기화 함수를 호출하여 페이지를 초기화한다
// init 그리고 anon_initializer 또는 file_backed_initializer
static bool
uninit_initialize (struct page *page, void *kva) {
	struct uninit_page *uninit = &page->uninit;

	/* Fetch first, page_initialize may overwrite the values */
	vm_initializer *init = uninit->init;
	void *aux = uninit->aux;

	/* TODO: You may need to fix this function. */
	// page_initializer, init이 모두 true여야 true를 반환한다.
	return uninit->page_initializer (page, uninit->type, kva) &&
		// init이 성공하거나, init이 아예 없으면 true 반환
		// init이 실패했다면 false 반환
		// 즉 init은 선택사항인 것이다.
		(init ? init (page, aux) : true);
}

/* Free the resources hold by uninit_page. Although most of pages are transmuted
 * to other page objects, it is possible to have uninit pages when the process
 * exit, which are never referenced during the execution.
 * PAGE will be freed by the caller. */
static void
uninit_destroy (struct page *page) {
	struct uninit_page *uninit UNUSED = &page->uninit;
	/* TODO: Fill this function.
	 * TODO: If you don't have anything to do, just return. */
}
