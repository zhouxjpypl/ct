#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <time.h>
#include <setjmp.h>

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

/* 字符串辅助函数 */
int str_len(const char *s) {
    return strlen(s);
}

char* str_sub(const char *s, int start, int end) {
    int len = strlen(s);
    if (start < 0) start = 0;
    if (end > len) end = len;
    if (start >= end) return safe_strdup("");

    char *result = safe_malloc(end - start + 1);
    strncpy(result, s + start, end - start);
    result[end - start] = '\0';
    return result;
}

char* str_replace(const char *s, const char *old, const char *new_str) {
    char *result = safe_malloc(MAX_STR_LEN);
    const char *p = s;
    char *r = result;
    int old_len = strlen(old);
    int new_len = strlen(new_str);

    while (*p) {
        if (strncmp(p, old, old_len) == 0) {
            strcpy(r, new_str);
            r += new_len;
            p += old_len;
        } else {
            *r++ = *p++;
        }
    }
    *r = '\0';
    return result;
}

char* str_toupper(const char *s) {
    char *result = safe_strdup(s);
    for (int i = 0; result[i]; i++) {
        result[i] = toupper(result[i]);
    }
    return result;
}

char* str_tolower(const char *s) {
    char *result = safe_strdup(s);
    for (int i = 0; result[i]; i++) {
        result[i] = tolower(result[i]);
    }
    return result;
}

/* 文件操作 */
char* file_read(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *content = safe_malloc(size + 1);
    fread(content, 1, size, f);
    content[size] = '\0';
    fclose(f);

    return content;
}

int file_write(const char *filename, const char *content) {
    FILE *f = fopen(filename, "w");
    if (!f) return -1;
    fprintf(f, "%s", content);
    fclose(f);
    return 1;
}

int file_append(const char *filename, const char *content) {
    FILE *f = fopen(filename, "a");
    if (!f) return -1;
    fprintf(f, "%s", content);
    fclose(f);
    return 1;
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
        case TYPE_NULL:
            printf("null");
            break;
        default:
            break;
    }
}

/* 环境操作 */
Env* create_env(Env *parent) {
    Env *env = safe_malloc(sizeof(Env));
    env->vars = NULL;
    env->parent = parent;
    env->breaker = NULL;
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
    else if (strncmp(*p, "len", 3) == 0 && !isalnum((*p)[3])) {
        *p += 3;
        if (**p == '(') (*p)++;
        Value arg = eval_expr(p, env);
        if (**p == ')') (*p)++;

        int len = 0;
        if (arg.type == TYPE_STRING) {
            len = strlen(arg.str);
        } else if (arg.type == TYPE_ARRAY) {
            len = arg.arr.length;
        }
        free_value(&arg);
        return make_number(len);
    }
    else if (strncmp(*p, "read", 4) == 0 && !isalnum((*p)[4])) {
        *p += 4;
        if (**p == '(') (*p)++;
        Value filename = eval_expr(p, env);
        if (**p == ')') (*p)++;

        char *content = file_read(filename.str);
        Value result;
        if (content) {
            result = make_string(content);
            free(content);
        } else {
            result = make_null();
        }
        free_value(&filename);
        return result;
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
        else if (**p == '=') {
            (*p)++;
            Value v = eval_expr(p, env);
            env_set(env, name, v);
            free(name);
            return make_null();
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

static Value eval_mul_div(const char **p, Env *env) {
    Value left = eval_primary(p, env);

    while (isspace(**p)) (*p)++;
    while (**p == '*' || **p == '/') {
        char op = **p;
        (*p)++;
        Value right = eval_primary(p, env);

        if (left.type == TYPE_NUMBER && right.type == TYPE_NUMBER) {
            if (op == '*') left.num = left.num * right.num;
            else left.num = left.num / right.num;
        } else if (left.type == TYPE_STRING && op == '*') {
            int repeat = (int)right.num;
            char *new_str = safe_malloc(strlen(left.str) * repeat + 1);
            new_str[0] = '\0';
            for (int i = 0; i < repeat; i++) {
                strcat(new_str, left.str);
            }
            free_value(&left);
            left = make_string(new_str);
            free(new_str);
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

        if (left.type == TYPE_NUMBER && right.type == TYPE_NUMBER) {
            if (op == '+') left.num = left.num + right.num;
            else left.num = left.num - right.num;
        }
        else if (left.type == TYPE_STRING && op == '+') {
            char *new_str = safe_malloc(strlen(left.str) + strlen(right.str) + 1);
            strcpy(new_str, left.str);
            strcat(new_str, right.str);
            free_value(&left);
            left = make_string(new_str);
            free(new_str);
        }

        while (isspace(**p)) (*p)++;
    }
    return left;
}

static Value eval_comparison(const char **p, Env *env) {
    Value left = eval_add_sub(p, env);

    while (isspace(**p)) (*p)++;
    if (strncmp(*p, "==", 2) == 0) {
        *p += 2;
        Value right = eval_comparison(p, env);
        int result = 0;
        if (left.type == TYPE_NUMBER && right.type == TYPE_NUMBER) {
            result = (left.num == right.num);
        } else if (left.type == TYPE_STRING && right.type == TYPE_STRING) {
            result = (strcmp(left.str, right.str) == 0);
        }
        free_value(&left);
        free_value(&right);
        return make_number(result);
    }
    else if (strncmp(*p, "!=", 2) == 0) {
        *p += 2;
        Value right = eval_comparison(p, env);
        int result = 0;
        if (left.type == TYPE_NUMBER && right.type == TYPE_NUMBER) {
            result = (left.num != right.num);
        }
        free_value(&left);
        free_value(&right);
        return make_number(result);
    }
    else if (**p == '<') {
        (*p)++;
        int is_le = 0;
        if (**p == '=') {
            (*p)++;
            is_le = 1;
        }
        Value right = eval_comparison(p, env);
        int result = 0;
        if (left.type == TYPE_NUMBER && right.type == TYPE_NUMBER) {
            if (is_le) result = (left.num <= right.num);
            else result = (left.num < right.num);
        }
        free_value(&left);
        free_value(&right);
        return make_number(result);
    }
    else if (**p == '>') {
        (*p)++;
        int is_ge = 0;
        if (**p == '=') {
            (*p)++;
            is_ge = 1;
        }
        Value right = eval_comparison(p, env);
        int result = 0;
        if (left.type == TYPE_NUMBER && right.type == TYPE_NUMBER) {
            if (is_ge) result = (left.num >= right.num);
            else result = (left.num > right.num);
        }
        free_value(&left);
        free_value(&right);
        return make_number(result);
    }

    return left;
}

static Value eval_and(const char **p, Env *env) {
    Value left = eval_comparison(p, env);

    while (isspace(**p)) (*p)++;
    if (strncmp(*p, "and", 3) == 0 && !isalnum((*p)[3])) {
        *p += 3;
        Value right = eval_and(p, env);
        int left_true = (left.type == TYPE_NUMBER && left.num != 0);
        int right_true = (right.type == TYPE_NUMBER && right.num != 0);
        free_value(&left);
        free_value(&right);
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
        int left_true = (left.type == TYPE_NUMBER && left.num != 0);
        int right_true = (right.type == TYPE_NUMBER && right.num != 0);
        free_value(&left);
        free_value(&right);
        return make_number((left_true || right_true) ? 1 : 0);
    }
    return left;
}

Value eval_expr(const char **p, Env *env) {
    return eval_or(p, env);
}

/* 字符串方法调用 */
static Value call_string_method(const char *method, Value *self, const char **p, Env *env) {
    if (strcmp(method, "len") == 0) {
        return make_number(strlen(self->str));
    }
    else if (strcmp(method, "upper") == 0) {
        char *result = str_toupper(self->str);
        Value v = make_string(result);
        free(result);
        return v;
    }
    else if (strcmp(method, "lower") == 0) {
        char *result = str_tolower(self->str);
        Value v = make_string(result);
        free(result);
        return v;
    }
    else if (strcmp(method, "sub") == 0) {
        if (**p == '(') (*p)++;
        Value start = eval_expr(p, env);
        if (**p == ',') (*p)++;
        Value end = eval_expr(p, env);
        if (**p == ')') (*p)++;

        char *result = str_sub(self->str, (int)start.num, (int)end.num);
        Value v = make_string(result);
        free(result);
        free_value(&start);
        free_value(&end);
        return v;
    }
    else if (strcmp(method, "replace") == 0) {
        if (**p == '(') (*p)++;
        Value old_str = eval_expr(p, env);
        if (**p == ',') (*p)++;
        Value new_str = eval_expr(p, env);
        if (**p == ')') (*p)++;

        char *result = str_replace(self->str, old_str.str, new_str.str);
        Value v = make_string(result);
        free(result);
        free_value(&old_str);
        free_value(&new_str);
        return v;
    }

    error("Unknown string method: %s", method);
    return make_null();
}

/* 语句解析 */
static void parse_block(const char **p, Env *env) {
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
}

static void parse_for_loop(const char **p, Env *env) {
    // for i = 1 to 10 { ... }
    // for i = 10 downto 1 { ... }

    while (isspace(**p)) (*p)++;
    char *var_name = parse_ident(p);

    while (isspace(**p)) (*p)++;
    if (**p != '=') error("Expected '=' in for loop");
    (*p)++;

    Value start_val = eval_expr(p, env);

    while (isspace(**p)) (*p)++;
    int is_downto = 0;
    if (strncmp(*p, "to", 2) == 0) {
        *p += 2;
    } else if (strncmp(*p, "downto", 6) == 0) {
        *p += 6;
        is_downto = 1;
    } else {
        error("Expected 'to' or 'downto' in for loop");
    }

    while (isspace(**p)) (*p)++;
    Value end_val = eval_expr(p, env);

    while (isspace(**p)) (*p)++;
    parse_block(p, env);

    double start = start_val.num;
    double end = end_val.num;

    if (is_downto) {
        for (double i = start; i >= end; i = i - 1) {
            env_set(env, var_name, make_number(i));
            const char *save = *p;
            parse_block(&save, env);
        }
    } else {
        for (double i = start; i <= end; i = i + 1) {
            env_set(env, var_name, make_number(i));
            const char *save = *p;
            parse_block(&save, env);
        }
    }

    free_value(&start_val);
    free_value(&end_val);
    free(var_name);
}

static void parse_while(const char **p, Env *env) {
    const char *cond_start = *p;
    Value cond = eval_expr(&cond_start, env);

    while (cond.type == TYPE_NUMBER && cond.num != 0) {
        const char *body_ptr = cond_start;
        while (isspace(*body_ptr)) body_ptr++;
        Env *loop_env = create_env(env);

        if (*body_ptr == '{') {
            body_ptr++;
            while (*body_ptr && *body_ptr != '}') {
                eval_statement(&body_ptr, loop_env);
                while (isspace(*body_ptr)) body_ptr++;
            }
        } else {
            eval_statement(&body_ptr, loop_env);
        }

        destroy_env(loop_env);

        const char *new_cond = cond_start;
        free_value(&cond);
        cond = eval_expr(&new_cond, env);
    }

    free_value(&cond);

    // 跳过循环体
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

static void parse_if(const char **p, Env *env) {
    Value cond = eval_expr(p, env);
    int truth = (cond.type == TYPE_NUMBER && cond.num != 0);
    free_value(&cond);

    while (isspace(**p)) (*p)++;

    if (truth) {
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
        // 跳过 if 块
        if (**p == '{') {
            int brace = 1;
            (*p)++;
            while (brace > 0 && **p) {
                if (**p == '{') brace++;
                if (**p == '}') brace--;
                (*p)++;
            }
        } else {
            while (**p && **p != ';' && **p != '\n') (*p)++;
        }

        // 检查 else
        while (isspace(**p)) (*p)++;
        if (strncmp(*p, "else", 4) == 0 && !isalnum((*p)[4])) {
            *p += 4;
            while (isspace(**p)) (*p)++;

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
        }
    }
}

static void parse_function(const char **p, Env *env) {
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
    else if (strncmp(*p, "for", 3) == 0 && !isalnum((*p)[3])) {
        *p += 3;
        parse_for_loop(p, env);
    }
    else if (strncmp(*p, "while", 5) == 0 && !isalnum((*p)[5])) {
        *p += 5;
        parse_while(p, env);
    }
    else if (strncmp(*p, "if", 2) == 0 && !isalnum((*p)[2])) {
        *p += 2;
        parse_if(p, env);
    }
    else if (strncmp(*p, "func", 4) == 0 && !isalnum((*p)[4])) {
        *p += 4;
        parse_function(p, env);
    }
    else if (strncmp(*p, "return", 6) == 0 && !isalnum((*p)[6])) {
        *p += 6;
        Value v = eval_expr(p, env);
        // TODO: handle return value
        free_value(&v);
    }
    else if (isalpha(**p) || **p == '_') {
        // 可能是变量赋值或方法调用
        char *name = parse_ident(p);

        if (**p == '.') {
            (*p)++;
            char *method = parse_ident(p);

            Value *self = env_get(env, name);
            if (!self || self->type != TYPE_STRING) {
                error("Method call on non-string");
            }

            Value result = call_string_method(method, self, p, env);
            print_value(&result);
            printf("\n");
            free_value(&result);
            free(method);
        } else {
            while (isspace(**p)) (*p)++;
            if (**p == '=') {
                (*p)++;
                Value v = eval_expr(p, env);
                env_set(env, name, v);
            } else {
                Value *v = env_get(env, name);
                if (v) {
                    print_value(v);
                    printf("\n");
                }
            }
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

/* 文件操作语句 */
static void parse_write_file(const char **p, Env *env) {
    while (isspace(**p)) (*p)++;
    if (**p != '(') error("Expected '(' after write");
    (*p)++;

    Value filename = eval_expr(p, env);
    if (**p != ',') error("Expected ',' after filename");
    (*p)++;

    Value content = eval_expr(p, env);
    if (**p != ')') error("Expected ')'");
    (*p)++;

    int result = file_write(filename.str, content.str);
    if (result == -1) {
        error("Cannot write to file: %s", filename.str);
    }

    free_value(&filename);
    free_value(&content);
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