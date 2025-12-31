#include <stdarg.h>
#include <stdio.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "vm.h"

#include <stdio.h>

#include "debug.h"

VM vm;

static void resetStack() {
    vm.stackTop = vm.stack;
}

/*
 * Variadic function can take a varying number of arguments.
 * Callers can pass a format string to runtimeError() followed by a number of arguments,
 * just like they can when calling printf() directly.
 * runtimeError() then formats and prints those arguments.
 *
 * After we show the error message, we tell the user which line of their code was being executed when the error occurred.
 * Since we left the tokens behind in the compiler, we look up the line in the debug information compiled into the chunk.
 * If our compiler did its job right, that corresponds to the line of source code that the bytecode was compiled from.
 *
 * We look into the chunk's debug line array using the current bytecode instruction index minus one. That's because the interpreter
 * advances past each instruction before executing it. So, at the point we call runtimeError(),
 * the failed instruction is the previous one.
 */

static void runtimeError(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    size_t instruction = vm.ip - vm.chunk->code - 1;
    int line = vm.chunk->lines[instruction];
    fprintf(stderr, "[line %d] in script\n", line);
    resetStack();
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
 * The following function returns a Value from the stack but doesn't pop it.
 * The distance argument is how far down from the top of the stack to look: zero is the top, one is one slot down, etc.
 * It's important to remember that the stackTop pointer is pointing where the next value is going to be (e.g., [1] >[]<).
 */

static Value peek(int distance) {
    return vm.stackTop[-1 - distance];
}

/*
 * For unary minus, we made it an error to negate anything that isn't a number.
 * But Fer, like most scripting languages, is more permissive when it comes to ! and other contexts where a Boolean is exprected.
 * The rule for how other types are handled is called "falsiness", and we implement it here:
 */

static bool isFalsey(Value value) {
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
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
#define BINARY_OP(valueType, op) \
    do { \
        if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
            runtimeError("Operands must be numbers."); \
        } \
        double b = AS_NUMBER(pop()); \
        double a = AS_NUMBER(pop()); \
        push(valueType(a op b)); \
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
            case OP_NIL: push(NIL_VAL); break;
            case OP_TRUE: push(BOOL_VAL(true)); break;
            case OP_FALSE: push(BOOL_VAL(false)); break;
            case OP_EQUAL: {
                Value b = pop();
                Value a = pop();
                push(BOOL_VAL(valuesEqual(a, b)));
                break;
            }
            case OP_GREATER: BINARY_OP(BOOL_VAL, >); break;
            case OP_LESS: BINARY_OP(BOOL_VAL, <); break;
            case OP_ADD: BINARY_OP(NUMBER_VAL, +); break;
            case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -); break;
            case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *); break;
            case OP_DIVIDE: BINARY_OP(NUMBER_VAL, /); break;
            case OP_NOT:
                push(BOOL_VAL(isFalsey(pop())));
                break;
            case OP_NEGATE: {
                if (!IS_NUMBER(peek(0))) {
                    runtimeError("Operand must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(NUMBER_VAL(-AS_NUMBER(pop())));
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
 *
 * We create a new empty chunk and pass it over to the compiler. The compiler will take the user's program and fill up the chunk with bytecode.
 * That's what it will do if the program doesn't have any compile errors. If it does encounter an error,
 * compile() returns false and we discard the unusable chunk.
 *
 * Otherwise, we send the completed chunk over to the VM to be executed. When te VM finishes, we free the chunk and we're done.
 */

InterpretResult interpret(const char *source) {
    Chunk chunk;
    initChunk(&chunk);

    if (!compile(source, &chunk)) {
        freeChunk(&chunk);
        return INTERPRET_COMPILE_ERROR;
    }

    vm.chunk = &chunk;
    vm.ip = vm.chunk->code;

    InterpretResult result = run();

    freeChunk(&chunk);
    return result;
}
