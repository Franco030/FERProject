#include <stdlib.h>

#include "compiler.h"
#include "memory.h"
#include "value.h"
#include "vm.h"

#ifdef DEBUG_LOG_GC
#include <stdio.h>
#include "debug.h"
#endif


/*
 * oldSize  | newSize                | Operation
 * ------------------------------------------------------------
 * 0        | Non-zero               | Allocate new block
 * ------------------------------------------------------------
 * Non-zero | 0                      | Free allocation
 * ------------------------------------------------------------
 * Non-zero | Smaller than oldSize   | Shrink existing allocation
 * ------------------------------------------------------------
 * Non-zero | Larger than oldSize    | Grow existing allocation
 *
 * When newSize is zero, we handle the deallocation case ourselves by calling free().
 * Otherwise, we rely on the C standard library's realloc() function.
 * That function conveniently supports the other three aspects of our policy.
 * When oldSize is zero, realloc() is equivalent to calling malloc().
 *
 * The interesting cases are when both OldSize and newSize are not zero.
 * Those tell realloc() to resize the previously allocated block.
 * If the new size is smaller than the existing block of memory,
 * it simply updates the size of the block and returns the same pointer you gave it.
 * If the new size is larger, it attempts to grow the existing block of memory.
 *
 * It can do that only if the memory after that block isn't already in use. If there isn't room to grow the block,
 * realloc() instead allocates a new block of memory of the desired size,
 * copies over the old bytes, frees the old block, and then returns a pointer to the new block.
 * That's exactly the behavior we want for our dynamic array. (This is explained in more detail in the tutorial project)
 *
 * There's not really anything useful that our VM can do if it can't get the memory it needs,
 * but we at least detect that and abort the process immediately instead of return a NULL pointer
 * and letting it go off the rails later.
 */

void* reallocate(void *pointer, size_t oldSize, size_t newSize) {
    if (newSize > oldSize) {
#ifdef DEBUG_STRESS_GC
        collectGarbage();
#endif
    }

    if (newSize == 0) {
        free(pointer);
        return NULL;
    }

    void *result = realloc(pointer, newSize);
    if (result == NULL) exit(1);
    return result;
}

void markObject(Obj *object) {
    if (object == NULL) return;
#ifdef DEBUG_LOG_GC
    printf("%p mark ", (void*)object);
    printValue(OBJ_VAL(object));
    printf("\n");
#endif

    object->isMarked = true;
}

void markValue(Value value) {
    if (IS_OBJ(value)) markObject(AS_OBJ(value));
}

static void freeObject(Obj *object) {
#ifdef DEBUG_LOG_GC
    printf("%p free type %d\n", (void*)object, object->type);
#endif

    switch (object->type) {
        case OBJ_CLOSURE: {
            ObjClosure *closure = (ObjClosure*)object;
            FREE_ARRAY(ObjUpvalue*, closure->upvalues, closure->upvalueCount);
            FREE(ObjClosure, object);
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction *function = (ObjFunction*)object;
            freeChunk(&function->chunk);
            FREE(ObjFunction, object);
            break;
        }
        case OBJ_NATIVE:
            FREE(ObjNative, object);
            break;
        case OBJ_STRING: {
            ObjString *string = (ObjString*)object;
            FREE_ARRAY(char, string->chars, string->length + 1);
            FREE(ObjString, object);
        }
        case OBJ_UPVALUE:
            FREE(ObjUpvalue, object);
            break;
    }
}

static void markRoots() {
    for (Value *slot = vm.stack; slot < vm.stackTop; slot++) {
        markValue(*slot);
    }

    for (int i = 0; i < vm.frameCount; i++) {
        markObject((Obj*)vm.frames[i].closure);
    }

    for (ObjUpvalue *upvalue = vm.openUpvalues; upvalue != NULL; upvalue = upvalue->next) {
        markObject((Obj*)upvalue);
    }

    markTable(&vm.globals);
    markCompilerRoots();
}

void collectGarbage() {
#ifdef DEBUG_LOG_GC
    printf("-- gc begin\n");
#endif

    markRoots();

#ifdef DEBUG_LOG_GC
    printf("-- gc end\n");
#endif
}

void freeObjects() {
    Obj *object = vm.objects;
    while (object != NULL) {
        Obj *next = object->next;
        freeObject(object);
        object = next;
    }
}








