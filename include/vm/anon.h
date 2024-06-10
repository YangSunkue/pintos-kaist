#ifndef VM_ANON_H
#define VM_ANON_H
#include "vm/vm.h"
struct page;
enum vm_type;

// Anonymous page 구조체. 멤버 추가해야 한다.
struct anon_page {

    // swap out 될 때, 이 페이지가 swap 테이블에 저장된 slot 번호
    uint32_t slotNumber;
};

void vm_anon_init (void);
bool anon_initializer (struct page *page, enum vm_type type, void *kva);

#endif
