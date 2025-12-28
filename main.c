#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"

int main(void) {
    initVM();
    Chunk chunk;
    initChunk(&chunk);

    /*
     * IN this case, OP_CONSTANT takes a single byte operand that specifies which constant to load from the chunk's constant array.
     * Since we don't have a compiler yet, we "hand-compile" an instruction in our text chunk.
     */

    int constant = addConstant(&chunk, 1.2);
    writeChunk(&chunk, OP_CONSTANT, 123);
    writeChunk(&chunk, constant, 123);

    constant = addConstant(&chunk, 3.4);
    writeChunk(&chunk, OP_CONSTANT, 123);
    writeChunk(&chunk, constant, 123);

    writeChunk(&chunk, OP_ADD, 123);

    constant=addConstant(&chunk, 5.6);
    writeChunk(&chunk, OP_CONSTANT, 123);
    writeChunk(&chunk, constant, 123);

    writeChunk(&chunk, OP_DIVIDE, 123);

    writeChunk(&chunk, OP_NEGATE, 123);
    writeChunk(&chunk, OP_RETURN, 123);
    disassembleChunk(&chunk, "test chunk");
    interpret(&chunk);
    freeVM();
    freeChunk(&chunk);
    return 0;
}