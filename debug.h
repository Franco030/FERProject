#ifndef CFER_DEBUG_H
#define CFER_DEBUG_H

#include "chunk.h"

/*
 * In order to see what the bytecode is doing, we're going to create a disassembler.
 * It will contain human-readable mnemonic names for CPU instruction like "ADD" and "MULT".
 *
 * Given a chunk, it will print out all the instructions in it. A Fer user won't use this,
 * but Fer maintainers will certainly benefit from it.
 */

void disassembleChunk(Chunk *chunk, const char *name);
int disassembleInstruction(Chunk *chunk, int offset);

#endif //CFER_DEBUG_H