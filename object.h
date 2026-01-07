#ifndef CFER_OBJECT_H
#define CFER_OBJECT_H

#include "common.h"
#include "chunk.h"
#include "table.h"
#include "value.h"

#define OBJ_TYPE(value)         (AS_OBJ(value)->type)

#define IS_BOUND_METHOD(value)  isObjType(value, OBJ_BOUND_METHOD)
#define IS_CLASS(value)         isObjType(value, OBJ_CLASS)
#define IS_CLOSURE(value)       isObjType(value, OBJ_CLOSURE)
#define IS_FUNCTION(value)      isObjType(value, OBJ_FUNCTION)
#define IS_INSTANCE(value)      isObjType(value, OBJ_INSTANCE)
#define IS_NATIVE(value)        isObjType(value, OBJ_NATIVE)
#define IS_STRING(value)        isObjType(value, OBJ_STRING)
#define IS_LIST(value)          isObjType(value, OBJ_LIST)

#define AS_BOUND_METHOD(value)  ((ObjBoundMethod*)AS_OBJ(value))
#define AS_CLASS(value)         ((ObjClass*)AS_OBJ(value))
#define AS_CLOSURE(value)       ((ObjClosure*)AS_OBJ(value))
#define AS_FUNCTION(value)      ((ObjFunction*)AS_OBJ(value))
#define AS_INSTANCE(value)      ((ObjInstance*)AS_OBJ(value))
#define AS_NATIVE(value)        (((ObjNative*)AS_OBJ(value))->function)
#define AS_STRING(value)        ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)       (((ObjString*)AS_OBJ(value))->chars)
#define AS_LIST(value)          ((ObjList*)AS_OBJ(value))

typedef enum {
    OBJ_BOUND_METHOD,
    OBJ_CLASS,
    OBJ_CLOSURE,
    OBJ_FUNCTION,
    OBJ_INSTANCE,
    OBJ_NATIVE,
    OBJ_STRING,
    OBJ_LIST,
    OBJ_UPVALUE
} ObjType;

struct Obj {
    ObjType type;
    bool isMarked;
    struct Obj *next;
};

/*
 * Functions are first class in Fer, so they need to be actual Fer objects.
 * Thus, ObjFunction has te same Obj header that all object types share.
 * The arity field stores the number of parameters the function expects.
 * That will be handy for reporting readable runtime errors.
 */

typedef struct {
    Obj obj;
    int arity;
    int upvalueCount;
    Chunk chunk;
    ObjString *name;
} ObjFunction;

typedef Value (*NativeFn)(int argCount, Value *args);

typedef struct {
    Obj obj;
    NativeFn function;
} ObjNative;

/*
 * A string object contains an array of characters. Those are stored in a separate heap-allocated array
 * so that we set aside only as much room as needed for each string.
 * We also store the number of bytes in the array.
 * This isn't strictly necessary but lets us tell how much memory is allocated for the string without walking the character array to find the null terminator.
 *
 * Because ObjString is an Obj, it also needs the state all Objs share.
 * It accomplishes this by having its first field be an Obj.
 * C specifies that struct fields are arranged in memory in the order that they are declared.
 * Also, when you nest structs, the inner struct's fields are expanded right in place. So the memory for Obj and ObjString looks like this:
 *
 * Obj       [] [] [] []
 *           ObjType type
 *
 * ObjString [] [] [] [] [] [] [] [] [] [] [] [] [] [] [] []
 *           Obj obj    | int length| char *chars
 *
 * Note how the first bytes of ObjString exactly line up with Obj. This is not a coincidence, C mandates it.
 * This is designed to enable a clever pattern:
 * You can take a pointer to a struct and safely convert it to a pointer to its first field and back.
 *
 * Given an ObjString*, you can safely cast it to Obj* and then access the type field from it.
 * Every ObjString "is" and Obj in the OOP sense of "is".
 * When we later add other object types, each struct will have an Obj as its first field.
 * Any code that wants to work with all objects can treat them as base Obj* and ignore any other fields that may happen to follow.
 *
 * You can go in the other direction too. Given an Obj*, you can "downcast" it to an ObjString*.
 * Of course, you need to ensure that the Obj* pointer you have does point to the obj field of an actual ObjString.
 * Otherwise, you are unsafely reinterpreting random bits of memory. To detect that such cast is safe, we added another macro.
 */

struct ObjString {
    Obj obj;
    int length;
    char *chars;
    uint32_t hash;
};

typedef struct {
    Obj obj;
    int count;
    int capacity;
    Value *values;
} ObjList;

typedef struct ObjUpvalue {
    Obj obj;
    Value *location;
    Value closed;
    struct ObjUpvalue *next;
} ObjUpvalue;

typedef struct {
    Obj obj;
    ObjFunction *function;
    ObjUpvalue **upvalues;
    int upvalueCount;
} ObjClosure;

typedef struct {
    Obj obj;
    ObjString *name;
    Table methods;
} ObjClass;

typedef struct {
    Obj obj;
    ObjClass *cls;
    Table fields;
} ObjInstance;

typedef struct {
    Obj obj;
    Value receiver;
    ObjClosure *method;
} ObjBoundMethod;

ObjBoundMethod* newBoundMethod(Value receiver, ObjClosure *method);
ObjClass* newClass(ObjString *name);
ObjClosure* newClosure(ObjFunction *function);
ObjFunction* newFunction();
ObjInstance* newInstance(ObjClass *cls);
ObjNative* newNative(NativeFn function);
ObjString* takeString(char *chars, int length);
ObjString* copyString(const char *chars, int length);
ObjList* newList();
ObjUpvalue* newUpvalue(Value *slot);
void printObject(Value value);

static inline bool isObjType(Value value, ObjType type) {
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif //CFER_OBJECT_H