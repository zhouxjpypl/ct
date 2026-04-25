#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <time.h>

#define VERSION "1.0.0"
#define MAX_VARS 10000
#define MAX_STR_LEN 4096
#define MAX_ARRAY_SIZE 10000
#define MAX_FUNC_PARAMS 32
#define MAX_CALL_STACK 256

/* 类型定义 */
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

/* 变量 */
typedef struct Var {
    char name[256];
    Value value;
    struct Var *next;
} Var;

/* 执行环境 */
typedef struct Env {
    Var *vars;
    struct Env *parent;
} Env;

/* 全局状态 */
static Env *global_env;
static int call_depth = 0;

/* 函数声明 */
Value eval_expr(const char **p, Env *env);
void eval_statement(const char **p, Env *env);

/* 错误处理 */
void error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
    exit(1);
}

/* 内存管理 */
void* safe_malloc(size_t size) {
    void *ptr = malloc(size);
    if (!ptr) error("Out of memory");
    return ptr;
}

char* safe_strdup(const char *s) {
    char *dup = strdup(s);
    if (!dup) error("Out of memory");
    return dup;
}

/* 值操作 */
Value make_number(double n) {
    Value v;
    v.type = TYPE_NUMBER;
    v.num = n;
    return v;
}

Value make_string(const char *s) {
    Value v;
    v.type = TYPE_STRING;
    v.str = safe_strdup(s);
    return v;
}

Value make_array(int capacity) {
    Value v;
    v.type = TYPE_ARRAY;
    v.arr.items = safe_malloc(sizeof(Value) * capacity);
    v.arr.length = 0;
    v.arr.capacity = capacity;
    return v;
}

Value make_null(void) {
    Value v;
    v.type = TYPE_NULL;
    return v;
}

void free_value(Value *v) {
    if (v->type == TYPE_STRING && v->str) {
        free(v->str);
    } else if (v->type == TYPE_ARRAY && v->arr.items) {
        for (int i = 0; i < v->arr.length; i++) {
            free_value(&v->arr.items[i]);
        }
        free(v->arr.items);
    } else if (v->type == TYPE_FUNCTION) {
        if (v->func.param_names) {
            for (int i = 0; i < v->func.param_count; i++) {
                free(v->func.param_names[i]);
            }
            free(v->func.param_names);
        }
        if (v->func.body) free(v->func.body);
        if (v->func.closure) free(v->func.closure);
    }
}

void print_value(Value *v) {
    switch (v->type) {
        case TYPE_NUMBER:
            if (v->num == (int)v->num)
                printf("%d", (int)v->num);
            else
                printf("%g", v->num);
            break;
        case TYPE_STRING:
            printf("%s", v->str);
            break;
        case TYPE_ARRAY:
            printf("[");
            for (int i = 0; i < v->arr.length; i++) {
                if (i > 0) printf(", ");
                print_value(&v->arr.items[i]);
            }
            printf("]");
            break;
        case TYPE_FUNCTION:
            printf("<function>");
            break;
        case TYPE_NULL:
            printf("null");
            break;
    }
}

/* 环境操作 */
Env* create_env(Env *parent) {
    Env *env = safe_malloc(sizeof(Env));
    env->vars = NULL;
    env->parent = parent;
    return env;
}

void destroy_env(Env *env) {
    Var *var = env->vars;
    while (var) {
        Var *next = var->next;
        free_value(&var->value);
        free(var);
        var = next;
    }
    free(env);
}

Value* env_get(Env *env, const char *name) {
    while (env) {
        Var *var = env->vars;
        while (var) {
            if (strcmp(var->name, name) == 0) {
                return &var->value;
            }
            var = var->next;
        }
        env = env->parent;
    }
    return NULL;
}

void env_set(Env *env, const char *name, Value value) {
    Var *var = env->vars;
    while (var) {
        if (strcmp(var->name, name) == 0) {
            free_value(&var->value);
            var->value = value;
            return;
        }
        var = var->next;
    }

    var = safe_malloc(sizeof(Var));
    strcpy(var->name, name);
    var->value = value;
    var->next = env->vars;
    env->vars = var;
}

/* 表达式解析 */
static double parse_number(const char **p) {
    char buf[64];
    int i = 0;
    while (isdigit(**p) || **p == '.') {
        buf[i++] = **p;
        (*p)++;
    }
    buf[i] = '\0';
    return atof(buf);
}

static char* parse_string(const char **p) {
    if (**p != '"') return NULL;
    (*p)++;

    char *str = safe_malloc(MAX_STR_LEN);
    int i = 0;
    while (**p && **p != '"') {
        if (**p == '\\') {
            (*p)++;
            switch (**p) {
                case 'n': str[i++] = '\n'; break;
                case 't': str[i++] = '\t'; break;
                case '\\': str[i++] = '\\'; break;
                case '"': str[i++] = '"'; break;
                default: str[i++] = **p; break;
            }
        } else {
            str[i++] = **p;
        }
        (*p)++;
    }
    str[i] = '\0';
    if (**p == '"') (*p)++;
    return str;
}

static char* parse_ident(const char **p) {
    char *name = safe_malloc(256);
    int i = 0;
    while (isalnum(**p) || **p == '_') {
        name[i++] = **p;
        (*p)++;
    }
    name[i] = '\0';
    return name;
}

static Value eval_primary(const char **p, Env *env) {
    while (isspace(**p)) (*p)++;

    if (isdigit(**p) || **p == '.') {
        return make_number(parse_number(p));
    }
    else if (**p == '"') {
        char *str = parse_string(p);
        Value v = make_string(str);
        free(str);
        return v;
    }
    else if (**p == '(') {
        (*p)++;
        Value v = eval_expr(p, env);
        if (**p == ')') (*p)++;
        return v;
    }
    else if (**p == '[') {
        (*p)++;
        Value arr = make_array(MAX_ARRAY_SIZE);
        while (**p && **p != ']') {
            Value elem = eval_expr(p, env);
            if (arr.arr.length >= arr.arr.capacity) {
                arr.arr.capacity *= 2;
                arr.arr.items = realloc(arr.arr.items, sizeof(Value) * arr.arr.capacity);
            }
            arr.arr.items[arr.arr.length++] = elem;
            if (**p == ',') (*p)++;
            while (isspace(**p)) (*p)++;
        }
        if (**p == ']') (*p)++;
        return arr;
    }
    else if (isalpha(**p) || **p == '_') {
        char *name = parse_ident(p);

        if (**p == '(') {
            (*p)++;
            Value func_val;
            func_val.type = TYPE_FUNCTION;
            func_val.func.param_names = NULL;
            func_val.func.param_count = 0;
            func_val.func.body = NULL;
            func_val.func.closure = NULL;

            Value *found = env_get(env, name);
            if (found && found->type == TYPE_FUNCTION) {
                Env *call_env = create_env(global_env);

                for (int i = 0; i < found->func.param_count; i++) {
                    Value arg = eval_expr(p, env);
                    env_set(call_env, found->func.param_names[i], arg);
                    if (**p == ',') (*p)++;
                    while (isspace(**p)) (*p)++;
                }

                if (**p == ')') (*p)++;

                call_depth++;
                if (call_depth > MAX_CALL_STACK) {
                    error("Stack overflow");
                }

                const char *body_ptr = found->func.body;
                while (*body_ptr) {
                    eval_statement(&body_ptr, call_env);
                }

                call_depth--;
                destroy_env(call_env);
            }

            free(name);
            return make_null();
        }
        else if (**p == '[') {
            (*p)++;
            Value index = eval_expr(p, env);
            if (**p == ']') (*p)++;

            Value *arr = env_get(env, name);
            if (!arr || arr->type != TYPE_ARRAY) {
                error("Not an array: %s", name);
            }

            int idx = (int)index.num;
            if (idx < 0 || idx >= arr->arr.length) {
                error("Array index out of bounds");
            }

            if (**p == '=') {
                (*p)++;
                Value val = eval_expr(p, env);
                free_value(&arr->arr.items[idx]);
                arr->arr.items[idx] = val;
                free(name);
                return make_null();
            } else {
                free(name);
                return arr->arr.items[idx];
            }
        }
        else {
            Value *v = env_get(env, name);
            free(name);
            if (v) return *v;
            return make_null();
        }
    }

    return make_null();
}

static Value eval_power(const char **p, Env *env) {
    Value left = eval_primary(p, env);

    while (isspace(**p)) (*p)++;
    if (**p == '^') {
        (*p)++;
        Value right = eval_power(p, env);
        if (left.type == TYPE_NUMBER && right.type == TYPE_NUMBER) {
            return make_number(pow(left.num, right.num));
        }
    }
    return left;
}

static Value eval_unary(const char **p, Env *env) {
    while (isspace(**p)) (*p)++;

    if (**p == '-' || **p == '+') {
        char op = **p;
        (*p)++;
        Value v = eval_unary(p, env);
        if (v.type == TYPE_NUMBER) {
            return make_number(op == '-' ? -v.num : v.num);
        }
    }
    else if (**p == '!') {
        (*p)++;
        Value v = eval_unary(p, env);
        return make_number(v.type == TYPE_NULL ||
                           (v.type == TYPE_NUMBER && v.num == 0) ? 1 : 0);
    }

    return eval_power(p, env);
}

static Value eval_mul_div(const char **p, Env *env) {
    Value left = eval_unary(p, env);

    while (isspace(**p)) (*p)++;
    while (**p == '*' || **p == '/') {
        char op = **p;
        (*p)++;
        Value right = eval_unary(p, env);

        if (left.type != TYPE_NUMBER || right.type != TYPE_NUMBER) {
            error("Arithmetic on non-numbers");
        }

        if (op == '*') {
            left.num = left.num * right.num;
        } else {
            if (right.num == 0) error("Division by zero");
            left.num = left.num / right.num;
        }

        while (isspace(**p)) (*p)++;
    }
    return left;
}

static Value eval_add_sub(const char **p, Env *env) {
    Value left = eval_mul_div(p, env);

    while (isspace(**p)) (*p)++;
    while (**p == '+' || **p == '-') {
        char op = **p;
        (*p)++;
        Value right = eval_mul_div(p, env);

        if (left.type == TYPE_STRING || right.type == TYPE_STRING) {
            if (op == '+') {
                char *str1 = left.type == TYPE_STRING ? left.str : "";
                char *str2 = right.type == TYPE_STRING ? right.str : "";
                char *result = safe_malloc(strlen(str1) + strlen(str2) + 1);
                sprintf(result, "%s%s", str1, str2);
                left = make_string(result);
                free(result);
            } else {
                error("Cannot subtract from string");
            }
        } else if (left.type == TYPE_NUMBER && right.type == TYPE_NUMBER) {
            if (op == '+') left.num = left.num + right.num;
            else left.num = left.num - right.num;
        } else {
            error("Invalid operands for + or -");
        }

        while (isspace(**p)) (*p)++;
    }
    return left;
}

static Value eval_comparison(const char **p, Env *env) {
    Value left = eval_add_sub(p, env);

    while (isspace(**p)) (*p)++;
    if (**p == '=' && (*p)[1] == '=') {
        *p += 2;
        Value right = eval_comparison(p, env);
        int equal = 0;
        if (left.type == TYPE_NUMBER && right.type == TYPE_NUMBER) {
            equal = (left.num == right.num);
        } else if (left.type == TYPE_STRING && right.type == TYPE_STRING) {
            equal = (strcmp(left.str, right.str) == 0);
        }
        return make_number(equal ? 1 : 0);
    }
    else if (**p == '!' && (*p)[1] == '=') {
        *p += 2;
        Value right = eval_comparison(p, env);
        int not_equal = 1;
        if (left.type == TYPE_NUMBER && right.type == TYPE_NUMBER) {
            not_equal = (left.num != right.num);
        }
        return make_number(not_equal ? 1 : 0);
    }
    else if (**p == '<') {
        (*p)++;
        int less = 0;
        if ((*p)[0] == '=') {
            (*p)++;
            Value right = eval_comparison(p, env);
            if (left.type == TYPE_NUMBER && right.type == TYPE_NUMBER)
                less = (left.num <= right.num);
        } else {
            Value right = eval_comparison(p, env);
            if (left.type == TYPE_NUMBER && right.type == TYPE_NUMBER)
                less = (left.num < right.num);
        }
        return make_number(less ? 1 : 0);
    }
    else if (**p == '>') {
        (*p)++;
        int greater = 0;
        if ((*p)[0] == '=') {
            (*p)++;
            Value right = eval_comparison(p, env);
            if (left.type == TYPE_NUMBER && right.type == TYPE_NUMBER)
                greater = (left.num >= right.num);
        } else {
            Value right = eval_comparison(p, env);
            if (left.type == TYPE_NUMBER && right.type == TYPE_NUMBER)
                greater = (left.num > right.num);
        }
        return make_number(greater ? 1 : 0);
    }

    return left;
}

static Value eval_and(const char **p, Env *env) {
    Value left = eval_comparison(p, env);

    while (isspace(**p)) (*p)++;
    if (strncmp(*p, "and", 3) == 0 && !isalnum((*p)[3])) {
        *p += 3;
        Value right = eval_and(p, env);
        int left_true = (left.type == TYPE_NUMBER && left.num != 0) ||
                        (left.type == TYPE_STRING && left.str && left.str[0]);
        int right_true = (right.type == TYPE_NUMBER && right.num != 0) ||
                         (right.type == TYPE_STRING && right.str && right.str[0]);
        return make_number((left_true && right_true) ? 1 : 0);
    }
    return left;
}

static Value eval_or(const char **p, Env *env) {
    Value left = eval_and(p, env);

    while (isspace(**p)) (*p)++;
    if (strncmp(*p, "or", 2) == 0 && !isalnum((*p)[2])) {
        *p += 2;
        Value right = eval_or(p, env);
        int left_true = (left.type == TYPE_NUMBER && left.num != 0) ||
                        (left.type == TYPE_STRING && left.str && left.str[0]);
        int right_true = (right.type == TYPE_NUMBER && right.num != 0) ||
                         (right.type == TYPE_STRING && right.str && right.str[0]);
        return make_number((left_true || right_true) ? 1 : 0);
    }
    return left;
}

Value eval_expr(const char **p, Env *env) {
    return eval_or(p, env);
}

/* 语句解析 */
void eval_statement(const char **p, Env *env) {
    while (isspace(**p)) (*p)++;
    if (!**p) return;

    if (strncmp(*p, "print", 5) == 0 && !isalnum((*p)[5])) {
        *p += 5;
        while (isspace(**p)) (*p)++;

        Value v = eval_expr(p, env);
        print_value(&v);
        printf("\n");
        free_value(&v);
    }
    else if (strncmp(*p, "if", 2) == 0 && !isalnum((*p)[2])) {
        *p += 2;
        Value cond = eval_expr(p, env);
        int true_branch = (cond.type == TYPE_NUMBER && cond.num != 0);
        free_value(&cond);

        while (isspace(**p)) (*p)++;

        if (true_branch) {
            if (**p == '{') {
                (*p)++;
                while (**p && **p != '}') {
                    eval_statement(p, env);
                    while (isspace(**p)) (*p)++;
                }
                if (**p == '}') (*p)++;
            } else {
                eval_statement(p, env);
            }
        } else {
            int brace = 0;
            if (**p == '{') {
                brace = 1;
                (*p)++;
                while (brace > 0 && **p) {
                    if (**p == '{') brace++;
                    if (**p == '}') brace--;
                    (*p)++;
                }
            } else {
                while (**p && **p != ';') (*p)++;
            }
        }

        while (isspace(**p)) (*p)++;
        if (strncmp(*p, "else", 4) == 0 && !isalnum((*p)[4])) {
            *p += 4;
            while (isspace(**p)) (*p)++;

            if (!true_branch) {
                if (**p == '{') {
                    (*p)++;
                    while (**p && **p != '}') {
                        eval_statement(p, env);
                        while (isspace(**p)) (*p)++;
                    }
                    if (**p == '}') (*p)++;
                } else {
                    eval_statement(p, env);
                }
            } else {
                if (**p == '{') {
                    int brace = 1;
                    (*p)++;
                    while (brace > 0 && **p) {
                        if (**p == '{') brace++;
                        if (**p == '}') brace--;
                        (*p)++;
                    }
                } else {
                    while (**p && **p != ';') (*p)++;
                }
            }
        }
    }
    else if (strncmp(*p, "while", 5) == 0 && !isalnum((*p)[5])) {
        *p += 5;
        const char *cond_start = *p;

        while (1) {
            const char *save = cond_start;
            Value cond = eval_expr(&save, env);
            if (cond.type != TYPE_NUMBER || cond.num == 0) {
                free_value(&cond);
                break;
            }
            free_value(&cond);

            const char *body_ptr = save;
            while (isspace(*body_ptr)) body_ptr++;

            if (*body_ptr == '{') {
                body_ptr++;
                Env *loop_env = create_env(env);
                while (*body_ptr && *body_ptr != '}') {
                    eval_statement(&body_ptr, loop_env);
                    while (isspace(*body_ptr)) body_ptr++;
                }
                destroy_env(loop_env);
            } else {
                Env *loop_env = create_env(env);
                eval_statement(&body_ptr, loop_env);
                destroy_env(loop_env);
            }
        }

        int brace = 0;
        while (**p) {
            if (**p == '{') brace++;
            if (**p == '}') {
                if (brace == 0) break;
                brace--;
            }
            (*p)++;
        }
        if (**p == '}') (*p)++;
    }
    else if (strncmp(*p, "func", 4) == 0 && !isalnum((*p)[4])) {
        *p += 4;
        while (isspace(**p)) (*p)++;

        char *name = parse_ident(p);
        while (isspace(**p)) (*p)++;

        Value func_val;
        func_val.type = TYPE_FUNCTION;
        func_val.func.param_names = NULL;
        func_val.func.param_count = 0;
        func_val.func.body = NULL;
        func_val.func.closure = create_env(global_env);

        if (**p == '(') {
            (*p)++;
            while (**p && **p != ')') {
                char *param = parse_ident(p);
                func_val.func.param_names = realloc(func_val.func.param_names,
                                                    sizeof(char*) * (func_val.func.param_count + 1));
                func_val.func.param_names[func_val.func.param_count++] = param;
                if (**p == ',') (*p)++;
                while (isspace(**p)) (*p)++;
            }
            if (**p == ')') (*p)++;
        }

        while (isspace(**p)) (*p)++;
        if (**p == '{') {
            const char *start = *p;
            int brace = 1;
            (*p)++;
            while (brace > 0 && **p) {
                if (**p == '{') brace++;
                if (**p == '}') brace--;
                (*p)++;
            }
            int len = *p - start;
            func_val.func.body = safe_malloc(len + 1);
            strncpy(func_val.func.body, start, len);
            func_val.func.body[len] = '\0';
        }

        env_set(env, name, func_val);
        free(name);
    }
    else if (strncmp(*p, "return", 6) == 0 && !isalnum((*p)[6])) {
        *p += 6;
        Value v = eval_expr(p, env);
        longjmp(*(jmp_buf*)env->parent, 1);
        free_value(&v);
    }
    else if (strncmp(*p, "break", 5) == 0 && !isalnum((*p)[5])) {
        *p += 5;
        longjmp(*(jmp_buf*)env->parent, 2);
    }
    else if (strncmp(*p, "continue", 8) == 0 && !isalnum((*p)[8])) {
        *p += 8;
        longjmp(*(jmp_buf*)env->parent, 3);
    }
    else if (isalpha(**p) || **p == '_') {
        char *name = parse_ident(p);

        if (**p == '=') {
            (*p)++;
            Value v = eval_expr(p, env);
            env_set(env, name, v);
        } else if (**p == '[') {
            (*p)++;
            Value index = eval_expr(p, env);
            if (**p == ']') (*p)++;
            if (**p == '=') {
                (*p)++;
                Value val = eval_expr(p, env);

                Value *arr = env_get(env, name);
                if (!arr || arr->type != TYPE_ARRAY) {
                    error("Not an array: %s", name);
                }

                int idx = (int)index.num;
                if (idx < 0 || idx >= arr->arr.capacity) {
                    error("Array index out of bounds");
                }

                while (idx >= arr->arr.length) {
                    arr->arr.items[arr->arr.length++] = make_null();
                }

                free_value(&arr->arr.items[idx]);
                arr->arr.items[idx] = val;
            }
        } else {
            Value v = eval_expr(p, env);
            print_value(&v);
            printf("\n");
            free_value(&v);
        }

        free(name);
    }

    if (**p == ';') (*p)++;
}

void eval_program(const char *code, Env *env) {
    const char *p = code;

    while (*p) {
        while (isspace(*p)) p++;
        if (*p == '#') {
            while (*p && *p != '\n') p++;
            continue;
        }
        if (*p) {
            eval_statement(&p, env);
        }
    }
}

/* 交互式环境 */
void repl(void) {
    char line[MAX_STR_LEN];
    Env *env = create_env(NULL);
    global_env = env;

    printf("CT Language v%s\n", VERSION);
    printf("Type 'exit' to quit\n\n");

    while (1) {
        printf("> ");
        if (!fgets(line, sizeof(line), stdin)) break;

        line[strcspn(line, "\n")] = 0;
        if (strcmp(line, "exit") == 0) break;
        if (strlen(line) > 0) {
            eval_program(line, env);
        }
    }

    destroy_env(env);
}

/* 文件执行 */
int run_file(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) return -1;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *code = safe_malloc(size + 2);
    fread(code, 1, size, f);
    code[size] = '\n';
    code[size+1] = '\0';
    fclose(f);

    Env *env = create_env(NULL);
    global_env = env;
    eval_program(code, env);
    destroy_env(env);
    free(code);

    return 0;
}

/* 主函数 */
int main(int argc, char *argv[]) {
    if (argc == 1) {
        repl();
    } else if (argc == 2) {
        if (run_file(argv[1]) != 0) {
            fprintf(stderr, "Error: cannot open %s\n", argv[1]);
            return 1;
        }
    } else if (argc == 3 && strcmp(argv[1], "-e") == 0) {
        Env *env = create_env(NULL);
        global_env = env;
        eval_program(argv[2], env);
        destroy_env(env);
    } else {
        fprintf(stderr, "Usage: %s [file] or %s -e 'code'\n", argv[0], argv[0]);
        return 1;
    }

    return 0;
}