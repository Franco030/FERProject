#include <stdio.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "vm.h"
#include "natives.h"
#include "memory.h"
#include "common.h"

static void ensureListCapacity(ObjList *list, int capacityNeeded) {
    if (list->capacity < capacityNeeded) {
        int oldCapacity = list->capacity;
        list->capacity = GROW_CAPACITY(oldCapacity);
        list->values = GROW_ARRAY(Value, list->values, oldCapacity, list->capacity);
    }
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

static Value subStrNative(int argCount, Value *args) {
    if (argCount < 2 || argCount > 3) return NIL_VAL;
    if (!IS_STRING(args[0]) || !IS_NUMBER(args[1])) return NIL_VAL;

    ObjString *str = AS_STRING(args[0]);
    int start = (int)AS_NUMBER(args[1]);
    int length = str->length - start;

    if (argCount == 3) {
        if (!IS_NUMBER(args[2])) return NIL_VAL;
        int lenArg = (int)AS_NUMBER(args[2]);
        if (lenArg < length) length = lenArg;
    }

    if (start < 0 || start >= str->length || length <= 0) {
        return OBJ_VAL(copyString("", 0));
    }

    return OBJ_VAL(copyString(str->chars + start, length));
}

static Value toUpperNative(int argCount, Value *args) {
    if (argCount != 1 || !IS_STRING(args[0])) return NIL_VAL;

    ObjString *str = AS_STRING(args[0]);
    char *upper = malloc(str->length + 1);

    for (int i = 0; i < str->length; i++) {
        upper[i] = toupper(str->chars[i]);
    }
    upper[str->length] = '\0';

    ObjString *result = takeString(upper, str->length);
    return OBJ_VAL(result);
}

static Value toLowerNative(int argCount, Value *args) {
    if (argCount != 1 || !IS_STRING(args[0])) return NIL_VAL;

    ObjString *str = AS_STRING(args[0]);
    char *lower = malloc(str->length + 1);

    for (int i = 0; i < str->length; i++) {
        lower[i] = tolower(str->chars[i]);
    }
    lower[str->length] = '\0';

    ObjString *result = takeString(lower, str->length);
    return OBJ_VAL(result);
}

static Value indexOfNative(int argCount, Value *args) {
    if (argCount != 2 || !IS_STRING(args[0]) || !IS_STRING(args[1])) return NIL_VAL;

    ObjString *haystack = AS_STRING(args[0]);
    ObjString *needle = AS_STRING(args[1]);

    char *found = strstr(haystack->chars, needle->chars);
    if (found != NULL) {
        return NUMBER_VAL((int)(found - haystack->chars));
    }
    return NUMBER_VAL(-1);
}

static Value splitStrNative(int argCount, Value *args) {
    if (argCount != 2 || !IS_STRING(args[0]) || !IS_STRING(args[1])) return NIL_VAL;

    ObjString *str = AS_STRING(args[0]);
    ObjString *delimiter = AS_STRING(args[1]);

    ObjList *list = newList();
    push(OBJ_VAL(list));

    char *strCopy = strdup(str->chars);
    char *token = strtok(strCopy, delimiter->chars);

    while (token != NULL) {
        Value val = OBJ_VAL(copyString(token, (int)strlen(token)));
        push(val);
        ensureListCapacity(list, list->count + 1);
        list->values[list->count++] = val;
        pop();

        token = strtok(NULL, delimiter->chars);
    }

    free(strCopy);
    pop();
    return OBJ_VAL(list);
}

static Value trimStrNative(int argCount, Value *args) {
    if (argCount != 1 || !IS_STRING(args[0])) return NIL_VAL;

    ObjString *str = AS_STRING(args[0]);
    if (str->length == 0) return args[0];

    int start = 0;
    while (start < str->length && isspace(str->chars[start])) {
        start++;
    }

    int end = str->length - 1;
    while (end > start && isspace(str->chars[end])) {
        end--;
    }

    int newLen = end - start + 1;
    if (newLen <= 0) return OBJ_VAL(copyString("", 0));

    return OBJ_VAL(copyString(str->chars + start, newLen));
}

static Value chrNative(int argCount, Value *args) {
    if (argCount != 1 || !IS_NUMBER(args[0])) return NIL_VAL;

    char c = (char)AS_NUMBER(args[0]);
    return OBJ_VAL(copyString(&c, 1));
}

static Value ordNative(int argCount, Value *args) {
    if (argCount != 1 || !IS_STRING(args[0])) return NIL_VAL;

    ObjString *str = AS_STRING(args[0]);
    if (str->length == 0) return NIL_VAL;

    return NUMBER_VAL((unsigned char)str->chars[0]);
}


/*
 * ----------------------------------------- MATH LIBRARY -----------------------------------------
 */

static Value sqrtNative(int argCount, Value *args) {
    if (argCount != 1 || !IS_NUMBER(args[0])) return NIL_VAL;
    return NUMBER_VAL(sqrt(AS_NUMBER(args[0])));
}

static Value powNative(int argCount, Value *args) {
    if (argCount != 2 || !IS_NUMBER(args[0]) || !IS_NUMBER(args[1])) return NIL_VAL;
    return NUMBER_VAL(pow(AS_NUMBER(args[0]), AS_NUMBER(args[1])));
}

static Value floorNative(int argCount, Value *args) {
    if (argCount != 1 || !IS_NUMBER(args[0])) return NIL_VAL;
    return NUMBER_VAL(floor(AS_NUMBER(args[0])));
}

static Value ceilNative(int argCount, Value *args) {
    if (argCount != 1 || !IS_NUMBER(args[0])) return NIL_VAL;
    return NUMBER_VAL(ceil(AS_NUMBER(args[0])));
}

static Value randomNative(int argCount, Value *args) {
    return NUMBER_VAL((double)rand() / (double)RAND_MAX);
}

static Value seedNative(int argCount, Value *args) {
    if (argCount != 1 || !IS_NUMBER(args[0])) return NIL_VAL;
    srand((unsigned int)AS_NUMBER(args[0]));
    return NIL_VAL;
}

static Value sinNative(int argCount, Value *args) {
    if (argCount != 1 || !IS_NUMBER(args[0])) return NIL_VAL;
    return NUMBER_VAL(sin(AS_NUMBER(args[0])));
}

static Value cosNative(int argCount, Value *args) {
    if (argCount != 1 || !IS_NUMBER(args[0])) return NIL_VAL;
    return NUMBER_VAL(cos(AS_NUMBER(args[0])));
}

static Value tanNative(int argCount, Value *args) {
    if (argCount != 1 || !IS_NUMBER(args[0])) return NIL_VAL;
    return NUMBER_VAL(tan(AS_NUMBER(args[0])));
}

/*
 * ----------------------------------------- COLLECTIONS LIBRARY -----------------------------------------
 */

static Value lengthNative(int argCount, Value *args) {
    if (argCount != 1) return NIL_VAL;

    if (IS_LIST(args[0])) {
        ObjList* list = AS_LIST(args[0]);
        return NUMBER_VAL(list->count);
    }
    else if (IS_STRING(args[0])) {
        ObjString* str = AS_STRING(args[0]);
        return NUMBER_VAL(str->length);
    }
    else if (IS_DICTIONARY(args[0])) {
        ObjDictionary* dict = AS_DICTIONARY(args[0]);
        return NUMBER_VAL(dict->table.count);
    }

    return NIL_VAL;
}

static Value pushLsNative(int argCount, Value *args) {
    if (argCount != 2 || !IS_LIST(args[0])) return NIL_VAL;

    ObjList *list = AS_LIST(args[0]);
    Value item = args[1];

    ensureListCapacity(list, list->count + 1);
    list->values[list->count++] = item;

    return item;
}

static Value popLsNative(int argCount, Value *args) {
    if (argCount != 1 || !IS_LIST(args[0])) return NIL_VAL;

    ObjList *list = AS_LIST(args[0]);
    if (list->count == 0) return NIL_VAL;

    return list->values[--list->count];
}

static Value insertLsNative(int argCount, Value *args) {
    if (argCount != 3 || !IS_LIST(args[0]) || !IS_NUMBER(args[1])) return NIL_VAL;

    ObjList *list = AS_LIST(args[0]);
    int index = (int)AS_NUMBER(args[1]);
    Value item = args[2];

    if (index < 0 || index > list->count) return NIL_VAL;
    ensureListCapacity(list, list->count + 1);

    for (int i = list->count; i > index; i--) {
        list->values[i] = list->values[i - 1];
    }

    list->values[index] = item;
    list->count++;

    return item;
}

static Value removeLsNative(int argCount, Value *args) {
    if (argCount != 2 || !IS_LIST(args[0]) || !IS_NUMBER(args[1])) return NIL_VAL;

    ObjList *list = AS_LIST(args[0]);
    int index = (int)AS_NUMBER(args[1]);

    if (index < 0 || index >= list->count) return NIL_VAL;

    Value removed = list->values[index];

    for (int i = index; i < list->count - 1; i++) {
        list->values[i] = list->values[i + 1];
    }

    list->count--;
    return removed;
}

static Value containsLsNative(int argCount, Value *args) {
    if (argCount != 2 || !IS_LIST(args[0])) return NIL_VAL;

    ObjList *list = AS_LIST(args[0]);
    Value target = args[1];

    for (int i = 0; i < list->count; i++) {
        if (valuesEqual(list->values[i], target)) {
            return BOOL_VAL(true);
        }
    }

    return BOOL_VAL(false);
}

static Value keysDctNative(int argCount, Value *args) {
    if (argCount != 1 || !IS_DICTIONARY(args[0])) return NIL_VAL;

    ObjDictionary *dict = AS_DICTIONARY(args[0]);
    ObjList *list = newList();
    push(OBJ_VAL(list));

    for (int i = 0; i < dict->table.capacity; i++) {
        Entry *entry = &dict->table.entries[i];
        if (entry->key != NULL) {
            Value keyVal = OBJ_VAL(entry->key);
            push(keyVal);
            ensureListCapacity(list, list->count + 1);
            list->values[list->count++] = keyVal;
            pop();
        }
    }

    pop();
    return OBJ_VAL(list);
}

static Value hasKeyDctNative(int argCount, Value *args) {
    if (argCount != 2 || !IS_DICTIONARY(args[0]) || !IS_STRING(args[1])) return NIL_VAL;

    ObjDictionary *dict = AS_DICTIONARY(args[0]);
    ObjString *key = AS_STRING(args[1]);
    Value dummy;

    return BOOL_VAL(tableGet(&dict->table, key, &dummy));
}

static Value deleteKeyDctNative(int argCount, Value *args) {
    if (argCount != 2 || !IS_DICTIONARY(args[0]) || !IS_STRING(args[1])) return NIL_VAL;

    ObjDictionary *dict = AS_DICTIONARY(args[0]);
    ObjString *key = AS_STRING(args[1]);

    return BOOL_VAL(tableDelete(&dict->table, key));
}

/*
 * ----------------------------------------- TYPES LIBRARY -----------------------------------------
 */

static Value typeofNative(int argCount, Value *args) {
    if (argCount != 1) return NIL_VAL;

    Value v = args[0];
    const char *typeStr = "unknown";

    if (IS_NIL(v)) typeStr = "nil";
    else if (IS_BOOL(v)) typeStr = "bool";
    else if (IS_NUMBER(v)) typeStr = "number";
    else if (IS_STRING(v)) typeStr = "string";
    else if (IS_LIST(v)) typeStr = "list";
    else if (IS_DICTIONARY(v)) typeStr = "dictionary";
    else if (IS_FUNCTION(v) || IS_CLOSURE(v) || IS_NATIVE(v) || IS_BOUND_METHOD(v)) typeStr = "function";
    else if (IS_CLASS(v)) typeStr = "class";
    else if (IS_INSTANCE(v)) typeStr = "instance";

    return OBJ_VAL(copyString(typeStr, (int)strlen(typeStr)));
}

static Value assertNative(int argCount, Value *args) {
    if (argCount < 1) return NIL_VAL;

    if (isFalsey(args[0])) {
        const char *msg = "Assertion failed.";
        if (argCount > 1 && IS_STRING(args[1])) {
            msg = AS_CSTRING(args[1]);
        }
        fprintf(stderr, "%s\n", msg);
        exit(1);
    }
    return BOOL_VAL(true);
}

/*
 * ----------------------------------------- TIME LIBRARY -----------------------------------------
 */

static Value clockNative(int argCount, Value *args) {
    return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}
static Value timeNative(int argCount, Value *args) {
    return NUMBER_VAL((double)time(NULL));
}

/*
 * ----------------------------------------- IO LIBRARY -----------------------------------------
 */

static Value inputNative(int argCount, Value *args) {
    if (argCount > 0) {
        if (IS_STRING(args[0])) {
            printf("%s", AS_CSTRING(args[0]));
        } else {
            printValue(args[0]);
        }
    }

    char buffer[1024];
    if (fgets(buffer, sizeof(buffer), stdin) != NULL) {
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len - 1] == '\n') {
            buffer[len - 1] = '\0';
            len--;
        }
        return OBJ_VAL(copyString(buffer, (int)len));
    }

    return NIL_VAL;
}

static Value readFileNative(int argCount, Value *args) {
    if (argCount != 1 || !IS_STRING(args[0])) return NIL_VAL;

    ObjString *path = AS_STRING(args[0]);
    char *content = readFile(path->chars);

    if (content == NULL) return NIL_VAL;

    Value val = OBJ_VAL(copyString(content, (int)strlen(content)));
    free(content);
    return val;
}

static Value writeFileNative(int argCount, Value *args) {
    if (argCount != 2 || !IS_STRING(args[0]) || !IS_STRING(args[1])) return NIL_VAL;

    ObjString *path = AS_STRING(args[0]);
    ObjString *content = AS_STRING(args[1]);

    FILE *file = fopen(path->chars, "w");
    if (file == NULL) return BOOL_VAL(false);

    fprintf(file, "%s", content->chars);
    fclose(file);
    return BOOL_VAL(true);
}

/*
 * ----------------------------------------- UTILS LIBRARY -----------------------------------------
 */

static Value exitNative(int argCount, Value *args) {
    int status = 0;
    if (argCount == 1 && IS_NUMBER(args[0])) {
        status = (int)AS_NUMBER(args[0]);
    }
    exit(status);
    return NIL_VAL;
}

void defineAllNatives() {
    // Strings
    defineNative("str", strNative, 1);
    defineNative("len", lengthNative, 1);
    defineNative("sub", subStrNative, 2);
    defineNative("upper", toUpperNative, 1);
    defineNative("lower", toLowerNative, 1);
    defineNative("index", indexOfNative, 2);
    defineNative("split", splitStrNative, 2);
    defineNative("trim", trimStrNative, 1);
    defineNative("chr", chrNative, 1);
    defineNative("ord", ordNative, 1);

    // Collections
    defineNative("push", pushLsNative, 2);
    defineNative("pop", popLsNative, 1);
    defineNative("insert", insertLsNative, 3);
    defineNative("remove", removeLsNative, 2);
    defineNative("contains", containsLsNative, 2);
    defineNative("keys", keysDctNative, 1);
    defineNative("hasKey", hasKeyDctNative, 2);
    defineNative("delete", deleteKeyDctNative, 2);

    // Types
    defineNative("typeof", typeofNative, 1);
    defineNative("assert", assertNative, 1);
}

void defineTimeNatives() {
    defineNative("clock", clockNative, 0);
    defineNative("now", timeNative, 0);
}

void defineMathNatives() {
    defineNative("sqrt", sqrtNative, 1);
    defineNative("pow", powNative, 2);
    defineNative("floor", floorNative, 1);
    defineNative("ceil", ceilNative, 1);
    defineNative("rand", randomNative, 0);
    defineNative("seed", seedNative, 1);
    defineNative("sin", sinNative, 1);
    defineNative("cos", cosNative, 1);
    defineNative("tan", tanNative, 1);
}

void defineIONatives() {
    defineNative("input", inputNative, 1);
    defineNative("read", readFileNative, 1);
    defineNative("write", writeFileNative, 2);
    defineNative("exit", exitNative, 1);
}