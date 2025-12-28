#include <stdio.h>

#include "common.h"
#include "debug.h"
#include "vm.h"

#include <stdio.h>

#include "debug.h"

VM vm;

static void resetStack() {
    vm.stackTop = vm.stack;
}

void initVM() {
    resetStack();
}

void freeVM() {

}

void push(Value value) {
    *vm.stackTop = value;
    vm.stackTop++;
}

Value pop() {
    vm.stackTop--;
    return *vm.stackTop;
}

/*
 * This is the single most important function in all of cfer, by far.
 * When te interpreter executes a user's program, it will spend something like 90% of its time inside run().
 * It is the beating heart of the VM.
 *
 * We have an outer loop that goes and goes. Each turn through that loop, we rean and execute a single bytecode instruction.
 * To process an instruction, we first figure out what kind of instruction we're dealing with.
 * The READ_BYTE macro reads the byte currently pointed at by ip and then advances the instruction pointer.
 * The first byte on any instruction is the opcode. Given a numeric opcode, we need to get to the right C code that implements
 * that instruction's semantics. This process is called decoding or dispatching the instruction.
 * Note that ip advances as soon as we read the opcode, before we've actually started executing the instruction.
 * So, again, ip points to the next byte of code to be used. How (*vm.ip++) works:
 * Even though it looks simple, C operator precedence rules are strict here. The expression is evaluated in this specific order:
 *  1. vm.ip++ (Post-increment): The ++ operator has higher precedence than *. Becuase it is a post-increment, it effectively says:
 *  "Remember the current value of ip for the rest of this expression, but increase the actual variable ip by 1 immediately after."
 *  2. * (Dereference): The code dereferences the original address (the one remembered before the increment happened)
 * It's the same as writing:
 * uint8_t instruction = *vm.ip;
 * vm.ip++;
 * But macros avoid the overhead of a function call.
 *
 * We do that process for every single instruction, every single time one is executed,
 * so this is the most performance critical part of the entire virtual machine.
 * There are clever techniques to do bytecode dispatch efficiently, but the fastest solutions require either non-standard extensions to C,
 * or handwritten assembly code. We'll keep it simple for now. Just like our disassembler, we have a single giant switch statement with a case for each opcode.
 * The body of each case implements that opcode's behavior.
 *
 */

static InterpretResult run() {
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
#define BINARY_OP(op) \
    do { \
        double b = pop(); \
        double a = pop(); \
        push (a op b); \
    } while (false)
    for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
        printf("          ");
        for (Value *slot = vm.stack; slot < vm.stackTop; slot++) {
            printf("[ ");
            printValue(*slot);
            printf(" ]");
        }
        disassembleInstruction(vm.chunk, (int)(vm.ip-vm.chunk->code));
#endif
        uint8_t instruction;
        switch (instruction = READ_BYTE()) {
            case OP_CONSTANT: {
                Value constant = READ_CONSTANT();
                push(constant);
                break;
            }
            case OP_ADD: BINARY_OP(+); break;
            case OP_SUBTRACT: BINARY_OP(-); break;
            case OP_MULTIPLY: BINARY_OP(*); break;
            case OP_DIVIDE: BINARY_OP(/); break;
            case OP_NEGATE: {
                push(-pop());
                break;
            }
            case OP_RETURN: {
                printValue(pop());
                printf("\n");
                return INTERPRET_OK;
            }
        }
    }
#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
}

/*
 * First we store the chunk being executed in the VM.
 * Then we call run(), and internal helper function that actually runs the bytecode instructions.
 * Between those two parts there's 'ip'.
 *
 * As the VM works its way through the bytecode, it keeps track of where it is,
 * the location of the instruction currently being executed.
 * We don't use a local variable inside run() for this because eventually other functions will need to access it.
 */

InterpretResult interpret(Chunk *chunk) {
    vm.chunk = chunk;
    vm.ip = vm.chunk->code;
    return run();
}