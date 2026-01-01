#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"

/*
 * The following macro and function work together to allocate an object of the given size on the heap.
 * Note that the size is not just the size of Obj itself. The caller passes in the number of bytes so that there is room
 * for the extra payload fields needed by the specific object type being created.
 * Then it initializes the Obj state.
 *
 * The macro exists mainly to avoid the need to redundantly cast a void* back to a desired type.
 */

#define ALLOCATE_OBJ(type, objectType) \
    (type*)allocateObject(sizeof(type), objectType)

static Obj* allocateObject(size_t size, ObjType type) {
    Obj *object = (Obj*)reallocate(NULL, 0, size);
    object->type = type;

    object->next = vm.objects;
    vm.objects = object;
    return object;
}

/*
 * The following functions creates a new ObjString on the heap and then initializes its fields.
 * It's sort of like a constructor in an OOP language. As such, it first calls the "base class"
 * constructor to initialize the Obj state, using the ALLOCATE_OBJ macro.
 */

static ObjString* allocateString(char *chars, int length, uint32_t hash) {
    ObjString *string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;
    tableSet(&vm.strings, string, NIL_VAL);
    return string;
}

/*
 * This algorithm is called "FNV-1a"
 */

static uint32_t hashString(const char *key, int length) {
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; i++) {
        hash ^= (uint8_t)key[i];
        hash *= 16777619;
    }
    return hash;
}

ObjString* takeString(char *chars, int length) {
    uint32_t hash = hashString(chars, length);
    ObjString *interned = tableFindString(&vm.strings, chars, length, hash);
    if (interned != NULL) {
        FREE_ARRAY(char, chars, length + 1);
        return interned;
    }

    return allocateString(chars, length, hash);
}

/*
 * First we allocate a new array on the heap, just big enough for the string's characters and the trailing terminator.
 * Once we have the array, we copy over the caracters from the lexeme and terminate it.
 *
 * You might wonder why the ObjString can't just point back to the original characters in the source string.
 * Some ObjStrings will be created dynamically at runtime as a result of string operations like concatenation.
 * Those strings obviously need to dynamically allocate memory for the characters,
 * which means the string needs to free that memory when it's no longer needed.
 *
 * If we had an ObjString for a string literal,
 * and tried to free its character array that pointed into the original source code string, bad things would happen.
 * So, for literals, we preemptively copy the characters over to the heap. This way, every ObjString reliably owns its character arran anc can free it.
 */

ObjString* copyString(const char *chars, int length) {
    uint32_t hash = hashString(chars, length);
    ObjString *interned = tableFindString(&vm.strings, chars, length, hash);
    if (interned != NULL) return interned;

    char *heapChars = ALLOCATE(char, length + 1);
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';
    return allocateString(heapChars, length, hash);
}

void printObject(Value value) {
    switch (OBJ_TYPE(value)) {
        case OBJ_STRING:
            printf("%s", AS_CSTRING(value));
            break;
    }
}