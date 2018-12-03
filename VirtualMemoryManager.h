#ifndef VMM_H
#define VMM_H

enum {
	NUM_FRAMES = 128, // number of physical memory frames of PAGE_SIZE bytes
	PAGE_SIZE = 256, // size in bytes of each memory page
	PAGE_TABLE_ENTRIES = 256, // number of entries in page table
	TLB_ENTRIES = 16 // number of entries in the TLB
};

typedef struct TLB_Data {
	int page, frame;
} TLB_Data;

typedef struct TLB {
	TLB_Data data[TLB_ENTRIES];
	int num_entries, front, hits;
} TLB;

typedef struct PageTable {
	int data[PAGE_TABLE_ENTRIES];
	int replace_queue[NUM_FRAMES];
	int faults, oldest, next_empty;
} PageTable;

typedef struct DRAM {
	signed char memory[NUM_FRAMES][PAGE_SIZE];
	int num_frames_used;
} DRAM;

typedef struct MMU {
	PageTable page_table;
	TLB tlb;
	DRAM ram;
	FILE *backing_store;
	int translation_count;
} MMU;

// MMU functions
void MMU_init(MMU *mmu);
void MMU_destroy(MMU *mmu);
void MMU_get_byte(MMU *mmu, int logical_addr);
int MMU_read_from_store(MMU *mmu, int page);
int MMU_replace_page(MMU *mmu);

// TLB functions
void TLB_insert(TLB *tlb, int page, int frame);
int TLB_lookup(TLB *tlb, int page);

// Page Table functions
void PageTable_insert(PageTable *pt, int page, int frame);
int PageTable_lookup(PageTable *pt, int page);

#endif