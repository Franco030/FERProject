#include <stdio.h>

#include "debug.h"
#include "object.h"
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

static void printScriptString(ObjString *string) {
    printf("\"");
    for (int i = 0; i < string->length; i++) {
        char c = string->chars[i];
        switch (c) {
            case '\n': printf("\\n"); break;
            case '\r': printf("\\r"); break;
            case '\t': printf("\\t"); break;
            case '\\': printf("\\\\"); break;
            case '"':  printf("\\\""); break;
            default:   printf("%c", c); break;
        }
    }
    printf("\"");
}

void printValueDebug(Value value) {
    if (IS_STRING(value)) {
        printScriptString(AS_STRING(value));
    } else {
        printValue(value);
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

static int jumpInstruction(const char *name, int sign, Chunk *chunk, int offset) {
    uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8);
    jump |= chunk->code[offset + 2];
    printf("%-16s %4d -> %d\n", name, offset, offset + 3 + sign * jump);
    return offset + 3;
}

static int constantInstruction(const char *name, Chunk *chunk, int offset) {
    uint8_t constant = chunk->code[offset + 1];
    printf("%-16s %4d '", name, constant);

    Value value = chunk->constants.values[constant];
    if (IS_STRING(value)) {
        printScriptString(AS_STRING(value));
    } else {
        printValue(value);
    }

    printf("'\n");
    return offset + 2; // Remember that disassembleInstruction() also returns a number to tell the caller the offset of the beginning of the next instruction.
                       // Where OP_RETURN was only a single byte, OP_CONSTANT is two, one for the opcode and one for the operand.
}

static int invokeInstruction(const char *name, Chunk *chunk, int offset) {
    uint8_t constant = chunk->code[offset + 1];
    uint8_t argCount = chunk->code[offset + 2];
    printf("%-16s (%d args) %4d '", name, argCount, constant);

    Value value = chunk->constants.values[constant];
    if (IS_STRING(value)) {
        printScriptString(AS_STRING(value));
    } else {
        printValue(value);
    }

    printf("'\n");
    return offset + 3;
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
        case OP_GET_ITEM:
            return byteInstruction("OP_GET_ITEM", chunk, offset);
        case OP_SET_ITEM:
            return byteInstruction("OP_SET_ITEM", chunk, offset);
        case OP_GET_GLOBAL:
            return constantInstruction("OP_GET_GLOBAL", chunk, offset);
        case OP_DEFINE_GLOBAL:
            return constantInstruction("OP_DEFINE_GLOBAL", chunk, offset);
        case OP_DEFINE_GLOBAL_PERM:
            return constantInstruction("OP_DEFINE_GLOBAL_PERM", chunk, offset);
        case OP_SET_GLOBAL:
            return constantInstruction("OP_SET_GLOBAL", chunk, offset);
        case OP_GET_UPVALUE:
            return byteInstruction("OP_GET_UPVALUE", chunk, offset);
        case OP_SET_UPVALUE:
            return byteInstruction("OP_SET_UPVALUE", chunk, offset);
        case OP_GET_PROPERTY:
            return constantInstruction("OP_GET_PROPERTY", chunk, offset);
        case OP_SET_PROPERTY:
            return constantInstruction("OP_SET_PROPERTY", chunk, offset);
        case OP_GET_SUPER:
            return constantInstruction("OP_GET_SUPER", chunk, offset);
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
        case OP_JUMP:
            return jumpInstruction("OP_JUMP", 1, chunk, offset);
        case OP_JUMP_IF_FALSE:
            return jumpInstruction("OP_JUMP_IF_FALSE", 1, chunk, offset);
        case OP_LOOP:
            return jumpInstruction("OP_LOOP", -1, chunk, offset);
        case OP_CALL:
            return byteInstruction("OP_CALL", chunk, offset);
        case OP_INVOKE:
            return invokeInstruction("OP_INVOKE", chunk, offset);
        case OP_SUPER_INVOKE:
            return invokeInstruction("OP_SUPER_INVOKE", chunk, offset);
        case OP_CLOSURE: {
            offset++;
            uint8_t constant = chunk->code[offset++];
            printf("%-16s %4d ", "OP_CLOSURE", constant);
            printValue(chunk->constants.values[constant]);
            printf("\n");

            ObjFunction *function = AS_FUNCTION(chunk->constants.values[constant]);
            for (int j = 0; j < function->upvalueCount; j++) {
                int isLocal = chunk->code[offset++];
                int index = chunk->code[offset++];
                printf("%04d      |                     %s %d\n", offset - 2, isLocal ? "local" : "upvalue", index);
            }

            return offset;
        }
        case OP_LIST:
            return simpleInstruction("OP_LIST", offset);
        case OP_DICTIONARY:
            return simpleInstruction("OP_DICTIONARY", offset);
        case OP_CLOSE_UPVALUE:
            return simpleInstruction("OP_CLOSE_UPVALUE", offset);
        case OP_RETURN:
            return simpleInstruction("OP_RETURN", offset);
        case OP_CLASS:
            return constantInstruction("OP_CLASS", chunk, offset);
        case OP_INHERIT:
            return simpleInstruction("OP_INHERIT", offset);
        case OP_METHOD:
            return constantInstruction("OP_METHOD", chunk, offset);
        default:
            printf("Unknown opcode %d\n", instruction);
            return offset + 1;
    }
}
