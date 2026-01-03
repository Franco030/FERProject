#ifndef CFER_CHUNK_H
#define CFER_CHUNK_H

// chunks will be sequences of bytecode

#include "common.h"
#include "value.h"

/*
 * In our bytecode format, each instruction has a one-byte operation code
 * (universally shortened to opcode). That number controls what kind of instruction
 * we're dealing with, add, subtract, look up variable, etc. We define those here.
 *
 * The compiled chunk needs to not only contain the values 1 and 2, but know when to produce them
 * so that they are printed in the right order. Thus, we need an instruction that produces a particular constant.
 *
 * When the VM executes a constant instruction, it "loads" the constant for use.
 * This new instruction is a little more complex than OP_RETURN.
 * A single bare opcode isn't enough to know which constant to load.
 *
 * To handle cases like this, our bytecode, like most others, allows instructions to have operands.
 * These are stores as binary data immediately after the opcode in the instruction stream
 * and let us parameterize what the instruction does
 *
 * OP_RETURN        OP_CONSTANT
 * [01] <- opcode   [00][23] <- ([opcode][constant index])
 * 1 byte           2 bytes
 *
 * Each opcode determines how many operand bytes it has and what they mean.
 * For example, a simple operation like "return" may have no operands, where an instruction for "load local variable"
 * needs an operand to identify which variable to load. Each time we add a new opcode to cfer, we specify what its operands look like
 * its instruction format.
 *
 * Something to take into account as a personal note is that we could create instructions for !=, <= and >=, and as how the book says, we should do that.
 * But it's important to note that bytecode instructions don't need to closely follow the user's source code.
 * The VM has total freedom to use whatever instruction set and code sequences it wants as long as they have the right user-visible behavior.
 *
 * The expression a != b has the same semantics as !(a == b), so the compiler is free to compile the former as if it were the latter.
 * Instead of a dedicated OP_NOT_EQUAL instruction, it can output an OP_EQUAL followed by an OP_NOT.
 * Likewise, a <= b is the same as !(a > b) and a >= b is !(a < b). Thus, we only made three new instructions.
 *
 * Over in the parser, though, we do have six new operators to slot into the parse table.
*/

typedef enum {
    OP_CONSTANT,
    OP_NIL,
    OP_TRUE,
    OP_FALSE,
    OP_POP,
    OP_GET_LOCAL,
    OP_SET_LOCAL,
    OP_GET_GLOBAL,
    OP_DEFINE_GLOBAL,
    OP_SET_GLOBAL,
    OP_EQUAL,
    OP_GREATER,
    OP_LESS,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_NOT,
    OP_NEGATE,
    OP_PRINT,
    OP_JUMP,
    OP_JUMP_IF_FALSE,
    OP_LOOP,
    OP_CALL,
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
 *
 * Chunks contains almost all the information that the runtime needs from the user's source code.
 * There's only one piece of data we're missing.
 *
 * When a runtime error occurs, we show the user the line number of the offending source code.
 * In pfer, those number lived in tokens, which we in turn store in the AST nodes.
 * We need a different solution for cfer now that we've ditched syntax trees in favor of bytecode.
 * Given any bytecode instruction, we need to be able to determine the line of the user's source program that it was compiled from.
 *
 * There are a lot of clever ways we could encode this, we took the absolute simplest approach.
 * In the chunk, we store a separate array of integers that parallels the bytecode.
 * Each number in the array is the line number for the corresponding byte in the bytecode.
 * When a runtime error occurs, we look up the line number at the same index as the current instruction's offset in the code array.
 * To implement this, we add another array to Chunk.
 */

typedef struct {
    int count;
    int capacity;
    uint8_t *code;
    int *lines;
    ValueArray constants;
} Chunk;

/*
 * Let's implement the functions to work with the Chunk.
 * C doesn't have constructors, so we declare a function to initialize a new chunk
 */

void initChunk(Chunk *chunk); // And implement it in chunk.c

/*
 * They dynamic array starts off completely empty. We don't even allocate a raw array yet.
 * To append a byte to the end of the chunk, we use a new function
 */

void writeChunk(Chunk *chunk, uint8_t byte, int line);

/*
 * We have to manage memory ourselves, and that means freeing it too.
 */

void freeChunk(Chunk *chunk);

/*
 * We define a convenience method to add a new constant to the chunk.
 * Our compiler could write to the constant array inside Chunk directly,
 * but it's a little nicer to add an explicit function.
 */

int addConstant(Chunk *chunk, Value value);









#endif //CFER_CHUNK_H