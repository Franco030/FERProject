#include <stdio.h>
#include "debug.h"

#include "chunk.h"

/*
 * To disassemble a chunk, we print a little header
 * (so we can tell which chunk we're looking at) and then crank through the bytecode,
 * disassembling each instruction. The way we iterate through the code is a little odd.
 * Instead of incrementing offset in the loop, we let disassembleInstruction() do it for us.
 * When we call that function, after disassembling the instruction at the given offset,
 * it returns the offset of the next instruction.
 * This is because, as we'll see later, instructions can have different sizes.
 */

void disassembleChunk(Chunk *chunk, const char *name) {
    printf("== %s ==\n", name);

    for (int offset = 0; offset < chunk->count;) {
        offset = disassembleInstruction(chunk, offset);
    }
}

/*
 * For the one instruction we do have, OP_RETURN, the display function is here.
 * A static function can only be called by other function within the same source file.
 * This is a form of encapsulation, hiding internal helper functions from the rest of the program.
 *
 * They are allocated in the data segment of memory, not the stack.
 */

static int simpleInstruction(const char *name, int offset) {
    printf("%s\n", name);
    return offset +1;
}

/*
 * The core of the "debug" module is this function.
 * First, it prints the byte offset of the given instruction,
 * that tells us where in the chunk this instruction is.
 * This will be helpful signpost when we start doing control flow and jumping around in the bytecode.
 *
 * Next, it reads a single byte from the bytecode at a given offset. That's our opcode.
 * We switch on that. For each kind of instruction, we dispatch to a little utility function for displaying it.
 * On the off chance that the given byte doesn't look like an instruction at all -- a bug in our compiler -- we print that too.
 */

int disassembleInstruction(Chunk *chunk, int offset) {
    printf("%04d ", offset);

    uint8_t instruction = chunk->code[offset];
    switch (instruction) {
        case OP_RETURN:
            return simpleInstruction("OP_RETURN", offset);
        default:
            printf("Unknown opcode %d\n", instruction);
            return offset + 1;
    }
}
