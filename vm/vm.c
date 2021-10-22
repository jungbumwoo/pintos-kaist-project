/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "intrinsic.h"
// #include "threads/mmu.h"

static struct lock spt_kill_lock;

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
	lock_init(&spt_kill_lock);
	
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

//project 3-2
static struct lock spt_kill_lock;

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;
	bool writable_aux = writable;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */

		/* TODO: Insert the page into the spt. */
		struct page *page = malloc(sizeof (struct page));
		if(VM_TYPE(type) == VM_ANON){
			uninit_new (page, upage, init, type, aux, anon_initializer);
		} else if (VM_TYPE(type) == VM_FILE){
			uninit_new (page, upage, init, type, aux, file_backed_initializer);
		}

		page->writable = writable_aux; // 얘 뭐더라?
		// page -> on_memory = 0;
		spt_insert_page(spt, page);
		return true;
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	/* TODO: Fill this function. */
	/* hash_find() 함수를 사용해서 hash_elem 구조체 얻음 */
	/* 만약 존재하지 않는다면 NULL 리턴 */
	/* hash_entry()로 해당 hash_elem의 vm_entry 구조체 리턴 */

	// 인자로 받은 vaddr에 해당하는 page(entry)? 검색 후 반환 
	struct page page;
	page.va = pg_round_down(va); /* pg_round_down()으로 vaddr의 페이지 번호를 얻음 */
	struct hash_elem *e = hash_find(spt->page_table, &page.hash_elem);
	if (e == NULL)
		return NULL;
	struct page *result = hash_entry(e, struct page, hash_elem);
	return result;

}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	/* TODO: Fill this function. */
	/*
		hash_insert() 함수를 이용하여 vm_entry를 해시 테이블에 삽입
		삽입 성공 시 true 반환
		실패 시 false 반환
	*/
	struct hash_elem *result = hash_insert(spt->page_table, &page->hash_elem);
	return (result == NULL) ? true : false;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	struct hash_elem *e = hash_delete(spt->page_table, &page->hash_elem);
	if (e != NULL)
		vm_dealloc_page(page);
	return;
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
	if (victim == NULL)
		return NULL;

	/* Swap out the victim and return the evicted frame */
	struct page *page = victim->page;
	bool swap_done = swap_out(page);
	if (!swap_done)
		PANIC("Swap is full\n");

	// clear frame
	// &page = NULL; : err(lvalue required as left operand of assignment)
	page = NULL; // 명확하게 해주자면 victim -> page
	memset(victim->kva, 0, PGSIZE);

	return victim;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {
	/* TODO: Fill this function. */
	// Project 3-Memory Management. gitbook
	struct frame *frame = malloc(sizeof(struct frame));
	frame->kva = palloc_get_page(PAL_USER);
	frame->page = NULL;

		// Add swap case handling
	if (frame->kva == NULL)
	{
		free(frame);
		frame = vm_evict_frame();
	}
	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
	void *stack_bottom = pg_round_down(addr);
	size_t req_stack_size = USER_STACK - (uintptr_t)stack_bottom;
	if (req_stack_size > (1 << 20)) PANIC("Stack limit exceeded");

	void *growing_stack_bottom = stack_bottom;
	while((uintptr_t) growing_stack_bottom < USER_STACK &&
		vm_alloc_page(VM_ANON | VM_MARKER_1, growing_stack_bottom, true)){
			growing_stack_bottom += PGSIZE;
		};
	vm_claim_page(stack_bottom);
	// if(vm_alloc_page(VM_ANON | VM_MARKER_0, addr, 1))
	// {
	// 	vm_claim_page(stack_bottom);
	// 	thread_current()->stack_bottom -= PGSIZE; // 이거 아닌거같아서 주석처리함. 
	// }

	// Alloc page from tested region to re
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
	return false;
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	// struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;

	struct thread *curr = thread_current ();
	struct supplemental_page_table *spt = &curr->spt;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */

	uint64_t fault_addr = rcr2();
	if (is_kernel_vaddr(addr) && user) return false;

	void *stack_bottom = pg_round_down(curr->stack_bottom);
	if (write && (stack_bottom - PGSIZE <= addr && (uintptr_t) addr < USER_STACK)){
		vm_stack_growth(addr);
		return true;
	}

	struct page *page = spt_find_page (spt, (void *) addr);
	if (page == NULL) return false; 

	if (write && !not_present) return vm_handle_wp(page);
	return vm_do_claim_page (page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
bool
vm_claim_page (void *va UNUSED) {
	// Project 3-Memory Management
	// You will first need to get a page and then calls vm_do_claim_page with the page.
	/* TODO: Fill this function */
	struct page *page = spt_find_page(&thread_current()-> spt, va);
	if (page == NULL)
		return false;
	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
// claim : allocate a physical frame
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame (); // for allocate a physical frame
	struct thread *curr = thread_current();
	/*
	Project 3-Memory Management
	Then, you need to set up the MMU. 
	In other words, add the mapping from the virtual address to the physical address in the page table. 
	The return value should indicate whether the operation was successful or not.
	*/

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	// if (install_page(page->va, frame->kva, page->writable))
	// {
	// 	return swap_in (page, frame->kva);
	// }
	
	// // Add to frame_list for eviction clock algorithm
	// if (clock_elem != NULL)
	// 	// Just before current clock
	// 	list_insert(clock_elem, &frame->elem);
	// else
	// 	list_push_back(&frame_list, &frame->elem);
	if (!pml4_set_page(curr->pml4, page->va, frame->kva, page->writable))
		return false;
	return swap_in(page, frame->kva);
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	/* hash_init()으로 해시테이블 초기화 */
	/* 인자로 해시 테이블과 vm_hash_func과 vm_less_func 사용 */
	
	// table 초기화
	// vm_entry 들을 해시 테이블에 추가
	struct hash* page_table = malloc(sizeof (struct hash));
	hash_init(page_table, page_hash, page_less, NULL);
	spt->page_table = page_table;
}

/* Copy supplemental page table from src to dst */
// __do_fork 할 때 쓰임
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
	/*Iterate Source spt hash table*/
	struct hash_iterator i;
	hash_first(&i, src->page_table);
	while (hash_next(&i))
	{
		struct page *page = hash_entry(hash_cur(&i), struct page, hash_elem);

		/*Handle UNINIT page*/
		if (page->operations->type == VM_UNINIT)
		{
			vm_initializer *init = page->uninit.init;
			bool writable = page->writable;
			int type = page->uninit.type;
			if (type & VM_ANON)
			{
				struct load_info *li = malloc(sizeof(struct load_info));
				li->file = file_duplicate(((struct load_info *)page->uninit.aux)->file);
				li->page_read_bytes = ((struct load_info *)page->uninit.aux)->page_read_bytes;
				li->page_zero_bytes = ((struct load_info *)page->uninit.aux)->page_zero_bytes;
				li->ofs = ((struct load_info *)page->uninit.aux)->ofs;
				vm_alloc_page_with_initializer(type, page->va, writable, init, (void *)li);
			}
			else if (type & VM_FILE)
			{
				//Do_nothing(it should not inherit mmap)
			}
		}

		/* Handle ANON/FILE page*/
		else if (page_get_type(page) == VM_ANON)
		{
			if (!vm_alloc_page(page->operations->type, page->va, page->writable))
				return false;
			struct page *new_page = spt_find_page(&thread_current()->spt, page->va);
			if (!vm_do_claim_page(new_page))
				return false;
			memcpy(new_page->frame->kva, page->frame->kva, PGSIZE);
		}
		else if (page_get_type(page) == VM_FILE)
		{
			//Do nothing(it should not inherit mmap)
		}
	}
	return true;
	
}

static void
spt_destroy(struct hash_elem *e, void *aux UNUSED)
{
	struct page *page = hash_entry(e, struct page, hash_elem);
	ASSERT(page != NULL);
	destroy(page);
	free(page);
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	// 버킷리스트와 vm_entry들 제거
	// hash_destroy 하면 될 덧

	/* Destroy all the supplemental_page_table hold by thread and
	 * writeback all the modified contents to the storage. */
	if (spt->page_table == NULL){
		return;
	}
	lock_acquire(&spt_kill_lock);
	hash_destroy(spt->page_table, spt_destroy);
	free(spt->page_table);
	lock_release(&spt_kill_lock);
}
// 해시값 구해주는 함수
unsigned page_hash(const struct hash_elem *p_, void *aux UNUSED)
{
	const struct page *p = hash_entry(p_, struct page, hash_elem);
	return hash_bytes(&p->va, sizeof p->va);
}

// returns true is page a < page b
bool page_less(const struct hash_elem *a_,
				const struct hash_elem *b_, void *aux UNUSED)
{
	const struct page *a = hash_entry(a_, struct page, hash_elem);
	const struct page *b = hash_entry(b_, struct page, hash_elem);
	return a->va < b->va;
}