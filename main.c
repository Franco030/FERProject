#include "common.h"
#include "chunk.h"
#include "debug.h"

int main(void) {
    Chunk chunk;
    initChunk(&chunk);

    /*
     * IN this case, OP_CONSTANT takes a single byte operand that specifies which constant to load from the chunk's constant array.
     * Since we don't have a compiler yet, we "hand-compile" an instruction in our text chunk.
     */

    int constant = addConstant(&chunk, 1.2);
    writeChunk(&chunk, OP_CONSTANT, 123);
    writeChunk(&chunk, constant, 123);
    writeChunk(&chunk, OP_RETURN, 123);
    disassembleChunk(&chunk, "test chunk");
    freeChunk(&chunk);
    return 0;
}