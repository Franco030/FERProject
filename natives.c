#include <stdio.h>
#include <time.h>

#include "vm.h"
#include "natives.h"

static Value clockNative(int argCount, Value *args) {
    return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

static Value strNative(int argCount, Value *args) {
    if (argCount != 1) {
        return NIL_VAL;
    }

    Value value = args[0];

    if (IS_STRING(value)) {
        return value;
    }

    char buffer[1024];
    int length = 0;

    if (IS_BOOL(value)) {
        length = sprintf(buffer, "%s", AS_BOOL(value) ? "true" : "false");
    } else if (IS_NIL(value)) {
        length = sprintf(buffer, "nil");
    } else if (IS_NUMBER(value)) {
        length = sprintf(buffer, "%g", AS_NUMBER(value));
    }

    return OBJ_VAL(copyString(buffer, length));
}

static Value lengthNative(int argCount, Value *args) {
    if (argCount > 1 || !IS_LIST(args[0])) {
        return NIL_VAL;
    }

    ObjList* list = AS_LIST(args[0]);
    return NUMBER_VAL(list->count);
}

void defineAllNatives() {
    defineNative("clock", clockNative);
    defineNative("str", strNative);
    defineNative("len", lengthNative);
}