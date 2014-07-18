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


The garbage collection system is already working (mark and sweep), and you can manually call it from lisp using the (gc) function (it returns the amount of memory used by the interpreter, in bytes).

Packages (namespaces) are implemented, but defpackage supports only the :export argument
