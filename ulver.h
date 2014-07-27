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
#include <errno.h>
#ifdef __WIN32__
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#include <pthread.h>

#define ULVER_LIST 0
#define ULVER_SYMBOL 1
#define ULVER_NUM 2
#define ULVER_STRING 3
#define ULVER_FLOAT 4
#define ULVER_FUNC 5
#define ULVER_KEYWORD 6
#define ULVER_PACKAGE 7
#define ULVER_FORM 8
#define ULVER_STREAM 9
#define ULVER_THREAD 10
#define ULVER_CHANNEL 11
#define ULVER_MULTIVALUE 11
#define ULVER_TRUE 255

typedef struct ulver_env ulver_env;
typedef struct ulver_object ulver_object;
typedef struct ulver_object_item ulver_object_item;
typedef struct ulver_form ulver_form;
typedef struct ulver_symbol ulver_symbol;
typedef struct ulver_stackframe ulver_stackframe;
typedef struct ulver_symbolmap ulver_symbolmap;
typedef struct ulver_source ulver_source;
typedef struct ulver_thread ulver_thread;
typedef struct ulver_message ulver_message;

struct ulver_stackframe {
	struct ulver_stackframe *prev;
	ulver_symbolmap *locals;
	ulver_object *objects;
	ulver_object *ret;
};

struct ulver_source {
	char *str;
	uint64_t len;
	ulver_source *next;
	uint64_t lines;
	uint64_t pos;
	uint64_t line_pos;
	char *token;
	uint64_t token_len;
	ulver_form *form_root;
	ulver_form *form_list_current;
	ulver_form *form_new;
	uint8_t is_doublequoted;
	uint8_t is_escaped;
	uint8_t is_comment;
};


struct ulver_thread {
	pthread_t t;
	//pthread_mutex_t lock;
	char *error;
	uint64_t error_len;
	uint64_t error_buf_len;
        ulver_object *caller;
	ulver_stackframe *stack;
	ulver_thread *prev;
	ulver_thread *next;
	// when set, the structure can be destroyed
	uint8_t dead;
	uint8_t trigger_gc;
};

struct ulver_message {
	ulver_object *value;
	ulver_message *prev;
	ulver_message *next;
};

struct ulver_env {

	pthread_t creator_thread;

	void *(*alloc)(ulver_env *, uint64_t);
	void (*free)(ulver_env *, void *, uint64_t);

	FILE *stdout;
	FILE *stderr;

	ulver_object *t;
	ulver_object *nil;
	ulver_object *stdin;

	// a key mapping to the currently running thread
	pthread_key_t thread;

	pthread_rwlock_t threads_lock;
	ulver_thread *threads;

	pthread_rwlock_t unsafe_lock;

	// protect them at every access
	pthread_mutex_t sources_lock;
	ulver_source *sources;

	pthread_rwlock_t packages_lock;
	ulver_symbolmap *packages;

	ulver_object *cl_user;

	pthread_rwlock_t current_package_lock;
	ulver_object *current_package;

	//  gc always runs in locked context
	pthread_mutex_t gc_lock;
	uint64_t gc_rounds;

	pthread_mutex_t mem_lock;
        uint64_t mem;
        uint64_t calls;
	uint64_t gc_freq;
	uint64_t max_memory;


	pthread_mutex_t gc_root_lock;
	ulver_object *gc_root;

	pthread_rwlock_t globals_lock;
	ulver_symbolmap *globals;
	pthread_rwlock_t funcs_lock;
	ulver_symbolmap *funcs;
	pthread_rwlock_t channels_lock;
	ulver_symbolmap *channels;
};

struct ulver_object_item {
	ulver_object *o;
	ulver_object_item *next;	
};

struct ulver_object {
        uint8_t type; // list, func, num, string
        uint64_t len;
        char *str;
        int64_t n;
        double d;
        ulver_object *(*func)(ulver_env *, ulver_form *);
        ulver_form *lambda_list;
        ulver_object_item *list;
	ulver_object *gc_prev;
	ulver_object *gc_next;
	uint8_t gc_mark;
	uint8_t gc_protected;
	ulver_symbolmap *map;
	ulver_form *form;
	ulver_thread *thread;
	ulver_message *msg_head;
	ulver_message *msg_tail;
	ulver_object *stack_next;
	uint8_t unsafe;
	uint8_t _return;
	int fd;
	uint8_t closed;
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
	uint8_t free_name;
};

void ulver_utils_print_list(ulver_env *, ulver_object *);
void ulver_utils_tolower(char *, uint64_t);
void ulver_utils_toupper(char *, uint64_t);
int ulver_utils_memicmp(char *, char *, uint64_t);
uint64_t ulver_utils_length(ulver_object *);

ulver_symbol *ulver_symbolmap_get(ulver_env *, ulver_symbolmap *, char *, uint64_t, uint8_t);
ulver_symbol *ulver_symbolmap_set(ulver_env *, ulver_symbolmap *, char *, uint64_t, ulver_object *, uint8_t);

ulver_stackframe *ulver_stack_push(ulver_env *, ulver_thread *);
void ulver_stack_pop(ulver_env *, ulver_thread *);

ulver_object *ulver_object_new(ulver_env *, uint8_t);
ulver_object *ulver_object_push(ulver_env *, ulver_object *, ulver_object *);
ulver_object *ulver_object_from_num(ulver_env *, int64_t);
ulver_object *ulver_object_from_float(ulver_env *, double);
ulver_object *ulver_object_from_string(ulver_env *, char *, uint64_t);
ulver_object *ulver_object_from_keyword(ulver_env *, char *, uint64_t);
ulver_object *ulver_object_from_fd(ulver_env *, int);
ulver_symbol *ulver_symbol_set(ulver_env *, char *, uint64_t, ulver_object *);
ulver_object *ulver_symbol_get(ulver_env *, char *, uint64_t);
ulver_object *ulver_eval(ulver_env *, ulver_form *);
ulver_object *ulver_eval_list(ulver_env *, ulver_form *);
ulver_object *ulver_object_copy(ulver_env *, ulver_object *);

void ulver_symbolmap_destroy(ulver_env *, ulver_symbolmap *);

ulver_object *ulver_fun_print(ulver_env *, ulver_form *);

ulver_symbol *ulver_register_fun2(ulver_env *, char *, uint64_t, ulver_object *(*)(ulver_env *, ulver_form *), ulver_form *, ulver_form *);
ulver_symbol *ulver_register_fun(ulver_env *, char *, ulver_object *(*)(ulver_env *, ulver_form *));

void *ulver_alloc(ulver_env *, uint64_t);
void ulver_free(ulver_env *, void *, uint64_t);
void ulver_gc(ulver_env *);
void ulver_object_destroy(ulver_env *, ulver_object *);

int ulver_symbolmap_delete(ulver_env *, ulver_symbolmap *, char *, uint64_t, uint8_t);
int ulver_symbol_delete(ulver_env *, char *, uint64_t);

ulver_object *ulver_error(ulver_env *, char *, ...);
ulver_object *ulver_error_form(ulver_env *, ulver_form *, char *);

ulver_form *ulver_parse(ulver_env *, char *, size_t);

int ulver_utils_is_a_number(char *, uint64_t);
int ulver_utils_is_a_float(char *, uint64_t);

ulver_env *ulver_init();
uint64_t ulver_destroy(ulver_env *);

void ulver_form_destroy(ulver_env *, ulver_form *);

char *ulver_utils_strndup(ulver_env *, char *, uint64_t);
char *ulver_utils_strdup(ulver_env *, char *);

ulver_object *ulver_load(ulver_env *, char *);
void ulver_report_error(ulver_env *);

ulver_symbolmap *ulver_symbolmap_new(ulver_env *);

ulver_object *ulver_package(ulver_env *, char *, uint64_t);

int ulver_utils_eq(ulver_object *, ulver_object *);

ulver_object *ulver_run(ulver_env *, char *);

ulver_object *ulver_utils_nth(ulver_object *, uint64_t);
void ulver_utils_print_form(ulver_env *, ulver_form *);

ulver_thread *ulver_current_thread(ulver_env *);

char *ulver_utils_is_library(ulver_env *env, char *);
char *ulver_utils_readline_from_fd(int, uint64_t *);
