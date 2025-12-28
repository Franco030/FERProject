#ifndef CFER_VM_H
#define CFER_VM_H

#include "chunk.h"
#include "value.h"

/*
 * The virtual machine is one part of our interpreter's internal architecture.
 * We hand it a chunk of code, literally a Chunk, and it runs it.
 *
 * As the VM works its way through the bytecode, it keeps track of where it is,
 * the location of the instruction currently being executed.
 * We don't use a local variable inside run() for this because eventually other functions will need to access it.
 *
 * Its type is a byte pointer. We use an actual real C pointer right into the middle of the bytecode array
 * instead of something like an integer index because it's faster to dereference a pointer than look up an element
 * in an array by index.
 *
 * The name "IP" is traditional, it's an instruction pointer.
 * We initialize ip by pointing it at the first byte of code in the chunk. We haven't executed that instruction yes,
 * so ip points to the instruction about to be executed. This will be true during the entire time the VM is running:
 * the IP always points to the next instruction, not the one currently being handled.
 *
 * Our compiled bytecode needs a way to shuttle values around between the different instructions that need them.
 * For example:
 * print 3 - 2;
 * We obviously need instructions for the constants 3 and 2, the print statement, and the substraction.
 * But how does the substraction know that 3 is the minuend and 2 is the subtrahend? How does the print instruction know to print the result of that?
 * The operands to an arithmetic operator obviously need to be evaluated before we can perform the operation itself.
 *
 * Since the temporary values we need to track naturally have stack-like behavior,
 * our VM will use a stack to manage them. When it needs to consume one or more values,
 * it gets them by popping them off the stack.
 *
 * We implement the stack semantics ourselves on top of a raw C array.
 * The bottom of the stack, the first value pushed and the last to be popped, is at element zero in the array,
 * and later pushed values follow it. If we push the letters of "hello" on to the stack, in order,
 * the resulting C array looks like this:
 *
 *         0   1   2   3   4   5
 * bottom [h] [e] [l] [l] [o] [ ] top
 *
 * Since the stack grows and shrinks as values are pushed and popped, we need to track where the top of the stack in the array.
 * As with ip, we use a direct pointer instead of an integer index since it's faster to dereference the pointer than calculate the offset
 * from the index each time we need it.
 *
 * The pointer points at the array element just past the element containing the top value on the stack.
 * That seems a little odd, but almost every implementation does this. It means we can indicate that the stack is empty
 * by pointing at element zero in the array.
 *  0        1   2   3   4   5
 * [ ]      [ ] [ ] [ ] [ ] [ ]
 * stackTop
 *
 * If we pointed to the top element, then for an empty stack we'd need to point at element -1. That's undefined in C. As we push values onto the stack...
 *  0   1        2   3   4   5
 * [h] [ ]      [ ] [ ] [ ] [ ]
 *     stackTop
 *
 * ...stackTop always points just past the last item
 * The maximum number of values we can store on the stack (for now, at least) is 256.
 *
 * Giving out VM a fixed stack size means it's possible for some sequence of instructions to push too many values and run out of stack space.
 * We could grow the stack dynamically as needed, but for now, we'll keep it simple.
 */

#define STACK_MAX 256

typedef struct {
    Chunk *chunk;
    uint8_t *ip;
    Value stack[STACK_MAX];
    Value *stackTop;
} VM;

/*
 * The VM runs the chunk and then responds with a value from this enum.
 * When the compiler reports static errors and the VM detect runtime errors,
 * the interpreter will use this to know how to set the exit code of the process.
 */

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

/*
 * Like we do with most of the data structures we create,
 * we also define functions to create and tear down a VM.
 */

void initVM();
void freeVM();
InterpretResult interpret(const char *source);

/*
 * The stack protocol supports two operations
 */

void push(Value value);
Value pop();

#endif //CFER_VM_H