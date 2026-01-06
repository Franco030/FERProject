#ifndef CFER_VALUE_H
#define CFER_VALUE_H

#include <string.h>

#include "common.h"

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
 */

/*
 * Every heap-allocated value is an Obj, but Objs are not all the same.
 * For strings, we need the array of characters. When we get to instances, they will need their data fields.
 * A function object will need its chunk of bytecode. How do we handle different payloads and sizes?
 * We can't use another union like we did for Value since the sizes are all over the place.
 *
 * Instead, we'll use another technique. It's an example of type punning, but that term is too broad.
 * It's going to be called struct inheritance, because it relies on struct and roughly follows how single-inheritance of state works
 * in object-oriented languages.
 *
 * Like a tagged union, each Obj starts with a tag field that identifies which kind of object it is, string, instance, etc.
 * Following that are the payload fields.
 * Instead of a union with cases for each type, each type is its own separate struct.
 * The tricky part is how to treat these structs uniformly since C has no concept of inheritance or polymorphism.
 *
 * The name "Obj" itself refers to a struct that contains the state shared across all object types.
 * It's sort of like the "base class" for object. Because of some cyclic dependencies between values and object, we forward-declare it.
 */

typedef struct Obj Obj;
typedef struct ObjString ObjString;

/*
 * 1. How do we represent the type of a value? If you try to, say, multiply a number by true, we need to detect that error at runtime and report it.
 * In order to do that, we need to be able to tell what a value's type is.
 *
 * 2. How do we store the value itself? We need to not only be able to tell that three is a number,
 * but that it's different from the number four.
 *
 * For now, we'll start with the simplest, classic solution: a tagged union. A value contains two parts: a type "tag",
 * and a payload for the actual value. To store the value's type, we define an enum for each kind of value the VM supports.
 *
 * The cases here cover each kind of value that has built-in support in the VM. When we get to adding classes to the language,
 * each class the user defines doesn't need its own entry in this enum. As far as the VM is concerned, every instance of a class is the same type:
 * "instance".
 *
 * In other words, this is the VM's notion of "type", not the user's
 *
 *
 * In order to support strings, we need a way to support values whose sizes vary.
 * This is exactly what dynamic allocation on the heap is designed for.
 * We can allocate as many bytes as we need. We get back a pointer that we'll use to keep track of the value as it flows through the VM.
 *
 * Using the heap for larger, variable-sized values and the stack for smaller, atomic ones leads to a two-level representation.
 * Every Fer value that you can store in a variable or return from an expression will be a Value.
 * For small, fixed-size types like numbers, the payload is stored directly inside the Value struct itself.
 *
 * If the object is larger, its data lives on the heap.
 * Then the Value's payload is a pointer to that blob of memory. We'll eventually have a handful of heap-allocated types in cfer:
 * strings, instances, functions. Each type has its own unique data, but there is also state they all share that the garbage collector will use
 * to manage their memory.
 *
 */

#ifdef NAN_BOXING

#define SIGN_BIT ((uint64_t)0x8000000000000000)
// This has set all the quiet NaN bits to 1. Exactly all the exponent bits, plus the quiet NaN bit, plus one extra to dodge that Intel value.
#define QNAN ((uint64_t)0x7ffc000000000000)

#define TAG_NIL     1 // 01
#define TAG_FALSE   2 // 10
#define TAG_TRUE    3 // 11

typedef uint64_t Value;

#define IS_BOOL(value)          ((value | 1) == TRUE_VAL)
#define IS_NIL(value)           ((value) == NIL_VAL)
#define IS_NUMBER(value)        (((value) & QNAN) != QNAN)
#define IS_OBJ(value)           (((value) & (QNAN | SIGN_BIT)) == (QNAN | SIGN_BIT))

#define AS_BOOL(value)          ((value) == TRUE_VAL)
#define AS_NUMBER(value)        valueToNum(value)
#define AS_OBJ(value)           ((Obj*)(uintptr_t)((value) & ~(SIGN_BIT | QNAN)))

#define BOOL_VAL(b)             ((b) ? TRUE_VAL : FALSE_VAL)
#define FALSE_VAL               ((Value)(uint64_t)(QNAN | TAG_FALSE))
#define TRUE_VAL                ((Value)(uint64_t)(QNAN | TAG_TRUE))
#define NIL_VAL                 ((Value)(uint64_t)(QNAN | TAG_NIL))
#define NUMBER_VAL(num)         numToValue(num)
#define OBJ_VAL(obj)            (Value)(SIGN_BIT | QNAN | (uint64_t)(uintptr_t)(obj))

static inline double valueToNum(Value value) {
    double num;
    memcpy(&num, &value, sizeof(Value));
    return num;
}

static inline Value numToValue(double num) {
    Value value;
    memcpy(&value, &num, sizeof(double));
    return value;
}

#else

typedef enum {
    VAL_BOOL,
    VAL_NIL,
    VAL_NUMBER,
    VAL_OBJ
} ValueType;

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

typedef struct {
    ValueType type;
    union {
        bool boolean;
        double number;
        Obj *obj;
    } as;
} Value;

/*
 * Given a Value of the right type, the macros unwrap it and return the corresponding raw C value. The "right type" part is important!
 * These macros directly access the union fields. If we were to do something like:
 * Value value = BOOL_VAL(true);
 * double number = AS_NUMBER(value);
 * It's not safe to use any of the AS_ macros unless we know the Value contains the appropriate type.
 * To that end, we define a few macros to check a Value's type
 */

#define IS_BOOL(value)      ((value).type == VAL_BOOL)
#define IS_NIL(value)       ((value).type == VAL_NIL)
#define IS_NUMBER(value)    ((value).type == VAL_NUMBER)
#define IS_OBJ(value)       ((value).type == VAL_OBJ)

#define AS_OBJ(value)       ((value).as.obj)
#define AS_BOOL(value)      ((value).as.boolean)
#define AS_NUMBER(value)    ((value).as.number)

#define BOOL_VAL(value)     ((Value){VAL_BOOL, {.boolean = value}})
#define NIL_VAL             ((Value){VAL_NIL, {.number = 0}})
#define NUMBER_VAL(value)   ((Value){VAL_NUMBER, {.number = value}})
#define OBJ_VAL(object)      ((Value){VAL_OBJ, {.obj = (Obj*)object}})

#endif

typedef struct {
    int capacity;
    int count;
    Value *values;
} ValueArray;

bool valuesEqual(Value a, Value b);
void initValueArray(ValueArray *array);
void writeValueArray(ValueArray *array, Value value);
void freeValueArray(ValueArray *array);
void printValue(Value value);

#endif //CFER_VALUE_H