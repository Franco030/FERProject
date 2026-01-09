#include <stdio.h>
#include <time.h>

#include "vm.h"
#include "natives.h"

static Value lengthNative(int argCount, Value *args) {
    if (argCount > 1 || !IS_LIST(args[0])) {
        return NIL_VAL;
    }

    ObjList* list = AS_LIST(args[0]);
    return NUMBER_VAL(list->count);
}

/*
 * ----------------------------------------- STRING LIBRARY -----------------------------------------
 */

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

static Value subStrNative(int argCount, Value *args) {}
static Value toUpperNative(int argCount, Value *args) {}
static Value toLowerNative(int argCount, Value *args) {}
static Value indexOfNative(int argCount, Value *args) {}
static Value splitStrNative(int argCount, Value *args) {}
static Value trimStrNative(int argCount, Value *args) {}
static Value chrNative(int argCount, Value *args) {}
static Value ordNative(int argCount, Value *args) {}


/*
 * ----------------------------------------- MATH LIBRARY -----------------------------------------
 */

static Value sqrtNative(int argCount, Value *args) {}
static Value powNative(int argCount, Value *args) {}
static Value floorNative(int argCount, Value *args) {}
static Value ceilNative(int argCount, Value *args) {}
static Value randomNative(int argCount, Value *args) {}
static Value seedNative(int argCount, Value *args) {}
static Value sinNative(int argCount, Value *args) {}
static Value cosNative(int argCount, Value *args) {}
static Value tanNative(int argCount, Value *args) {}

/*
 * ----------------------------------------- COLLECTIONS LIBRARY -----------------------------------------
 */

static Value pushLsNative(int argCount, Value *args) {}
static Value popLsNative(int argCount, Value *args) {}
static Value insertLsNative(int argCount, Value *args) {}
static Value removeNative(int argCount, Value *args) {}
static Value containsNative(int argCount, Value *args) {}
static Value keysNative(int argCount, Value *args) {}
static Value hasKeyNative(int argCount, Value *args) {}
static Value deleteKeyNative(int argCount, Value *args) {}

/*
 * ----------------------------------------- TYPES LIBRARY -----------------------------------------
 */

static Value typeofNative(int argCount, Value *args) {}
static Value assertNative(int argCount, Value *args) {}

/*
 * ----------------------------------------- TIME LIBRARY -----------------------------------------
 */

static Value clockNative(int argCount, Value *args) {
    return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}
static Value timeNative(int argCount, Value *args) {}

/*
 * ----------------------------------------- IO LIBRARY -----------------------------------------
 */

static Value inputNative(int argCount, Value *args) {}
static Value readFileNative(int argCount, Value *args) {}
static Value writeFileNative(int argCount, Value *args) {}
static Value exitNative(int argCount, Value *args) {}

void defineAllNatives() {
    defineNative("clock", clockNative, 0);
    defineNative("str", strNative, 1);
    defineNative("len", lengthNative, 1);
}