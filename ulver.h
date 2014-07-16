#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <ctype.h>

#define ULVER_LIST 0
#define ULVER_SYMBOL 1
#define ULVER_NUM 2
#define ULVER_STRING 3
#define ULVER_FLOAT 4
#define ULVER_FUNC 5
#define ULVER_KEYWORD 6
#define ULVER_TRUE 255

typedef struct ulver_env ulver_env;
typedef struct ulver_object ulver_object;
typedef struct ulver_form ulver_form;
typedef struct ulver_symbol ulver_symbol;
typedef struct ulver_stackframe ulver_stackframe;
typedef struct ulver_symbolmap ulver_symbolmap;

struct ulver_stackframe {
	struct ulver_stackframe *prev;
	ulver_symbolmap *locals;
	ulver_symbolmap *fun_locals;
	ulver_object *ret;
};

struct ulver_env {
        struct ulver_object *caller;
	struct ulver_stackframe *stack;
	struct ulver_stackframe *global_stack;
	void *(*alloc)(ulver_env *, uint64_t);
	void (*free)(ulver_env *, void *);
	ulver_object *t;
	ulver_object *nil;
        uint64_t mem;
        uint64_t calls;
	ulver_object *gc_root;
	char *error;
	uint64_t error_len;
	uint64_t lines;
	uint64_t pos;
	uint64_t line_pos;
	char *token;
	uint64_t token_len;
	ulver_form *form_root;
	ulver_form *form_list_current;
	uint8_t is_quoted;
};

struct ulver_object {
        uint8_t type; // list, func, num, string
        uint64_t len;
        char *str;
        int64_t n;
        double d;
        ulver_object *(*func)(ulver_env *, ulver_form *);
        ulver_form *lambda_list;
        ulver_form *progn;
        ulver_object *list;
        ulver_object *next;
	uint64_t size;
	ulver_object *gc_prev;
	ulver_object *gc_next;
	uint8_t gc_mark;
	uint8_t gc_protected;
};

struct ulver_form {
        uint8_t type; // list, symbol, num, string
        char *value;
        uint64_t len;
        ulver_form *next;
        ulver_form *parent;
        ulver_form *list;
	uint64_t line;
	uint64_t line_pos;
};

struct ulver_symbolmap {
	uint64_t items;
	uint64_t hashtable_size;
	ulver_symbol **hashtable;
};

struct ulver_symbol {
        char *name;
        uint64_t len;
        ulver_object *value;
        ulver_symbol *prev;
        ulver_symbol *next;
};

void ulver_utils_print_list(ulver_env *, ulver_object *);
void ulver_utils_tolower(char *, uint64_t);
void ulver_utils_toupper(char *, uint64_t);
int ulver_utils_memicmp(char *, char *, uint64_t);

ulver_symbol *ulver_symbolmap_get(ulver_env *, ulver_symbolmap *, char *, uint64_t);
ulver_symbol *ulver_symbolmap_set(ulver_env *, ulver_symbolmap *, char *, uint64_t, ulver_object *);

ulver_stackframe *ulver_stack_push(ulver_env *);
void ulver_stack_pop(ulver_env *);

ulver_object *ulver_object_new(ulver_env *, uint8_t);
ulver_object *ulver_object_push(ulver_env *, ulver_object *, ulver_object *);
ulver_object *ulver_object_from_num(ulver_env *, int64_t);
ulver_object *ulver_object_from_float(ulver_env *, double);
ulver_object *ulver_object_from_string(ulver_env *, char *, uint64_t);
ulver_symbol *ulver_symbol_set(ulver_env *, char *, uint64_t, ulver_object *);
ulver_object *ulver_eval(ulver_env *, ulver_form *);
ulver_object *ulver_object_copy(ulver_env *, ulver_object *);

ulver_object *ulver_fun_print(ulver_env *, ulver_form *);

ulver_symbol *ulver_register_fun2(ulver_env *, char *, uint64_t, ulver_object *(*)(ulver_env *, ulver_form *), ulver_form *, ulver_form *);

void *ulver_alloc(ulver_env *, uint64_t);
void ulver_free(ulver_env *, void *);
void ulver_gc(ulver_env *);
void ulver_object_destroy(ulver_env *, ulver_object *);

int ulver_symbolmap_delete(ulver_env *, ulver_symbolmap *, char *, uint64_t);
int ulver_symbol_delete(ulver_env *, char *, uint64_t);

ulver_object *ulver_error(ulver_env *, char *, ...);
ulver_object *ulver_error_form(ulver_env *, ulver_form *, char *);

ulver_form *ulver_parse(ulver_env *, char *, size_t);

int ulver_utils_is_a_number(char *, uint64_t);
int ulver_utils_is_a_float(char *, uint64_t);

void ulver_init(ulver_env *);
