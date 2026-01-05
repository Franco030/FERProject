#ifndef CFER_COMPILER_H
#define CFER_COMPILER_H

#include "object.h"
#include "vm.h"

/*
 * A compiler has roughly two jobs. It parses the user's source code to understand what it means.
 * Then it takes that knowledge and outputs low-level instructions that produce the same semantics.
 * Many languages split those two roles into two separate passes in the implementation.
 * A parser produces and AST, just like pfer does, and then a code generator traverses the AST and outputs target code.
 *
 * In cfer, we're doing merging these two passes into one.
 * Single-pass compilers don't work well for all languages.
 * Since the compiler has only a peephole view into the user's program while generating code,
 * the language must be designed such that you don't need much surrounding context to understand a piece of syntax.
 *
 * What this means is that our "compiler" C module has functionality similar to pfer parser, consuming tokens,
 * matching expected token types, etc. And it also has functions for code gen, emitting bytecode, and adding constants to the destination chunk.
 */

ObjFunction* compile(const char *source);
void markCompilerRoots();

#endif //CFER_COMPILER_H