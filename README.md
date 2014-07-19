ulver
=====

Unbit Lisp Version

This is an almost didactical ANSI C implementation of a Common LISP interpreter.

It is built to be easily extended and for being embedded in C/C++ applications

It has been tested on Linux, FreeBSD, Mac OSX and windows

Installation
============

you need a c compiler (gcc or clang, on windows you can use the mingw tools)

just run

```sh
make
```

you will end with

* libulver.so (ulver.dll on windows, a shared library you can use to embed the ulver engine in your apps or to build extensions)
* libulver.a (static version of libulver.so)
* ulver (readline based executable implementing REPL and file runner (note: on windows you do not have readline support but a simple/raw fgets() loop)

Now run

```sh
./ulver
```

to access the REPL interface:

```lisp
> (+ 1 1)

2
```

(on non-windows systems the GNU readline library is used, so you have history and advanced terminal operations)

Instead, if you pass a filename to the ulver command, it will parse and run it

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

You can generate multiple lisp environments in the same thread or process. Each intepreter is mapped to a ulver_env structure. You can initialize a new interpreter with the function:

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

Once objects are created you can bind them to symbols (or variables for more conventional naming):

```c
ulver_symbol *ulver_symbol_set(ulver_env *env, char *name, uint64_t name_len, ulver_object *object);
ulver_symbol *ulver_symbol_get(ulver_env *env, char *name, uint64_t name_len);
```

both return a ulver_symbol structure (or NULL on error/notfound). The ->value field of this structure is the pointer to the ulver_object structure

both take in account the current stack frame, so if you call them before entering a function (read: soon after env_init() call) you will automatically create global variables. Otherwise use the \*name\* syntax:

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
ulver_object *funny_lisp_function(ulver_env *env, ulver_form *argv) {
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
ulver_object *funny_lisp_function(ulver_env *env, ulver_form *argv) {
        if (!argv) return ulver_error("funny needs an argument");
        ...
}
```

the ulver_error function sets the interpreter global error state (something you should always check) and returns NULL;

We have an argument (a form), and we now need to evaluate it (remember, lisp is lazy, arguments are evaluated only when needed, so your functions must choose when and how to manage arguments).

```c
ulver_object *funny_lisp_function(ulver_env *env, ulver_form *argv) {
        if (!argv) return ulver_error("funny needs an argument");
        ulver_object *string1 = ulver_eval(env, argv);
        // we always need to check for eval return value
        if (!string1) return NULL;
        if (string1->type != ULVER_STRING) return ulver_error_form(env, argv, "is not a string");
        ...
}
```

the previous code should be quite self-explanatory, the only new thing is the ulver_error_form() commodity function, that is like ulver_error() but adds the body of the form generating the error. (it is only for giving users a more handy error report, you are free to set the error message to whatever you want)

Now we have our argument object, our purpose is generating a new one with the same body of the argument, and then we uppercase each char:

```c
ulver_object *funny_lisp_function(ulver_env *env, ulver_form *argv) {
        if (!argv) return ulver_error("funny needs an argument");
        ulver_object *string1 = ulver_eval(env, argv);
        // we always need to check for eval return value
        if (!string1) return NULL;
        if (string1->type != ULVER_STRING) return ulver_error_form(env, argv, "is not a string");
        // generate a new string object with the same content of string1
        ulver_object *new_string = ulver_object_from_string(env, string1->str, string1->len);
        // now iterate each char, and uppercase it
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

What about passing multiple args ? To understand it we need to explore how ulver_form and ulver_object are structured (in respect of list and atoms).

Both structures have the ->next and ->list fields. The ->next field is a pointer to the following element while ->list identifies that the current one is a list and ->list value is a pointer to the first element of the list.

Confused ? Let's make an example:

```c
//a form
"(list 1 2 3 (list 4 5))"
```

once we eval_parse() the previous string we get a ulver_form structure like this (pseudo-code):

```
        form => the_whole_form ['(list 1 2 3 (list 4 5))']
        
        form->list => the_function_name ['list']
        form->next => NULL
        
        the_function_name->list => NULL
        the_function_name->next => the_number_1 ['1']
        
        the_number_1->list => NULL
        the_number_1->next => the_number_2 ['2']
        
        the_number_2->list => NULL
        the_number_2->next => the_number_3 ['3']
        
        the_number_3->list => NULL
        the_number_3->next => a_new_list ['(list 4 5)']
        
        
        a_new_list->list => the_number_4 ['4']
        a_new_list->next => NULL
        
        the_number_4->list => NULL
        the_number_4->next => the_number_5 ['5']
       
        the_number_5->list => NULL
        the_number_5->next => NULL
        
```

Back to the "funny" function, the form we get it is something like that:

```
        form => the_whole_form ['(funny "helloworld")']
        form->list => the_function_name ['funny']
        form->next => NULL;
        
        the_function_name->list => NULL
        the_function_name->next => the_string_arg ['"helloworld"']
        
        the_string_arg->list => NULL
        the_string_arg->next => NULL
```

The C function gets the first item after the function name (this simplify functions development). In the "funny" example the *argv pointer maps directly to "the_string_arg" seen before

Still confused ?

Let's make a new example where "funny" takes three arguments

```c
ulver_object *funny_lisp_function(ulver_env *env, ulver_form *argv) {
        if (!argv || !argv->next || !argv->next->next) return ulver_error("funny needs three arguments");
        
        ulver_object *string1 = ulver_eval(env, argv);
        // we always need to check for eval return value
        if (!string1) return NULL;
        if (string1->type != ULVER_STRING) return ulver_error_form(env, argv, "is not a string");
        
        ulver_object *string2 = ulver_eval(env, argv->next);
        // we always need to check for eval return value
        if (!string2) return NULL;
        if (string2->type != ULVER_STRING) return ulver_error_form(env, argv->next, "is not a string");
        
        ulver_object *string3 = ulver_eval(env, argv->next->next);
        // we always need to check for eval return value
        if (!string3) return NULL;
        if (string3->type != ULVER_STRING) return ulver_error_form(env, argv->next->next, "is not a string");
        ...
}
```

But more important, this approach to arguments passing allows you to implement functions taking an arbitrary number of arguments, pretty easily:

```c
// this function prints the "fun!" string on the stdout for every evaluated argument (yeah pretty useless)
ulver_object *funny_printer_function(ulver_env *env, ulver_form *argv) {
        while(argv) {
                ulver_object *new_one = ulver_eval(env, argv);
                // raise error on failed eval
                if (!new_one) return NULL;
                printf("fun!\n");
                argv = argv->next;
        }
        return env->nil;
}
```

In the same way, your function could take a list as argument:


```c
// this function prints the "fun!" string on the stdout for every element of the passed list (again pretty useless)
ulver_object *funny_printer_function(ulver_env *env, ulver_form *argv) {
        if (!argv) return ulver_error("funny_printer requires an argument");
        // we evaluate the first argument (remember: evaluation traverse the whole form)
        ulver_object *arg1 = ulver_eval(env, argv);
        if (!arg1) return NULL;
        if (arg1->type != ULVER_LIST) return ulver_error_form(env, argv, "is not a list");
        ulver_object *item = arg1->list;
        while(item) {
                printf("fun!\n");
                item = item->next;
        }
        return env->nil;
}
```

As you can see, you get the first element of the list via ->list (warning, it could be NULL in an empty list), and then you gets its siblings via the ->next attribute.

With this concept you are already able to implement the hystoric 'car' function:

```c
ulver_object *the_car_function(ulver_env *env, ulver_form *argv) {
        if (!argv) return ulver_error("car requires an argument");
        // evaluate the first argument
        ulver_object *arg1 = ulver_eval(env, argv);
        // check the argument is a list
        if (arg1->type != ULVER_LIST) return ulver_error_form(env, argv, "is not a list");
        // if it is an empty list, return nil
        if (!arg1->list) return env->nil;
        // otherwise return the first item
        return arg1->list;
}

ulver_register_fun(env, "car", the_car_function)
```

A note about env->nil, it is part of a series of global objects created on interpreter initialization. It is mapped to the "nil" object, as well as env->t is mapped to the "T" (true) object.

Memory management
=================

When you have finished with an intepreter you can destroy it with 

```c
uint64_t ulver_destroy(ulver_env *);
```

the function returns the amount of leaked memory (in bytes, MUST BE 0 !!!).

This function internally calls (in addition to other steps)

```c
void ulver_gc(ulver_env *);
```

that invokes the mark & sweep garbage collector.

The gc is automatically called when env->max_memory value (default to 30MB) is reached or env->gc_freq (default to 1000) functions have been called. In addition to this you can manually invoke it from your code with the (gc) function.

When the gc is invoked beacause the memory limit is reached, and after its work the memory is still higher, the interpreter will trigger an error. So if you need more than 30 megabytes of memory for your app, consider increasing it:

```c
ulver_env *env = ulver_init();
// set memory limit to 64MB
env->max_memory = 64 * 1024 * 1024;
...
```

Extending with shared libraries
===============================

The (load) function is able to load features from shared libraries (in addition to lisp source files). When the argument ends with .so, .dylib or .dll the specified library is opened and the 'libraryname_init' symbol is searched and executed.

Let's write a simple extension shared library

```c
#include <ulver.h>

int black_init(ulver_env *env) {
        ulver_symbol_set(env, "*black*", 7, ulver_object_from_string(env, "metal", 5));
        return 0;
}
```

and build it

```sh
# gcc
gcc -shared -fPIC -I. -o black.so black.c
# clang (OSX)
gcc -I. -undefined dynamic_lookup -shared -fPIC -o black.dylib black.c
# windows (mingw)
gcc -shared -I. -o black.dll black.c -L. -lulver
```

(change -I value if you are not running from the ulver sources directory, on windows you do not have lazy symbol resolving for libraries, so you have to link extensions with ulver.dll, change -L accordingly)

The library entrypoint (black_init in our case) has to return 0 on success. Every other value will be treated as error.

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
