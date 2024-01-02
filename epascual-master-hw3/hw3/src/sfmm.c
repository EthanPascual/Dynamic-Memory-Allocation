/**
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "debug.h"
#include "sfmm.h"
#include "errno.h"

#define ALLOC 0x8
#define PREV_ALLOC 0x4
#define WSIZE 8
#define DSIZE 16 //double the size to include both the header and the footer
#define block_size_mask 0xfffffff0LU
#define payload_mask 0x00000000FFFFFFFF
#define payload_mask1 0xffffffff00000000

double totalPayload = 0;
double totalPayloadSize = 0;
double totalBlockSize = 0;
double totalHeapSize = PAGE_SZ;


sf_block *epilogue;

size_t getSize(sf_block *blk){
    return blk->header & block_size_mask;
}
int getAlloc(sf_block *blk){
    return blk->header & ALLOC;
}
int getPrevAlloc(sf_block *blk){
    return blk->header & PREV_ALLOC;
}
sf_block* nextBlock(sf_block *blk){
    return (sf_block *)((void *)(blk) + getSize(blk));
}
sf_block* prevBlock(sf_block *blk){
    return (sf_block *)((void *)(blk) - (blk->prev_footer & block_size_mask));
}
int isWilderness(sf_block *ptr){
    sf_block *epiloguePtr = (sf_block *)((void *) sf_mem_end() - (sizeof(sf_header) + sizeof(sf_footer)));
    sf_block *next = nextBlock(ptr);
    if (next == epiloguePtr)
    {
        return 1; //true
    } else {
        return 0; //false
    }
    
}
sf_block* getFooter(sf_block* ptr){
    return (sf_block *)((void *)(ptr) + getSize(ptr) - WSIZE);
}

int createPadding(int num){ //creates padding to make it a multiple of 19
    if (num <= DSIZE){
        return 2*DSIZE;
    }else if (num % DSIZE != 0){
        return num + (DSIZE - (num % DSIZE));
    } else  {
        return num;
    }
       
}

void insert_free_block(sf_block* head, sf_block* block){
    //checks to see if the free list is empty
    if(head->body.links.prev == head && head->body.links.next == head){
        head->body.links.next = block;
        block->body.links.prev = head;
        head->body.links.prev = block;
        block->body.links.next = head;
    } else {
        //free list is not empty
        head->body.links.next->body.links.prev = block;
        block->body.links.next = head->body.links.next;
        head->body.links.next = block;
        block->body.links.prev = head;
    }
}

//returns the index of the free list to start at when looking for free blocks
int findSizeClass(int s){
    if(s == 32){
        return 0;
    }else if(s > 32 && s <= 64){
        return 1;
    }else if(s > 64 && s <= 3 * 32){
        return 2;
    }else if(s > 3*32 && s <= 5 * 32){
        return 3;
    }else if(s > 5*32 && s <= 8 * 32){
        return 4;
    }else if(s > 8*32 && s <= 13 * 32){
        return 5;
    }else if(s > 13*32 && s <= 21 * 32){
        return 6;
    }else if(s > 21*32 && s <= 34 * 32){
        return 7;
    }else{
        return 8;
    }
}

int valid_args(void *ptr){
    
    //pointer can't be null
    if(ptr == NULL){
        return 0;
    }
    //pointer address has to be divisible by 16
    if((uintptr_t)ptr % 16 != 0){
        return 0;
    }
    //block size has to be at least 32 bytes
    sf_block *block = (sf_block*)((void*)ptr - DSIZE);
    if(getSize(block) < 32){
        return 0;
    }
    //block size has to be a multiple of 16
    if(getSize(block) % 16 != 0){
        return 0;
    }
    //allocated bit in header has to be 1
    if(!getAlloc(block)){
        return 0;
    }
    //prev_alloc and alloc of previous have to match
    sf_block *previous = prevBlock(block);
    if(getPrevAlloc(block) == 0 && getAlloc(previous) == 8){
        return 0;
    }

    //out of bounds
    // if(block < sf_mem_start() || getFooter(block) > (sf_mem_end() + WSIZE)){
    //     return 0;
    // }

    return 1;




}

//returns a block of free data that can fit the amount of memory we are trying to allocate.
sf_block* find_first(int size, sf_block *head){
    //first check to see if the list is not empty, if it is return null
    if(head->body.links.prev == head && head->body.links.next == head){
        return NULL;
    }
    //if it is not empty, loop through until you go through all the free blocks in that list. Check to see if you can fit the current block
    sf_block *current = head;
    while(current->body.links.next != head){ //keeps looping through until you circle back
        current = current->body.links.next;
        int available_space = current->header & block_size_mask;
        if(available_space >= size){
            return current;
        }
    }
    //if it can't find one, return NULL
    return NULL;
}

void remove_free(sf_block *ptr){
    ptr->body.links.next->body.links.prev = ptr->body.links.prev;
    ptr->body.links.prev->body.links.next = ptr->body.links.next;
}

sf_block* coalesce(sf_block *ptr){
    int size = getSize(ptr);
    int prevAlloc = getPrevAlloc(ptr);
    sf_block *next = nextBlock(ptr);
    int nextAlloc = getAlloc(next);

    sf_block *start;

    if(prevAlloc && nextAlloc){ //allocated, free, allocated
        //nothing to merge
        return ptr;
    }else if(prevAlloc && !nextAlloc){//allocated, free, free
        //merge ptr and next block
        start = ptr;
        int newSize = size + getSize(next);
        remove_free(ptr);
        remove_free(next);
        if(prevAlloc){
            start->header = newSize;
            start->header |= PREV_ALLOC;
        }else{
            start->header = newSize;
        }
        sf_block *p_footer = getFooter(next);
        p_footer->header = start->header;


    }else if(!prevAlloc && nextAlloc){//free, free, allocated
        //merge prev block and ptr
        sf_block *prev = prevBlock(ptr);
        start = prev;

        int newSize = size + getSize(prev);
        if(isWilderness(ptr)){
            remove_free(prev);
        }else{
            remove_free(ptr);
            remove_free(prev);
        }
        if(prevAlloc){
            start->header = newSize;
            start->header |= PREV_ALLOC;
        }else{
            start->header = newSize;
        }
        sf_block *p_footer = getFooter(ptr);
        p_footer->header = start->header;

    }else{//free, free, free
        //merge all blocks
        sf_block *prev = prevBlock(ptr);
        sf_block *next = nextBlock(ptr);

        remove_free(prev);
        remove_free(next);
        remove_free(ptr);
        
        start = prev;
        int newSize = getSize(prev) + getSize(next) + size;

        if(prevAlloc){
            start->header = newSize;
            start->header |= PREV_ALLOC;
        }else{
            start->header = newSize;
        }
        sf_block *p_footer = getFooter(next);
        p_footer->header = start->header;

    }

    int sizeClass;
    if(isWilderness(start)){
        sizeClass = NUM_FREE_LISTS - 1;
    } else {
        sizeClass = findSizeClass(getSize(start));
    }
    insert_free_block(&sf_free_list_heads[sizeClass], start);
    return start; 
}



//place block in the heap, will also deal with splitting if necessary
//place block in the heap, will also deal with splitting if necessary
void placeOrSplit(sf_block *ptr, int size, size_t payload, int isPlace){
    //first check to see if there is enough remaining space to split the block
    int checkWilderness = isWilderness(ptr);
    int size_left = getSize(ptr) - size;
    if(size_left >= 32){
        //you need to split

        sf_block *bottom = ptr; //this will be tha allocated block
        if(isPlace){
            remove_free(bottom);
        }
        

        sf_block *top = (sf_block*)((void*) ptr + size); // becomes the free block

        if(getPrevAlloc(bottom)){
            ptr->header = (payload << 32) | size | ALLOC | PREV_ALLOC;
        }else{
            ptr->header = (payload << 32) | size | ALLOC;
        }

        top->header = (size_left & block_size_mask)  | PREV_ALLOC;
        sf_block *top_footer = (sf_block *)((void *)top + size_left - WSIZE);
        top_footer->header =  top->header;
        top_footer->prev_footer = bottom -> header;

        if(checkWilderness){
            //if its the wilderness, we have to add the top back to the wilderness
            insert_free_block(&sf_free_list_heads[NUM_FREE_LISTS-1],top);
        }else{
            //if its not the wilderness, just find the size class and insert the free block there.
            int size_class = findSizeClass(size_left);
            insert_free_block(&sf_free_list_heads[size_class], top);
        }
        coalesce(top);

    }else{
        //you don't need to split

        if(!isPlace){
            size += size_left;
        }

        //change to allocated
        if(getPrevAlloc(ptr)){
            ptr->header = (payload << 32) | size | ALLOC | PREV_ALLOC;
        }else{
            ptr->header = (payload << 32) | size | ALLOC;
        }
        
        if(isPlace){
            //make sure the next block after now says previously allocated
            sf_block *next = nextBlock(ptr);
            next->prev_footer = ptr->header;
            next->header |= PREV_ALLOC;
        }
        
       

        //remove the free block from the list
        if(isPlace){
            remove_free(ptr);
        }
        
    }
}

int init_heap(){
    //initialize heap
    for(int i =0 ; i < NUM_FREE_LISTS; i++){ //creating sentinel nodes
            sf_free_list_heads[i].header = 0;
            sf_free_list_heads[i].body.links.next = &sf_free_list_heads[i];
            sf_free_list_heads[i].body.links.prev = &sf_free_list_heads[i];
        }
        if(sf_mem_grow() == NULL){
            return -1;
        }

        //make prologue
        sf_block *prologue = (sf_block*)(sf_mem_start()); //prologue is at the start of the heap
        prologue->header = 32; //minumum block size
        prologue->header |= ALLOC; // set to allocated

        //make epilogue
        sf_block *epilogue = (sf_block *)((void *) sf_mem_end() - DSIZE);
        epilogue->header |= ALLOC;

        //the remaining memory in the page should be stored as a wilderness block
        sf_block *wilderness = (sf_block *)(sf_mem_start()+DSIZE * 2);
        int remainingSize = PAGE_SZ - 48; //32 from prologue block, 8 from epilogue, and 8 from the unused row in the beginning
        //wilderness->prev_footer = prologue->header;
        wilderness->header=remainingSize;
        wilderness->header |= PREV_ALLOC;

        //put into free lists        
        insert_free_block(&sf_free_list_heads[9], wilderness);

        //make sure the footer of the wilderness matches the header
        epilogue->prev_footer = wilderness->header;


        return 0;

}

void *sf_malloc(size_t size) {

    //first check to see if the heap was initialized or not
    if(sf_mem_end() == sf_mem_start()){
        if(init_heap() == -1){
            sf_errno = ENOMEM;
            return NULL;
        }
        
    }
    

    if(size == 0) return NULL;

    //create size you need to look for, including header and footer
    int size_with_header_and_footer = size + DSIZE;
    int alignedSize = createPadding(size_with_header_and_footer);

    totalPayloadSize += size;
    totalPayload += size;
    totalBlockSize += alignedSize;


    //find size class
    int sizeClass = findSizeClass(alignedSize);

    //using this, loop through each size class following current one and see if there is any free blocks to use
    sf_block *available_free = NULL;
    for(int i = sizeClass; i < NUM_FREE_LISTS; i++){ //includes the wilderness block
             available_free = find_first(alignedSize, &sf_free_list_heads[i]); //this will also detach the free block from the free list if found
             if(available_free != NULL){ //if we found an availabe space, we fill it and stop checking
                placeOrSplit(available_free, alignedSize, size, 1);
                return available_free->body.payload;
             }
            
    }

    //if it reaches here, you need more data and you need to call memgrow however many times
    sf_block *epilogue = (sf_block *)((void *) sf_mem_end() - DSIZE);
    sf_block *wilderness = prevBlock(epilogue);
    int amt = alignedSize - getSize(wilderness);
    do{
        sf_block *ptr = sf_mem_grow();
        if(ptr == NULL){
            sf_errno = ENOMEM;
            return NULL;
        }

        sf_block *epilogue = (sf_block *)((void *) sf_mem_end() - DSIZE);
        epilogue->header = 0;
        epilogue->header |= ALLOC;
        sf_block *wilderness = (sf_block *)sf_free_list_heads[NUM_FREE_LISTS-1].body.links.prev;
        if (wilderness != &sf_free_list_heads[NUM_FREE_LISTS-1]) {
            epilogue->prev_footer = wilderness->header;
        }

        sf_block *newPage = (sf_block *)((void *)ptr - DSIZE);
        sf_block *prev = prevBlock(newPage);

        if(getAlloc(prev)){
            newPage->header = PAGE_SZ;
            newPage->header |= PREV_ALLOC;
        }else{
            newPage->header = PAGE_SZ;
        }

        sf_block *p_footer = getFooter(newPage);
        p_footer -> header = newPage -> header;

        
        //if there isn't a wilderness
        if(sf_free_list_heads[NUM_FREE_LISTS-1].body.links.next == &sf_free_list_heads[NUM_FREE_LISTS-1]){
            insert_free_block(&sf_free_list_heads[NUM_FREE_LISTS - 1], newPage);
        }

        available_free = coalesce(newPage);

        totalHeapSize += PAGE_SZ;

        amt -= PAGE_SZ;
    }while(amt > 0);

    placeOrSplit(available_free, alignedSize, size,1);
    return available_free->body.payload;

}

void sf_free(void *pp) {
    
    //checks to see if the given pointer is valid. If it is not, abort()
    if(valid_args(pp) == 0){
        abort();
    }

    //get the block
    sf_block *block = (sf_block *)((void *)pp - DSIZE);
    //change the block, make it unallocated
    block->header = block->header & ~ALLOC;
    totalPayload -= (block->header & payload_mask1) >> 32;

    //make sure footer matches header
    sf_block *footer = getFooter(block);
    block->header = block->header & payload_mask;
    footer->header = block->header;

    //change next blocks prev allocated to 0
    sf_block *next =  nextBlock(block);
    next->header = next->header & ~PREV_ALLOC;
    
    //add to free list
    int sizeClass = findSizeClass(getSize(block));
    insert_free_block(&sf_free_list_heads[sizeClass], block);

    coalesce(block);
    
}

void *sf_realloc(void *pp, size_t rsize) {
    
    if((int)rsize == 0){
        free(pp);
        
        return NULL;
    }

    if(valid_args(pp) == 0){
        sf_errno = EINVAL;
        abort();
    }

    //get block from pointer
    sf_block *block = (sf_block *)((void *) pp - DSIZE);
    int originalSize = getSize(block);
    int payload = (block->header & payload_mask1) >> 32;

    //get new size from rsize
    int newSize = rsize + DSIZE;
    int alignedSize = createPadding(newSize);

    if(originalSize == newSize){
        return pp;
    }

    if(originalSize < newSize){
        //reallocating block to a larger block
        //calling malloc to obtain the larger block
        void *newBlock = sf_malloc(rsize);

        //make sure it not null
        if(newBlock == NULL){
            return NULL;
        }

        //call memcpy
        memcpy(newBlock, pp, rsize);

        //free original block
        sf_free(pp);
        return newBlock;

    } else {
        totalPayload -= (payload - rsize);
        //split block
        placeOrSplit(block, alignedSize, rsize, 0);
        return block->body.payload;
    }

    
}



double sf_fragmentation() {
    if(sf_mem_start() == sf_mem_grow()){
        return 0.0;
    } else {
        totalBlockSize = totalHeapSize - 16;
        return totalPayload / totalBlockSize;
    }

}

double sf_utilization() {
    if(sf_mem_start() == sf_mem_grow()){
        return 0.0;
    } else {
        return totalPayloadSize / totalHeapSize;
    }

}
