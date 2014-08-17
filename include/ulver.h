#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <math.h>
#include <uv.h>
#ifndef __WIN32__
#include <dlfcn.h>
#endif

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
#define ULVER_CORO 12
#define ULVER_CORO_DEAD 13
#define ULVER_HASHTABLE 14
#define ULVER_ARRAY 14
#define ULVER_TRUE 255

#define ULVER_ERR_NOERROR	0
#define ULVER_ERR_ONE_ARG	1
#define ULVER_ERR_TWO_ARG	2
#define ULVER_ERR_THREE_ARG	3
#define ULVER_ERR_NOT_NUM	4
#define ULVER_ERR_NOT_FLOAT	5
#define ULVER_ERR_NOT_STRING	6
#define ULVER_ERR_NOT_KEYWORD	7
#define ULVER_ERR_NOT_NUMFLOAT	8
#define ULVER_ERR_FPE		9
#define ULVER_ERR_PKG_NOTFOUND	10
#define ULVER_ERR_PKG		11
#define ULVER_ERR_THREAD	12
#define ULVER_ERR_NOT_THREAD	13
#define ULVER_ERR_NOT_CORO	14
#define ULVER_ERR_NOT_STREAM	15
#define ULVER_ERR_ZERO		16
#define ULVER_ERR_NOT_SEQ	17
#define ULVER_ERR_RANGE		18
#define ULVER_ERR_NOT_LIST	19
#define ULVER_ERR_ODD		20
#define ULVER_ERR_NOT_SYMBOL	21
#define ULVER_ERR_PARSE		22
#define ULVER_ERR_NOT_FUNC	23
#define ULVER_ERR_UNK_FUNC	24
#define ULVER_ERR_NOT_FORM	25
#define ULVER_ERR_IO		26
#define ULVER_ERR_NOT_KEYSTRING	27
#define ULVER_ERR_LAMBDA	28
#define ULVER_ERR_CROSS_THREAD	29
#define ULVER_ERR_CORO_DEAD	30
#define ULVER_ERR_CORO_BLOCKED	31
#define ULVER_ERR_UNBOUND	32
#define ULVER_ERR_NOT_HASHTABLE	33
#define ULVER_ERR_FOUR_ARG	34
#define ULVER_ERR_NEGATIVE	35
#define ULVER_ERR_NOT_CHANNEL	36
#define ULVER_ERR_CUSTOM	255

typedef struct ulver_env ulver_env;
typedef struct ulver_object ulver_object;
typedef struct ulver_object_item ulver_object_item;
typedef struct ulver_form ulver_form;
typedef struct ulver_symbol ulver_symbol;
typedef struct ulver_stackframe ulver_stackframe;
typedef struct ulver_symbolmap ulver_symbolmap;
typedef struct ulver_source ulver_source;
typedef struct ulver_thread ulver_thread;
typedef struct ulver_coro ulver_coro;
typedef struct ulver_scheduled_coro ulver_scheduled_coro;
typedef struct ulver_uv_stream ulver_uv_stream;
typedef struct ulver_err ulver_err;
typedef struct ulver_msgpack ulver_msgpack;
typedef struct ulver_channel_msg ulver_channel_msg;
typedef struct ulver_channel ulver_channel;

struct ulver_stackframe {
	struct ulver_stackframe *prev;
	ulver_symbolmap *locals;
	ulver_object *objects;
	ulver_object *ret;
        ulver_object *caller;
	ulver_form *argv;
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
	uint8_t requires_decoding;
	// could be NULL
	char *filename;
	uint64_t filename_len;
};

struct ulver_scheduled_coro {
	ulver_coro *coro;
	ulver_scheduled_coro *prev;
	ulver_scheduled_coro *next;
};

struct ulver_coro {
	void *context;
	ulver_coro *prev;
	ulver_coro *next;
	ulver_stackframe *stack;
	uint8_t trigger_gc;
	uint8_t dead;
	ulver_object *ret;
	uint8_t blocked;
	ulver_coro *waiting_for;
	// this is required to avoid cross-thread violation
	ulver_thread *thread;
	ulver_env *env;
	ulver_form *argv;
	void *data;
};

struct ulver_thread {
	uv_thread_t t;
	ulver_thread *prev;
	ulver_thread *next;
	// when set, the structure can be destroyed
	uint8_t dead;
	uv_loop_t *hub_loop;
	ulver_coro *hub;
	ulver_coro *main_coro;
	ulver_coro *coros;
	ulver_coro *current_coro;
	ulver_scheduled_coro *scheduled_coros_head;
	ulver_scheduled_coro *scheduled_coros_tail;
	ulver_env *env;
	ulver_coro *hub_creator;

	// until it is > 0, the thread cannot die
	uint64_t refs;

	// error condition
	uint8_t err_code;
	char *error;
	uint64_t error_len;
	ulver_form *error_form;
	char *error_fun;
};

struct ulver_err {
	char *string;
	uint64_t string_len;
	uint8_t fatal;
	uint8_t to_nil;
	uint8_t _errno;
};

struct ulver_env {

	uv_thread_t creator_thread;

	void *(*alloc)(ulver_env *, uint64_t);
	void (*free)(ulver_env *, void *, uint64_t);

	ulver_err err_table[256];

	FILE *_stdout;
	FILE *_stderr;

	ulver_object *t;
	ulver_object *nil;
	ulver_object *_stdin;

	// a key mapping to the currently running thread
	uv_key_t thread;

	uv_rwlock_t threads_lock;
	ulver_thread *threads;

	uv_rwlock_t gc_lock;

	// protect them at every access
	uv_mutex_t sources_lock;
	ulver_source *sources;

	ulver_object *cl_user;

	uv_rwlock_t current_package_lock;
	ulver_object *current_package;

	// memory counters must be protected
	uv_mutex_t mem_lock;
        uint64_t mem;
	uint64_t max_memory;
	uint64_t gc_freq;

	uv_rwlock_t globals_lock;
	ulver_symbolmap *globals;
	uv_rwlock_t funcs_lock;
	ulver_symbolmap *funcs;
	uv_rwlock_t packages_lock;
	ulver_symbolmap *packages;

	uint64_t calls;
	uint64_t gc_rounds;
	ulver_object *gc_root;
	uv_mutex_t gc_root_lock;

	uint64_t coro_stacksize;
};

struct ulver_object_item {
	ulver_object *o;
	ulver_object_item *next;	
};

// this structure is required for all
// io/operations (as they generally live
// in a specific coro
struct ulver_uv_stream {
        union {
		uv_handle_t h;
		uv_process_t p;
                uv_stream_t s;
                uv_tcp_t tcp;
                uv_udp_t udp;
                uv_tty_t tty;
        } handle;
        uv_write_t writer;
	uv_buf_t wbuf;
        ulver_env *env;
        ulver_thread *ut;
        ulver_coro *coro;
        ulver_object *func;
        char *buf;
        uint64_t len;
};

struct ulver_channel_msg {
	ulver_object *o;
	ulver_channel_msg *next;
};

struct ulver_channel {
	uv_mutex_t lock;
	int _pipe[2];
	ulver_channel_msg *head;	
	ulver_channel_msg *tail;	
};

struct ulver_object {
        uint8_t type;
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
	// is it mapped to a form ?
	ulver_form *form;
	// is it mapped to a thread ?
	ulver_thread *thread;
	// is it mapped to a coro ?
	ulver_coro *coro;
	// next object in the stack
	ulver_object *stack_next;
	// is it a return value ?
	uint8_t _return;
	// custom data
	void *data;
	// custom hook to call on destroy
	void (*on_destroy)(ulver_object *);
	// lambda is manually managed
	uint8_t no_lambda;

	uv_fs_t file;
	ulver_uv_stream *stream;

	// some object holds a return value 	
	ulver_object *ret;

	uint8_t ready;

	ulver_channel *chan;
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
	uint64_t need_free;
	ulver_source *source;
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
	ulver_object *key;
};

// serialize/deserialize objects and forms
struct ulver_msgpack {
	ulver_env *env;
	char *base;
	char *buf;
	uint64_t pos;
	uint64_t len;
};

void ulver_utils_print_list(ulver_env *, ulver_object *);
void ulver_utils_tolower(char *, uint64_t);
void ulver_utils_toupper(char *, uint64_t);
int ulver_utils_memicmp(char *, char *, uint64_t);
uint64_t ulver_utils_length(ulver_object *);

ulver_symbol *ulver_symbolmap_get(ulver_env *, ulver_symbolmap *, char *, uint64_t, uint8_t);
ulver_symbol *ulver_symbolmap_set(ulver_env *, ulver_symbolmap *, char *, uint64_t, ulver_object *, uint8_t);

ulver_stackframe *ulver_stack_push(ulver_env *, ulver_thread *, ulver_coro *);
void ulver_stack_pop(ulver_env *, ulver_thread *, ulver_coro *);

ulver_object *ulver_object_new(ulver_env *, uint8_t);
ulver_object *ulver_object_push(ulver_env *, ulver_object *, ulver_object *);
ulver_object *ulver_object_from_num(ulver_env *, int64_t);
ulver_object *ulver_object_from_float(ulver_env *, double);
ulver_object *ulver_object_from_string(ulver_env *, char *, uint64_t);
ulver_object *ulver_object_from_keyword(ulver_env *, char *, uint64_t);
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

ulver_object *ulver_error_form(ulver_env *, uint8_t, ulver_form *, char *);
ulver_object *ulver_error(ulver_env *, uint8_t);

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

void ulver_hub_schedule_coro(ulver_env *, ulver_coro *);

void ulver_coro_yield(ulver_env *, ulver_object *);

void ulver_hub_wait(ulver_env *, ulver_coro *);
void ulver_hub(ulver_env *);
ulver_coro *ulver_coro_new(ulver_env *, void *, void *);
void ulver_coro_switch(ulver_env *, ulver_coro *);
void ulver_coro_fast_switch(ulver_env *, ulver_coro *);
void *ulver_coro_alloc_context(ulver_env *);
void ulver_coro_free_context(ulver_env *, ulver_coro *);
void ulver_hub_schedule_waiters(ulver_env *, ulver_thread *ut, ulver_coro *);

void ulver_hub_destroy(ulver_env *, ulver_thread *);

ulver_object *ulver_call0(ulver_env *, ulver_object *);
void ulver_err_table_fill(ulver_env *);
uint8_t ulver_error_code(ulver_env *);

ulver_msgpack *ulver_form_serialize(ulver_env *, ulver_form *, ulver_msgpack *);
ulver_form *ulver_form_deserialize(ulver_env *, ulver_form *, char **, uint64_t *);
ulver_form *ulver_form_push_form(ulver_env *, ulver_form *, uint8_t);

char *ulver_util_str2num(ulver_env *env, int64_t, uint64_t *);

ulver_msgpack *ulver_object_serialize(ulver_env *, ulver_object *, ulver_msgpack *);

ulver_symbolmap *ulver_symbolmap_copy(ulver_env *, ulver_symbolmap *);

ulver_object *ulver_object_copy_to(ulver_env *, ulver_object *, ulver_object *);
