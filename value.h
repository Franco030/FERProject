#ifndef CFER_VALUE_H
#define CFER_VALUE_H

/*
 * Now that we have a rudimentary chunk structure working, let's start making it more useful.
 * We can store code in chunks, but what about data?
 * Many values the interpreter works with are created at runtime as the result of operations.
 * | 1 + 2; |
 * The value 3 appears nowhere in the code here. However, the literals 1 and 2 do.
 * To compile that statement to bytecode, we need some sort of instruction that means "produce a constant"
 * and those literal values need to get stored in the chunk somewhere.
 * On an AST Interpreter the Expr.Literal AST node held the value.
 * We need a different solution now that we don't have a syntax tree
 *
 * For now, we're going to start as simple as possible, we'll support only double precision, floating-point numbers.
 * This will obviously expand over time.
 */

/*
 * This typedef abstracts how Fer values are concretely represented in C.
 * That way we can change that representation without needing to go back and fix existing code
 * that passes around .
 *
 * Back to the question of where to store constants in a chunk.
 * For small fixed-size values like integers, many instruction sets store the value directly in the code stream right after the opcode.
 * These are called immediate instructions because the bits for the value are immediately after the opcode.
 *
 * That doesn't work well for larger of variable-sized constants like strings.
 * In a native compiler to machine code, those bigger constants get stored in a separate "constant data" region in the binary executable.
 * Then, the instruction to load a constant has an address or offset pointing to where the value is stored in that section.
 *
 * Most virtual machines do something similar. For example, the Java Virtual Machine associates a constant pool with each compiled class.
 * That sounds good enough for cfer. Each chunk will carry with a list of the values that appear as literals in the program.
 * To keep things simpler, we'll put all constants in there, even simple integers.
 */

typedef double Value;

typedef struct {
    int capacity;
    int count;
    Value *values;
} ValueArray;

void initValueArray(ValueArray *array);
void writeValueArray(ValueArray *array, Value value);
void freeValueArray(ValueArray *array);
void printValue(Value value);

#endif //CFER_VALUE_H