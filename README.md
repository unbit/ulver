ulver
=====

Unbit Lisp Version

This is an almost didactical ANSI C implementation of a Common LISP interpreter.

It is built to be easily extended and for being embedded in C/C++ applications

Installation
============

just run

```sh
make
```

you will end with

* libulver.so (a shared library you can use to embed the ulver engine in your apps)
* libulver.a (static version of libulver.so)
* ulver (readline based executable implementing REPL and file runner)

Now run

```sh
./ulver
```

to access the REPL interface:

```lisp
> (+ 1 1)

2
```

(the GNU readline library is used, so you have history and advanced terminal operations)

Instead, if you pass a filename to the ulver command it will parse it

```sh
./ulver myscript.lisp
```

If you want to run the test suite, just do

```sh
make test
```

from the sources directory

Embedding
=========

You can generate multiple list environments in the same thread or process. Each intepreter is mapped to a ulver_env structure. You can initialize a new interpreter with the function:

```c
ulver_env *ulver_init();
```

After an interpreter is initialized you can start parsing code.

To parse code you use

```c
ulver_form *ulver_parse(ulver_env *env, char *source, uint64_t source_len);
```

The parsed tree is built upon a serie of ulver_form structs. The ulver_form returned by ulver_parse is the root of the tree.

Once you have a form you can evaluate it with

```c
ulver_object *ulver_eval(ulver_env *env, ulver_form *form);
```

The ulver_object struct is the result of a form evaluation

Example:

```c
#include <stdlib.h>
#include <stdio.h>
#include <ulver.h>

int main() {
        ulver_env *env = ulver_init();
        ulver_form *root_form = ulver_parse(env, "(print 17)", 11);
        if (!root_form) {
                printf("unable to parse your lisp code !!!\n");
                exit(1);
        }
        ulver_eval(env, root_form);
}
```

you can build it with (supposing you are still in the ulver source directory):

```sh
gcc -I. -o example example.c libulver.a
```

running ```./example``` will print "17 "

If you prefer to use the dynamic library (and you have copied it in a library directory)

```sh
gcc -I. -o example example.c -lulver
```

(oviously you can copy ulver.h to an include directory and avoid the -I. flag)

A bunch of shortcuts are available:

```c
ulver_object *ulver_run(ulver_env *env, char *source); 
```

parse and evaluate the "source" string, returning a ulver_object structure

```c
ulver_object *ulver_load(ulver_env *env, char *filename); 
```

parse and evaluate the specified filename, returning a ulver_object structure

You can define new objects and functions from your app once the ulver_env structure is ready.

The following object types are defined:

```c
#define ULVER_LIST 0
#define ULVER_SYMBOL 1
#define ULVER_NUM 2
#define ULVER_STRING 3
#define ULVER_FLOAT 4
#define ULVER_FUNC 5
#define ULVER_KEYWORD 6
#define ULVER_PACKAGE 7
#define ULVER_FORM 8
#define ULVER_TRUE 255
```

So to create a num (note: all integers in ulver are signed 64bit):

```c
ulver_object *my_number = ulver_object_new(env, ULVER_NUM);
```

now the object is created, but you need to assign a value to it:

```c
ulver_object *my_number = ulver_object_new(env, ULVER_NUM);
my_number->n = 17;
```

for a float you will use

```c
ulver_object *my_number = ulver_object_new(env, ULVER_FLOAT);
my_number->d = 17.0;
```

Creating objects in this way is not the handiest way, the api exposes a bunch of commodity functions:

```c
ulver_object *ulver_object_from_num(ulver_env *, int64_t);
ulver_object *ulver_object_from_float(ulver_env *, double);
ulver_object *ulver_object_from_string(ulver_env *, char *, uint64_t);
ulver_object *ulver_object_from_keyword(ulver_env *, char *, uint64_t);
```

(as you can see even strings and keywords lengths are 64bit)

One objects are created you can bind them to symbols (or variables for more conventional naming):

```c
ulver_symbol *ulver_symbol_set(ulver_env *env, char *name, uint64_t name_len, ulver_object *object);
ulver_symbol *ulver_symbol_get(ulver_env *env, char *name, uint64_t name_len);
```

both returns a ulver_symbol structure (or NULL on error/notfound). The ->value field of this structure is the pointer to the ulver_object structure

both take in account the current stack frame, so if you call them before entering a function (read: son after env_init() call) you will automatically create global variables. Otherwise use the \*name\* syntax:

```c
ulver_symbol_set(env, "*foobar*", 8, ulver_object_from_num(env, 17));
```

now the \*foobar\* symbol is globally available to your interpreter

As well as new symbols, you can define new functions:

```c
ulver_symbol *ulver_register_fun(ulver_env *env, char *name, ulver_object *(*func)(ulver_env *, ulver_form *));
```

a lisp function in ulver is a standard C function with the following prototype:

```c
ulver_object *funny_lisp_function(ulver_env *env, ulver_form *form) {
        ...
}
```

lisp functions take a form as input and returns an ulver_object (or NULL on error)

As an example, take this function:

```lisp
(funny "helloworld")
```

the string "helloworld" is our form. The objective of our function is generating a new string with uppercased characters.

First step is checking an argument has been passed to funny

```c
ulver_object *funny_lisp_function(ulver_env *env, ulver_form *form) {
        if (!argv) return ulver_error("funny needs an argument");
        ...
}
```

the ulver_error function sets the interpreter global error state (something you should always check) and returns NULL;

We have an argument (a form), and we now need to evaluate it (remember, lisp is lazy, arguments are evaluated only when needed, so your functions must choose when and how to manage arguments).

```c
ulver_object *funny_lisp_function(ulver_env *env, ulver_form *form) {
        if (!argv) return ulver_error("funny needs an argument");
        ulver_object *string1 = ulver_eval(env, argv);
        // we always need to check for eval return value
        if (!string1) return NULL;
        if (string1->type != ULVER_STRING) return ulver_error_form(env, argv, "is not a string");
        ...
}
```

the previous code should be quite self-explanatory, the only new thing is the ulver_error_form() commodity function, that is like ulver_error() but adds the body of the form generating the error. (it is only for giving users a more handy error report, you are free to set the error message to whatever you want)

Now we have our argument object, our purpose is generating a new with the same body of the argument, and then we uppercase each char:

```c
ulver_object *funny_lisp_function(ulver_env *env, ulver_form *form) {
        if (!argv) return ulver_error("funny needs an argument");
        ulver_object *string1 = ulver_eval(env, argv);
        // we always need to check for eval return value
        if (!string1) return NULL;
        if (string1->type != ULVER_STRING) return ulver_error_form(env, argv, "is not a string");
        // generate a new string object with the same content of string1
        ulver_object *new_string = ulver_object_from_string(env, string1->str, string1->len);
        // now iterate each char
        uint64_t i;
        for(i=0;i<new_string->len;i++) {
                new_string->str[i] = toupper(new_string->str[i]);
        }
        return new_string;
}
```

the function is now complete and you can register it in the functions symbol table:

```c
ulver_register_fun(env, "funny", funny_lisp_function);
```

now your lisp engine has the (funny) function available

Status
======

The project is in alpha stage (it is still mainly a proof of concept), the following functions are implemented:

* +
* -
* *
* >
* =
* print
* parse-integer
* setq
* progn
* read-line
* if
* cond
* defun
* list
* cons
* car
* cdr
* atom
* let
* gc
* unintern
* error
* load
* defpackage
* in-package
* eq
* getf
* first
* second
* third
* fourth
* fifth
* sixth
* seventh
* eighth
* ninth
* tenth
* quote
* eval
* lambda
* funcall
* find
* position
* read-from-string
* subseq
* length


The garbage collection system is already working (mark and sweep), and you can manually call it from lisp using the (gc) function (it returns the amount of memory used by the interpreter, in bytes).

Packages (namespaces) are implemented, but defpackage supports only the :export argument
