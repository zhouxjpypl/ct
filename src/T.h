#ifndef CT_H
#define CT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <setjmp.h>

#define CT_VERSION "1.0.0"
#define MAX_VARS 10000
#define MAX_STR_LEN 4096
#define MAX_ARRAY_SIZE 10000
#define MAX_FUNC_PARAMS 32
#define MAX_CALL_STACK 256

typedef enum {
    TYPE_NUMBER,
    TYPE_STRING,
    TYPE_ARRAY,
    TYPE_FUNCTION,
    TYPE_NULL
} ValueType;

typedef struct Value {
    ValueType type;
    union {
        double num;
        char *str;
        struct {
            struct Value *items;
            int length;
            int capacity;
        } arr;
        struct {
            char **param_names;
            int param_count;
            char *body;
            struct Env *closure;
        } func;
    };
} Value;

typedef struct Var {
    char name[256];
    Value value;
    struct Var *next;
} Var;

typedef struct Env {
    Var *vars;
    struct Env *parent;
    jmp_buf *breaker;
} Env;

// env.c
Env* env_create(Env *parent);
void env_destroy(Env *env);
Value* env_get(Env *env, const char *name);
void env_set(Env *env, const char *name, Value value);

// value.c
Value val_number(double n);
Value val_string(const char *s);
Value val_array(int capacity);
Value val_null(void);
void val_free(Value *v);
void val_print(Value *v);

// eval.c
Value eval_expr(const char **p, Env *env);
void eval_statement(const char **p, Env *env);
void eval_program(const char *code, Env *env);

// main.c
void repl(void);
int run_file(const char *filename);

#endif