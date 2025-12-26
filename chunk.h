#ifndef FERPROJECT_CHUNK_H
#define FERPROJECT_CHUNK_H

// chunks will be sequences of bytecode

#include "common.h"

/*
 * In our bytecode format, each instruction has a one-byte operation code
 * (universally shortened to opcode). That number controls what kind of instruction
 * we're dealing with, add, subtract, look up variable, etc. We define those here:
*/

typedef enum {
    OP_RETURN,
} OpCode;

/*
 * Bytecode is a series of instructions. Eventually, we'll store some other data along with the instructions,
 * so let's create a struct to hold it all.
 * At the moment, this is simply a wrapper around an array of bytes
 * Since we don't know how big the array needs to be before we start compiling a chunk,
 * it must be dynamic.
 * Dynamic arrays provide:
 *  - Cache-friendly, dense storage
 *  - Constant-time indexed element lookup
 *  - Constant-time appending to the end of the array
 *
 * In addition to the array itself, we keep two numbers: the number of elements in the array we have allocated ("capacity")
 * and how many of those allocated entries are actually in use ("count").
 *
 * When we add an element, if the count is less than the capacity, then there is already available space in the array.
 * We store the new element right in there and bump the count.
 *
 * If we have no spare capacity, then the process is a little more involved.
 *  1. Allocate a new array with more capacity.
 *  2. Copy the existing elements from the old array to the new one.
 *  3. Store the new capacity.
 *  4. Delete the old array.
 *  5. Update code to point to the new array.
 *  6. Store the element in hte new array now that there is room
 *  7. Update the count
 */

typedef struct {
    int count;
    int capacity;
    uint8_t *code;
} Chunk;

/*
 * Let's implement the functions to work with the Chunk.
 * C doesn't have constructors, so we declare a function to initalize a new chunk
 */

void initChunk(Chunk *chunk); // And implement it in chunk.c

/*
 * They dynamic array starts off completely empty. We don't even allocate a raw array yet.
 * To append a byte to the end of the chunk, we use a new function
 */

void writeChunk(Chunk *chunk, uint8_t byte);

/*
 * We have to manage memory ourselves, and that means freeing it too.
 */

void freeChunk(Chunk *chunk);


























#endif //FERPROJECT_CHUNK_H