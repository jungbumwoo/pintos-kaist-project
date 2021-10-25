/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "userprog/process.h"

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};


static struct list mmap_file_list;

/* The initializer of file vm */
void
vm_file_init (void) {
	list_init(&mmap_file_list);
};

struct mmap_file_info{
	struct list_elem elem;
	uint64_t start;
	uint64_t end;
};

/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;

    if (page == NULL)
        return false;

    struct load_info *aux = (struct load_info *)page->uninit.aux;

    struct file *file = aux->file;
    off_t offset = aux->ofs;
    size_t page_read_bytes = aux->page_read_bytes;
    size_t page_zero_bytes = PGSIZE - page_read_bytes;

    /* Get a page of memory. */

    /* Load this page. */

    file_seek(file, offset);

    if (file_read(file, kva, page_read_bytes) != (int)page_read_bytes)
    {
        // palloc_free_page (kva);
        return false;
    }
    // printf("여기서 터지나요??\n");
    // printf("lazy load file file pos :: %d\n", file->pos);
    memset(kva + page_read_bytes, 0, page_zero_bytes);
    // /* Add the page to the process's address space. */

    return true;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;

    if (page == NULL)
        return false;

    struct load_info *aux = (struct load_info *)page->uninit.aux;

    //! DIRTY CHECK
    if (pml4_is_dirty(thread_current()->pml4, page->va))
    {
        file_write_at(aux->file, page->va, aux->page_read_bytes, aux->ofs);
        pml4_set_dirty(thread_current()->pml4, page->va, 0);
    }

    pml4_clear_page(thread_current()->pml4, page->va);
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

static bool lazy_load_file(struct page *page, void* aux){
	struct mmap_info* mi = (struct mmap_info*) aux;
	file_seek (mi->file, mi->ofs);
	page -> file.size = file_read(mi->file, page->va, mi->page_read_bytes);
	page -> file.ofs = mi->ofs;
	if (page->file.size != PGSIZE){
		memset (page->va + page ->file.size, 0, PGSIZE - page->file.size);
	}
	pml4_set_dirty(thread_current()->pml4, page->va, false);
	free(mi);
	return true;
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {
	off_t ofs;
	uint64_t read_bytes;
	for (uint64_t i = 0; i < length; i += PGSIZE){
		struct mmap_info * mi = malloc(sizeof(struct mmap_info));
		ofs = offset + i;
		read_bytes = length - i >= PGSIZE ? PGSIZE : length -i;
		mi->file = file_reopen(file);
		mi->ofs = ofs;
		mi->page_read_bytes;
		vm_alloc_page_with_initializer(VM_FILE, (void *) ((uint64_t) addr + i), writable, lazy_load_file, (void *)mi);
	}
	struct mmap_file_info* mfi = malloc(sizeof (struct mmap_file_info));
	mfi->start = (uint64_t) addr;
	mfi->end = (uint64_t) pg_round_down((uint64_t) addr + length -1);
	list_push_back(&mmap_file_list, &mfi->elem);
	return addr;
	/* 
	A call to mmap may fail if the file opened as fd has a length of zero bytes. 
	파일의 	byte 수가 0은 아닌지
	It must fail if addr is not page-aligned 
	or if the range of pages mapped overlaps any existing set of mapped pages, 
	including the stack or pages mapped at executable load time. 

	In Linux, if addr is NULL, the kernel finds an appropriate address at which to create the mapping. 
	For simplicity, you can just attempt to mmap at the given addr. 

	Therefore, if addr is 0, it must fail, because some Pintos code assumes virtual page 0 is not mapped. 
	Your mmap should also fail when length is zero. 

	Finally, the file descriptors representing console input and output are not mappable.
	Memory-mapped pages should be also allocated in a lazy manner just like anonymous pages. 
	You can use vm_alloc_page_with_initializer or vm_alloc_page to make a page object.
	*/

	// void *ori_addr = addr;
	// size_t read_bytes = length > file_length(file) ? file_length(file) : length;
	// // size_t zero_bytes = PGSIZE - read_bytes % PGSIZE; //마지막 페이지 잔여 공간인듯
	// size_t zero_bytes = PGSIZE - read_bytes % PGSIZE; //마지막 페이지 잔여 공간인듯

	// while (read_bytes > 0 || zero_bytes > 0) // zoro_bytes는 맨 마지막 페이지에만 생길텐데 빼줘도 되는거아닌가
	// {
	// 	size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
	// 	size_t page_zero_bytes = PGSIZE - page_read_bytes;

	// 	struct mmap_info *mmap_info = malloc(sizeof (struct mmap_info));
	// 	struct file *mfile = file_reopen(file);
	// 	mmap_info->file = mfile;
	// 	mmap_info->ofs = offset;
	// 	mmap_info->page_read_bytes = page_read_bytes;
	// 	// mmap_info->page_zero_bytes = page_zero_bytes;

	// 	if(!vm_alloc_page_with_initializer(VM_FILE, addr, writable, lazy_load_segment, mmap_info))
	// 	{
	// 		printf("\n\n err at file.c \n\n");
	// 		return NULL;
	// 	}

	// 	read_bytes -= page_read_bytes;
	// 	zero_bytes -= page_zero_bytes;
	// 	addr += PGSIZE;
	// 	offset += page_read_bytes;	// 그냥 PGSIZE 해줘도 무관할거같기도
	// }
	// return ori_addr;
}

/* Do the munmap */
void
do_munmap (void *addr) {
	if (list_empty (&mmap_file_list)) return;
	for (struct list_elem* i = list_front (&mmap_file_list); i != list_end (&mmap_file_list); i = list_next (i))
	{
		struct mmap_file_info* mfi = list_entry (i, struct mmap_file_info, elem);
		if (mfi -> start == (uint64_t) addr){
			for (uint64_t j = (uint64_t)addr; j<= mfi -> end; j += PGSIZE){
				struct page* page = spt_find_page(&thread_current() -> spt, (void*) j);
				spt_remove_page(&thread_current()->spt, page);
			};
			list_remove(&mfi->elem);
			free(mfi);
			return;
		}
	}
}
