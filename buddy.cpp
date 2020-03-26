/*
 * Buddy Page Allocation Algorithm
 * SKELETON IMPLEMENTATION -- TO BE FILLED IN FOR TASK (2)
 */

/*
 * STUDENT NUMBER: s1752778
 */
#include <infos/mm/page-allocator.h>
#include <infos/mm/mm.h>
#include <infos/kernel/kernel.h>
#include <infos/kernel/log.h>
#include <infos/util/math.h>
#include <infos/util/printf.h>

using namespace infos::kernel;
using namespace infos::mm;
using namespace infos::util;

#define MAX_ORDER 17

/**
 * A buddy page allocation algorithm.
 */
class BuddyPageAllocator : public PageAllocatorAlgorithm
{
  private:
	/**
	 * Returns the number of pages that comprise a 'block', in a given order.
	 * @param order The order to base the calculation off of.
	 * @return Returns the number of pages in a block, in the order.
	 */
	static inline constexpr uint64_t pages_per_block(int order)
	{
		/* The number of pages per block in a given order is simply 1, shifted left by the order number.
		 * For example, in order-2, there are (1 << 2) == 4 pages in each block.
		 */
		return (1 << order);
	}

	/**
	 * Returns TRUE if the supplied page descriptor is correctly aligned for the 
	 * given order.  Returns FALSE otherwise.
	 * @param pgd The page descriptor to test alignment for.
	 * @param order The order to use for calculations.
	 */
	static inline bool is_correct_alignment_for_order(const PageDescriptor *pgd, int order)
	{
		// Calculate the page-frame-number for the page descriptor, and return TRUE if
		// it divides evenly into the number pages in a block of the given order.
		return (sys.mm().pgalloc().pgd_to_pfn(pgd) % pages_per_block(order)) == 0;
	}

	/** Given a page descriptor, and an order, returns the buddy PGD.  The buddy could either be
	 * to the left or the right of PGD, in the given order.
	 * @param pgd The page descriptor to find the buddy for.
	 * @param order The order in which the page descriptor lives.
	 * @return Returns the buddy of the given page descriptor, in the given order.
	 */
	PageDescriptor *buddy_of(PageDescriptor *pgd, int order)
	{
		// (1) Make sure 'order' is within range
		if (order >= MAX_ORDER)
		{
			return NULL;
		}

		// (2) Check to make sure that PGD is correctly aligned in the order
		if (!is_correct_alignment_for_order(pgd, order))
		{
			return NULL;
		}

		// (3) Calculate the page-frame-number of the buddy of this page.
		// * If the PFN is aligned to the next order, then the buddy is the next block in THIS order.
		// * If it's not aligned, then the buddy must be the previous block in THIS order.
		uint64_t buddy_pfn = is_correct_alignment_for_order(pgd, order + 1) ? sys.mm().pgalloc().pgd_to_pfn(pgd) + pages_per_block(order) : sys.mm().pgalloc().pgd_to_pfn(pgd) - pages_per_block(order);

		// (4) Return the page descriptor associated with the buddy page-frame-number.
		return sys.mm().pgalloc().pfn_to_pgd(buddy_pfn);
	}

	/**
	 * Inserts a block into the free list of the given order.  The block is inserted in ascending order.
	 * @param pgd The page descriptor of the block to insert.
	 * @param order The order in which to insert the block.
	 * @return Returns the slot (i.e. a pointer to the pointer that points to the block) that the block
	 * was inserted into.
	 */
	PageDescriptor **insert_block(PageDescriptor *pgd, int order)
	{
		// Starting from the _free_area array, find the slot in which the page descriptor
		// should be inserted.
		PageDescriptor **slot = &_free_areas[order];

		// Iterate whilst there is a slot, and whilst the page descriptor pointer is numerically
		// greater than what the slot is pointing to.
		while (*slot && pgd > *slot)
		{
			slot = &(*slot)->next_free;
		}

		// Insert the page descriptor into the linked list.
		pgd->next_free = *slot;
		*slot = pgd;

		// Return the insert point (i.e. slot)
		return slot;
	}

	/**
	 * Removes a block from the free list of the given order.  The block MUST be present in the free-list, otherwise
	 * the system will panic.
	 * @param pgd The page descriptor of the block to remove.
	 * @param order The order in which to remove the block from.
	 */
	void remove_block(PageDescriptor *pgd, int order)
	{
		// Starting from the _free_area array, iterate until the block has been located in the linked-list.
		PageDescriptor **slot = &_free_areas[order];
		while (*slot && pgd != *slot)
		{
			slot = &(*slot)->next_free;
		}

		// Make sure the block actually exists.  Panic the system if it does not.
		assert(*slot == pgd);

		// Remove the block from the free list.
		*slot = pgd->next_free;
		pgd->next_free = NULL;
	}

	/**
	 * Given a pointer to a block of free memory in the order "source_order", this function will
	 * split the block in half, and insert it into the order below.
	 * @param block_pointer A pointer to a pointer containing the beginning of a block of free memory.
	 * @param source_order The order in which the block of free memory exists.  Naturally,
	 * the split will insert the two new blocks into the order below.
	 * @return Returns the left-hand-side of the new block.
	 */
	PageDescriptor *split_block(PageDescriptor **block_pointer, int source_order)
	{
		// Make sure there is an incoming pointer.
		assert(*block_pointer);

		// Make sure the block_pointer is correctly aligned.
		assert(is_correct_alignment_for_order(*block_pointer, source_order));

		// If the block is at level 0, just return it.
		if (source_order == 0)
		{
			return *block_pointer;
		}

		// Get the block's buddy at one level lower than the current one.
		PageDescriptor *block = *block_pointer;
		PageDescriptor *buddy = *block_pointer + pages_per_block(source_order - 1);

		// Make sure the block and its buddy are correctly aligned.
		assert(is_correct_alignment_for_order(block, source_order - 1));
		assert(is_correct_alignment_for_order(buddy, source_order - 1));

		// Remove the inital block from the initial order.
		remove_block(block, source_order);

		// Add the block and its buddy to the order below.
		insert_block(block, source_order - 1);
		insert_block(buddy, source_order - 1);

		return *block_pointer;
	}

	/**
	 * Takes a block in the given source order, and merges it (and its buddy) into the next order.
	 * This function assumes both the source block and the buddy block are in the free list for the
	 * source order.  If they aren't this function will panic the system.
	 * @param block_pointer A pointer to a pointer containing a block in the pair to merge.
	 * @param source_order The order in which the pair of blocks live.
	 * @return Returns the new slot that points to the merged block.
	 */
	PageDescriptor **merge_block(PageDescriptor **block_pointer, int source_order)
	{
		assert(*block_pointer);

		// Make sure the area_pointer is correctly aligned.
		assert(is_correct_alignment_for_order(*block_pointer, source_order));

		// If the block is at the highest order, just return it.
		if (source_order == MAX_ORDER - 1)
		{
			return block_pointer;
		};

		// Get the block's buddy at the current level.
		PageDescriptor *block = *block_pointer;
		PageDescriptor *buddy = buddy_of(*block_pointer, source_order);

		// Remove the block and its buddy from the current level.
		remove_block(*block_pointer, source_order);
		remove_block(buddy, source_order);

		// Find if the original block or its buddy is situated at a lower address.
		// The starting block's address will be the one that comes first in memory.
		PageDescriptor **start_block;
		if (*block_pointer < buddy)
		{

			start_block = &block;
		}
		else
		{
			start_block = &buddy;
		}

		// Insert the merged block at the start address, on the level above and return it.
		return insert_block(*start_block, source_order + 1);
	}

  public:
	/**
	 * Constructs a new instance of the Buddy Page Allocator.
	 */
	BuddyPageAllocator()
	{
		// Iterate over each free area, and clear it.
		for (unsigned int i = 0; i < ARRAY_SIZE(_free_areas); i++)
		{
			_free_areas[i] = NULL;
		}
	}

	/**
	 * Allocates 2^order number of contiguous pages
	 * @param order The power of two, of the number of contiguous pages to allocate.
	 * @return Returns a pointer to the first page descriptor for the newly allocated page range, or NULL if
	 * allocation failed.
	 */
	PageDescriptor *alloc_pages(int order) override
	{
		int higher_order = order;

		// If there is a free block in the given order, insert the block there and remove it from the free list.
		if (_free_areas[order])
		{
			PageDescriptor *free_block = _free_areas[order];
			remove_block(free_block, order);
			return free_block;
		}

		// Otherwise find the highest order where there is a free space and keep splitting from there
		// until you get to the given order.
		else
		{
			// Find highest order with a free space
			while (higher_order < MAX_ORDER && _free_areas[higher_order] == NULL)
			{
				higher_order++;
			}

			// Block which will be split is the first free area at the highest order.
			PageDescriptor *block = _free_areas[higher_order];

			// Split until you get to the given order.
			while (higher_order > order)
			{
				block = split_block(&block, higher_order);
				higher_order--;
			}

			// Remove the block from the free list and return it.
			if (block)
			{
				remove_block(block, order);
				return block;
			}
		}

		// If block cannot be allocated, return NULL.
		return NULL;
	}

	/**
	 * Frees 2^order contiguous pages.
	 * @param pgd A pointer to an array of page descriptors to be freed.
	 * @param order The power of two number of contiguous pages to free.
	 */
	void free_pages(PageDescriptor *pgd, int order) override
	{
		// Make sure that the incoming page descriptor is correctly aligned
		// for the order on which it is being freed, for example, it is
		// illegal to free page 1 in order-1.
		assert(is_correct_alignment_for_order(pgd, order));

		// Insert the block into the free list.
		insert_block(pgd, order);

		PageDescriptor *block = pgd;

		// Iterate from the current order to the maximum order
		while (order < MAX_ORDER)
		{
			bool merge = false;
			// If the given block's buddy is free, merge them together
			if (buddy_free(block, order))
			{
				block = *merge_block(&block, order);

				// Make sure that the blocks are correctly aligned.
				if (is_correct_alignment_for_order(block, order + 1))
				{
					merge = true;
				}
			}

			// If the block is not aligned or not able to be merged with its buddy, then stop the loop
			if (merge == false)
			{
				break;
			}
			order++;
		}
	}

	bool buddy_free(PageDescriptor *pgd, int order)
	{
		PageDescriptor *buddy = buddy_of(pgd, order);
		PageDescriptor *first_free = _free_areas[order];

		// Iterate until you find a free block that matches the buddy's address
		// or until there are no more free block on the given order
		while (first_free)
		{
			if (buddy == first_free)
			{
				// Return true if the block's buddy is free
				return true;
			}
			first_free = first_free->next_free;
		}

		// Or false if it isn't
		return false;
	}

	/**
	 * Reserves a specific page, so that it cannot be allocated.
	 * @param pgd The page descriptor of the page to reserve.
	 * @return Returns TRUE if the reservation was successful, FALSE otherwise.
	 */
	bool reserve_page(PageDescriptor *pgd)
	{
		// Find the block and the order that the page currently belongs to.
		Parent parent = find_block(pgd);
		PageDescriptor *parent_block = parent.parent;
		int order = parent.order;

		// If a parent block is found, then split it until you reach the page on order 0
		if (parent_block)
		{

			while (order > 0)
			{

				PageDescriptor *start_block = split_block(&parent_block, order);
				PageDescriptor *buddy = buddy_of(start_block, order - 1);

				// If after the split the page's address is higher that the second half of the original block,
				// make the buddy its parent
				if (pgd >= buddy)
				{
					parent_block = buddy;
				}
				// Otherwise make the parent the start block
				else
				{
					parent_block = start_block;
				}
				order--;
			}
		}

		PageDescriptor *page = _free_areas[0];

		// Make sure that the address you want to reserve is currently free
		while (page)
		{
			// If the page's address is free, remove it from the free list and return true
			if (pgd == page)
			{
				remove_block(pgd, 0);
				return true;
			}
			// Keep iterating through the free pages at level 0
			else
			{
				page = page->next_free;
			}
		}

		// If the page cannot be reserved, return false
		return false;
	}

	// Simple structure consisting of a page descriptor and an order
	struct Parent
	{
		PageDescriptor *parent = NULL;
		int order = -1;
	};

	// Helper function tasked with finding the parent block of a given page
	// Returns the start address of the block and the order where it's found
	Parent find_block(PageDescriptor *pgd)
	{
		PageDescriptor *parent_block = NULL;
		int order = -1;
		Parent parent;

		// Loop through the orders
		for (int i = 0; i < MAX_ORDER; i++)
		{
			PageDescriptor *next_free = _free_areas[i];
			// While there are free pages on the current order and a parent block has not been found
			// keep on looping
			while (next_free != NULL && parent_block == NULL)
			{
				// If the page's address is found in the boundaries of a block, attribute it to that block
				// and return the Parent structure containing the address of the block and its order
				if (next_free <= pgd && pgd < next_free + pages_per_block(i))
				{
					parent_block = next_free;
					order = i;
					parent.order = order;
					parent.parent = parent_block;
					return parent;
				}

				// Keep looping through the free block at the given order
				else
				{

					next_free = next_free->next_free;
				}
			}
		}

		// Return a NULL parent if no suitable parent block has been found
		return parent;
	}

	/**
	 * Initialises the allocation algorithm.
	 * @return Returns TRUE if the algorithm was successfully initialised, FALSE otherwise.
	 */
	bool init(PageDescriptor *page_descriptors, uint64_t nr_page_descriptors) override
	{
		mm_log.messagef(LogLevel::DEBUG, "Buddy Allocator Initialising pd=%p, nr=0x%lx", page_descriptors, nr_page_descriptors);

		uint64_t diff = nr_page_descriptors % pages_per_block(MAX_ORDER - 1);
		uint64_t load_to_MAX = nr_page_descriptors - diff;
		uint64_t block_size = pages_per_block(MAX_ORDER - 1);
		uint64_t block_number = load_to_MAX / block_size;
		uint64_t allocated = 0;

		if (block_number != 0)
		{
			_free_areas[MAX_ORDER - 1] = page_descriptors;
			PageDescriptor *to_load = _free_areas[MAX_ORDER - 1];
			allocated += block_size;

			for (uint64_t i = 0; i < block_number - 1; i++)
			{
				to_load->next_free = to_load + block_size;
				to_load = to_load->next_free;
				allocated += block_size;
			}

			to_load->next_free = NULL;
		}

		if (diff == 0)
		{
			return true;
		}
		else
		{

			for (int i = MAX_ORDER - 2; i >= 0; i--)
			{

				if (diff == 0)
				{
					break;
				}

				if (diff == pages_per_block(i))
				{
					_free_areas[i] = page_descriptors + allocated;
					break;
				}
				else if (diff > pages_per_block(i))
				{

					uint64_t blk_size = pages_per_block(i);
					uint64_t buffer = diff;

					diff = diff % blk_size;
					uint64_t blocks_to_fill = (buffer - diff) / blk_size;
					_free_areas[i] = page_descriptors + allocated;
					PageDescriptor *start = _free_areas[i];
					allocated += blk_size;

					// Iteratively allocate to currentlevel.
					for (uint64_t j = 0; j < blocks_to_fill - 1; j++)
					{
						start->next_free = start + blk_size;
						start = start->next_free;
						allocated += blk_size;
					}

					start->next_free = NULL;
				}
			}
		}
		return true;
	}

	/**
	 * Returns the friendly name of the allocation algorithm, for debugging and selection purposes.
	 */
	const char *name() const override { return "buddy"; }

	/**
	 * Dumps out the current state of the buddy system
	 */
	void dump_state() const override
	{
		// Print out a header, so we can find the output in the logs.
		mm_log.messagef(LogLevel::DEBUG, "BUDDY STATE:");

		// Iterate over each free area.
		for (unsigned int i = 0; i < ARRAY_SIZE(_free_areas); i++)
		{
			char buffer[256];
			snprintf(buffer, sizeof(buffer), "[%d] ", i);

			// Iterate over each block in the free area.
			PageDescriptor *pg = _free_areas[i];
			while (pg)
			{
				// Append the PFN of the free block to the output buffer.
				snprintf(buffer, sizeof(buffer), "%s%lx ", buffer, sys.mm().pgalloc().pgd_to_pfn(pg));
				pg = pg->next_free;
			}

			mm_log.messagef(LogLevel::DEBUG, "%s", buffer);
		}
	}

  private:
	PageDescriptor *_free_areas[MAX_ORDER];
};

/* --- DO NOT CHANGE ANYTHING BELOW THIS LINE --- */

/*
 * Allocation algorithm registration framework
 */
RegisterPageAllocator(BuddyPageAllocator);
