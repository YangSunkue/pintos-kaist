#ifndef VM_UNINIT_H
#define VM_UNINIT_H
#include "vm/vm.h"

struct page;
enum vm_type;

typedef bool vm_initializer (struct page *, void *aux);

/* Uninitlialized page. The type for implementing the
 * "Lazy loading". */
// 초기화되지 않은 페이지들을 위한 구조체
// 즉, vm_alloc_page_with_initializer에서 초기화된 페이지는 uninit page 상태이다. ( 이후 anon 또는 file-backed 형태로 초기화에 필요한 데이터는 갖고 있다 )
// page fault가 일어나 실제 접근이 되면 uninit_initialize 함수에서 anon 또는 file-backed로 초기화된다.
struct uninit_page {
	/* Initiate the contets of the page */
	vm_initializer *init;
	enum vm_type type;
	void *aux;

	/* Initiate the struct page and maps the pa to the va */
	// 함수 포인터이다. anon_initializer 또는 file_backed_initializer가 될 수 있다.
	// switch문으로 입력받은 type에 따라서 적절한 초기화 함수를 할당하여 사용하자.
	// anon, filebacked랑 인자 동일하다.
	bool (*page_initializer) (struct page *, enum vm_type, void *kva);
};

void uninit_new (struct page *page, void *va, vm_initializer *init,
		enum vm_type type, void *aux,
		bool (*initializer)(struct page *, enum vm_type, void *kva));
#endif
