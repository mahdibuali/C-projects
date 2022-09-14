#include <errno.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
//this is test 2
#include "myMalloc.h"
#include "printing.h"

/* Due to the way assert() prints error messges we use out own assert function
 * for deteminism when testing assertions
 */
#ifdef TEST_ASSERT
  inline static void assert(int e) {
    if (!e) {
      const char * msg = "Assertion Failed!\n";
      write(2, msg, strlen(msg));
      exit(1);
    }
  }
#else
  #include <assert.h>
#endif

/*
 * Mutex to ensure thread safety for the freelist
 */
static pthread_mutex_t mutex;

/*
 * Array of sentinel nodes for the freelists
 */
header freelistSentinels[N_LISTS];

/*
 * Pointer to the second fencepost in the most recently allocated chunk from
 * the OS. Used for coalescing chunks
 */
header * lastFencePost;

/*
 * Pointer to maintian the base of the heap to allow printing based on the
 * distance from the base of the heap
 */ 
void * base;

/*
 * List of chunks allocated by  the OS for printing boundary tags
 */
header * osChunkList [MAX_OS_CHUNKS];
size_t numOsChunks = 0;

/*
 * direct the compiler to run the init function before running main
 * this allows initialization of required globals
 */
static void init (void) __attribute__ ((constructor));

// Helper functions for manipulating pointers to headers
static inline header * get_header_from_offset(void * ptr, ptrdiff_t off);
static inline header * get_left_header(header * h);
static inline header * ptr_to_header(void * p);

// Helper functions for allocating more memory from the OS
static inline void initialize_fencepost(header * fp, size_t left_size);
static inline void insert_os_chunk(header * hdr);
static inline void insert_fenceposts(void * raw_mem, size_t size);
static header * allocate_chunk(size_t size);

// Helper functions for freeing a block
static inline void deallocate_object(void * p);

// Helper functions for allocating a block
static inline header * allocate_object(size_t raw_size);

// Helper functions for verifying that the data structures are structurally 
// valid
static inline header * detect_cycles();
static inline header * verify_pointers();
static inline bool verify_freelist();
static inline header * verify_chunk(header* chunk);
static inline bool verify_tags();

static void init();

static bool isMallocInitialized;

// Helper functions
static inline size_t round_up_mul8(size_t size);
static inline size_t max(size_t a, size_t b);
static inline size_t min(size_t a, size_t b);
static inline bool is_empty_sentinel(size_t i);
static inline void insert_block(header * block, size_t i);
static inline void insert_chunk();
/**
 * @brief Helper function to retrieve a header pointer from a pointer and an 
 *        offset
 *
 * @param ptr base pointer
 * @param off number of bytes from base pointer where header is located
 *
 * @return a pointer to a header offset bytes from pointer
 */
static inline header * get_header_from_offset(void * ptr, ptrdiff_t off) {
	return (header *)((char *) ptr + off);
}

/**
 * @brief Helper function to get the header to the right of a given header
 *
 * @param h original header
 *
 * @return header to the right of h
 */
header * get_right_header(header * h) {
	return get_header_from_offset(h, get_size(h));
}

/**
 * @brief Helper function to get the header to the left of a given header
 *
 * @param h original header
 *
 * @return header to the right of h
 */
inline static header * get_left_header(header * h) {
  return get_header_from_offset(h, -h->left_size);
}

/**
 * @brief Fenceposts are marked as always allocated and may need to have
 * a left object size to ensure coalescing happens properly
 *
 * @param fp a pointer to the header being used as a fencepost
 * @param left_size the size of the object to the left of the fencepost
 */
inline static void initialize_fencepost(header * fp, size_t left_size) {
	set_state(fp,FENCEPOST);
	set_size(fp, ALLOC_HEADER_SIZE);
	fp->left_size = left_size;
}

/**
 * @brief Helper function to maintain list of chunks from the OS for debugging
 *
 * @param hdr the first fencepost in the chunk allocated by the OS
 */
inline static void insert_os_chunk(header * hdr) {
  if (numOsChunks < MAX_OS_CHUNKS) {
    osChunkList[numOsChunks++] = hdr;
  }
}

/**
 * @brief given a chunk of memory insert fenceposts at the left and 
 * right boundaries of the block to prevent coalescing outside of the
 * block
 *
 * @param raw_mem a void pointer to the memory chunk to initialize
 * @param size the size of the allocated chunk
 */
inline static void insert_fenceposts(void * raw_mem, size_t size) {
  // Convert to char * before performing operations
  char * mem = (char *) raw_mem;

  // Insert a fencepost at the left edge of the block
  header * leftFencePost = (header *) mem;
  initialize_fencepost(leftFencePost, ALLOC_HEADER_SIZE);

  // Insert a fencepost at the right edge of the block
  header * rightFencePost = get_header_from_offset(mem, size - ALLOC_HEADER_SIZE);
  initialize_fencepost(rightFencePost, size - 2 * ALLOC_HEADER_SIZE);
}

/**
 * @brief Allocate another chunk from the OS and prepare to insert it
 * into the free list
 *
 * @param size The size to allocate from the OS
 *
 * @return A pointer to the allocable block in the chunk (just after the 
 * first fencpost)
 */
static header * allocate_chunk(size_t size) {
  void * mem = sbrk(size);
  
  insert_fenceposts(mem, size);
  header * hdr = (header *) ((char *)mem + ALLOC_HEADER_SIZE);
  set_state(hdr, UNALLOCATED);
  set_size(hdr, size - 2 * ALLOC_HEADER_SIZE);
  hdr->left_size = ALLOC_HEADER_SIZE;
  return hdr;
}

/**
 * @brief Helper allocate an object given a raw request size from the user
 *
 * @param raw_size number of bytes the user needs
 *
 * @return A block satisfying the user's request
 */
static inline size_t round_up_mul8(size_t size) {
  if (size % 8 == 0) {
    return size;
  } else {
    return size + 8 - size % 8;
  }
}

static inline size_t max(size_t a, size_t b) {
  if (a > b) {
    return a;
  }
  return b;
}

static inline size_t min(size_t a, size_t b) {
  if (a < b) {
    return a;
  }
  return b;
}

static inline bool is_empty_sentinel(size_t i) {
  if (freelistSentinels[i].next == &freelistSentinels[i]) {
    return true;
  }
  return false;
}

static inline void insert_block(header * block, size_t i) {
  header * head = &freelistSentinels[i];
  head->next->prev = block;
  block->next = head->next;
  head->next = block;
  block->prev = head;
}
static inline header* split_block(header *block, size_t actual_size) {

  int prev_i = min(((get_size(block) -  ALLOC_HEADER_SIZE)/ 8) - 1, N_LISTS - 1);

  size_t rem_size = get_size(block) - actual_size;

  header * b2 = get_header_from_offset(block, rem_size);
  set_size(b2, actual_size);
  b2->left_size = rem_size;

  set_state(b2, ALLOCATED);
  set_size(block, rem_size);
  get_right_header(b2)->left_size = actual_size;

  size_t alloc_rem_size =  rem_size - ALLOC_HEADER_SIZE;
  size_t i = min((alloc_rem_size / 8) - 1, N_LISTS - 1);

  if (i != prev_i) {
    block->prev->next = block->next;
    block->next->prev = block->prev;
    insert_block(block, i);
  }


  return b2;
}

static inline void insert_chunk() {
  header * block = allocate_chunk(ARENA_SIZE);
  header * last_fp = get_left_header(get_left_header(block));
  if (last_fp != lastFencePost) {
    size_t j = min(((get_size(block) - ALLOC_HEADER_SIZE) / 8) - 1, N_LISTS - 1);
    insert_os_chunk(get_left_header(block));
    insert_block(block, j);
    lastFencePost = get_right_header(block);
  } else {
    header * last_chunk = get_left_header(last_fp);
    if (get_state(last_chunk) == ALLOCATED) {
      lastFencePost = get_right_header(block);
      set_state(last_fp, UNALLOCATED);
      set_size(last_fp, get_size(block) + 2 * get_size(lastFencePost));
      lastFencePost->left_size = get_size(last_fp);
      size_t j = min(((get_size(last_fp) - ALLOC_HEADER_SIZE) / 8) - 1, N_LISTS - 1);
      insert_block(last_fp, j);
    } else {
      size_t prev_j = min(((get_size(last_chunk) - ALLOC_HEADER_SIZE) / 8) - 1, N_LISTS - 1);
      lastFencePost = get_right_header(block);
      set_size(last_chunk, get_size(last_chunk) + get_size(block) + 2 * get_size(lastFencePost));
      lastFencePost->left_size = get_size(last_chunk);
      size_t j = min(((get_size(last_chunk) - ALLOC_HEADER_SIZE) / 8) - 1, N_LISTS - 1);
      if (j != prev_j) {
        last_chunk->next->prev = last_chunk->prev;
        last_chunk->prev->next = last_chunk->next;
        insert_block(last_chunk, j);
      }
    }
  }
}
static inline header * allocate_object(size_t raw_size) {
  // TODO implement allocation
//  (void) raw_size;
//  assert(false);
//  exit(1);
  if (raw_size == 0) {
    return NULL;
  }
  // calculating block size and index
  size_t req_size = round_up_mul8(raw_size);
  assert(req_size % 8 == 0);

  size_t actual_size = max(req_size + ALLOC_HEADER_SIZE, sizeof(header));
//  assert(actual_size % 8 == 0);
  size_t alloc_size = actual_size - ALLOC_HEADER_SIZE;
  size_t i = min((alloc_size / 8) - 1, N_LISTS - 1);


  // find the correct sentinel
  while (i < N_LISTS && is_empty_sentinel(i)) {
    i++;
  }

  if (i > N_LISTS - 1) {
    insert_chunk();
    i = min((alloc_size / 8) - 1, N_LISTS - 1);
    while (i < N_LISTS && is_empty_sentinel(i)) {
      i++;
    }
  }

// Cases of allocating

  if (i < N_LISTS - 1) {
    header* block = freelistSentinels[i].next;
    if (get_size(block) - actual_size >= sizeof(header)) {
      header * x = split_block(block, actual_size);
      return (header *) x->data;
    } else {
      block->prev->next = block->next;
      block->next->prev = block->prev;
      set_state(block, ALLOCATED);
      return (header *) block->data;
    }
  } else if (i == N_LISTS - 1) {
      header* block = freelistSentinels[i].next;
      bool more_mem = false;
      while (get_size(block) < actual_size) {
        block = block->next;
        if (block == &freelistSentinels[i]) {
          more_mem = true;
          break;
        }
      }
      if (!more_mem) {
        if (get_size(block) - actual_size >= sizeof(header)) {
          header * x = split_block(block, actual_size);

          return (header *) x->data;
        } else {
          block->prev->next = block->next;
          block->next->prev = block->prev;
          set_state(block, ALLOCATED);
          return (header *) block->data;
        }
      }
  }
  return NULL;
}


/**
 * @brief Helper to get the header from a pointer allocated with malloc
 *
 * @param p pointer to the data region of the block
 *
 * @return A pointer to the header of the block
 */
static inline header * ptr_to_header(void * p) {
  return (header *)((char *) p - ALLOC_HEADER_SIZE); //sizeof(header));
}

/**
 * @brief Helper to manage deallocation of a pointer returned by the user
 *
 * @param p The pointer returned to the user by a call to malloc
 */
static inline void deallocate_object(void * p) {
  // TODO implement deallocation
  //  (void) p;
  //  assert(false);
  //  exit(1);
  // split does not reinsert
  if (p != NULL) {
    header * block = ptr_to_header(p);
    header * left_block = get_left_header(block);
    header * right_block = get_right_header(block);
    if (get_state(block) == ALLOCATED) {
      if (get_state(left_block) != UNALLOCATED && get_state(right_block) != UNALLOCATED) {
        set_state(block, UNALLOCATED);
        size_t alloc_size = get_size(block) - ALLOC_HEADER_SIZE;
        size_t i = min((alloc_size / 8) - 1, N_LISTS - 1);
        insert_block(block, i);
      } else if (get_state(right_block) == UNALLOCATED && get_state(left_block) != UNALLOCATED) {
        set_state(block, UNALLOCATED);
        size_t prev_i = min(((get_size(right_block) - ALLOC_HEADER_SIZE) / 8) - 1, N_LISTS - 1);
        set_size(block, get_size(right_block) + get_size(block));
        header * right_right_block = get_right_header(right_block);
        right_right_block->left_size = get_size(block);
        if (prev_i < N_LISTS - 1) {
          right_block->prev->next = right_block->next;
          right_block->next->prev = right_block->prev;
          size_t i = min(((get_size(block) - ALLOC_HEADER_SIZE) / 8) - 1, N_LISTS - 1);
          if (get_state(left_block) != UNALLOCATED) {
            insert_block(block, i);
          }
        }
      } else if (get_state(left_block) == UNALLOCATED && get_state(right_block) != UNALLOCATED) {
        set_state(block, UNALLOCATED);
        size_t prev_i = min(((get_size(left_block) - ALLOC_HEADER_SIZE) / 8) - 1, N_LISTS - 1);
        set_size(left_block, get_size(left_block) + get_size(block));
        right_block->left_size = get_size(left_block);
        if (prev_i < N_LISTS - 1) {
          left_block->prev->next = left_block->next;
          left_block->next->prev = left_block->prev;
          size_t i = min(((get_size(left_block) - ALLOC_HEADER_SIZE) / 8) - 1, N_LISTS - 1);
          insert_block(left_block, i);
        }
      } else if (get_state(left_block) == UNALLOCATED && get_state(right_block) == UNALLOCATED) {
        set_state(block, UNALLOCATED);
        size_t prev_i = min(((get_size(left_block) - ALLOC_HEADER_SIZE) / 8) - 1, N_LISTS - 1);
        set_size(left_block, get_size(left_block) + get_size(block) + get_size(right_block));
        right_block->left_size = get_size(left_block);
        header * right_right_block = get_right_header(right_block);
        right_right_block->left_size = get_size(left_block);
        right_block->prev->next = right_block->next;
        right_block->next->prev = right_block->prev;
        if (prev_i < N_LISTS - 1) {
          left_block->prev->next = left_block->next;
          left_block->next->prev = left_block->prev;
          size_t i = min(((get_size(left_block) - ALLOC_HEADER_SIZE) / 8) - 1, N_LISTS - 1);
          insert_block(left_block, i);
        }
      }
    } else {
      printf("Double Free Detected\n");
      assert(false);
    }
  }
}

/**
 * @brief Helper to detect cycles in the free list
 * https://en.wikipedia.org/wiki/Cycle_detection#Floyd's_Tortoise_and_Hare
 *
 * @return One of the nodes in the cycle or NULL if no cycle is present
 */
static inline header * detect_cycles() {
  for (int i = 0; i < N_LISTS; i++) {
    header * freelist = &freelistSentinels[i];
    for (header * slow = freelist->next, * fast = freelist->next->next; 
         fast != freelist; 
         slow = slow->next, fast = fast->next->next) {
      if (slow == fast) {
        return slow;
      }
    }
  }
  return NULL;
}

/**
 * @brief Helper to verify that there are no unlinked previous or next pointers
 *        in the free list
 *
 * @return A node whose previous and next pointers are incorrect or NULL if no
 *         such node exists
 */
static inline header * verify_pointers() {
  for (int i = 0; i < N_LISTS; i++) {
    header * freelist = &freelistSentinels[i];
    for (header * cur = freelist->next; cur != freelist; cur = cur->next) {
      if (cur->next->prev != cur || cur->prev->next != cur) {
        return cur;
      }
    }
  }
  return NULL;
}

/**
 * @brief Verify the structure of the free list is correct by checkin for 
 *        cycles and misdirected pointers
 *
 * @return true if the list is valid
 */
static inline bool verify_freelist() {
  header * cycle = detect_cycles();
  if (cycle != NULL) {
    fprintf(stderr, "Cycle Detected\n");
    print_sublist(print_object, cycle->next, cycle);
    return false;
  }

  header * invalid = verify_pointers();
  if (invalid != NULL) {
    fprintf(stderr, "Invalid pointers\n");
    print_object(invalid);
    return false;
  }

  return true;
}

/**
 * @brief Helper to verify that the sizes in a chunk from the OS are correct
 *        and that allocated node's canary values are correct
 *
 * @param chunk AREA_SIZE chunk allocated from the OS
 *
 * @return a pointer to an invalid header or NULL if all header's are valid
 */
static inline header * verify_chunk(header * chunk) {
	if (get_state(chunk) != FENCEPOST) {
		fprintf(stderr, "Invalid fencepost\n");
		print_object(chunk);
		return chunk;
	}
	
	for (; get_state(chunk) != FENCEPOST; chunk = get_right_header(chunk)) {
		if (get_size(chunk)  != get_right_header(chunk)->left_size) {
			fprintf(stderr, "Invalid sizes\n");
			print_object(chunk);
			return chunk;
		}
	}
	
	return NULL;
}

/**
 * @brief For each chunk allocated by the OS verify that the boundary tags
 *        are consistent
 *
 * @return true if the boundary tags are valid
 */
static inline bool verify_tags() {
  for (size_t i = 0; i < numOsChunks; i++) {
    header * invalid = verify_chunk(osChunkList[i]);
    if (invalid != NULL) {
      return invalid;
    }
  }

  return NULL;
}

/**
 * @brief Initialize mutex lock and prepare an initial chunk of memory for allocation
 */
static void init() {
  // Initialize mutex for thread safety
  pthread_mutex_init(&mutex, NULL);

#ifdef DEBUG
  // Manually set printf buffer so it won't call malloc when debugging the allocator
  setvbuf(stdout, NULL, _IONBF, 0);
#endif // DEBUG

  // Allocate the first chunk from the OS
  header * block = allocate_chunk(ARENA_SIZE);

  header * prevFencePost = get_header_from_offset(block, -ALLOC_HEADER_SIZE);
  insert_os_chunk(prevFencePost);

  lastFencePost = get_header_from_offset(block, get_size(block));

  // Set the base pointer to the beginning of the first fencepost in the first
  // chunk from the OS
  base = ((char *) block) - ALLOC_HEADER_SIZE; //sizeof(header);

  // Initialize freelist sentinels
  for (int i = 0; i < N_LISTS; i++) {
    header * freelist = &freelistSentinels[i];
    freelist->next = freelist;
    freelist->prev = freelist;
  }

  // Insert first chunk into the free list
  header * freelist = &freelistSentinels[N_LISTS - 1];
  freelist->next = block;
  freelist->prev = block;
  block->next = freelist;
  block->prev = freelist;
}

/* 
 * External interface
 */
void * my_malloc(size_t size) {
  pthread_mutex_lock(&mutex);
  header * hdr = allocate_object(size); 
  pthread_mutex_unlock(&mutex);
  return hdr;
}

void * my_calloc(size_t nmemb, size_t size) {
  return memset(my_malloc(size * nmemb), 0, size * nmemb);
}

void * my_realloc(void * ptr, size_t size) {
  void * mem = my_malloc(size);
  memcpy(mem, ptr, size);
  my_free(ptr);
  return mem; 
}

void my_free(void * p) {
  pthread_mutex_lock(&mutex);
  deallocate_object(p);
  pthread_mutex_unlock(&mutex);
}

bool verify() {
  return verify_freelist() && verify_tags();
}
