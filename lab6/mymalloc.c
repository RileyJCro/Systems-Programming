#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include "mymalloc.h"

/* mymalloc.c
   Riley Crockett
   11/1/2022

   This program defines the procedures declared in mymalloc.h.
   These procedures are defined at the bottom of this program, starting
   with my_malloc. There are also helper functions / data defined below.
   */

/* A pointer to the head of the free list. */
void *free_head = NULL;

/* A struct for blocks in the free list. */
typedef struct block {
   unsigned int size;
   struct block *flink;
} *Block;

/* @name: alloc_block
   @brief: Allocates memory with sbrk and creates a Block struct.
   @param[in] size: The block size in bytes (size >= 8192).
   @param[out]: Returns the newly created block struct.
   */
Block alloc_block(size_t size) {
  Block b = sbrk(size);
  b->size = size;
  b->flink = NULL;
  return b;
}

/* @name: new_block
   @brief: A helper function that initializes blocks and chunks.
   @param[in] size: The chunk size in bytes.
   @param[out]: Returns a pointer to the start of the block's data section.
   */
void *new_block(size_t size) {
  Block addr, rem;
  if (size > 8192) return (void *)alloc_block(size) + 8;

  addr = alloc_block(8192);
  rem = (void *)addr + size;
  addr->size = size;
  rem->size = 8192 - size;

  my_free((void *)rem + 8);
  return (void *)addr + 8;
}

/* @name: get_prev_block
   @brief: A helper function to get the previous free list block.
   @param[in] b: The comparison block.
   @param[in] new_fb: Is 0 if the comparison block is in the free list.
   @param[out]: Returns the previous block.
   */
Block get_prev_block(void *b, int new_fb) {
  void *p = NULL; // ----x
  Block prev;
  for (p = free_head; p != NULL; p = free_list_next(p)) {
    prev = (Block) p;
    if (new_fb == 0 && prev->flink == b) break;
    if (new_fb == 1 && (void *)prev->flink > b) break;
  }
  return prev;
}

/* @name: my_malloc
   @brief: This function allocates memory for a chunk if needed, updates
   the free list connections, and returns a pointer to chunk data.
   @param[in] size: The chunk size in bytes.
   @param[out]: Returns a pointer to the start of the chunk's data section.
   */
void *my_malloc(size_t size) {
  void *l;
  Block b, rem_b, pb;
  int total_bytes;

  total_bytes = size+8;
  if (size % 8 != 0) total_bytes = (size + 7 - (size + 7) % 8) + 8;

  for (l = free_head; l != NULL; l = free_list_next(l)) {
    b = (Block) l;
    if (b->size >= total_bytes) break;
  }

  if (l == NULL) return new_block(total_bytes);
  if (l != free_head) pb = get_prev_block(l, 0);

  if (b->size - total_bytes > 8) {
    rem_b = l + total_bytes;
    rem_b->size = b->size - total_bytes;
    rem_b->flink = b->flink;
    b->size = total_bytes;

    if (l == free_head) {
      free_head = rem_b;
    } else {
      pb->flink = rem_b;
    }
  }
  else {
    if (l == free_head) {
      free_head = ((Block)free_head)->flink;
    } else {
      pb->flink = b->flink;
    }
  }
  return (void *) b + 8;
}

/* @name: my_free
   @brief: Adds a block to the free list (ordered by address).
   @param[in] ptr: A pointer to the chunk to be freed.
   */
void my_free(void *ptr) {
  void *f_node = ptr - 8;
  Block fb = (Block) f_node;

  if (!free_head) {
    free_head = fb;
    return;
  }
  if (f_node == free_head) {
    free_head = NULL;
  } else if (f_node < free_head) {
    fb->flink = (Block) free_head;
    free_head = f_node;
  } else {
    Block pb = get_prev_block(fb, 1);
    fb->flink = pb->flink;
    pb->flink = fb;
  }
}

void *free_list_begin() {
  return free_head;
}

void *free_list_next(void *node) {
  return ((Block) node)->flink;
}

/* @name: coalesce_free_list
   @brief: This function combines adjacent chunks in the free list.
   */
void coalesce_free_list() {
  void *l = free_list_begin();
  Block b, next;

  while (l != NULL) {
    b = (Block) l;
    next = free_list_next(l);
    if (l + b->size == (void *)next) {
      b->size += next->size;
      b->flink = next->flink;
      l = free_list_begin();
      continue;
    }
    l = free_list_next(l);
  }
}