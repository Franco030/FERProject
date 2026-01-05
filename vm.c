#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "object.h"
#include "memory.h"
#include "vm.h"

#include <stdio.h>
#include <time.h>

#include "debug.h"

VM vm;

static Value clockNative(int argCount, Value *args) {
    return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

static void resetStack() {
    vm.stackTop = vm.stack;
    vm.frameCount = 0;
    vm.openUpvalues = NULL;
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

    for (int i = vm.frameCount - 1; i >= 0; i--) {
        CallFrame *frame = &vm.frames[i];
        ObjFunction *function = frame->closure->function;
        size_t instruction = frame->ip - function->chunk.code - 1;
        fprintf(stderr, "[line %d] in ", function->chunk.lines[instruction]);
        if (function->name == NULL) {
            fprintf(stderr, "script\n");
        } else {
            fprintf(stderr, "%s()\n", function->name->chars);
        }
    }

    resetStack();
}

static void defineNative(const char *name, NativeFn function) {
    push(OBJ_VAL(copyString(name, (int)strlen(name))));
    push(OBJ_VAL(newNative(function)));
    tableSet(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
    pop();
    pop();
}

void initVM() {
    resetStack();
    vm.objects = NULL;
    initTable(&vm.globals);
    initTable(&vm.strings);

    defineNative("clock", clockNative);
}

void freeVM() {
    freeTable(&vm.globals);
    freeTable(&vm.strings);
    freeObjects();
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

static bool call(ObjClosure *closure, int argCount) {
    if (argCount != closure->function->arity) {
        runtimeError("Expected %d arguments but got %d.", closure->function->arity, argCount);
        return false;
    }

    if (vm.frameCount == FRAMES_MAX) {
        runtimeError("Stack overflow.");
        return false;
    }

    CallFrame *frame = &vm.frames[vm.frameCount++];
    frame->closure = closure;
    frame->ip = closure->function->chunk.code;
    frame->slots = vm.stackTop - argCount - 1;
    return true;
}

static bool callValue(Value callee, int argCount) {
    if (IS_OBJ(callee)) {
        switch (OBJ_TYPE(callee)) {
            case OBJ_CLOSURE:
                return call(AS_CLOSURE(callee), argCount);
            case OBJ_NATIVE: {
                NativeFn native = AS_NATIVE(callee);
                Value result = native(argCount, vm.stackTop - argCount);
                vm.stackTop -= argCount + 1;
                push(result);
                return true;
            }
            default:
                break; // Non-callable object type
        }
    }
    runtimeError("Can only call functions and classes.");
    return false;
}

static ObjUpvalue* captureUpvalue(Value *local) {
    ObjUpvalue *prevUpvalue = NULL;
    ObjUpvalue *upvalue = vm.openUpvalues;
    while (upvalue != NULL && upvalue->location > local) {
        prevUpvalue = upvalue;
        upvalue = upvalue->next;
    }

    if (upvalue != NULL && upvalue->location == local) {
        return upvalue;
    }

    ObjUpvalue *createdUpvalue = newUpvalue(local);
    createdUpvalue->next = upvalue;

    if (prevUpvalue == NULL) {
        vm.openUpvalues = createdUpvalue;
    } else {
        prevUpvalue->next = createdUpvalue;
    }

    return createdUpvalue;
}

static void closeUpvalues(Value *last) {
    while (vm.openUpvalues != NULL && vm.openUpvalues->location >= last) {
        ObjUpvalue *upvalue = vm.openUpvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        vm.openUpvalues = upvalue->next;
    }
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
 * First, we calculate the length of the result string based on the lengths of the operands.
 * We allocates a character array for the result and then copy the two halves in.
 */

static void concatenate() {
    ObjString *b = AS_STRING(pop());
    ObjString *a = AS_STRING(pop());

    int length = a->length + b->length;
    char *chars = ALLOCATE(char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    ObjString *result = takeString(chars, length);
    push(OBJ_VAL(result));
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

/*
 * Note that we don't always push values after executing a bytecode. This is a key difference between expressions and statements in the VM.
 * Every bytecode instruction has a stack effect that describes how the instruction modifies the stack.
 * For example, OP_ADD pops two vlaues and pushes one, leaving the stack one element smaller than before.
 *
 * You can sum the stack effects of a series of instructions to get their total effect.
 * When you add the stack effects of the series of instructions compiled from any complete expression, it will total one.
 * Each expression leaves one result value on the stack.
 *
 * The bytecode for an entire statement has a total stack effect of zero.
 * Since statement produces no values, it ultimately leaves the stack unchanged,
 * though it of course uses the stack while it's doing its thing.
 * This is important because when we get to control flow and looping, a program might execute a long series of statements.
 * If each statement grew of shrank the stack, it might eventually overflow or underflow.
 */

static InterpretResult run() {
    CallFrame *frame = &vm.frames[vm.frameCount - 1];
#define READ_BYTE() (*frame->ip++)
#define READ_CONSTANT() (frame->closure->function->chunk.constants.values[READ_BYTE()])
#define READ_SHORT() (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_STRING() AS_STRING(READ_CONSTANT())
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
        disassembleInstruction(&frame->closure->function->chunk, (int)(frame->ip - frame->closure->function->chunk.code));
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
            case OP_POP: pop(); break;
            case OP_GET_LOCAL: {
                uint8_t slot = READ_BYTE();
                push(frame->slots[slot]);
                break;
            }
            case OP_SET_LOCAL: {
                uint8_t slot = READ_BYTE();
                frame->slots[slot] = peek(0);
                break;
            }
            case OP_GET_GLOBAL: {
                ObjString *name = READ_STRING();
                Value value;
                if (!tableGet(&vm.globals, name, &value)) {
                    runtimeError("Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(value);
                break;
            }
            case OP_DEFINE_GLOBAL: {
                ObjString *name = READ_STRING();
                tableSet(&vm.globals, name, peek(0));
                pop();
                break;
            }
            case OP_SET_GLOBAL: {
                ObjString *name = READ_STRING();
                if (tableSet(&vm.globals, name, peek(0))) {
                    tableDelete(&vm.globals, name);
                    runtimeError("Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_GET_UPVALUE: {
                uint8_t slot = READ_BYTE();
                push(*frame->closure->upvalues[slot]->location);
                break;
            }
            case OP_SET_UPVALUE: {
                uint8_t slot = READ_BYTE();
                *frame->closure->upvalues[slot]->location = peek(0);
                break;
            }
            case OP_EQUAL: {
                Value b = pop();
                Value a = pop();
                push(BOOL_VAL(valuesEqual(a, b)));
                break;
            }
            case OP_GREATER: BINARY_OP(BOOL_VAL, >); break;
            case OP_LESS: BINARY_OP(BOOL_VAL, <); break;
            case OP_ADD: {
                if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
                    concatenate();
                } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
                    double b = AS_NUMBER(pop());
                    double a = AS_NUMBER(pop());
                    push(NUMBER_VAL(b + a));
                } else {
                    runtimeError("Operands must be two numbers or two strings");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
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
            case OP_PRINT: {
                printValue(pop());
                printf("\n");
                break;
            }
            case OP_JUMP: {
                uint16_t offset = READ_SHORT();
                frame->ip += offset;
                break;
            }
            case OP_JUMP_IF_FALSE: {
                uint16_t offset = READ_SHORT();
                if (isFalsey(peek(0))) frame->ip += offset;
                break;
            }
            case OP_LOOP: {
                uint16_t offset = READ_SHORT();
                frame->ip -= offset;
                break;
            }
            case OP_CALL: {
                int argCount = READ_BYTE();
                if (!callValue(peek(argCount), argCount)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm.frames[vm.frameCount - 1];
                break;
            }
            case OP_CLOSURE: {
                ObjFunction *function = AS_FUNCTION(READ_CONSTANT());
                ObjClosure *closure = newClosure(function);
                push(OBJ_VAL(closure));
                for (int i = 0; i < closure->upvalueCount; i++) {
                    uint8_t isLocal = READ_BYTE();
                    uint8_t index = READ_BYTE();
                    if (isLocal) {
                        closure->upvalues[i] = captureUpvalue(frame->slots + index);
                    } else {
                        closure->upvalues[i] = frame->closure->upvalues[index];
                    }
                }
                break;
            }
            case OP_CLOSE_UPVALUE:
                closeUpvalues(vm.stackTop - 1);
                pop();
                break;
            case OP_RETURN: {
                Value result = pop();
                closeUpvalues(frame->slots);
                vm.frameCount--;
                if (vm.frameCount == 0) {
                    pop();
                    return INTERPRET_OK;
                }

                vm.stackTop = frame->slots;
                push(result);
                frame = &vm.frames[vm.frameCount - 1];
                break;
            }
        }
    }
#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_STRING
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
    ObjFunction *function = compile(source);
    if (function == NULL) return INTERPRET_COMPILE_ERROR;

    push(OBJ_VAL(function));
    ObjClosure *closure = newClosure(function);
    pop();
    push(OBJ_VAL(closure));
    call(closure, 0);

    return run();
}