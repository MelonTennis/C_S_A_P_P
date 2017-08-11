/*
 * mm.c
 *
 *
 * Dynamic storage allocator for C programs.
 * Implement functions mm_init, malloc, free, realloc, calloc, mm_checkheap.
 *
 * Mini size of block is 16(dsize).
 * Each block has a header contains message of block's size, one byte to show
 * whether previous block is alloc and one byte to show whether previous
 * block's size = 16 byte. And one byte showing whether this block is alloc.
 * Free blocks whose size is more than 16 byte has footer, alloc block do not
 * have footer. 16 byte blocks do not have footer.
 * Free blocks size more than 16 bytes is managed by double linked list,
 * 16 byte free block is managed by single linked list.
 *
 * Free blocks with different size is arranged by segregated list.
 * For segregated list, size index small than BUCKET_THRE use first fit policy
 * to get free block. Bucket index more than BUCKET_THRE, use best fit policy.
 * The segregated list has 15 buckets. First LINEAR_THRE bucket size increase
 * linearly, then other bucket size increase by powers of 2.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "mm.h"
#include "memlib.h"

/*
 * If you want debugging output, uncomment the following. Be sure not
 * to have debugging enabled in your final submission
 */
//#define DEBUG

#ifdef DEBUG
/* When debugging is enabled, the underlying functions get called */
#define dbg_printf(...) printf(__VA_ARGS__)
#define dbg_assert(...) assert(__VA_ARGS__)
#define dbg_requires(...) assert(__VA_ARGS__)
#define dbg_ensures(...) assert(__VA_ARGS__)
#define dbg_checkheap(...) mm_checkheap(__VA_ARGS__)
#define dbg_printheap(...) print_heap(__VA_ARGS__)
#else
/* When debugging is disabled, no code gets generated */
#define dbg_printf(...)
#define dbg_assert(...)
#define dbg_requires(...)
#define dbg_ensures(...)
#define dbg_checkheap(...)
#define dbg_printheap(...)
#endif

/* do not change the following! */
#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#define memset mem_memset
#define memcpy mem_memcpy
#endif /* def DRIVER */

/* What is the correct alignment? */
#define ALIGNMENT 16
// 15 segregated group in total
#define SEG_NUM 15
// segregated group number < BUCKET_THRE use first fit, else use best fit
#define BUCKET_THRE 12
// segregated group number < LINEAR_THRE, max size of group increase linear
// segregated group number > LINEAR_THRE, max size increase by 2 power
#define LINEAR_THRE 8

/* Basic constants */
typedef uint64_t word_t;
// word and header and footer size (bytes)
static const size_t wsize = sizeof(word_t);
// double word size (bytes)
static const size_t dsize = 2 * wsize;
// minimum block size
static const size_t min_block_size = dsize;
// requires (chunksize % 16 == 0)
static const size_t chunksize = (1 << 12);

static const word_t alloc_mask = (word_t) 0x1;
static const word_t size_mask = ~(word_t) 0xF;
// 1 if previous block is alloc
static const word_t pre_mask = (word_t) 0x2;
// 1 if previous block is dsize
static const word_t pre_dsize = (word_t) 0x4;

typedef struct block_free
{
    // header: size + allocation flag
    word_t header;
    // payload with unknown size
    char payload[0];
    // ptr to next
    struct block_free *ptr_next;
    // ptr to prev;
    struct block_free *ptr_prev;
    // unknown start position footer not declared
} block_t;

/* global vars */
// ptr to first block
static block_t *head_start = NULL;
// size <= 16 singly linked list, size > 16 double linked list
static block_t *seg_list[SEG_NUM];

// helper functions
static size_t align(size_t x);
static word_t pack(size_t size, bool alloc);
static void write_header(block_t *block, size_t size, bool alloc);
static void write_footer(block_t *block, size_t size, bool alloc);

static size_t get_size(block_t *block);
static size_t extract_size(word_t word);
static bool extract_alloc(word_t word);
static bool get_alloc(block_t *block);
static block_t *payload_to_header(void *bp);
static size_t max(size_t x, size_t y);
static void *header_to_payload(block_t *block);
static word_t get_payload_size(block_t *block);

static block_t *find_next(block_t *block);
static block_t *find_prev(block_t *block);
static word_t *find_prev_footer(block_t *block);
static block_t *find_next_free(block_t *block);

static block_t *find_first_fit(block_t *head, size_t asize);
static block_t *find_seg_fit(size_t asize);

static bool aligned(const void *p);
static bool in_heap(const void *p);
static size_t round_up(size_t size, size_t n);
static void set_ptr_prev(block_t *block);
static void print_list();

static void insert_seg(block_t *block);
static void remove_seg(block_t *block);
static void insert_single(block_t *block, block_t *prev, block_t *next);
static void insert_double(block_t *block, block_t *prev, block_t *next);
static void remove_dsize_block(block_t *block);

static void init_all_head(block_t *block);
static void init_group_head(block_t *block);
static int find_group(size_t asize);
static block_t *find_group_head(size_t asize);
static int find_search_group(size_t asize);
static size_t group_thre(int idx);

static block_t *coalesce_seg(block_t *block);
static block_t *extend_heap_seg(size_t size);
static void place_alloc_seg(block_t *block, size_t asize);

static word_t pack_prev(word_t word, bool alloc);
static void add_prev_header(block_t *block, bool pre_alloc, bool is_dize);
static bool get_prev_dsize(block_t *block);
static word_t pack_dsize(word_t word, bool is_dsize);
static bool get_prev_alloc(block_t *block);
static block_t *find_dsize_prev(block_t *block);
static block_t *find_best_fit(block_t *head, size_t asize);

/* rounds up to the nearest multiple of ALIGNMENT */
static size_t align(size_t x)
{
    return ALIGNMENT * ((x + ALIGNMENT - 1) / ALIGNMENT);
}

/*
 * Initialize: return false on error, true on success.
 */
bool mm_init(void)
{
    // create initial empty heap
    init_all_head(NULL);
    word_t *start = (word_t *) (mem_sbrk(2 * wsize));
    if (start == (void *) -1)
    {
        return false;
    }
    start[0] = pack(0, true);  // prologue footer
    start[1] = pack(0, true);  // epilogue header
    start[0] = pack_prev(start[0], false);
    head_start = (block_t *) &(start[1]);
    add_prev_header(head_start, true, false);
    dbg_requires(head_start != NULL);
    if (extend_heap_seg(chunksize) == NULL)
    {
        return false;
    }
    dbg_ensures(mm_checkheap(__LINE__));
    return true;
}

/*
 * malloc: returns a pointer to an allocated block payload of at least
 *         size bytes. Minimum size is 16 byte. Entire block contains
 *         header but not footer. Should not overlap with other blocks.
 *         Search in segregated list to find one free block.
 */
void *malloc(size_t size)
{

    size_t asize; // adjusted block size
    size_t extendsize; // amount to extend heap if no fit found
    block_t *block;
    void *bp = NULL;
    // if not initialized
    if (head_start == NULL)
    {
        mm_init();
    }
    // ignore spurious request
    if (size == 0)
    {
        dbg_ensures(mm_checkheap(__LINE__));
        return bp;
    }
    // alignment to dsize
    asize = round_up(size + wsize, dsize);
    // find in segregated list
    block = find_seg_fit(asize);
    // if not found, request more memory from heap
    if (block == NULL)
    {
        extendsize = max(asize, chunksize);
        block = extend_heap_seg(extendsize);
        if (block == NULL)
        {
            return bp;
        }
    }
    place_alloc_seg(block, asize);
    bp = header_to_payload(block);
    dbg_ensures(mm_checkheap(__LINE__));
    return bp;
}

/*
 * free: frees the block pointed to by ptr. It returns nothing.
 *       only guaranteed to work when ptr was returned by  malloc,
 *       calloc, or realloc and has not yet been freed.
 *       free(NULL) has no effect.
 *       Freed block was insert into segregated list.
 */
void free(void *ptr)
{
    // if ptr free do nothing
    if (ptr == NULL)
    {
        return;
    }
    block_t *block = payload_to_header(ptr);
    dbg_requires(get_alloc(block));
    size_t size = get_size(block);
    dbg_requires(size >= min_block_size);
    // prev block info
    bool pre_alloc = get_prev_alloc(block);
    bool pre_dsize = get_prev_dsize(block);
    write_header(block, size, false);
    if (size > dsize)
    {
        write_footer(block, size, false);
    }
    // resume previous status
    add_prev_header(block, pre_alloc, pre_dsize);
    add_prev_header(find_next(block), false, size == dsize);
    insert_seg(block);
    block = coalesce_seg(block);
    dbg_ensures(mm_checkheap(__LINE__));
}

/*
 * realloc: returns a pointer to an allocated region of at least size bytes
 *          if ptr == NULL, equivalent to malloc(size);
 *          if size == zero, equivalent to free(ptr) and return NULL
 *          else takes an existing block of memory, pointed to by ptr.
 *          return new block with same content, size is min(old size, new size)
 */
void *realloc(void *ptr, size_t size)
{
    block_t *block = payload_to_header(ptr);
    size_t copysize;
    void *newptr;

    // If size == 0, then free block and return NULL
    if (size == 0)
    {
        free(ptr);
        return NULL;
    }

    // If ptr is NULL, then equivalent to malloc
    if (ptr == NULL)
    {
        return malloc(size);
    }

    // Otherwise, proceed with reallocation
    newptr = malloc(size);
    // If malloc fails, the original block is left untouched
    if (newptr == NULL)
    {
        return NULL;
    }

    // Copy the old data
    copysize = get_payload_size(block); // gets size of old payload
    if (size < copysize)
    {
        copysize = size;
    }
    memcpy(newptr, ptr, copysize);

    // Free the old block
    free(ptr);
    return newptr;
}

/*
 * calloc: allocates memory for an array of nmemb elements of size bytes each
 *         returns a pointer to the allocated memory.
 *         The memory is set to zero before returning.
 */
void *calloc(size_t elements, size_t size)
{
    void *bp;
    size_t asize = elements * size;

    if (asize / elements != size)
        // Multiplication overflowed
        return NULL;

    bp = malloc(asize);
    if (bp == NULL)
    {
        return NULL;
    }
    // Initialize all bits to 0
    memset(bp, 0, asize);

    return bp;
}


/*
 *****************************************************************************
 * Do not delete the following super-secret(tm) lines,                       *
 * except if you're replacing the entire code in this file                   *
 * with the entire code contained in mm-baseline.c!                          *
 *                                                                           *
 * 54 68 69 73 20 69 73 20 61 20 73 75 62 6c 69 6d 69 6e 61 6c               *
 *                                                                           *
 * 20 6d 65 73 73 61 67 69 6e 67 20 65 6e 63 6f 75 72 61 67 69               *
 * 6e 67 20 79 6f 75 20 74 6f 20 73 74 61 72 74 20 77 69 74 68               *
 * 20 74 68 65 20 63 6f 64 65 20 69 6e 20 6d 6d 2d 62 61 73 65               *
 *                                                                           *
 * 6c 69 6e 65 2e 63 21 20 2d 44 72 2e 20 45 76 69 6c 0a de ad               *
 * be ef 0a 0a 0a 0a 0a 0a 0a 0a 0a 0a 0a 0a 0a 0a 0a 0a 0a 0a               *
 *                                                                           *
 *****************************************************************************
 */

/*
 * mm_checkheap: scans heap and segregated list to find possible errors
 *               input is current line number
 *               if any error print out error message at current line
 *               if error return FALSE else return TRUE
 *
 */
bool mm_checkheap(int lineno)
{
    block_t *heap_start = head_start;
    // check epilogue and prologue blocks
    block_t *prologue = (block_t *)((char *)heap_start - wsize);
    if(prologue != (block_t *)mem_heap_lo())
    {
        dbg_printf("err: prologue address fault\n");
    }
    block_t *epilogue = (block_t *)mem_heap_hi();
    if(!get_alloc(prologue) || get_size(prologue) != 0)
    {
        dbg_printf("err: prologue block check false\n");
    }
    if(!get_alloc(epilogue) || get_size(epilogue) != 0)
    {
        dbg_printf("err: epilogue block check false\n");
    }
    block_t *block;
    int cnt_free_heap = 0, cnt_free_list = 0;

    // block
    for (block = heap_start; block != NULL && get_size(block) > 0;
         block = find_next(block))
    {
        // payload address aligned
        if (!aligned(header_to_payload(block)))
        {
            dbg_printf("err: block %p not aligned\n", block);
            return false;
        }

        // size
        if (get_size(block) < min_block_size)
        {
            dbg_printf("err: block %zu size < min\n", get_size(block));
            return false;
        }

        // size
        if (get_size(block) % dsize != 0){
            dbg_printf("err: block %zu size not aligned\n", get_size(block));
            return false;
        }


        // heap boundary
        if (!in_heap(block))
        {
            dbg_printf("err: %zu block out of heap\n", get_size(block));
            return false;
        }

        // coalesce
        if (!get_alloc(block) && !get_alloc(find_next(block)))
        {
            dbg_printf("err: free block %zu not coalesce\n", get_size(block));
            return false;
        }

        // header match footer
        if (get_size(block) > dsize && !get_alloc(block))
        {
            word_t *footerp = (word_t *) ((block->payload) +
                    get_size(block) - dsize);
            if (block->header != *footerp)
            {
                dbg_printf("err: %zu block header not match footer\n",
                           get_size(block));
                dbg_printf("header is %lu, footer is %lu\n",
                           block->header, *footerp);
                return false;
            }
        }

        // pre_alloc consistency
        if (get_size(block) == dsize)
        {
            if (!get_prev_dsize(find_next(block)))
            {
                dbg_printf("err: %zu block is dsize but not mark next block\n",
                           get_size(block));
                return false;
            }
        }
        if (get_alloc(block) != get_prev_alloc(find_next(block)))
        {
            dbg_printf("err: %zu block alloc not set right next block\n",
                       get_size(block));
            return false;
        }

        if (!get_alloc(block))
        {
            cnt_free_heap++;
        }

        if (!get_alloc(block))
        {
            dbg_printf("freeheap size %zu ", get_size(block));
        }
    }

    // seg list
    for (int bs = 0; bs < SEG_NUM; bs++)
    {
        block = seg_list[bs];
        if (block == NULL)
        {
            dbg_assert(seg_list[bs] == NULL);
        }
        else
        {
            while (block != NULL)
            {
                // prev next consistent
                if (get_size(block) > dsize)
                {
                    if (block->ptr_prev == NULL)
                    {
                        dbg_requires(seg_list[bs] == block);
                    }
                    else if (block->ptr_prev->ptr_next != block)
                    {
                        dbg_printf("err: %zu block prev not consistent\n",
                                   get_size(block));
                        return false;
                    }
                }

                // alloc
                if (get_alloc(block))
                {
                    dbg_printf("err: %zu block alloc in free list\n",
                               get_size(block));
                    return false;
                }

                // in heap
                if (!in_heap(block))
                {
                    dbg_printf("err: %zu block out of heap\n", get_size(block));
                    return false;
                }

                // in bucket range
                if(bs < SEG_NUM -1 && get_size(block) > group_thre(bs))
                {
                    dbg_printf("err: %zu block out of bucket upper range\n",
                               get_size(block));
                    return false;
                }
                if(bs > 0 && get_size(block) <= group_thre(bs - 1))
                {
                    dbg_printf("err: %zu block out of bucket lower range\n",
                               get_size(block));
                    return false;
                }
                cnt_free_list++;
                block = block->ptr_next;
            }
        }
    }

    // check whether all free blocks are in free block list
    if (cnt_free_heap != cnt_free_list)
    {
        dbg_printf("cnt heap free = %d, cnt list free = %d\n",
                   cnt_free_heap, cnt_free_list);
        dbg_printf("err: free block cnt not match\n");
        return false;
    }
    return true;
}

/*
 * extend_heap_seg: if no free block is fit for current malloc requirement
 *                  extend heap with size of space
 *                  if false extend heap return NULL, or return pointer to
 *                  free block whose size >= size
 */
static block_t *extend_heap_seg(size_t size)
{
    void *bp;
    size = align(size);
    if ((bp = mem_sbrk(size)) == (void *) -1)
    {
        return NULL;
    }
    // initialize free block header/footer/ptr
    block_t *block = payload_to_header(bp);
    // pre block characters
    bool pre_alloc = get_prev_alloc(block);
    bool pre_dsize = get_prev_dsize(block);
    write_header(block, size, false);
    write_footer(block, size, false);
    add_prev_header(block, pre_alloc, pre_dsize);
    dbg_assert(block != NULL);
    // find right head
    insert_seg(block);
    // Create new epilogue header
    block_t *block_next = find_next(block);
    write_header(block_next, 0, true);
    block = coalesce_seg(block);
    dbg_ensures(mm_checkheap(__LINE__));
    return block;
}

/*
 * find_seg_fit: find a free block in segregated list
 *               before bucket BUCKET_THRE use first fit
 *               after bucket BUCKET_THRE use best fit
 */
static block_t *find_seg_fit(size_t asize)
{
    int search_group = find_search_group(asize);
    if (search_group < 0)
    {
        return NULL;
    }
    block_t *group_head = seg_list[search_group];
    block_t *block;
    if (search_group < BUCKET_THRE)
    {
        block = find_first_fit(group_head, asize);
    }
    else
    {
        block = find_best_fit(group_head, asize);
    }
    if (block == NULL)
    {
        for (int i = search_group + 1; i < SEG_NUM; i++)
        {
            if (seg_list[i] != NULL)
            {
                group_head = seg_list[i];
                break;
            }
        }
        if (search_group < BUCKET_THRE)
        {
            block = find_first_fit(group_head, asize);
        }
        else
        {
            block = find_best_fit(group_head, asize);
        }
    }
    return block;
}

/*
 * find_best_fit: for segregated list whose head is head
 *                use find best policy to find a free block
 *                whose size is no less than asize
 */
static block_t *find_best_fit(block_t *head, size_t asize)
{
    block_t *block = head;
    block_t *best = NULL;
    while (block != NULL && get_size(block) > 0)
    {
        if (asize <= get_size(block))
        {
            if(best == NULL || get_size(best) > get_size(block))
            {
                best = block;
            }
        }
        block = block->ptr_next;
    }
    return best;
}

/*
 * find_first_fit: for segregated list whose head is head
 *                use find first policy to find a free block
 *                whose size is no less than asize
 */
static block_t *find_first_fit(block_t *head, size_t asize)
{
    block_t *block = head;
    while (block != NULL && get_size(block) > 0)
    {
        if (asize <= get_size(block))
        {
            return block;
        }
        block = block->ptr_next;
    }
    return NULL;
}

/*
 * place_alloc_seg: place alloc block size asize in a free block block
 *                  if block size minue asize is more than mini_block_size
 *                  generate a new free block by remain size and insert it
 *                  in free list. Remove block from free list.
 */
static void place_alloc_seg(block_t *block, size_t asize)
{
    size_t csize = get_size(block);
    // character of block before block
    bool pre_alloc = get_prev_alloc(block);
    bool pre_dsize = get_prev_dsize(block);
    // new free block in free list
    if ((csize - asize) >= min_block_size)
    {
        size_t rsize = csize - asize;
        block_t *block_next;
        remove_seg(block);
        write_header(block, asize, true);
        block_next = find_next(block);
        // dize block without footer
        if (rsize == dsize)
        {
            write_header(block_next, rsize, false);
        }
        else
        {
            write_header(block_next, rsize, false);
            write_footer(block_next, rsize, false);
        }
        insert_seg(block_next);
        add_prev_header(block_next, true, asize == dsize);
        add_prev_header(find_next(block_next), false, rsize == dsize);
    }
    else
    {
        write_header(block, csize, true);
        block_t *block_next = find_next(block);
        add_prev_header(block_next, true, csize == dsize);
        remove_seg(block);
    }
    // resume pre block character
    add_prev_header(block, pre_alloc, pre_dsize);
    dbg_ensures(mm_checkheap(__LINE__));
}

/*
 * coalesce_seg: check block's previous block and next block
 *                        if free then combined adjacent free blocks
 *                        case 1 means no adjacent free block
 *                        case 2 means next block is free, coalesce block
 *                        with next block to a combined block
 *                        case 3 means previous block is free, coalesce block
 *                        with previous block to a combined block
 *                        case 4 means previous and next block are free
 *                        coalesce three blocks into one free block
 *                        adjust place in segregated list after coalesce
 */
static block_t *coalesce_seg(block_t *block)
{
    block_t *block_next = find_next(block);
    block_t *block_prev = find_prev(block);
    // alloc status
    bool prev_alloc = get_prev_alloc(block);
    bool next_alloc = get_alloc(block_next);
    size_t size = get_size(block);

    // case 1
    if (prev_alloc && next_alloc)
    {
        return block;
    }
    // case 2
    else if (prev_alloc && !next_alloc)
    {
        remove_seg(block_next);
        remove_seg(block);
        bool pre_alloc = get_prev_alloc(block);
        bool pre_dsize = get_prev_dsize(block);
        size += get_size(block_next);
        write_header(block, size, false);
        if (size != dsize)
        {
            write_footer(block, size, false);
        }
        // maintain previous block status
        add_prev_header(block, pre_alloc, pre_dsize);
        add_prev_header(find_next(block), false, size == dsize);
        insert_seg(block);
    }
    // case 3
    else if (!prev_alloc && next_alloc)
    {
        if (size == dsize)
        {
            add_prev_header(block_next, false, false);
        }
        remove_seg(block_prev);
        size += get_size(block_prev);
        // previous block of prev block
        bool pre_pre_alloc = get_prev_alloc(block_prev);
        bool pre_pre_dsize = get_prev_dsize(block_prev);
        remove_seg(block);
        write_header(block_prev, size, false);
        write_footer(block_prev, size, false);
        add_prev_header(block_prev, pre_pre_alloc, pre_pre_dsize);
        block = block_prev;
        add_prev_header(find_next(block), false, size == dsize);
        insert_seg(block);
    }
    // case 4
    else if (!prev_alloc && !next_alloc)
    {
        if (get_size(block_next) == dsize)
        {
            add_prev_header(find_next(block_next), false, false);
        }
        remove_seg(block_prev);
        size += get_size(block_prev) + get_size(block_next);
        bool pre_pre_alloc = get_prev_alloc(block_prev);
        bool pre_pre_dsize = get_prev_dsize(block_prev);
        remove_seg(block_next);
        remove_seg(block);
        write_header(block_prev, size, false);
        write_footer(block_prev, size, false);
        add_prev_header(block_prev, pre_pre_alloc, pre_pre_dsize);
        block = block_prev;
        add_prev_header(find_next(block), false, size == dsize);
        insert_seg(block);
    }
    dbg_ensures(mm_checkheap(__LINE__));
    return block;
}

/*
 * print_list: print all free blocks in seg_list
 */
static void print_list()
{
    for (int i = 0; i < SEG_NUM; i++)
    {
        block_t *block = seg_list[i];
        int cnt = 0;
        while (block != NULL && get_size(block) > 0)
        {
            cnt++;
            dbg_printf("block %d: size: %zu ", cnt, get_size(block));
            block = block->ptr_next;
        }
        dbg_printf("\n");
    }
    return;
}

/*
 * find_group: giving size of block, which is asize
 *             return the group index in segregated list
 */
static int find_group(size_t asize)
{
    if (asize == dsize)
    {
        return 0;
    }
    for (int i = 1; i < SEG_NUM - 1; i++)
    {
        if (group_thre(i) >= asize)
        {
            return i;
        }
    }
    return SEG_NUM - 1;
}

/*
 * find_group_head: giving size of block, which is asize
 *                  return the group head in segregated list
 *                  if no free blocks in that group return NULL
 */
static block_t *find_group_head(size_t asize)
{
    int head = find_group(asize);
    dbg_requires(head >= 0 && head < SEG_NUM);
    return seg_list[head];
}

/*
 * find_search_group: given a block size asize, find the index of group
 *                    for searching for a free block
 *                    if the segregated list head of asize is NULL
 *                    return the next head index which is not NULL
 *                    if no head is found return -1
 */
static int find_search_group(size_t asize)
{
    if (asize == dsize && seg_list[0] != NULL)
    {
        return 0;
    } else
    {
        for (int i = 1; i < SEG_NUM -1; i++)
        {
            if (group_thre(i) >= asize && seg_list[i] != NULL)
            {
                return i;
            }
        }
        if(seg_list[SEG_NUM - 1] != NULL){
            return SEG_NUM - 1;
        }
    }
    return -1;
}

/*
 * group_thre: return max size of seg_list[idx]
 *
 */
static size_t group_thre(int idx){
    // min block size dsize
    if(idx == 0) return dsize;
    // then max size increase linear
    if(idx < LINEAR_THRE)
    {
        return idx * dsize * 2;
    // then max size increase by 2 power
    }else if(idx < SEG_NUM - 1)
    {
        return (1 << idx)*2;
    }
    return SEG_NUM - 1;
}

/*
 * insert_single: insert block into single linked list,
 *                insert block between prev and next
 */
static void insert_single(block_t *block, block_t *prev, block_t *next)
{
    dbg_assert(prev != NULL);
    dbg_assert(block != NULL);
    block->ptr_next = next;
    prev->ptr_next = block;
}

/*
 * insert_double: insert block into double linked list,
 *                insert block between prev and next
 */
static void insert_double(block_t *block, block_t *prev, block_t *next)
{
    dbg_requires(block != NULL);
    block->ptr_prev = prev;
    block->ptr_next = next;
    if (prev != NULL)
    {
        prev->ptr_next = block;
    }
    if (next != NULL)
    {
        next->ptr_prev = block;
    }
    dbg_ensures(mm_checkheap(__LINE__));
}

/*
 * insert_seg: insert block into segregated list
 *             always insert to the head of each list
 */
static void insert_seg(block_t *block)
{
    dbg_assert(block != NULL);
    size_t size = get_size(block);
    // dsize, single list
    if (size == dsize)
    {
        if (seg_list[0] == NULL)
        {
            seg_list[0] = block;
            block->ptr_next = NULL;
        }
        else
        {
            block->ptr_next = seg_list[0];
            seg_list[0] = block;
        }
        return;
    }
    // other, double linked list
    else
    {
        int group = find_group(size);
        block_t *group_head = find_group_head(size);
        if (group_head != NULL)
        {
            block->ptr_prev = NULL;
            block->ptr_next = group_head;
            group_head->ptr_prev = block;
            seg_list[group] = block;
        } else
        {
            seg_list[group] = block;
            block->ptr_next = NULL;
            block->ptr_prev = NULL;
        }
    }
}

/*
 * init_all_head: initialize all head in segregated list to block
 *                only when block == NULL, this method should be used
 */
static void init_all_head(block_t *block)
{
    if (block == NULL)
    {
        for (int i = 0; i < SEG_NUM; i++)
        {
            seg_list[i] = NULL;
        }
        return;
    }
    dbg_requires(block == NULL);
}

/*
 * init_group_head: initialize block to its bucket head
 */
static void init_group_head(block_t *block)
{
    dbg_requires(block != NULL);
    size_t asize = get_size(block);
    int group = find_group(asize);
    dbg_requires(seg_list[group] == NULL);
    seg_list[group] = block;
    return;
}

/*
 * remove_dsize_block: remove a dsize block from segregated list
 */
static void remove_dsize_block(block_t *block)
{
    dbg_assert(get_size(block) == dsize);
    block_t *next = block->ptr_next;
    if (seg_list[0] == block)
    {
        seg_list[0] = next;
        block->ptr_next = NULL;
        return;
    }
    block_t *prev = find_dsize_prev(block);
    prev->ptr_next = next;
    block->ptr_next = NULL;
    return;
}

/*
 * find_dsize_prev: in dsize single linked list, find the previous
 *                  dsize block whose next is point to block
 */
static block_t *find_dsize_prev(block_t *block)
{
    dbg_assert(seg_list[0] != NULL);
    if (block == seg_list[0]) return NULL;
    block_t *cur_block = seg_list[0];
    block_t *prev = seg_list[0];
    while (cur_block != block)
    {
        dbg_assert(cur_block != NULL);
        prev = cur_block;
        cur_block = cur_block->ptr_next;
    }
    return prev;
}

/*
 * remove_seg: remove a free block from segregated list
 *             find its bucket and remove from linked list
 */
static void remove_seg(block_t *block)
{
    dbg_assert(block != NULL);
    // dsize remove from single linked list
    if (get_size(block) == dsize)
    {
        remove_dsize_block(block);
        return;
    }
    // remove from double linked list
    block_t *prev = block->ptr_prev;
    block_t *next = block->ptr_next;
    if (prev != NULL)
    {
        prev->ptr_next = next;
    }
    if (next != NULL)
    {
        next->ptr_prev = prev;
    }
    block->ptr_prev = NULL;
    block->ptr_next = NULL;
    block_t *group_head = find_group_head(get_size(block));
    if (group_head == block)
    {
        int group = find_group(get_size(block));
        if (next != NULL && find_group(get_size(next)) == group)
        {
            seg_list[group] = next;
        }
        else
        {
            seg_list[group] = NULL;
        }
    }
}

/*
 * add_prev_header: add information of whether adjacent previous block is
 *                  alloc and dsize to current block's header and footer if
 *                  block has footer
 */
static void add_prev_header(block_t *block, bool pre_alloc, bool pre_dsize)
{
    dbg_assert(block != NULL);
    block->header = pack_prev(block->header, pre_alloc);
    block->header = pack_dsize(block->header, pre_dsize);
    size_t size = get_size(block);
    if (size > dsize && !get_alloc(block))
    {
        word_t *footerp = (word_t *)((block->payload) + size - dsize);
        *footerp = pack_prev(*footerp, pre_alloc);
        *footerp = pack_dsize(*footerp, pre_dsize);
    }
}

/*
 * pack_prev: add adjacent previous block alloc status into current
 *            block header/footer, if alloc 1
 */
static word_t pack_prev(word_t word, bool alloc)
{
    return alloc ? (word | pre_mask) : (word & ~pre_mask);
}

/*
 * pack_dsize: add adjacent previous block dsize status into current
 *             block header/footer, if dsize 1
 */
static word_t pack_dsize(word_t word, bool is_dsize)
{
    return is_dsize ? (word | pre_dsize) : (word & ~pre_dsize);
}

/*
 * Return whether the pointer is in the heap.
 * May be useful for debugging.
 */
static bool in_heap(const void *p)
{
    return p <= mem_heap_hi() && p >= mem_heap_lo();
}

/*
 * Return whether the pointer is aligned.
 * May be useful for debugging.
 */
static bool aligned(const void *p)
{
    size_t ip = (size_t) p;
    return align(ip) == ip;
}

/*
 * get_payload_size: returns the payload size of a given block, equal to
 *                   the entire block size minus the header
 *                   free block payload = 0, no need minus ptr size
 *                   only used for alloc block, no footer
 */
static word_t get_payload_size(block_t *block)
{
    dbg_assert(get_alloc(block));
    size_t asize = get_size(block);
    return asize - wsize;
}

/*
 * extract_alloc: returns the allocation status of a given header value based
 *                on the header specification above.
 */
static bool extract_alloc(word_t word)
{
    return (bool) (word & alloc_mask);
}

/*
 * get_alloc: returns true when the block is allocated based on the
 *            block header's lowest bit, and false otherwise.
 */
static bool get_alloc(block_t *block)
{
    return extract_alloc(block->header);
}

/*
 * get_prev_alloc: extract from current block header
 *                 if adjacent previous block is free return true
 *                 else return false
 */
static bool get_prev_alloc(block_t *block)
{
    return (bool) (block->header & pre_mask);
}

/*
 * get_prev_alloc: extract from current block header
 *                 if adjacent previous block is dsize block return true
 *                 else return false
 */
static bool get_prev_dsize(block_t *block)
{
    return (bool) (block->header & pre_dsize);
}

/*
 * find_prev: returns the previous block position by checking the previous
 *            block's footer and calculating the start of the previous block
 *            based on its size if previous is free and not dsize
 *            if previous block is dsize, address minus dsize previous block
 *            if previous block is alloc, this method should not be called
 */
static block_t *find_prev(block_t *block)
{
    if (get_prev_dsize(block))
    {
        return (block_t *) ((char *) block - dsize);
    } else
    {
        word_t *footerp = find_prev_footer(block);
        size_t size = extract_size(*footerp);
        if (size != 0)
        {
            return (block_t *) ((char *) block - size);
        } else
        {
            return (block_t *) ((char *) block - wsize);
        }

    }
}

/*
 * find_prev_footer: returns the footer of the previous block.
 *                   only for free previous block and size != dsize
 */
static word_t *find_prev_footer(block_t *block)
{
    // Compute previous footer position as one word before the header
    return (&(block->header)) - 1;
}

/*
 * find_next: returns the next consecutive block on the heap by adding the
 *            size of the block.
 */
static block_t *find_next(block_t *block)
{
    dbg_requires(block != NULL);
    block_t *block_next = (block_t *) (((char *) block) + get_size(block));
    dbg_ensures(block_next != NULL);
    return block_next;
}

/*
 * find_next_free: returns the next free block on the heap
 */
static block_t *find_next_free(block_t *block)
{
    dbg_printf("find_next_free\n");
    dbg_requires(block != NULL);
    block_t *next_free;
    for (next_free = block; get_size(next_free) > 0;
         next_free = find_next(next_free))
    {
        if (!get_alloc(next_free))
        {
            break;
        }
    }
    dbg_ensures(next_free != NULL);
    if (get_alloc(next_free))
    {
        return NULL;
    }
    return next_free;
}

/*
 * header_to_payload: given a block pointer, returns a pointer to the
 *                    corresponding payload.
 */
static void *header_to_payload(block_t *block)
{
    dbg_printf("header_to_payload\n");
    return (void *) (block->payload);
}

/*
 * max: returns x if x > y, and y otherwise.
 */
static size_t max(size_t x, size_t y)
{
    return (x > y) ? x : y;
}

/*
 * get_size: returns the size of a given block by clearing the lowest 4 bits
 *           (as the heap is 16-byte aligned).
 */
static size_t get_size(block_t *block)
{
    return extract_size(block->header);
}

/*
 * extract_size: returns the size of a given header value based on the header
 *               specification above.
 */
static size_t extract_size(word_t word)
{
    return (word & size_mask);
}

/*
 * payload_to_header: given a payload pointer, returns a pointer to the
 *                    corresponding block.
 */
static block_t *payload_to_header(void *bp)
{
    return (block_t *) (((char *) bp) - offsetof(block_t, payload));
}

/*
 * pack: returns a header reflecting a specified size and its alloc status.
 *       If the block is allocated, the lowest bit is set to 1, and 0 otherwise.
 */
static word_t pack(size_t size, bool alloc)
{
    return alloc ? (size | alloc_mask) : size;
}

/*
 * write_header: given a block and its size and allocation status,
 *               writes an appropriate value to the block header.
 */
static void write_header(block_t *block, size_t size, bool alloc)
{
    dbg_assert(block != NULL);
    block->header = pack(size, alloc);
}

/*
 * write_footer: given a block and its size and allocation status,
 *               writes an appropriate value to the block footer by first
 *               computing the position of the footer.
 */
static void write_footer(block_t *block, size_t size, bool alloc)
{
    word_t *footerp = (word_t *) ((block->payload) + get_size(block) - dsize);
    *footerp = pack(size, alloc);
}

/*
 * set_ptr: set block ptr to NULL
 */
static void set_ptr_prev(block_t *block)
{
    block->ptr_prev = NULL;
    block->ptr_next = NULL;
}

/*
 * round_up: Rounds size up to next multiple of n
 */
static size_t round_up(size_t size, size_t n)
{
    return (n * ((size + (n - 1)) / n));
}
