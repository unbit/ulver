ulver
=====

Unbit Lisp Version

This is an almost didactical ANSI C implementation of a Common LISP interpreter.

It is built for being easily embeddable in C/C++ applications

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
        ulver_env *env = ulver_init()
        ulver_form *root_form = ulver_parse(env, "(print 17)", 11);
        if (!root_form) {
                printf("unable to parse your lisp code !!!\n");
                exit(1);
        }
        ulver_eval(env, root_form);
}
```


