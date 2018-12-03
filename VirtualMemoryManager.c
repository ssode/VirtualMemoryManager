/*
Steven Soderquist
CSCI 343 Operating Systems Project 4 - Virtual Memory Manager
Dr. Don Allison
Due date: 3 December 2018

This program simulates a virtual memory manager using a FIFO TLB as a cache to speed up address translation
Page faults are handled by reading the page from BACKING_STORE.bin into a free frame in memory
If no free frame exists, then a frame is replaced in FIFO order
*/

#include <stdlib.h>
#include <stdio.h>
#include "VirtualMemoryManager.h"

// reads addresses from the input file, passing them to the mmu, which will translate the address and fetch bytes from physical memory
int main(int argc, char **argv) {
	if (argc != 2) {
		fprintf(stderr, "Input filename must be given as a command line argument\n"); // exit if no input file was given
		return 1;
	}
	FILE *input = fopen(argv[1], "r");
	if (input == NULL) {
		fprintf(stderr, "Could not open input file: %s\n", argv[1]); // exit if we cannot read the input file
		return 1;
	}
	MMU mmu;
	MMU_init(&mmu); // init our mmu
	char address[10]; // buffer to read addresses into
	while (fgets(address, 10, input) != NULL) { // converts each line of input into an integer and sends it to the mmu
		int logical_addr = atoi(address);
		MMU_get_byte(&mmu, logical_addr);
	}
	printf("Total addresses translated: %d  Total TLB hits: %d  Total Page faults: %d\n", mmu.translation_count, mmu.tlb.hits, mmu.page_table.faults);
	double tlb_hit_rate = ((double)mmu.tlb.hits / (double)mmu.translation_count) * 100;
	double page_fault_rate = ((double)mmu.page_table.faults / (double)mmu.translation_count) * 100;
	printf("TLB hit rate: %.2f%%  Page fault rate: %.2f%%\n", tlb_hit_rate, page_fault_rate);
	MMU_destroy(&mmu);
	fclose(input); // close input file
	input = NULL;
	return 0;
}

// MMU functions

// initializes our virtual mmu
void MMU_init(MMU *mmu) {
	mmu->translation_count = 0;
	mmu->ram.num_frames_used = 0;
	mmu->page_table.oldest = 0;
	mmu->page_table.next_empty = 0;
	mmu->page_table.faults = 0;
	for (int i = 0; i < PAGE_TABLE_ENTRIES; i++) {
		mmu->page_table.data[i] = -1;
	}
	mmu->tlb.hits = 0;
	mmu->tlb.num_entries = 0;
	mmu->tlb.front = 0;
	mmu->backing_store = fopen("BACKING_STORE.bin", "rb");
	if (mmu->backing_store == NULL) {
		fprintf(stderr, "Error opening BACKING_STORE.bin\n"); 
		exit(1); // exit if we cannot open the backing store
	}
}

// just to make sure the file pointer is taken care of
void MMU_destroy(MMU *mmu) { 
	fclose(mmu->backing_store);
	mmu->backing_store = NULL;
}

// attempts to translate address in order of TLB -> Page table -> backing store, and prints out the addresses and value
void MMU_get_byte(MMU *mmu, int logical_addr) {
	int page = (logical_addr & 0xFFFF) >> 8; // page is the highest 8 bits in the 16-bit logical address, so take the smallest 16 bits of the 32-bit int and right shift by 8
	int offset = logical_addr & 0xFF; // offset is the smaller 8 bits of the logical address, so just take the first 8

	int frame = TLB_lookup(&mmu->tlb, page); // first check the TLB for the page

	if (frame == -1) { // page wasn't in TLB, check the page table
		frame = PageTable_lookup(&mmu->page_table, page);
		if (frame == -1) { // not in page table, read form backing store
			mmu->page_table.faults++;
			frame = MMU_read_from_store(mmu, page);
		}
		TLB_insert(&mmu->tlb, page, frame); // insert to TLB since it wasn't already there
	} else {
		mmu->tlb.hits++; // it was found in TLB, inc hits
	}
	// get the signed byte from memory
	signed char val = mmu->ram.memory[frame][offset];
	int physical_addr = (frame << 8) | offset; // physical address is highest 8 bits frame number lowest 8 bits offset, so left shift frame num by 8 and or with offset to set those bits
	printf("Virtual address: %d  Physical address: %d  Value: %d\n", logical_addr, physical_addr, val);
	mmu->translation_count++;
}

// reads in a page from BACKING_STORE.bin and calls a page replacement if necessary
int MMU_read_from_store(MMU *mmu, int page) {
	int frame;
	if (mmu->ram.num_frames_used == NUM_FRAMES) { // all frames are full, need to replace one
		frame = MMU_replace_page(mmu);
	} else { // we will read into the next empty frame
		frame = mmu->ram.num_frames_used;
		mmu->ram.num_frames_used++;
	}
	fseek(mmu->backing_store, page * PAGE_SIZE, 0); // seek to start of given page
	signed char buffer[PAGE_SIZE];
	fread(buffer, sizeof(signed char), PAGE_SIZE, mmu->backing_store); // read 256 bytes into buffer
	for (int i = 0; i < PAGE_SIZE; i++) {
		mmu->ram.memory[frame][i] = buffer[i];
	}
	PageTable_insert(&mmu->page_table, page, frame); // insert the page and frame to the table
	return frame;
}

// finds the frame to replace and sets the page referencing it to -1
int MMU_replace_page(MMU *mmu) {
	int page_to_replace = mmu->page_table.replace_queue[mmu->page_table.oldest];
	int frame_to_replace = mmu->page_table.data[page_to_replace];
	mmu->page_table.oldest = (mmu->page_table.oldest + 1) % (NUM_FRAMES);
	mmu->page_table.data[page_to_replace] = -1;
	return frame_to_replace;
}

// TLB functions

// inserts page and table into TLB in FIFO order
void TLB_insert(TLB *tlb, int page, int frame) {
	if (tlb->num_entries < TLB_ENTRIES) { // if it's not at capacity, just insert to first empty position
		tlb->data[tlb->num_entries].page = page;
		tlb->data[tlb->num_entries].frame = frame;
		tlb->num_entries++;
	} else { // otherwise, replace the oldest entry with the new one
		tlb->data[tlb->front].page = page;
		tlb->data[tlb->front].frame = frame;
		tlb->front = (tlb->front + 1) % TLB_ENTRIES;
	}
}

// checks the TLB for a page number, returns the frame number if found, otherwise returns -1
int TLB_lookup(TLB *tlb, int page) {
	for (int i = 0; i < tlb->num_entries; i++) {
		if (tlb->data[i].page == page)
			return tlb->data[i].frame;
	}
	return -1;
}

// Page Table functions

// associates a frame number with the page index in the page table
void PageTable_insert(PageTable *pt, int page, int frame) {
	pt->data[page] = frame;
	pt->replace_queue[pt->next_empty] = page;
	pt->next_empty = (pt->next_empty + 1) % (NUM_FRAMES);
}

// returns the frame that a page is referencing, or -1
int PageTable_lookup(PageTable *pt, int page) {
	return pt->data[page];
}


