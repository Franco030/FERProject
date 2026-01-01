#include <stdio.h>
#include "debug.h"

#include "chunk.h"
#include "value.h"

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

static int byteInstruction(const char *name, Chunk *chunk, int offset) {
    uint8_t slot = chunk->code[offset + 1];
    printf("%-16s %4d\n", name, slot);
    return offset + 2;
}

static int constantInstruction(const char *name, Chunk *chunk, int offset) {
    uint8_t constant = chunk->code[offset + 1];
    printf("%-16s %4d '", name, constant);
    printValue(chunk->constants.values[constant]);
    printf("\n");
    return offset + 2; // Remember that disassembleInstruction() also returns a number to tell the caller the offset of the beginning of the next instruction.
                       // Where OP_RETURN was only a single byte, OP_CONSTANT is two, one for the opcode and one for the operand.
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
    if (offset >0 && chunk->lines[offset] == chunk->lines[offset-1]) {
        printf("   | ");
    } else {
        printf("%4d ", chunk->lines[offset]);
    }

    uint8_t instruction = chunk->code[offset];
    switch (instruction) {
        case OP_CONSTANT:
            return constantInstruction("OP_CONSTANT", chunk, offset);
        case OP_NIL:
            return simpleInstruction("OP_NIL", offset);
        case OP_TRUE:
            return simpleInstruction("OP_TRUE", offset);
        case OP_FALSE:
            return simpleInstruction("OP_FALSE", offset);
        case OP_POP:
            return simpleInstruction("OP_POP", offset);
        case OP_GET_LOCAL:
            return byteInstruction("OP_GET_LOCAL", chunk, offset);
        case OP_SET_LOCAL:
            return byteInstruction("OP_SET_LOCAL", chunk, offset);
        case OP_GET_GLOBAL:
            return constantInstruction("OP_GET_GLOBAL", chunk, offset);
        case OP_DEFINE_GLOBAL:
            return constantInstruction("OP_DEFINE_GLOBAL", chunk, offset);
        case OP_SET_GLOBAL:
            return constantInstruction("OP_SET_GLOBAL", chunk, offset);
        case OP_EQUAL:
            return simpleInstruction("OP_EQUAL", offset);
        case OP_GREATER:
            return simpleInstruction("OP_GREATER", offset);
        case OP_LESS:
            return simpleInstruction("OP_LESS", offset);
        case OP_ADD:
            return simpleInstruction("OP_ADD", offset);
        case OP_SUBTRACT:
            return simpleInstruction("OP_SUBTRACT", offset);
        case OP_MULTIPLY:
            return simpleInstruction("OP_MULTIPLY", offset);
        case OP_DIVIDE:
            return simpleInstruction("OP_DIVIDE", offset);
        case OP_NOT:
            return simpleInstruction("OP_NOT", offset);
        case OP_NEGATE:
            return simpleInstruction("OP_NEGATE", offset);
        case OP_PRINT:
            return simpleInstruction("OP_PRINT", offset);
        case OP_RETURN:
            return simpleInstruction("OP_RETURN", offset);
        default:
            printf("Unknown opcode %d\n", instruction);
            return offset + 1;
    }
}
