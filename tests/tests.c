#include <criterion/criterion.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "sfmm.h"
#include <errno.h>

/**
 *  HERE ARE OUR TEST CASES NOT ALL SHOULD BE GIVEN STUDENTS
 *  REMINDER MAX ALLOCATIONS MAY NOT EXCEED 4 * 4096 or 16384 or 128KB
 */



Test(sf_memsuite, Malloc_an_Integer, .init = sf_mem_init, .fini = sf_mem_fini) {
  int *x = sf_malloc(sizeof(int));
  *x = 4;
  cr_assert(*x == 4, "Failed to properly sf_malloc space for an integer!");
}

Test(sf_memsuite, Free_block_check_header_footer_values, .init = sf_mem_init, .fini = sf_mem_fini) {
  void *pointer = sf_malloc(sizeof(short));
  sf_free(pointer);
  pointer = (char*)pointer - 8;
  sf_header *sfHeader = (sf_header *) pointer;
  cr_assert(sfHeader->alloc == 0, "Alloc bit in header is not 0!\n");
  sf_footer *sfFooter = (sf_footer *) ((char*)pointer - 8 + (sfHeader->block_size << 4));
  cr_assert(sfFooter->alloc == 0, "Alloc bit in the footer is not 0!\n");
}

Test(sf_memsuite, SplinterSize_Check_char, .init = sf_mem_init, .fini = sf_mem_fini){
  void* x = sf_malloc(32);
  void* y = sf_malloc(32);
  (void)y;

  sf_free(x);

  x = sf_malloc(16);

  sf_header *sfHeader = (sf_header *)((char*)x - 8);
  cr_assert(sfHeader->splinter == 1, "Splinter bit in header is not 1!");
  cr_assert(sfHeader->splinter_size == 16, "Splinter size is not 16");

  sf_footer *sfFooter = (sf_footer *)((char*)sfHeader + (sfHeader->block_size << 4) - 8);
  cr_assert(sfFooter->splinter == 1, "Splinter bit in header is not 1!");
}

Test(sf_memsuite, Check_next_prev_pointers_of_free_block_at_head_of_list, .init = sf_mem_init, .fini = sf_mem_fini) {
  int *x = sf_malloc(4);
  memset(x, 0, 0);
  cr_assert(freelist_head->next == NULL);
  cr_assert(freelist_head->prev == NULL);
}

Test(sf_memsuite, Coalesce_no_coalescing, .init = sf_mem_init, .fini = sf_mem_fini) {
  int *x = sf_malloc(4);
  int *y = sf_malloc(4);
  memset(y, 0, 0);
  sf_free(x);

  //just simply checking there are more than two things in list
  //and that they point to each other
  cr_assert(freelist_head->next != NULL);
  cr_assert(freelist_head->next->prev != NULL);
}

//#
//STUDENT UNIT TESTS SHOULD BE WRITTEN BELOW
//DO NOT DELETE THESE COMMENTS
//#

///////////////////////////  UNIT TEST FOR MELLOC ////////////////////////////////

Test(sf_memsuite, ut1_bestFit_checker, .init = sf_mem_init, .fini = sf_mem_fini) {
    // printf("\n== Test: bestFit_checker ==\n");
    int *value1 = sf_malloc(1);
    int *value2 = sf_malloc(20);
    int *value3 = sf_malloc(1);
    int *value4 = sf_malloc(1);
    int *value5 = sf_malloc(1);

    // sf_varprint(value1);
    // sf_varprint(value2);
    // sf_varprint(value3);
    // sf_varprint(value4);
    // sf_varprint(value5);
    // sf_varprint(((void*)freelist_head + 8));

   // printf("\n== BestFit_checker Free(2,4) ==\n");

    sf_free(value2);
    sf_free(value4);
    int *value6 = sf_malloc(1);

    // sf_varprint(value1);
    // sf_varprint(value2);
    // sf_varprint(value3);
    // sf_varprint(value4);
    // sf_varprint(value5);
    // sf_varprint(value6);
    // sf_varprint(((void*)freelist_head + 8));

    cr_assert(value6 == value4, "You have to malloc in this block");

    sf_free(value1);
    sf_free(value2);
    sf_free(value3);
    sf_free(value4);
    sf_free(value5);
}

Test(sf_memsuite, simpleMalloc_checker, .init = sf_mem_init, .fini = sf_mem_fini) {
 // printf("====================== simpleMalloc_checker ============================ \n");

  char *a = sf_malloc(4096);

  // sf_varprint(a);
  // sf_varprint(((void*)freelist_head + 8));

  cr_assert((freelist_head->header).block_size << 4 == 4080, "freelist_head block_size is %d", (freelist_head->header).block_size);

  sf_free(a);
  //sf_varprint(((void*)freelist_head + 8));

  cr_assert((freelist_head->header).block_size << 4 == 8192, "freelist_head block_size is %d", (freelist_head->header).block_size);
}


Test(sf_memsuite, simpleFullSizeMalloc_checker, .init = sf_mem_init, .fini = sf_mem_fini) {
 // printf("====================== simpleFullSizeMalloc_checker ============================ \n");
  int *a = sf_malloc(16368); // 4097 * 4 = 16384

  //sf_varprint(a);
  //sf_varprint(((void*)freelist_head + 8));
  cr_assert(freelist_head == NULL, "freelist_head must be Null");
  sf_free(a);
  cr_assert((freelist_head->header.block_size << 4) == 16384, "freelist_head block size is wrong");
}

Test(sf_memsuite, mallocSize16370, .init = sf_mem_init, .fini = sf_mem_fini) {
  char *a = sf_malloc(16370);
  cr_assert(a == NULL, "You can not allocate memory");
  cr_assert(errno == ENOMEM, "ERRNO must be ENOMEM");
}

Test(sf_memsuite, mallocSizeZero, .init = sf_mem_init, .fini = sf_mem_fini) {
  int *a = sf_malloc(0);
  cr_assert(a == NULL, "You can not allocate memory");
  cr_assert(errno == EINVAL, "ERRNO must be EINVAL");
}

///////////////////////////  UNIT TEST FOR FREE ////////////////////////////////

Test(sf_memsuite, nullFree, .init = sf_mem_init, .fini = sf_mem_fini) {
  sf_free(0x00);
  cr_assert(errno == EINVAL, "errno must be EINVAL");
}

Test(sf_memsuite, coalescing3blck_checker, .init = sf_mem_init, .fini = sf_mem_fini) {
    char *a = sf_malloc(1);
    char *b = sf_malloc(1);
    char *c = sf_malloc(1);
    char *d = sf_malloc(1);
    char *e = sf_malloc(1);
    char *f = sf_malloc(1);

    sf_free(a);
    sf_free(c);
    sf_free(e);
    sf_free(b);

    sf_header *newBlock = (sf_header *)((char*)a - 8);
    sf_free_header *newBlockH = (sf_free_header *)((char*)a - 8) ;
    cr_assert(newBlock->block_size << 4 == 96, "Block size must be 96 bytes");
    cr_assert((char*)newBlockH->next == ((char*)e - 8), "Next Block should be e block");
    sf_free(d);
    sf_free(f);
}


Test(sf_memsuite, LinkedList_checker, .init = sf_mem_init, .fini = sf_mem_fini) {
    int *value1 = sf_malloc(48);
    int *value2 = sf_malloc(48);
    int *value3 = sf_malloc(48);
    int *value4 = sf_malloc(48);
    int *value5 = sf_malloc(48);
    int *value6 = sf_malloc(48);
    int *value7 = sf_malloc(48);

    // sf_varprint(value1);
    // sf_varprint(value2);
    // sf_varprint(value3);
    // sf_varprint(value4);
    // sf_varprint(value5);
    // sf_varprint(value6);
    // sf_varprint(value7);
    // printf("     MAIN FUNCTION - freelist_head address %p, add 8 : %p\n", freelist_head, ((void*)freelist_head + 8));
    // sf_varprint(((void*)freelist_head + 8));

    sf_free(value1);
    sf_free(value3);
    sf_free(value6);

    // sf_varprint(value1);
    // sf_varprint(value2);
    // sf_varprint(value3);
    // sf_varprint(value4);
    // sf_varprint(value5);
    // sf_varprint(value6);
    // sf_varprint(value7);
    // printf("     MAIN FUNCTION - freelist_head address %p, add 8 : %p\n", freelist_head, ((void*)freelist_head + 8));
    // sf_varprint(((void*)freelist_head + 8));

    cr_assert(freelist_head->next->next->next->next == NULL); //  1 > 3 > 6 > free-> next = null
    cr_assert(freelist_head->prev == NULL);

    sf_free(value2);
    sf_free(value4);
    sf_free(value5);
    sf_free(value7);
}

Test(sf_memsuite, rightCoalescing_checker, .init = sf_mem_init, .fini = sf_mem_fini) {
    char *a = sf_malloc(1);
    char *b = sf_malloc(1);
    char *c = sf_malloc(1);
    char *d = sf_malloc(1);
    char *e = sf_malloc(1);
    char *f = sf_malloc(1);

    a = a;
    d = d;
    sf_free(c);
    sf_free(e);
    sf_free(b);

    f = f;

    sf_free_header* bInfo = (sf_free_header*)((char*)b - 8);
    cr_assert(bInfo->header.block_size<<4 == 64);
}

Test(sf_memsuite, leftCoalescing_checker, .init = sf_mem_init, .fini = sf_mem_fini) {
    char *a = sf_malloc(1);
    char *b = sf_malloc(1);
    char *c = sf_malloc(1);
    char *d = sf_malloc(1);
    char *e = sf_malloc(1);
    char *f = sf_malloc(1);

    a = a;
    d = d;
    b = b;
    sf_free(c);
    sf_free(d);

    e = e;
    f = f;

    sf_free_header* cInfo = (sf_free_header*)((char*)c - 8);
    cr_assert(cInfo->header.block_size<<4 == 64);
}


/////////////////////////  UNIT TEST FOR REALLOC ////////////////////////////////


Test(sf_memsuite, ExpandRealloc_checker, .init = sf_mem_init, .fini = sf_mem_fini) {
   // printf("\n== Test: ExpandRealloc_FindNewPlace_checker ==\n");

    int *value1 = sf_malloc(48);
    int *value2 = sf_malloc(100);

    sf_free(value2);
    sf_realloc(value1, 60);

    sf_header* freelist_header = (sf_header*)((char*)freelist_head);
    sf_free_header* value1Info = (sf_free_header*)((char*)value1 - 8);

    cr_assert(value1Info->header.padding_size == 4, "Padding size must be 4");
    cr_assert(freelist_header->block_size<<4 == 4016, "Block size should be 4016 bytes");
    cr_assert(freelist_head != NULL, "Freelist_head is NULL!");
    cr_assert(freelist_head->next == NULL, "Freelist_head's next must be NULL!");
}

Test(sf_memsuite, seachNewFreeBlockRealloc_checker, .init = sf_mem_init, .fini = sf_mem_fini) {
   // printf("\n== Test: seachNewFreeBlockRealloc_checker ==\n");

    int *value1 = sf_malloc(48);
    int *value2 = sf_malloc(100);

    sf_realloc(value1, 60);

    sf_header* value1Info = (sf_header*)((char*)value1 - 8);
    sf_header* value2Info = (sf_header*)((char*)value2 - 8);
    int value2Info_size = value2Info->block_size << 4;

    value2Info = (sf_header*)((char*)value2Info + value2Info_size);

    cr_assert(value1Info->alloc == 0, "Old block must be free block");
    cr_assert(value2Info->block_size<<4 == 80, "Block size should be 80 bytes");
    cr_assert(freelist_head->next != NULL, "Freelist_head's next should not be NULL!");
}

Test(sf_memsuite, ShrinkRealloc_checker, .init = sf_mem_init, .fini = sf_mem_fini) {
   // printf("\n== Test: ShrinkRealloc_shrink 16bytes block and make new 32bytes free block ==\n");
    int *value1 = sf_malloc(48);
    int *value2 = sf_malloc(100);

    // sf_varprint(value1);
    // sf_varprint(value2);
    // sf_varprint(((void*)freelist_head + 8));

    sf_realloc(value1, 16);

    // sf_varprint(value1);
    // sf_varprint(((void*)freelist_head + 8));
    // sf_varprint(value2);

    sf_header *newCreatedHeader = (sf_header *)((char*)value1 - 8 + 32) ;
    cr_assert((char*)freelist_head == (char*)newCreatedHeader, "freelist_head does not points new free block!");
    cr_assert(freelist_head->header.block_size<<4 == 32, "Block size should be 32bytes");
    cr_assert(freelist_head->next != NULL, "Freelist_head's next should not be NULL!");

    sf_free(value2);
}

Test(sf_memsuite, simpleRealloc_checker, .init = sf_mem_init, .fini = sf_mem_fini) {
   // printf("\n== Test: simpleRealloc_checker ==\n");

    char *a = sf_malloc(1);
    char* b = sf_realloc(a, 8);
    char* c = sf_realloc(b, 128);
    char* d = sf_realloc(c, 2048);
    char* e = sf_realloc(d, 4096);
    char* f = sf_realloc(e, 8192);
    char* g = sf_realloc(a, 16384);

    sf_header *check= (sf_header*)((char*)f - 8) ;
    cr_assert((freelist_head->header.block_size << 4) == 4080, "freelist_head have wrong block size!");
    cr_assert(check->block_size<<4 == 8208, "Block size should be 8208bytes");
    cr_assert(freelist_head->next == NULL, "Freelist_head's next is not NULL!");

    a = a;
    b = b;
    c = c;
    d = d;
    e = e;
    f = f;
    g = g;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////