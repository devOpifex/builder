#ifndef S7CHECK_H
#define S7CHECK_H

#include <Rinternals.h>
#include "file.h"

typedef struct S7Prop {
    char *name;
    char *type_ctor;
    struct S7Prop *next;
} S7Prop;

typedef struct S7Class {
    char *var_name;
    char *parent_name;
    S7Prop *props;
    struct S7Class *next;
} S7Class;

typedef struct S7Instance {
    char *var_name;
    struct S7Class *cls;
    struct S7Instance *next;
} S7Instance;

int analyse_s7(RFile *files);

#endif
