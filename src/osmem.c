// SPDX-License-Identifier: BSD-3-Clause

// sursa: https://danluu.com/malloc-tutorial/
// sursa: https://moss.cs.iit.edu/cs351/slides/slides-malloc.pdf

#include "osmem.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <syscall.h>
#include <unistd.h>

#include "block_meta.h"

#define ALIGNMENT 8
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))
#define META_SIZE sizeof(struct block_meta)
#define MMAP_THRESHOLD (128 * 1024)
#define PAGE_SIZE ((size_t)sysconf(_SC_PAGESIZE))
#define SIZE_MAX 4294967295	// 2 ^ 32 - 1

struct block_meta *global_base;
struct block_meta *last;
int prealloc;

/** Function to request memory by using sbrk */
struct block_meta *brk_block(size_t size)
{
	if (size <= 0)
		return NULL;

	struct block_meta *block = (struct block_meta *)sbrk(0);
	void *req = sbrk(size);

	DIE(req != (void *)block, "sbrk failed");

	if (req == (void *)-1)
		return NULL;

	block->status = STATUS_ALLOC;
	block->size = size;

	if (!last) {
		block->next = NULL;
		block->prev = NULL;
		last = block;
	} else {
		block->prev = last;
		block->next = last->next;
		last->next = block;
		last = block;
	}

	return block;
}

/** Function to divide a block into two new ones
 *  and the first one will have the size 'size'
 */
void split_block(struct block_meta *block, size_t size)
{
	if (block == NULL)
		return;

	// Calculate remaining size after split
	size_t rem_size = block->size - size;

	if (rem_size < META_SIZE + ALIGNMENT)
		return;

	// Create a new block meta for the remaining memory
	struct block_meta *new_block = (struct block_meta *)((char *)block + size);

	// Setup for the new block
	new_block->size = rem_size;
	new_block->status = STATUS_FREE;
	new_block->prev = block;
	new_block->next = block->next;

	// Update old block
	block->next = new_block;
	block->size = size;

	// Update next prev
	if (new_block->next)
		new_block->next->prev = new_block;

	// Update last
	if (last == block)
		last = new_block;
}

/** Function to unite all free blocks starting from block into one */
void coalesce_blocks(struct block_meta *block)
{
	if (!block || block->status != STATUS_FREE)
		return;

	// Coalesce with next block
	while (block->next && block->next->status == STATUS_FREE) {
		block->size += block->next->size;
		// Update links
		block->next = block->next->next;
		if (block->next)
			block->next->prev = block;
		else
			last = block;
	}
}

// Function to coalesce all possible blocks in the list
void coalesce_everywhere(void)
{
	struct block_meta *current = global_base;

	while (current) {
		if (current->status == STATUS_FREE) {
			coalesce_blocks(current);
		}

		current = current->next;
	}
}

// Function to find a block in the list that best alignes with the size required
// if no suitable block is found, but the last one is free, it is expanded to the
// size required
struct block_meta *find_best_fit(size_t size)
{
	struct block_meta *best_fit = NULL;
	size_t best_fit_size = SIZE_MAX;

	struct block_meta *current = global_base;

	while (current) {
		if (current->status == STATUS_FREE && current->size >= size) {
			if (current->size < best_fit_size) {
				best_fit = current;
				best_fit_size = current->size;
			}
		}
		current = current->next;
	}

	if (last->status == STATUS_FREE && last->size < size && best_fit_size > size) {
		size_t missing_size = size - last->size;
		void *req = sbrk(missing_size);

		if (req == (void *) -1)
			return best_fit;

		last->size = size;
		best_fit = last;
	}

	return best_fit;
}

void *os_malloc(size_t size)
{
	if (size <= 0)
		return NULL;

	struct block_meta *block = NULL;

	// Align the size
	size_t blk_size = ALIGN(size + META_SIZE);

	// Use mmap
	if (blk_size >= MMAP_THRESHOLD) {
		block = mmap(NULL, blk_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		DIE(block == (void *)-1, "mmap failed");

		if (block) {
			block->status = STATUS_MAPPED;
			block->size = blk_size;
		}
	} else {
		// First sbrk, must init the list
		if (!prealloc) {
			block = brk_block(MMAP_THRESHOLD);
			if (block->size > blk_size) {
				split_block(block, blk_size);
			}

			prealloc = 1;
			global_base = block;
			last = block;

			return (void *)(block + 1);
		}

		// Search for a suitable existing block
		coalesce_everywhere();

		block = find_best_fit(blk_size);

		if (block) {
			if (block->size > blk_size) {
				split_block(block, blk_size);
			}
			block->status = STATUS_ALLOC;
		} else {
			// If not found, request new memory from the system
			block = brk_block(blk_size);
		}
	}

	return (void *)(block + 1);
}

void os_free(void *ptr)
{
	if (ptr == NULL)
		return;

	struct block_meta *block = ptr - META_SIZE;

	if (block->status == STATUS_FREE)
		return;

	// munmap
	if (block->status == STATUS_MAPPED) {
		int res = munmap(block, block->size);
		DIE(res < 0, "munmap failed");
	} else {
		// Mark block as free in the list
		block->status = STATUS_FREE;
	}
}

void *os_calloc(size_t nmemb, size_t size)
{
	if (size <= 0 || nmemb <= 0)
		return NULL;

	struct block_meta *block = NULL;

	size_t blk_size = ALIGN(size * nmemb + META_SIZE);

	// mmap
	if (blk_size >= PAGE_SIZE) {
		block = mmap(NULL, blk_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		DIE(block == (void *)-1, "mmap failed");

		if (block) {
			block->status = STATUS_MAPPED;
			block->size = blk_size;
		}

		memset(block + 1, 0, blk_size - META_SIZE);
	} else {
		// Use os_malloc and set the memory to 0
		block = os_malloc(blk_size - META_SIZE);

		if (block) {
			memset(block, 0, blk_size - META_SIZE);
			return (void *)block;
		}
	}

	return (void *)(block + 1);
}

void *os_realloc(void *ptr, size_t size)
{
	if (size <= 0) {
		os_free(ptr);
		return NULL;
	}

	if (ptr == NULL) {
		return os_malloc(size);
	}

	struct block_meta *block = ptr - META_SIZE;
	size_t new_size = ALIGN(size + META_SIZE);

	if (!block || block->status == STATUS_FREE)
		return NULL;

	if (block->status == STATUS_MAPPED || new_size >= MMAP_THRESHOLD) {
		void *new_ptr = os_malloc(size);
		DIE(!new_ptr, "os_malloc failed");

		memcpy(new_ptr, ptr, block->size - META_SIZE);
		os_free(ptr);
	}

	// Existing block is larger
	if (block->size >= new_size) {
		split_block(block, new_size);
		return ptr;
	}

	// Expand last block if it's free
	if (block == last) {
		int missing_size = new_size - block->size;

		void *new_ptr = sbrk(missing_size);
		DIE(!new_ptr, "sbrk failed");

		block->size = new_size;

		return ptr;
	}

	// Expand existing block
	coalesce_blocks(block);

	if (block->size >= new_size) {
		split_block(block, new_size);
		return ptr;
	}

	// Allocate a new block and copy the old data
	void *new_ptr = os_malloc(size);
	DIE(!new_ptr, "os_malloc failed");

	struct block_meta *new_block = new_ptr - META_SIZE;

	memcpy(new_ptr, ptr, new_block->size - META_SIZE);
	os_free(ptr);

	return new_ptr;
}
