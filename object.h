#ifndef CFER_OBJECT_H
#define CFER_OBJECT_H

#include "common.h"
#include "value.h"

typedef enum {
    OBJ_STRING,
} ObjType;

struct Obj {
    ObjType type;
};

#endif //CFER_OBJECT_H