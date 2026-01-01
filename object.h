#ifndef CFER_OBJECT_H
#define CFER_OBJECT_H

#include "common.h"
#include "value.h"

#define OBJ_TYPE(value)     (AS_OBJ(value)->type)
#define IS_STRING(value)    isObjType(value, OBJ_STRING)

#define AS_STRING(value)    ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)   (((ObjString*)AS_OBJ(value))->chars)

typedef enum {
    OBJ_STRING,
} ObjType;

struct Obj {
    ObjType type;
    struct Obj *next;
};

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

ObjString* takeString(char *chars, int length);
ObjString* copyString(const char *chars, int length);
void printObject(Value value);

static inline bool isObjType(Value value, ObjType type) {
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif //CFER_OBJECT_H