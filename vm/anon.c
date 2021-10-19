/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"

#include <bitmap.h>

#define CEILING(x, y) (((x) + (y) - 1) / (y))
#define SECTORS_PER_PAGE CEILING(PGSIZE, DISK_SECTOR_SIZE)

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

static struct bitmap *swap_table;
/* Initialize the data for anonymous pages */
void
vm_anon_init (void) {
	/* TODO: Set up the swap_disk. */
	// Project3-2 Anonymous Page
	swap_disk = disk_get(1, 1);

	disk_sector_t num_sector = disk_size (swap_disk);
	size_t max_slot = num_sector / SECTORS_PER_PAGE;

	swap_table = bitmap_create(max_slot);
}

/* Initialize the file mapping */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	// project 3-2
	page->operations = &anon_ops;
	if (type & VM_MARKER_0) page->operations = &anon_ops;
	struct anon_page *anon_page = &page->anon;
	anon_page->owner = thread_current();
	anon_page->swap_slot_idx = INVALID_SLOT_IDX;
	return true;
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) {
	struct anon_page *anon_page = &page->anon;
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
	struct anon_page *anon_page = &page->anon;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	if (page -> frame!= NULL){
		list_remove (&page->frame->elem);
		free(page->frame);
	}
	else {
		// Swapped anon page case
		struct anon_page *anon_page = &page->anon;
		ASSERT (anon_page->swap_slot_idx != INVALID_SLOT_IDX);

		// Clear swap table
		bitmap_set (swap_table, anon_page->swap_slot_idx, false);
	}
}
