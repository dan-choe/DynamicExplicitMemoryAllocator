/**
 * All functions you make for the assignment must be implemented in this file.
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include "sfmm.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

int compareBlockAddress(void* start, void* end);
sf_header* searchEnoughSizeFreeBlock(int requestBlockSize);
sf_free_header* requestPage(int requestBlockSize);
void newPage(int num);

void setAllocatedBlock(sf_header* targetBlock, int isSplinter, int payload, int padding, int size);
sf_free_header* setNewFreeBlock(char* targetBlock, int size, sf_free_header *prev, sf_free_header *next);

sf_footer* getItsFooter(sf_header* targetBlock);
void setFooter(sf_footer* targetBlock, int size, int allocatedbit, int isSplinter);

int getBlockSizeFromHeader(sf_free_header* target);
void checkTotalAllocated();

void* remalloc(size_t size);
void* makeNewPageAndUseTogether(void *ptr, size_t size);
/**
 * You should store the head of your free list in this variable.
 * Doing so will make it accessible via the extern statement in sfmm.h
 * which will allow you to pass the address to sf_snapshot in a different file.
 */
sf_free_header* freelist_head = NULL;
sf_free_header* sbrkStart = NULL;
sf_free_header* sbrkPoint = NULL;
sf_footer * sbrkFooter = NULL;

int numPage = 1; // 4096
int increasedPage = 0;
int startNumberOfPages = 1;
int totalAllocated = 0;

size_t s_allocatedBlocks = 0;
size_t s_splinterBlocks = 0;

size_t s_padding = 0;
size_t s_splintering = 0;

size_t s_coalesces = 0;

double s_peakMemoryUtilization = 0;
int sumOfAllocatedPayloads = 0;

// if successful return 0, otherwise -1
int sf_info(info* ptr) {

	if(ptr == NULL){
		return -1;
	}

	ptr->allocatedBlocks = s_allocatedBlocks;
    ptr->splinterBlocks = s_splinterBlocks;
    ptr->padding = s_padding;
    ptr->splintering = s_splintering;
    ptr->coalesces = s_coalesces;
 //printf("ptr->splintering %d\n", (int)s_splintering);

   ///printf("numPage %d sumOfAllocatedPayloads %d\n", numPage, sumOfAllocatedPayloads);
    s_peakMemoryUtilization = (double)sumOfAllocatedPayloads / (4096 * numPage);
    ptr->peakMemoryUtilization = s_peakMemoryUtilization;

	return 0;
}



void *sf_malloc(size_t size) {

	//printf("\n[entered sf_malloc(size %d) totalAllocated + (int)size: %d]:\n", (int)size, totalAllocated + (int)size);
	sf_header* newBlock = NULL;

	if(size < 1){
		//printf("ERROR: %s\n", strerror(ENOMEM));
		errno = EINVAL;
		return NULL;
	}

	if(size > 16368){
		//printf("ERROR: %s\n", strerror(ENOMEM));
		errno = ENOMEM;
		return NULL;
	}

	if(sbrkStart == NULL){
		// sbrk(0) you get the current "break" address.
		// sbrk(size) you get the previous "break" address, i.e. the one before the change
		sbrkStart = sf_sbrk(0);
		//printf("\n  [ sbrkStart (%p)]\n", sbrkStart);
		sbrkPoint = sf_sbrk(4096);
		//printf("  [ sbrkPoint (%p)]\n", sbrkPoint);
		sbrkFooter = sf_sbrk(0);
		//printf("  [ sbrkFooter(%p)]\n", sbrkFooter);
	}

	uint64_t padding = (size%16 == 0) ? 0 : 16 - (size%16);
	int blocksize = 8 + size + padding + 8;
	//printf("	Calculated padding: %d, blocksize: %d\n", (int)padding, blocksize);

	int startPageSize = 4096;

	if(freelist_head == NULL){
		// 4096 - 16 = 4080
		if(blocksize <= 4096){

			// just pass this if statments
		}
		else if(blocksize > 4096 && blocksize < (4096*2)){
			// make 2nd page
			numPage = 2;
			startNumberOfPages = 2;
			startPageSize = 4096*2;
			sbrkPoint = sf_sbrk(4096);
			sbrkFooter = sf_sbrk(0);
		}else if(blocksize > (4096*2) && blocksize < (4096*3)){
			// make 3rd page
			numPage = 3;
			startNumberOfPages = 3;
			startPageSize = 4096*3;
			sbrkPoint = sf_sbrk(4096*2);
			sbrkFooter = sf_sbrk(0);
		}else if(blocksize > (4096*3) && blocksize <= (4096*4)){
			// make 4th page
			numPage = 4;
			startNumberOfPages = 4;
			startPageSize = 4096*4;
			sbrkPoint = sf_sbrk(4096*3);
			sbrkFooter = sf_sbrk(0);
		}else{
			//printf("ERROR: %s\n", strerror(ENOMEM));
			errno = ENOMEM;
			return NULL;

		}

		//printf("startPageSize(%d) - blocksize = %d\n", startPageSize, (startPageSize - blocksize));

		newBlock = &sbrkStart->header;
		newBlock->splinter = 0;
		newBlock->splinter_size = 0;

		setAllocatedBlock(newBlock, 0, blocksize, padding, (int)size);

		if((startPageSize - blocksize)>0){

			char* afterNewBlockFoot = (char*)getItsFooter(newBlock) + 8;
			int newFreeBSize = (startPageSize - blocksize);

			freelist_head = setNewFreeBlock(afterNewBlockFoot, newFreeBSize, NULL, NULL);
		}else{
			freelist_head = NULL;
		}

		return (((void*)newBlock) + 8);
	}else{
		//sf_header* searchEnoughSizeFreeBlock(int requestBlockSize)
		//newBlock = &freelist_head->header;
		newBlock = searchEnoughSizeFreeBlock(blocksize);
		sf_free_header* newBlockFree = NULL;

		if(newBlock == NULL){
			sf_free_header* result = requestPage(blocksize);
			result = result;
			//printf("line 144 requestPage %p, numPage: %d\n", (void*)result, numPage);

			if(result != NULL){
				int updatedPage = 4096 * (increasedPage);
				increasedPage = 0;
				int updateFreeBlockSize = updatedPage + (result->header.block_size<<4);

				sf_free_header* saveCurrPrev = result->prev;

				newBlockFree = setNewFreeBlock((void*)result, updateFreeBlockSize, NULL, NULL);
				newBlockFree->prev= saveCurrPrev;

				//sf_varprint((void*)newBlockFree+8);
				newBlock = (sf_header*) newBlockFree;
			}else{
				//printf("ERROR------------159\n");
				return NULL;
			}

		}

		int isFound = 0;

		while(!isFound){
			int currSizeFreeBlock = newBlock->block_size << 4;
			int requestedSizePlusHF = blocksize;//(int)size + 16;
			int availableSize = currSizeFreeBlock - requestedSizePlusHF;

			//printf("	currSizeFreeBlock: %d, requestedSizePlusHF: %d, availableSize: %d\n", currSizeFreeBlock, requestedSizePlusHF,availableSize);

			if(availableSize == 0){
				// just allocated block, do not make new free block, just set free block
				isFound = 1;
				newBlock->splinter = 0;
				newBlock->splinter_size = 0;

				setAllocatedBlock(newBlock, 0, requestedSizePlusHF, padding, (int)size);

				if(freelist_head->next != NULL){
					freelist_head->next->prev = NULL;
					freelist_head = freelist_head->next;
				}else{
					freelist_head = NULL;
				}

			}else if(availableSize == 32){
				// case 2 - allocated block and make new free blck 32byte
				isFound = 1;
				newBlock->splinter = 0;
				newBlock->splinter_size = 0;

				sf_free_header* tempPrev = NULL;
				sf_free_header* tempNext = NULL;

				if(freelist_head->prev != NULL){
					tempPrev = freelist_head->prev;
				}

				if(freelist_head->next != NULL){
					tempNext = freelist_head->next;
				}

				setAllocatedBlock(newBlock, 0, requestedSizePlusHF, padding, (int)size);

				if(tempNext != NULL){
					int compareAddress = compareBlockAddress(freelist_head, ((char*)newBlock+(int)size+16));
					if(compareAddress == 0){ // next free block is adjacent. merge them
						// this case will not be used
						s_coalesces += 1;
						int currFreeBlockSize = getBlockSizeFromHeader(freelist_head->next);
						int updateFreeBlockSize = currFreeBlockSize + 32;

						tempNext = freelist_head->next->next;
						freelist_head = setNewFreeBlock((char*)getItsFooter(newBlock) + 8, updateFreeBlockSize, NULL, NULL);
						freelist_head->next = tempNext;
						freelist_head->prev = tempPrev;
					}else{
						tempNext = freelist_head->next;
						freelist_head = setNewFreeBlock((char*)getItsFooter(newBlock) + 8, 32, NULL, NULL);
						freelist_head->next = tempNext;
						freelist_head->prev = tempPrev;
					}
				}else{
					freelist_head = setNewFreeBlock((char*)getItsFooter(newBlock) + 8, 32, NULL, NULL);
					freelist_head->next = tempNext;
					freelist_head->prev = tempPrev;
				}
			}else if(availableSize > 32){
				// case 3 - allocated block and make new free blck 32byte
				isFound = 1;
				newBlock->splinter = 0;
				newBlock->splinter_size = 0;

				sf_free_header* tempPrev = NULL;
				sf_free_header* tempNext = NULL;

				if(freelist_head->prev != NULL){
					tempPrev = freelist_head->prev;
				}

				if(freelist_head->next != NULL){
					tempNext = freelist_head->next;
				}

				setAllocatedBlock(newBlock, 0, requestedSizePlusHF, padding, (int)size);

				if(tempNext != NULL){
					int compareAddress = compareBlockAddress(freelist_head, ((char*)newBlock+(int)size+16));
					if(compareAddress == 0){ // next free block is adjacent. merge them
						// this case will not be used
						int currFreeBlockSize = getBlockSizeFromHeader(freelist_head->next);
						int updateFreeBlockSize = currFreeBlockSize + availableSize;

						tempNext = freelist_head->next->next;
						freelist_head = setNewFreeBlock((char*)getItsFooter(newBlock) + 8, updateFreeBlockSize, NULL, NULL);
						freelist_head->next = tempNext;
						freelist_head->prev = tempPrev;
					}else{
						tempNext = freelist_head->next;
						freelist_head = setNewFreeBlock((char*)getItsFooter(newBlock) + 8, availableSize, NULL, NULL);
						freelist_head->next = tempNext;
						freelist_head->prev = tempPrev;
					}
				}else{
					freelist_head = setNewFreeBlock((void*)getItsFooter(newBlock) + 8, availableSize, NULL, NULL);
					freelist_head->next = tempNext;
					freelist_head->prev = tempPrev;
				}

			}else if(availableSize < 0){
				// case 4 - unavailable, find next free block   or   make new space by new page
				if(freelist_head->next != NULL){
					//printf(" case 4 -1 - unavailable - read next free block\n");
					newBlock = &freelist_head->next->header;
				}else{
					//printf(" case 4 -2 - unavailable - make new page, availableSize: %d, currSizeFreeBlock: %d\n",availableSize, currSizeFreeBlock);

					if(numPage<4){
						isFound = 1;
						numPage++;
						sbrkPoint = sf_sbrk(4096);
						sbrkFooter = sf_sbrk(0);
						//printf("[ 	This is sbrk1 : %p, Merge with previous address: %p, freelist_head : %p]\n\n",(void*)sbrkPoint, (void*)sbrkPoint-(currSizeFreeBlock), (void*)freelist_head);

						newBlock = &freelist_head->header;
						newBlock->splinter = 0;
						newBlock->splinter_size = 0;

						setAllocatedBlock(newBlock, 0, requestedSizePlusHF, padding, (int)size);
						int updateFreeBlockSize = currSizeFreeBlock + 4096 - requestedSizePlusHF; // 4192
						sf_free_header* temp = NULL;
						freelist_head = setNewFreeBlock((char*)getItsFooter(newBlock) + 8, updateFreeBlockSize, NULL, NULL);
						freelist_head->next = temp;
					}else{
						//printf("%d NO MORE PAGE! ERROR: %s\n", numPage, strerror(ENOMEM));
						errno = ENOMEM;
						return NULL;
					}
				}

			}else if(availableSize < 32 && availableSize > 0){
				// case 5 - splinter case

					isFound = 1;
					newBlock->splinter = 1;
					newBlock->splinter_size = availableSize;
					//void setAllocatedBlock(sf_header* targetBlock, int isSplinter, int blocksize, int padding, int size){
					sf_free_header* tempPrev = NULL;
					sf_free_header* tempNext = NULL;

					if(freelist_head->prev != NULL){
						tempPrev = freelist_head->prev;
					}

					if(freelist_head->next != NULL){
						tempNext = freelist_head->next;
					}

					setAllocatedBlock(newBlock, availableSize, currSizeFreeBlock, padding, (int)size);

					if(tempNext != NULL){
						//printf("freelist_head->next : %p\n",(char*)freelist_head->next);
						tempNext = freelist_head->next->next;
						//tempNext->prev = tempNext;
						freelist_head = freelist_head->next;
						freelist_head->next = tempNext;
						freelist_head->prev = tempPrev;

					}else{
						//printf("freelist_head->prev : %p\n",(char*)freelist_head->prev);
						//freelist_head = setNewFreeBlock((void*)getItsFooter(newBlock) + 8, availableSize, NULL, NULL);
						tempNext = freelist_head->next;
						freelist_head = tempNext;
						freelist_head->next = NULL;
						freelist_head->prev = tempPrev;
					}

			}
		} //end while(!isFound)
	}
	return (((void*)newBlock) + 8);
}



void *sf_realloc(void *ptr, size_t size) {

	if(size > 16368){
		//printf("ERROR: %s\n", strerror(ENOMEM));
		errno = ENOMEM;
		return NULL;
	}

	//printf("\n[ entered sf_ReAlloc(ptr %p, size %d) ]:\n", (void*)ptr, (int)size);
	uint64_t padding = (size%16 == 0) ? 0 : 16 - (size%16);
	int blocksize = 8 + size + padding + 8;
	//printf("	Calculated padding: %d, blocksize: %d, Requested Size %d\n", (int)padding, blocksize, (int)size);

	sf_free_header* currentFreeHeader = ptr - 8;

	// check ptr is vaild address
	if(ptr == NULL || currentFreeHeader->header.alloc == 0){
		errno = EINVAL;
		return NULL;
	}



	sf_header* currentHeader = &currentFreeHeader->header;
	//printf("		sf_ReAlloc(currentFreeHeader %p, currentHeader %p) \n\n", (char*)currentFreeHeader, (char*)currentHeader);

	int currentBlockSize = getBlockSizeFromHeader(ptr - 8);
	//printf("		curretBlockSize : %d\n", currentBlockSize);

	int compareAddress = 0;
	if(freelist_head != NULL){
		compareAddress = compareBlockAddress(freelist_head, ((char*)currentHeader+currentBlockSize));
	}
	// int adjacentFreeBlockAddress = 0;
	// if(freelist_head != NULL){
	// 	adjacentFreeBlockAddress = compareBlockAddress(freelist_head, ((char*)currentHeader+curretBlockSize));
	// }

	//printf("		compareAddress : %d, (char*)currentHeader: %p, (currentHeader+currentBlockSize) : %p\n", compareAddress, (void*)currentHeader, ((void*)currentHeader+currentBlockSize));
	//printf("	adjacentFreeBlockAddress : %d\n", adjacentFreeBlockAddress);

	////////////////////////////////////////////////// case   ptr = wrong address

	if(size < 1){
		//printf("ERROR: %s\n", strerror(ENOMEM));
		sf_free(ptr);
		return NULL;
	}

	if(sbrkStart == NULL){
		sbrkStart = sf_sbrk(4096);
	//	printf("[	sbrkStart == NULL in realloc. This is sbrk start : %p]\n",(char*)sbrkStart);
	}
	// }else if(totalAllocated + (int)size >= 4096){
	// 	if(numPage<4){
	// 		numPage++;
	// 		sbrkStart = sf_sbrk(4096);
	// 		printf("[ 	This is sbrk1 : %p, Merge with previous address: %p, freelist_head : %p]\n\n",(char*)sbrkStart, (void*)sbrkStart-(4096-totalAllocated), (void*)freelist_head);
	// 	}else{
	// 		printf("%d NO MORE PAGE! ERROR: %s\n", numPage, strerror(ENOMEM));
	// 		return NULL;
	// 	}
	// }

	// after realloc AvailableSize
	int checkAvailableSize = currentBlockSize - blocksize; //(int)size - 16; // 64 - 16 - 16 = 32
	//printf("	checkAvailableSize : %d, currentBlockSize : %d, blocksize : %d\n", checkAvailableSize, currentBlockSize, blocksize);
//
	if(checkAvailableSize == 0){
		//printf("SAME SIZE REQUESTED. JUST RETURN ORIGINAL ADDRESS // padding %d \n", (int)padding);

		// currentHeader->requested_size = (int)size;
		// currentHeader->splinter = 0;
		// currentHeader->padding_size = 0;

		setAllocatedBlock(currentHeader, 0, blocksize, (int)padding, (int)size);

		return (((char*)currentHeader + 8));
	}
	else if(checkAvailableSize == 32){
		// newly splitted block
		// current freelist_head location
		// sf_free_header* currFreelist = freelist_head;

		// check whether currFreelist is an adjacent block with this block
		sf_free_header* tempPrev = NULL;
		//sf_free_header* tempNext = NULL;

		if(freelist_head->prev != NULL){
			tempPrev = freelist_head->prev;
		}

		// if(freelist_head->next != NULL){
		// 	tempNext = freelist_head->next;
		// }
		//printf("tempNext %p\n",(char*)tempNext);

		//currentHeader-> block_size = (int)size + 16;
		setAllocatedBlock(currentHeader, 0, (int)size + 16, 0, (int)size);
		//setAllocatedBlock(sf_header* targetBlock, int isSplinter, int blocksize, int padding, int size)

		//printf("			currFreeBlockSize: %d, updateFreeBlockSize: %d\n", currFreeBlockSize, updateFreeBlockSize);
		sf_free_header* newFreeBlock = NULL;

		// put this free block into freelist_head
		// check location freelist_head's prev next
		// and update those address

		//newFreeBlock = newFreeBlock;
		if(compareAddress == 0){
		// free block is adjacent
			if(tempPrev == NULL){
				//merge with free
				s_coalesces += 1;
				int currFreeBlockSize = getBlockSizeFromHeader(freelist_head);
				int updateFreeBlockSize = currFreeBlockSize + checkAvailableSize;

				newFreeBlock = setNewFreeBlock((char*)getItsFooter(currentHeader) + 8, updateFreeBlockSize, NULL, NULL);
				//printf("			(compareAddress == 0) newFreeBlock: %p\n", newFreeBlock);

				freelist_head->prev = newFreeBlock;
				if(freelist_head->next != NULL){
					newFreeBlock->next = freelist_head;
				}
				freelist_head = newFreeBlock;//((void*)newFreeBlock+(int)size + 16);
			}
		}else if(compareAddress < 0){
			// free block is located in left side
		}else{
			//printf("			(compareAddress > 0) compareAddress : %d\n", compareAddress);

			// free block is located in right side
			if(freelist_head->prev == NULL){
				// set this new freeblock is freelist_head's prev
				newFreeBlock = setNewFreeBlock((char*)getItsFooter(currentHeader) + 8, (int)size + 16, NULL, NULL);
				//printf("			newFreeBlock: %p\n", newFreeBlock);

				freelist_head->prev = newFreeBlock;
				newFreeBlock->next = freelist_head;
				freelist_head = newFreeBlock;

			}else{
				sf_free_header* temp = freelist_head->prev;
				compareAddress = compareBlockAddress(temp, ((char*)currentHeader+currentBlockSize));

				// free prev - [new free] - [allocated] - [free block] - [allocated] - [free block]
				while(temp!= NULL){
					compareAddress = compareBlockAddress(temp, ((char*)currentHeader+currentBlockSize));

					if(compareAddress == 0){
					// temp's pre free block is adjacent
						if(temp->prev != NULL){
							newFreeBlock->prev = temp->prev;
							newFreeBlock->next = temp;
							temp->next = newFreeBlock;
						}else{

						}

					}else if(compareAddress < 0){
						// free block is located in left side
						// free prev - [new free] - free block
					}else{

					}
					// call it's prev block
					// until set this new freeblock is freelist_head's prev

					temp = temp->prev;
				}
			}


		}

		//setNewFreeBlock(sf_free_header* targetBlock, int size, sf_free_header *prev, sf_free_header *next)
		//freelist_head = setNewFreeBlock((void*)getItsFooter(currentHeader) + 8, (int)size + 16, NULL, NULL);

	}else if(checkAvailableSize < 0){
		//printf("			(checkAvailableSize < 0) EXPAND CASE\n");
		// SOLUTION CASE 1 : IF THERE IS AN ADJACENT FREE BLOCK, COALESCE THE TWO BLOCKS
		//					 AND SPLIT IF POSSIBLE

		// SOLUTION CASE 2 : IF THERE IS NO FREE BLOCK, MOVE THE DATA TO A NEW BLOCK AND
		//					 FREE THE OLD BLOCK

		int isRealloced = 0;
		// SOLUTION CASE 1
		sf_header* rside_header = NULL;
		rside_header = ptr - 8 + currentBlockSize;

		int rside_alloc = rside_header->alloc;
		int rside_blocksize = 0;

		// merge it with rightside block
		if(!rside_alloc){
			int isFreeListHeader = 0;

			if((char*)rside_header == (char*)freelist_head){
				isFreeListHeader = 1;
			}

			rside_blocksize = rside_header->block_size << 4;
			//printf("		RIGHT SIDE IS FREE BLOCK! %d YOOHOOOO isFreeListHeader: %d\n", rside_blocksize, isFreeListHeader);
			// check how many bytes you need from free block
			// if it does not have enough space in free block, go to solution 2
			// blocksie is divisible by 16

			int availableSizeIfMerge = rside_blocksize + currentBlockSize;
			//printf("		availableSizeIfMerge %d \n",availableSizeIfMerge);


			// if(availableSizeIfMerge >= blocksize){
			// 	// uint64_t padding = (size%16 == 0) ? 0 : 16 - (size%16);
			// 	// int blocksize = 8 + size + padding + 8;
			// 	// printf("	Calculated padding: %d, blocksize: %d, Requested Size %d\n", (int)padding, blocksize, (int)size);
			// 	int afterAllocSize = availableSizeIfMerge - blocksize;

			// 	if(afterAllocSize >=32){
			// 		//make new free block
			// 		setAllocatedBlock(currentHeader, 0, blocksize, padding, (int)size);
			// 	}else{
			// 		int splinterSize = availableSizeIfMerge - blocksize;
			// 		setAllocatedBlock(currentHeader, splinterSize, blocksize, padding, (int)size);
			// 	}
			// }


			int needBytesFromFreeBlock = currentBlockSize - blocksize;
			needBytesFromFreeBlock = needBytesFromFreeBlock - needBytesFromFreeBlock - needBytesFromFreeBlock;
			// 64BYTE -> 80BYTES REQUEST + 16 = 96 -----> -32 BYTES NEED

			//printf("			needBytesFromFreeBlock: %d\n", needBytesFromFreeBlock);

			// after taking these bytes from free block
			// check u can make new free block from rest of bytes
			// if u can't, coalescing them and set it splinter bytes
			int enoughSpaceMakeNewFB = rside_blocksize - needBytesFromFreeBlock;
			//printf("			enoughSpaceMakeNewFB : %d, rside_blocksize: %d, currentBlockSize: %d\n", enoughSpaceMakeNewFB, rside_blocksize, currentBlockSize);

			if(enoughSpaceMakeNewFB < 0){

				sf_footer* rside_footer = getItsFooter(rside_header);
				//printf("			rside_footer : %p, sbrkFooter: %p\n", rside_footer, sbrkFooter);

				if(((char*)rside_footer+8) == (char*)sbrkFooter){
					isRealloced = 1;
					char* result = makeNewPageAndUseTogether(ptr, size);
					if(result ==NULL){
						//printf("ERROR ----- 569 FROM makeNewPageAndUseTogether\n");
						//printf("SET ERROR CODE\n");
						errno = ENOMEM;
						return NULL;
					}
					s_coalesces += 1;
					return (((void*)ptr));
				}


				// it does not have enough space in free block
				isRealloced = 0; // go to solution 2
			}
			else if(enoughSpaceMakeNewFB >= 32){ // move the footer and make new free block
				isRealloced = 1;

				sf_free_header* saveCurrPrev = currentFreeHeader->prev;
				sf_free_header* saveCurrNext = currentFreeHeader->prev;

				// update current allocated block's info
				//setAllocatedBlock(sf_header* targetBlock, int isSplinter, int blocksize, int padding, int size)
				setAllocatedBlock(currentHeader, 0, blocksize, padding, (int)size);

				int fbsize = enoughSpaceMakeNewFB - 16;
				uint64_t newFBpadding = (enoughSpaceMakeNewFB%16 == 0) ? 0 : 16 - (enoughSpaceMakeNewFB%16);
				int newFBblocksize = 8 + fbsize + newFBpadding + 8;

				//printf("			NewFB addr : %p\n", (char*)rside_header+needBytesFromFreeBlock);
				sf_free_header* newfbox = setNewFreeBlock((char*)rside_header+needBytesFromFreeBlock, newFBblocksize, NULL, NULL);

				newfbox->prev = saveCurrPrev;
				newfbox->next = saveCurrNext;


				/////////////////  update free list info

				if(isFreeListHeader){
					freelist_head = newfbox;
				}

				return (((char*)currentHeader) + 8);
				// sf_free_header* temp = freelist_head;

				// while(temp !=NULL){
				// 	//printf("	mergeRight Free blocks: %p\n",(void*)temp);
				// 	if(currentFreeHeader == temp){

				// 		if(temp->prev!=NULL){
				// 			temp->prev->next = free_box;
				// 		}
				// 		free_box->prev = temp->prev;
				// 		free_box->next = temp;
				// 		temp->prev = free_box;

				// 		if(temp==freelist_head){
				// 			freelist_head = free_box;
				// 		}
				// 	}
				// 	temp = temp->next;
				// }


			}else{
				// coalescing, set splinter
				isRealloced = 1;

				if(enoughSpaceMakeNewFB == 0){
					setAllocatedBlock(currentHeader, enoughSpaceMakeNewFB, blocksize, padding, (int)size);
					if(isFreeListHeader){
						freelist_head = NULL;
					}
				}else if(enoughSpaceMakeNewFB>=16){



					//printf("enoughSpaceMakeNewFB %d availableSizeIfMerge %d\n", enoughSpaceMakeNewFB, availableSizeIfMerge);
					setAllocatedBlock(currentHeader, enoughSpaceMakeNewFB, availableSizeIfMerge, padding, (int)size);
					if(isFreeListHeader){
						freelist_head = NULL;
					}
				}else{

					setAllocatedBlock(currentHeader, enoughSpaceMakeNewFB, blocksize, padding, (int)size);
					if(isFreeListHeader){
						freelist_head = NULL;
					}
				}
			}
		}
		// SOLUTION CASE 2
		if(!isRealloced){
			//printf("============== REALLOC SOLUTION 2 ----------  %d\n" , (currentHeader->block_size << 4));

			sf_header* findnewPlace = remalloc((int)size);//searchEnoughSizeFreeBlock(blocksize);
			//sf_free_header* makeRestFree = (sf_free_header*) findnewPlace;

			if(findnewPlace !=NULL){

				//int newPlaceBlockSize = findnewPlace->block_size << 4;

				// sf_free_header* savePrev = makeRestFree->prev;
				// sf_free_header* saveNext = makeRestFree->next;

				//setAllocatedBlock(findnewPlace, 0, blocksize, padding, (int)size);
				//printf("	findnewPlace: %p, (currentHeader->block_size << 4)-16: %d\n", findnewPlace, (currentHeader->block_size << 4)-16);


				memcpy((char*)findnewPlace+8, (char*)currentHeader+8, (currentHeader->block_size << 4)-16);

				//makeRestFree = setNewFreeBlock((void*)getItsFooter(findnewPlace) + 8, (newPlaceBlockSize - blocksize), NULL, NULL);

				// int isFound = 0;
				// sf_free_header* temp = freelist_head;

				// while(!isFound && temp !=NULL){
				// 	//printf("	mergeRight Free blocks: %p\n",(void*)temp);
				// 	if(free_box < temp){
				// 		isFound = 1;

				// 		if(temp->prev!=NULL){
				// 			temp->prev->next = free_box;
				// 		}
				// 		free_box->prev = temp->prev;
				// 		free_box->next = temp;
				// 		temp->prev = free_box;

				// 		if(temp==freelist_head){
				// 			freelist_head = free_box;
				// 		}
				// 	}
				// 	temp = temp->next;
				// }



				sf_free(ptr);

				return (((char*)findnewPlace) + 8);
			}else{
				errno = ENOMEM;
				return NULL;
				// make one more page or error
			}
		}



		//sf_free_header* searchEnoughSizeFreeBlock(sf_free_header *temp, int requestBlockSize)


	}else if(checkAvailableSize > 32){
		//printf("			(checkAvailableSize > 32) checkAvailableSize : %d\n", checkAvailableSize);
		// SHRINKING CASE

		sf_free_header* holdStartBlock = NULL;
		// sf_free_header* holdStartBlockPREV = NULL;
		// sf_free_header* holdStartBlockNEXT = NULL;


		int currBlockSize = getBlockSizeFromHeader(currentFreeHeader);
		setAllocatedBlock(currentHeader, 0, blocksize, padding, (int)size);

		int updateShrinked = currBlockSize - blocksize;
		sf_free_header* newFreeBlock = setNewFreeBlock((char*)getItsFooter(currentHeader) + 8, updateShrinked, NULL, NULL);
		holdStartBlock = newFreeBlock;


		//printf("			(newFreeBlock) : %p\n", newFreeBlock);

		////////////// right block is free ?? and coalescing

		sf_free_header* checkNextBlock = (sf_free_header*) ((char*)newFreeBlock + updateShrinked);

		//printf("		checkNextBlock : %p	prev : %p, next: %p\n", checkNextBlock, checkNextBlock->prev, checkNextBlock->next);

		if(checkNextBlock->header.alloc == 0){
			// merge
			if(checkNextBlock == freelist_head){
				freelist_head = NULL;
			}
			updateShrinked = (int)(checkNextBlock->header.block_size << 4)  + updateShrinked;
			newFreeBlock = setNewFreeBlock((char*)(holdStartBlock), updateShrinked, NULL, NULL);

			//newFreeBlock
		}

		////////////////////////////////////////////////////

		int isFound = 0;
		sf_free_header* temp = freelist_head;

		if(temp !=NULL){
			while(!isFound && temp !=NULL){
			//printf("	mergeRight Free blocks: %p\n",(void*)temp);
			if(newFreeBlock < temp){
				isFound = 1;

				if(temp->prev!=NULL){
					temp->prev->next = newFreeBlock;
				}
				newFreeBlock->prev = temp->prev;
				newFreeBlock->next = temp;
				temp->prev = newFreeBlock;

				if(temp==freelist_head){
					freelist_head = newFreeBlock;
				}
			}
				temp = temp->next;
			}
		}else{
			freelist_head = newFreeBlock;
		}

	}else{
		// NO EXPAND, SHRINKG CASE
		// checkAvailableSize < 32
		// splinter!!
		// solution case 1: coalesce the splinter with free block
		// solution case 2: no adjacent free block, no split, update header field
		//printf("			(checkAvailableSize < 32) checkAvailableSize : %d, compareAddress: %d\n", checkAvailableSize, compareAddress);

		// check the free block is adjacnet
		if(compareAddress == 0){
		// free block is adjacent
			//printf("			(compareAddress == 0) solution 1 currentHeader : %p, (int)size: %d\n", currentHeader, (int)size);
			//printf("			freelist_head->next %p\n", freelist_head->next);


			if(freelist_head->prev == NULL && freelist_head->next == NULL){
				// set this new freeblock is freelist_head's prev

				sf_free_header* tempPrev = NULL;
				sf_free_header* tempNext = NULL;

				if(freelist_head->prev != NULL){
					tempPrev = freelist_head->prev;
				}

				if(freelist_head->next != NULL){
					tempNext = freelist_head->next;
				}

				int currFreeBlockSize = getBlockSizeFromHeader(freelist_head);
				int updateFreeBlockSize = currFreeBlockSize + checkAvailableSize;

				setAllocatedBlock(currentHeader, 0, blocksize, padding, (int)size);

				//printf("			383 currFreeBlockSize: %d, updateFreeBlockSize: %d, newHead add: %p\n", currFreeBlockSize, updateFreeBlockSize, (void*)getItsFooter(currentHeader) + 8);

				sf_free_header* newFreeBlock = setNewFreeBlock((char*)getItsFooter(currentHeader) + 8, updateFreeBlockSize, NULL, NULL);


				newFreeBlock->next = tempNext;
				newFreeBlock->prev = tempPrev;

				freelist_head = newFreeBlock;//((void*)newFreeBlock+(int)size + 16);
			}else{

			}
		}else if(compareAddress < 0){
			// free block is located in left side
			// solution case 2: no adjacent free block, no split, update header field
			//printf("			(compareAddress < 0) solution 2-1 \n");

			int currBlockSize = getBlockSizeFromHeader(currentFreeHeader);
			int splinterSize = currBlockSize - (int)size;



			setAllocatedBlock(currentHeader, splinterSize, currBlockSize, padding, (int)size);
			//setAllocatedBlock(sf_header* targetBlock, int isSplinter, int blocksize, int padding, int size)

		}else{
			// solution case 2: no adjacent free block, no split, update header field
			// free block is located in right side

			int currBlockSize = getBlockSizeFromHeader(currentFreeHeader);
			int splinterSize = currBlockSize - (int)size - 16;

			//printf("			(compareAddress < 0) solution 2-2 currBlockSize : %d, splinterSize : %d\n",currBlockSize,splinterSize );
			//currentHeader->splinter_size = splinterSize;

			setAllocatedBlock(currentHeader, splinterSize, currBlockSize, 0, (int)size);
		}
	}
	return (((char*)currentHeader) + 8);
}

// immediate coalescing
void sf_free(void* ptr) {

	if(ptr == NULL){
		errno = EINVAL;
		return;
	}

	int isFinished = 0;
	//int compareAddress = 0;
	int mergedBlockSize = 0;
	sf_free_header* tempSidePrev = NULL;
	sf_free_header* tempSideNext = NULL;

	sf_free_header* free_box = ptr - 8;
	int blocksize = free_box->header.block_size << 4;

	if(free_box->header.alloc == 0){
		errno = EINVAL;
		return;
	}

	sf_footer* lside_footer = NULL;
	sf_header* rside_header = NULL;

	lside_footer = ptr - 16;
	rside_header = ptr - 8 + blocksize;

	int lside_alloc = lside_footer->alloc;
	int rside_alloc = rside_header->alloc;
	int lside_blocksize = 0;
	int rside_blocksize = 0;

	// merge it with rightside block first
	if(!rside_alloc){
		// coalescing
		rside_blocksize = rside_header->block_size << 4;
		//rside_header->block_size = 0 << 4;
		//printf("{sf_free - merge with rightside block} rside_blocksize %d, rside_header: %p \n", rside_blocksize, rside_header);

		if(rside_blocksize!=0){
			// if the freelist_head points the rightside free block
			// update freelist_head prev/next/itself
			sf_free_header* rside_block = NULL;
			tempSidePrev = NULL;
			tempSideNext = NULL;

			//compareAddress = compareBlockAddress(freelist_head, rside_header);

			//if(compareAddress != 0){
				rside_block = (sf_free_header*) rside_header;
				tempSidePrev = rside_block->prev;
				tempSideNext = rside_block->next;
			//}
			//printf("rside block\n");

			//sf_varprint((void*)rside_header+8);

			mergedBlockSize = rside_blocksize + blocksize;
			free_box = setNewFreeBlock((char*)free_box, mergedBlockSize, NULL, NULL);
			blocksize = mergedBlockSize;
			isFinished = 1;

			//sf_varprint((void*)free_box+8);

			// if(compareAddress == 0){ // right side block was freelist_head
			// 	freelist_head = free_box;
			// 	freelist_head->prev = tempPrev;
			// 	freelist_head->next = tempNext;
			// }else{
				free_box->prev = tempSidePrev;
				free_box->next = tempSideNext;
			//}

			//sf_varprint((void*)free_box+8);

			int isFound = 0;
			sf_free_header* temp = freelist_head;

			while(temp !=NULL){
				//printf("	mergeRight Free blocks: %p\n",(void*)temp);
				if(!isFound && free_box < temp){
					//printf("		found position: free_box %p, temp %p\n", free_box, (void*)temp);
					isFound = 1;
					if(temp->prev!=NULL){
						temp->prev->next = free_box;
						temp->prev = NULL;
					}

					if(temp->next!=NULL){
						temp->next->prev = free_box;
						temp->next = NULL;
					}

					if(temp==freelist_head){
						freelist_head = free_box;
					}
				}
				temp = temp->next;
			}

		}

	}

	// and then merge it with leftside block
	if(!lside_alloc){
		// coalescing it with leftside free block
		lside_blocksize = lside_footer->block_size << 4;

		if(lside_blocksize!=0){
			// if the freelist_head points the leftside free block
			// update freelist_head prev/next/itself
			sf_free_header* lside_block = NULL;
			lside_block = ptr - 8 - lside_blocksize;

			tempSidePrev = NULL;
			tempSideNext = NULL;

				tempSidePrev = lside_block->prev;
				tempSideNext = lside_block->next;


			mergedBlockSize = lside_blocksize + blocksize;
			free_box = setNewFreeBlock((char*)lside_block, mergedBlockSize, NULL, NULL);
			blocksize = mergedBlockSize;
			isFinished = 1;

				free_box->prev = tempSidePrev;
				free_box->next = tempSideNext;

			int isFound = 0;
			sf_free_header* temp = freelist_head;

			while(temp !=NULL){
				//printf("	mergeLeft Free blocks: %p\n",(void*)temp);
				if(!isFound && free_box < temp){
					//printf("		found position: free_box %p, temp %p\n", free_box, (void*)temp);
					isFound = 1;

					if(temp->next!=NULL){
						//printf("	temp->next: %p\n",(void*)temp->next);
						temp->next->prev = free_box;
						//free_box->next = temp->next;
					}

					if(temp->prev!=NULL){
						temp->prev->next = temp->next;//free_box;
						temp->prev = NULL;
						temp->next = NULL;
					}

					if(temp==freelist_head){
						freelist_head = free_box;
					}
				}
				temp = temp->next;
			}

		}
	}

	if(isFinished){
		s_coalesces += 1;
	}

	if(!isFinished){
		//printf("--- There are no adjacent free blocks ---\n");
		free_box = setNewFreeBlock((char*)free_box, blocksize, NULL, NULL);
		tempSidePrev = NULL;
		tempSideNext = NULL;

		// Find the position to put into linked list
		// Searching the nearest free block // tempPrev, tempNext
		int isFound = 0;
		sf_free_header* temp = freelist_head;
		// sf_free_header* tempLeft = NULL;
		// sf_free_header* tempRight = NULL;

		while(!isFound && temp !=NULL){
			//printf("	Free blocks: %p\n",(void*)temp);
			if(free_box < temp){
				isFound = 1;
				//printf("	Found! the nearest Free block: %p, left(%p), right(%p)\n",(void*)temp, tempLeft, tempRight);

				if(temp->prev!=NULL){
					temp->prev->next = free_box;
					free_box->prev = temp->prev;
					//temp->prev = NULL;
				}

				if(temp->next!=NULL){
					temp->next->prev = free_box;
					//temp->next = NULL;
				}

				free_box->next = temp;
				temp->prev = free_box;

				if(temp==freelist_head){
					freelist_head = free_box;
				}
			}
			temp = temp->next;
		}

		if(freelist_head == NULL){
			freelist_head = free_box;
		}

	}

	s_allocatedBlocks -= 1;
	sumOfAllocatedPayloads -= blocksize - 16;
	return;
}




void iterator(sf_free_header *temp){
	while(temp!=NULL){
		//sf_varprint(((void*)temp + 8)); //sf_varprint(((void*)freelist_head + 8));
		temp = temp->next;
	}
}

sf_header* searchEnoughSizeFreeBlock(int requestBlockSize){
	sf_free_header* temp = NULL;
	sf_free_header* bestFit = NULL;
	int bestFitBlockSize = 0;

	temp = freelist_head;

	if(temp == NULL){
		return NULL;
	}

	sf_header* tempHeader1 = (sf_header*) temp;
	int tempBlockSize = tempHeader1->block_size << 4;
	bestFitBlockSize = tempBlockSize;
	bestFit = freelist_head;

	int found = 0;

	while(temp!=NULL){
		sf_header* tempHeader = (sf_header*) temp;
		int tempBlockSize = tempHeader->block_size << 4;

		if(requestBlockSize <= tempBlockSize){

			found = 1;
			//printf("	[requestBlockSize(%d) <= tempBlockSize(%d)], bestFitBlockSize(%d)\n", requestBlockSize, tempBlockSize, bestFitBlockSize);

			if(bestFitBlockSize > tempBlockSize){
			//	printf("		[bestFitBlockSize(%d) > tempBlockSize(%d)]\n", bestFitBlockSize, tempBlockSize);
				bestFit = temp;
				bestFitBlockSize = tempBlockSize;
			}
		}
		temp = temp->next;
	}

	if(!found){ errno = ENOMEM; return NULL; } // NO FREE BLOCK SPACE

	sf_header* bestFitHeader = (sf_header*) bestFit;
	return bestFitHeader;
}

sf_free_header* requestPage(int requestBlockSize){
	sf_free_header* temp = freelist_head;

	//printf("requestPage / temp : %p\n", temp);

	if(numPage >= 4){
		errno = ENOMEM;
		return NULL; // cannot make more page.
	}

	while(temp!=NULL){ // move pointer to end of freeblock

		if(temp->next==NULL){
			break;
		}

		temp = temp->next;
	}

	//printf("last free block : %p, its size: %d, requestBlockSize: %d \n", temp, temp->header.block_size<<4, requestBlockSize);
	int requiredSize = 0;
	if(temp == NULL){
		requiredSize = requestBlockSize;
	}else{
		requiredSize = requestBlockSize - (temp->header.block_size<<4);
	}

	if(requiredSize > 0 && requiredSize <= 4096){
		if(numPage < 4){
			increasedPage = 1;
			newPage(1);
		}else{
			return NULL;
		}
	}else if(requiredSize > 4096 && requiredSize <= (4096*2)){
		if(numPage < 3){
			increasedPage = 2;
			newPage(2);
		}else{
			return NULL;
		}
	}else if(requiredSize > (4096*2) && requiredSize <= (4096*3)){
		if(numPage < 2){
			increasedPage = 3;
			newPage(3);
		}else{
			increasedPage = 0;
			return NULL;
		}

	}else{
		errno = ENOMEM;
		return NULL;
	}

	if(temp == NULL){
		char* afterNewBlockFoot = (char*)sbrkPoint;
		int newFreeBSize = (4096 * increasedPage);

		freelist_head = (void*)sbrkPoint;
		freelist_head = setNewFreeBlock(afterNewBlockFoot, newFreeBSize, NULL, NULL);
		temp = freelist_head;

		//sf_varprint((void*)temp + 8);
	}

	//printf("requestPage(requestedSize: %d) sbrkStart: %p, sbrkPoint: %p, sbrkFooter: %p\n", requestBlockSize, (void*)sbrkStart, (void*)sbrkPoint, (void*)sbrkFooter);

	// MOVE sbrkPoint to currentEnd of freeblock
	//sbrkPoint = getItsFooter(temp);

	return temp;
}

void newPage(int num){
	numPage = (num+numPage);
//	printf("newPage(num %d) (4096*num) %d \n", num, (4096*num));


	sbrkPoint = sf_sbrk(4096*num);
	sbrkFooter = sf_sbrk(0);

}


void changeHeaderStatus(void * ptr ,int alloc , int block_size , int unused_bits , int padding_size){
	sf_header * newType = ptr;
	newType->alloc = alloc;
	newType->block_size = block_size >> 4;
	if(unused_bits != -1){
		newType->unused_bits = unused_bits;
	}
	if(padding_size != -1){
		newType->padding_size = padding_size;
	}

}

void changeFooterStatus(sf_footer * ptr ,int alloc , int block_size){
	ptr->alloc = alloc;
	ptr->block_size = block_size >> 4;
}

int compareBlockAddress(void* start, void* end){
	int result = start - end;
	return result;
}

void setAllocatedBlock(sf_header* targetBlock, int isSplinter, int blocksize, int padding, int size){
	//printf("{setAllocatedBlock} isSplinter : %d\n",isSplinter);


	if((int)targetBlock->alloc == 0){
		s_allocatedBlocks += 1;
		sumOfAllocatedPayloads += blocksize - 16;
		//printf("{setAllocatedBlock} sumOfAllocatedPayloads : %d\n",sumOfAllocatedPayloads);
	}else{

		//printf("sumOfAllocatedPayloads -> targetBlock->block_size: %d\n",targetBlock->block_size);

		sumOfAllocatedPayloads -= (targetBlock->block_size << 4) - 16;
		sumOfAllocatedPayloads += blocksize - 16;

		//printf("{setAllocatedBlock} from realloc sumOfAllocatedPayloads : %d\n",sumOfAllocatedPayloads);
	}

	if((int)targetBlock->padding_size == 0){
		s_padding += padding;
	}else{
		s_padding -= (int)targetBlock->padding_size;
		s_padding += padding;
	}


	if((int)targetBlock->splinter_size == 0){
		if(isSplinter){
			s_splintering += isSplinter;
			s_splinterBlocks += 1;
		}
	}else{
		if(isSplinter){
			s_splintering += isSplinter;
			s_splinterBlocks += 1;
		}else{
			s_splintering -= targetBlock->splinter_size;
			s_splinterBlocks -= 1;
		}
	}


	if(isSplinter){

		if(isSplinter > 16){
			while(isSplinter != 16){
				isSplinter -= 1;
				padding += 1;
			}
		}

		targetBlock->alloc = 1;
		targetBlock->splinter_size = isSplinter;
		targetBlock->splinter = 1;
		targetBlock->block_size = blocksize >> 4;
		targetBlock->padding_size = padding;
		targetBlock->requested_size = size;

		sf_footer * newBlockFooter = getItsFooter(targetBlock);

		//printf("splinter - newBlockFooter : %p\n",newBlockFooter);

		newBlockFooter->alloc = 1;
		newBlockFooter->splinter = 1;
		newBlockFooter->block_size = blocksize >> 4;

	}else{
		targetBlock->alloc = 1;
		targetBlock->block_size = blocksize >> 4;

	//	printf("padding %d\n", padding);

	//	printf("size %d\n", size);

		targetBlock->padding_size = padding;
		targetBlock->requested_size = size;
		targetBlock->splinter_size = 0;
		targetBlock->splinter = 0;

		sf_footer * newBlockFooter = getItsFooter(targetBlock);
		newBlockFooter->alloc = 1;
		newBlockFooter->splinter = 0;
		newBlockFooter->block_size = blocksize >> 4;
	}
	return;
}

sf_free_header* setNewFreeBlock(char* targetBlockchar, int size, sf_free_header *prev, sf_free_header *next){
	//printf("		{setNewFreeBlock} targetBlock address: %p, size: %d, %p, NEXT: %p\n", targetBlockchar, size, prev, next);

	sf_free_header* targetBlock = (sf_free_header*) targetBlockchar;

	targetBlock->header.alloc = 0;
	targetBlock->header.splinter = 0;
	targetBlock->header.splinter_size = 0;
	targetBlock->header.block_size = size >> 4;
	targetBlock->header.requested_size = 0;
	targetBlock->header.unused_bits = 0;
	targetBlock->header.padding_size = 0;

	targetBlock->prev = prev;
	targetBlock->next = next;

	if(freelist_head == NULL){
		sf_footer* footerAddr = ((void*)sbrkStart) + (startNumberOfPages*4096 - 8); // 4088
		footerAddr->alloc = 0;
		footerAddr->splinter = 0;
		footerAddr->block_size = size >> 4; //(4096 - payload)
	}else{
		setFooter(getItsFooter(&targetBlock->header), size, 0, 0);
	}
	//printf("		{setNewFreeBlock-finisehd} targetBlock address: %p \n", targetBlockchar);

	return targetBlock;
}


sf_footer* getItsFooter(sf_header* targetBlock){
	int getItsSize = targetBlock->block_size << 4;

	void* footerAddr = ((void*)(targetBlock)) + getItsSize - 8;

	if((void*)footerAddr > (void*)sbrkFooter){
		//printf("ERROR ----------------------- footer exceeds %p, sbrkFooter: %p\n", footerAddr, sbrkFooter);
		return NULL;
	}
	//printf("	{getItsFooter} : targetBlock header address : %p, calculated footerAddr address : %p\n", targetBlock, (void*)footerAddr);
	return (sf_footer*) footerAddr;
}

void setFooter(sf_footer* targetBlock, int size, int allocatedbit, int isSplinter){

	if(targetBlock!=NULL){
		targetBlock->alloc = allocatedbit;
		targetBlock->splinter = isSplinter;
		targetBlock->block_size = size >> 4;
	}

	return;
}

int getBlockSizeFromHeader(sf_free_header* target){
	return target->header.block_size << 4;
}



void* remalloc(size_t size) {

	//printf("\n[entered remalloc(size %d) :\n", (int)size);
	sf_header* newBlock = NULL;
	sf_free_header* newBlockFree = NULL;

	uint64_t padding = (size%16 == 0) ? 0 : 16 - (size%16);
	int blocksize = 8 + size + padding + 8;
	//printf("	Calculated padding: %d, blocksize: %d\n", (int)padding, blocksize);

		newBlock = searchEnoughSizeFreeBlock(blocksize);

		if(newBlock == NULL){
			sf_free_header* result = requestPage(blocksize);
			result = result;
			//printf("line 1294 requestPage %p, numPage: %d\n", (void*)result, numPage);

			if(result != NULL){
				int updatedPage = 4096 * (increasedPage);
				increasedPage = 0;
				int updateFreeBlockSize = updatedPage + (result->header.block_size<<4);

				sf_free_header* saveCurrPrev = result->prev;

				newBlockFree = setNewFreeBlock((void*)result, updateFreeBlockSize, NULL, NULL);
				newBlockFree->prev= saveCurrPrev;

				//sf_varprint((void*)newBlockFree+8);
				newBlock = (sf_header*) newBlockFree;
			}else{
				//printf("ERROR------------1332\n");
				return NULL;
			}

		}else{
			//printf("	BEST FIT : %p \n", (void*) newBlock);
		}



		int isFound = 0;
		// 16368
		// search freeblocks for bestfit

		while(!isFound){
			int currSizeFreeBlock = newBlock->block_size << 4;
			int requestedSizePlusHF = blocksize;//(int)size + 16;
			int availableSize = currSizeFreeBlock - requestedSizePlusHF;

			//printf("	currSizeFreeBlock: %d, requestedSizePlusHF: %d, availableSize: %d\n", currSizeFreeBlock, requestedSizePlusHF,availableSize);

			if(availableSize == 0){
				// just allocated block, do not make new free block, just set free block
				isFound = 1;
				newBlock->splinter = 0;
				newBlock->splinter_size = 0;

				setAllocatedBlock(newBlock, 0, requestedSizePlusHF, padding, (int)size);

				if(freelist_head->next != NULL){
					freelist_head->next->prev = NULL;
					freelist_head = freelist_head->next;
				}else{
					freelist_head = NULL;
				}

			}else if(availableSize == 32){
				// case 2 - allocated block and make new free blck 32byte
				isFound = 1;
				newBlock->splinter = 0;
				newBlock->splinter_size = 0;

				sf_free_header* tempPrev = NULL;
				sf_free_header* tempNext = NULL;

				if(freelist_head->prev != NULL){
					tempPrev = freelist_head->prev;
				}

				if(freelist_head->next != NULL){
					tempNext = freelist_head->next;
				}

				setAllocatedBlock(newBlock, 0, requestedSizePlusHF, padding, (int)size);

				if(tempNext != NULL){
					int compareAddress = compareBlockAddress(freelist_head, ((void*)newBlock+(int)size+16));
					if(compareAddress == 0){ // next free block is adjacent. merge them
						// this case will not be used
						int currFreeBlockSize = getBlockSizeFromHeader(freelist_head->next);
						int updateFreeBlockSize = currFreeBlockSize + 32;

						tempNext = freelist_head->next->next;
						freelist_head = setNewFreeBlock((void*)getItsFooter(newBlock) + 8, updateFreeBlockSize, NULL, NULL);
						freelist_head->next = tempNext;
						freelist_head->prev = tempPrev;
					}else{
						tempNext = freelist_head->next;
						freelist_head = setNewFreeBlock((void*)getItsFooter(newBlock) + 8, 32, NULL, NULL);
						freelist_head->next = tempNext;
						freelist_head->prev = tempPrev;
					}
				}else{
					freelist_head = setNewFreeBlock((void*)getItsFooter(newBlock) + 8, 32, NULL, NULL);
					freelist_head->next = tempNext;
					freelist_head->prev = tempPrev;
				}
			}else if(availableSize > 32){
				// case 3 - allocated block and make new free blck 32byte
				isFound = 1;
				newBlock->splinter = 0;
				newBlock->splinter_size = 0;

				sf_free_header* tempPrev = NULL;
				sf_free_header* tempNext = NULL;

				if(freelist_head->prev != NULL){
					tempPrev = freelist_head->prev;
				}

				if(freelist_head->next != NULL){
					tempNext = freelist_head->next;
				}

				setAllocatedBlock(newBlock, 0, requestedSizePlusHF, padding, (int)size);

				if(tempNext != NULL){
					int compareAddress = compareBlockAddress(freelist_head, ((void*)newBlock+(int)size+16));
					if(compareAddress == 0){ // next free block is adjacent. merge them
						// this case will not be used
						int currFreeBlockSize = getBlockSizeFromHeader(freelist_head->next);
						int updateFreeBlockSize = currFreeBlockSize + availableSize;

						tempNext = freelist_head->next->next;
						freelist_head = setNewFreeBlock((void*)getItsFooter(newBlock) + 8, updateFreeBlockSize, NULL, NULL);
						freelist_head->next = tempNext;
						freelist_head->prev = tempPrev;
					}else{
						tempNext = freelist_head->next;
						freelist_head = setNewFreeBlock((void*)getItsFooter(newBlock) + 8, availableSize, NULL, NULL);
						freelist_head->next = tempNext;
						freelist_head->prev = tempPrev;
					}
				}else{
					freelist_head = setNewFreeBlock((void*)getItsFooter(newBlock) + 8, availableSize, NULL, NULL);
					freelist_head->next = tempNext;
					freelist_head->prev = tempPrev;
				}

			}else if(availableSize < 0){
				// case 4 - unavailable, find next free block   or   make new space by new page
				if(freelist_head->next != NULL){
					//printf(" case 4 -1 - unavailable - read next free block\n");
					newBlock = &freelist_head->next->header;
				}else{
					//printf(" case 4 -2 - unavailable - make new page, availableSize: %d, currSizeFreeBlock: %d\n",availableSize, currSizeFreeBlock);

					if(numPage<4){
						isFound = 1;
						numPage++;
						sbrkPoint = sf_sbrk(4096);
						sbrkFooter = sf_sbrk(0);
						//printf("[ 	This is sbrk1 : %p, Merge with previous address: %p, freelist_head : %p]\n\n",(void*)sbrkPoint, (void*)sbrkPoint-(currSizeFreeBlock), (void*)freelist_head);

						newBlock = &freelist_head->header;
						newBlock->splinter = 0;
						newBlock->splinter_size = 0;

						setAllocatedBlock(newBlock, 0, requestedSizePlusHF, padding, (int)size);
						int updateFreeBlockSize = currSizeFreeBlock + 4096 - requestedSizePlusHF; // 4192
						sf_free_header* temp = NULL;
						freelist_head = setNewFreeBlock((void*)getItsFooter(newBlock) + 8, updateFreeBlockSize, NULL, NULL);
						freelist_head->next = temp;
					}else{
					//	printf("%d NO MORE PAGE! ERROR: %s\n", numPage, strerror(ENOMEM));
						errno = ENOMEM;
						return NULL;
					}
				}

			}else if(availableSize < 32 && availableSize > 0){
				// case 5 - splinter case

				//printf(" case 5 -1 - splinter case, availableSize : %d\n",availableSize);


					isFound = 1;
					newBlock->splinter = 1;
					newBlock->splinter_size = availableSize;
					//void setAllocatedBlock(sf_header* targetBlock, int isSplinter, int blocksize, int padding, int size){
					sf_free_header* tempPrev = NULL;
					sf_free_header* tempNext = NULL;

					if(freelist_head->prev != NULL){
						tempPrev = freelist_head->prev;
					}

					if(freelist_head->next != NULL){
						tempNext = freelist_head->next;
					}


					setAllocatedBlock(newBlock, availableSize, currSizeFreeBlock, padding, (int)size);

					if(tempNext != NULL){
						//printf("freelist_head->next : %p\n",(void*)freelist_head->next);
						tempNext = freelist_head->next->next;
						//tempNext->prev = tempNext;
						freelist_head = freelist_head->next;
						freelist_head->next = tempNext;
						freelist_head->prev = tempPrev;
						// int compareAddress = compareBlockAddress(freelist_head, ((void*)newBlock+(int)size+16));


					}else{
						//printf("freelist_head->prev : %p\n",(void*)freelist_head->prev);
						//freelist_head = setNewFreeBlock((void*)getItsFooter(newBlock) + 8, availableSize, NULL, NULL);
						tempNext = freelist_head->next;
						freelist_head = tempNext;
						freelist_head->next = NULL;
						freelist_head->prev = tempPrev;
					}

			}
		} //end while(!isFound)
	return (((void*)newBlock));
}


void* makeNewPageAndUseTogether(void *ptr, size_t size) {

	//printf("\n[entered makeNewPageAndUseTogether(size %d) ptr: %p :\n", (int)size, ptr);
	sf_header* newBlock = NULL;
	sf_free_header* newBlockFree = NULL;

	sf_free_header* currentFreeHeader = ptr - 8;
	sf_header* currentHeader = &currentFreeHeader->header;
	//printf("		sf_ReAlloc(currentFreeHeader %p, currentHeader %p) \n\n", (void*)currentFreeHeader, (void*)currentHeader);

	int currentBlockSize = getBlockSizeFromHeader(ptr - 8);
	//printf("		curretBlockSize : %d\n", currentBlockSize);


	uint64_t padding = (size%16 == 0) ? 0 : 16 - (size%16);
	int blocksize = 8 + size + padding + 8;
	//printf("	Calculated padding: %d, blocksize: %d\n", (int)padding, blocksize);

	newBlock = searchEnoughSizeFreeBlock(blocksize);

	//printf("	newBlock: %p\n", newBlock);

	if(newBlock == NULL){

		//printf("	(blocksize %d - currentBlockSize %d): %d\n", blocksize, currentBlockSize, (blocksize - currentBlockSize));

		sf_free_header* result = requestPage((blocksize - currentBlockSize));
		result = result;

		//printf("line 1544 requestPage %p, numPage: %d\n", (void*)result, numPage);

		if(result != NULL){
			int updatedPage = 4096 * (increasedPage);
			increasedPage = 0;
			int updateFreeBlockSize = updatedPage + (result->header.block_size<<4);

			//printf("updatedPage: %d, updateFreeBlockSize: %d, (result->header.block_size<<4): %d\n",updatedPage, updateFreeBlockSize,(result->header.block_size<<4) );


			char* compareA = (char*)ptr - 8;
			char* compareB = (char*)result;

			sf_free_header* saveCurrPrev = result->prev;

			if(compareB - compareA == 0x1000){
				newBlockFree = setNewFreeBlock((char*)ptr - 8, updateFreeBlockSize, NULL, NULL);
			}else{
				newBlockFree = setNewFreeBlock((void*)result, updateFreeBlockSize, NULL, NULL);
			}

			newBlockFree->prev= saveCurrPrev;

			newBlock = (sf_header*) newBlockFree;
		}else{
				//printf("ERROR------------1627\n");
				//errno = ENOMEM;
				return NULL;
		}


	}else{
	//	printf("	BEST FIT : %p \n", (void*) newBlock);
	}



		// SOLUTION CASE 1
		sf_header* rside_header = NULL;
		rside_header = ptr - 8 + currentBlockSize;

		int rside_alloc = rside_header->alloc;
		int rside_blocksize = 0;

		// merge it with rightside block
		if(!rside_alloc){
			int isFreeListHeader = 0;
			if((void*)rside_header == (void*)freelist_head){
				isFreeListHeader = 1;
			}

			rside_blocksize = rside_header->block_size << 4;
			//printf("		2- RIGHT SIDE IS FREE BLOCK! YOOHOOOO\n");
			// check how many bytes you need from free block
			// if it does not have enough space in free block, go to solution 2
			// blocksie is divisible by 16
			int needBytesFromFreeBlock = currentBlockSize - blocksize;
			needBytesFromFreeBlock = needBytesFromFreeBlock - needBytesFromFreeBlock - needBytesFromFreeBlock;
			// 64BYTE -> 80BYTES REQUEST + 16 = 96 -----> -32 BYTES NEED

			//printf("			needBytesFromFreeBlock: %d\n", needBytesFromFreeBlock);

			// after taking these bytes from free block
			// check u can make new free block from rest of bytes
			// if u can't, coalescing them and set it splinter bytes
			int enoughSpaceMakeNewFB = rside_blocksize - needBytesFromFreeBlock;
			//printf("			enoughSpaceMakeNewFB : %d, rside_blocksize: %d, currentBlockSize: %d\n", enoughSpaceMakeNewFB, rside_blocksize, currentBlockSize);


			if(enoughSpaceMakeNewFB >= 32){ // move the footer and make new free block

				sf_free_header* saveCurrPrev = currentFreeHeader->prev;
				sf_free_header* saveCurrNext = currentFreeHeader->prev;

				// update current allocated block's info
				//setAllocatedBlock(sf_header* targetBlock, int isSplinter, int blocksize, int padding, int size)
				setAllocatedBlock(currentHeader, 0, blocksize, padding, (int)size);

				int fbsize = enoughSpaceMakeNewFB - 16;
				uint64_t newFBpadding = (enoughSpaceMakeNewFB%16 == 0) ? 0 : 16 - (enoughSpaceMakeNewFB%16);
				int newFBblocksize = 8 + fbsize + newFBpadding + 8;

				//printf("			NewFB addr : %p\n", (void*)rside_header+needBytesFromFreeBlock);
				sf_free_header* newfbox = setNewFreeBlock((void*)rside_header+needBytesFromFreeBlock, newFBblocksize, NULL, NULL);

				newfbox->prev = saveCurrPrev;
				newfbox->next = saveCurrNext;


				/////////////////  update free list info

				if(isFreeListHeader){
					freelist_head = newfbox;
				}

				return (((void*)currentHeader) + 8);


			}else{
				// coalescing, set splinter
				if(enoughSpaceMakeNewFB>16){
					//printf("***************ERROR 1734: SPLINTER IS BIGGER THAN 16\n");
				}else{


					setAllocatedBlock(currentHeader, enoughSpaceMakeNewFB, blocksize, padding, (int)size);
				}
			}
		}




	return (((void*)newBlock));
}
