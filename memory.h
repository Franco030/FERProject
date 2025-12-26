#ifndef CFER_MEMORY_H
#define CFER_MEMORY_H

#include "common.h"

/*
 * This macro calculates a new capacity based on a given current capacity.
 * In order to get the performance we want, the important part is that it scales based on the old size.
 * We grow by a factor of two, which is pretty typical.
 * 1.5x is another common choice.
 *
 * We also handle when the current capacity is zero.
 * In that case, we jump straight to eight elements instead of starting one.
 * That avoids a little extra memory churn when the array is very small,
 * at the expense of wasting a few bytes on very small chunks.
 */

#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)

/*
 * Once we know the desired capacity,
 * we create or grow the array to that size using GROW_ARRAY()
 *
 * This macro pretties up a function call to reallocate() where the real work happens.
 * The macro itself takes care of getting the size of the array's element type and casting the result void*
 * back to a pointer of the right type.
 *
 * This reallocate() function is the single function we'll use for all dynamic memory management in cfer,
 * allocating memory, freeing it, and changing the size of an existing allocation.
 * Routing all of those operations through a single function will be important later when we add gargace collector
 * that needs to keep track of how much memory is in use
 *
 * The two size arguments passed to reallocate() control which operation to perform:
 *
 * oldSize  | newSize                | Operation
 * ------------------------------------------------------------
 * 0        | Non-zero               | Allocate new block
 * ------------------------------------------------------------
 * Non-zero | 0                      | Free allocation
 * ------------------------------------------------------------
 * Non-zero | Smaller than oldSize   | Shrink existing allocation
 * ------------------------------------------------------------
 * Non-zero | Larger than oldSize    | Grow existing allocation
 */

#define GROW_ARRAY(type, pointer, oldCount, newCount) \
    (type*)reallocate(pointer, sizeof(type)*(oldCount), \
        sizeof(type)*(newCount))

/*
 * Like GROW_ARRAY(), this is a wrapper around a call to reallocate().
 * This one frees the memory by passing in zero for the new size.
 */

#define FREE_ARRAY(type, pointer, oldCount) \
    reallocate(pointer, sizeof(type)*(oldCount), 0)

void* reallocate(void *pointer, size_t oldSize, size_t newSize);

#endif //CFER_MEMORY_H