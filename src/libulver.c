#include <ulver_funcs.h>

ulver_object *ulver_fun_return(ulver_env *env, ulver_form *argv) {
	if (argv) {
		ulver_object *ret = ulver_eval(env, argv);
		ret->_return = 1;
		return ret;
	}
	ulver_object *ret = ulver_object_new(env, ULVER_LIST);	
	ret->_return = 1;
	return ret;
}

ulver_object *ulver_fun_loop(ulver_env *env, ulver_form *argv) {
	for(;;) {
		ulver_form *arg = argv;
		while(arg) {
			ulver_object *ret = ulver_eval(env, arg);
			if (!ret) return NULL;
			if (ret->_return) return ret;
			arg = arg->next;
		}
	}
        return NULL;
}

static ulver_object_item *new_list_item(ulver_env *, ulver_object *);
ulver_object *ulver_fun_values(ulver_env *env, ulver_form *argv) {
	if (!argv) return env->nil;
	ulver_thread *ut = ulver_current_thread(env);
	ulver_object *multivalue = ulver_object_new(env, ULVER_MULTIVALUE);
	while(argv) {
		ulver_object *ret = ulver_eval(env, argv);
		if (!ret) return NULL;
		ulver_object_push(env, multivalue, ret);
		argv = argv->next;
	}
	return multivalue;
}

ulver_object *ulver_fun_read(ulver_env *env, ulver_form *argv) {
	ulver_object *stream = env->_stdin;
        if (argv) {
                stream = ulver_eval(env, argv);
                if (!stream) return NULL;
                if (stream->type != ULVER_STREAM) return ulver_error_form(env, argv, "is not a stream");
        }
	//if (stream->closed) return ulver_error(env, "stream is closed");
        uint64_t slen = 0;
        char *buf = NULL;//ulver_utils_readline_from_fd(stream->fd, &slen);
        //if (!buf) return ulver_error(env, "error reading from fd %d", stream->fd);
        ulver_form *uf = ulver_parse(env, buf, slen);
        //free(buf);
	if (!uf) return ulver_error(env, "unable to parse");
        ulver_object *form = ulver_object_new(env, ULVER_FORM);
        form->form = uf;
       	return form;
}


ulver_object *ulver_fun_find(ulver_env *env, ulver_form *argv) {
	if (!argv || !argv->next) return ulver_error(env, "find requires two arguments");	
	ulver_object *item = ulver_eval(env, argv);
	if (!item) return NULL;
	ulver_object *sequence = ulver_eval(env, argv->next);
	if (!sequence) return NULL;
	if (sequence->type != ULVER_LIST) return ulver_error_form(env, argv->next, "is not a list");

	ulver_object_item *found = sequence->list;
	while(found) {
		if (ulver_utils_eq(found->o, item)) return found->o;
		found = found->next;
	}	

	return env->nil;
}

ulver_object *ulver_fun_position(ulver_env *env, ulver_form *argv) {
        if (!argv || !argv->next) return ulver_error(env, "position requires two arguments");
        ulver_object *item = ulver_eval(env, argv);
        if (!item) return NULL;
        ulver_object *sequence = ulver_eval(env, argv->next);
        if (!sequence) return NULL;
        if (sequence->type != ULVER_LIST) return ulver_error_form(env, argv->next, "is not a list");

	uint64_t count = 0;
        ulver_object_item *found = sequence->list;
        while(found) {
                if (ulver_utils_eq(found->o, item)) return ulver_object_from_num(env, count);
		count++;
                found = found->next;
        }
        return env->nil;
}

static ulver_object *call_do(ulver_env *env, ulver_object *u_func, ulver_form *uf) {
	ulver_thread *ut = ulver_current_thread(env);

	ut->current_coro->caller = u_func;
	// add a new stack frame
        ulver_stack_push(env, ut, ut->current_coro);
	// call the function
        ulver_object *ret = u_func->func(env, uf);

	ulver_stack_pop(env, ut, ut->current_coro);

	// this ensure the returned value is not garbage collected
	ut->current_coro->stack->ret = ret;

        return ret;
}

ulver_object *ulver_fun_funcall(ulver_env *env, ulver_form *argv) {
        if (!argv) return ulver_error(env, "funcall requires an argument");
        ulver_object *u_func = ulver_eval(env, argv);
        if (!u_func) return NULL;
        if (u_func->type != ULVER_FUNC) return ulver_error_form(env, argv, "is not a function");

        return call_do(env, u_func, argv->next);
}

ulver_object *ulver_fun_eval(ulver_env *env, ulver_form *argv) {
        if (!argv) return ulver_error(env, "eval requires an argument");
	ulver_object *uo = ulver_eval(env, argv);
	if (!uo) return NULL;
	if (uo->type == ULVER_FORM || uo->type == ULVER_LIST) {
        	return ulver_eval(env, uo->form);
	}
	return ulver_error(env, "eval requires a form as argument");
}

ulver_object *ulver_fun_quote(ulver_env *env, ulver_form *argv) {
	if (!argv) return ulver_error(env, "quote requires an argument");
	// generate the form object (could be a list)
	ulver_object *uo = NULL;
	if (argv->type == ULVER_LIST) {
		uo = ulver_object_new(env, ULVER_LIST);
		uo->form = argv;
		ulver_form *item = argv->list;
		while(item) {
			ulver_object *form_object = ulver_object_new(env, ULVER_FORM);
			form_object->form = item;
			ulver_object_push(env, uo, form_object);
			item = item->next;
		}
	}
	else {
		uo = ulver_object_new(env, ULVER_FORM);
		uo->form = argv;
	}
	return uo;
}

ulver_object *ulver_fun_eq(ulver_env *env, ulver_form *argv) {
	if (!argv || !argv->next) return ulver_error(env, "eq requires two arguments");
	ulver_object *uo1 = ulver_eval(env, argv);
	if (!uo1) return NULL;
	ulver_object *uo2 = ulver_eval(env, argv->next);
	if (!uo2) return NULL;
	if (ulver_utils_eq(uo1, uo2)) return env->t;
	return env->nil;
}

ulver_object *ulver_fun_in_package(ulver_env *env, ulver_form *argv) {
	if (!argv) return ulver_error(env, "in-package requires an argument");
	ulver_object *package_name = ulver_eval(env, argv);
        if (!package_name || (package_name->type != ULVER_STRING && package_name->type != ULVER_KEYWORD))
                return ulver_error(env, "package name must be a string or a keyword");

	// if we use a keyword, remove the colon
        char *name = package_name->str;
        uint64_t len = package_name->len;
        if (package_name->type == ULVER_KEYWORD) {
                name++; len--;
        }

	uv_rwlock_rdlock(&env->packages_lock);
	ulver_symbol *package = ulver_symbolmap_get(env, env->packages, name, len, 1);
	uv_rwlock_rdunlock(&env->packages_lock);
	if (!package) {
		return ulver_error(env, "unable to find package %.*s", len, name);
	}
	uv_rwlock_wrlock(&env->packages_lock);
	env->current_package = package->value;
	uv_rwlock_wrunlock(&env->packages_lock);
	ulver_symbol_set(env, "*package*", 9, env->current_package);
	return package->value;
}

ulver_object *ulver_fun_defpackage(ulver_env *env, ulver_form *argv) {
        if (!argv) return ulver_error(env, "defpackage requires an argument");

	ulver_object *package_name = ulver_eval(env, argv);
	if (!package_name || (package_name->type != ULVER_STRING && package_name->type != ULVER_KEYWORD))
		return ulver_error(env, "package name must be a string or a keyword");

	// if we use a keyword, remove the colon
	char *name = package_name->str;
	uint64_t len = package_name->len;
	if (package_name->type == ULVER_KEYWORD) {
		name++; len--;
	}
	
	// create the new package
	ulver_object *package = ulver_package(env, name, len);

	// now start scanning args
	ulver_form *arg = argv->next;
	while(arg) {
		// only lists are meaningful
		if (arg->type != ULVER_LIST) goto next;
		ulver_object *uo = ulver_eval_list(env, arg);
		if (!uo) return NULL;
		// has it a key ?
		if (uo->list && uo->list->o->type == ULVER_KEYWORD) {
			// :export ?
			if (uo->list->o->len == 7 && !ulver_utils_memicmp(uo->list->o->str, ":export", 7)) {
				// now start scanning for keywords and add them
				// to the package map
				ulver_object_item *exported = uo->list->next;
				while(exported) {
					if (exported->o->type == ULVER_KEYWORD) {
						// this cannot fail
						ulver_symbolmap_set(env, package->map, exported->o->str+1, exported->o->len-1, exported->o, 1);
					}
					exported = exported->next;
				}
			}
		}
next:
		arg = arg->next;
	}

	return package;
}

ulver_object *ulver_fun_load(ulver_env *env, ulver_form *argv) {
	if (!argv) return ulver_error(env, "load requires an argument");
	ulver_object *uo = ulver_eval(env, argv);
	if (!uo) return NULL;
	if (uo->type != ULVER_STRING) return ulver_error_form(env, argv, "must be a string"); 
	char *filename = ulver_utils_strndup(env, uo->str, uo->len);
	ulver_object *ret = ulver_load(env, filename);
	env->free(env, filename, uo->len+1);
	return ret;
}

ulver_object *ulver_fun_exit(ulver_env *env, ulver_form *argv) {
	exit(ulver_destroy(env));
}

ulver_object *ulver_fun_error(ulver_env *env, ulver_form *argv) {
	if (!argv) return ulver_error(env, "error requires an argument");
	if (argv->type != ULVER_STRING) {
		return ulver_error_form(env, argv, "must be a string"); 
	}
	return ulver_error(env, argv->value, argv->len);
}

ulver_object *ulver_fun_unintern(ulver_env *env, ulver_form *argv) {
	if (!argv) return ulver_error(env, "unintern requires an argument");
	if (argv->type != ULVER_SYMBOL) return ulver_error(env, "unintern requires a symbol name as argument");
	if (!ulver_symbol_delete(env, argv->value, argv->len)) {
		return env->t;
	}
        return env->nil;
}

ulver_object *ulver_fun_function(ulver_env *env, ulver_form *argv) {
        if (!argv) return ulver_error(env, "function requires an argument");
        if (argv->type != ULVER_SYMBOL) return ulver_error(env, "function requires a symbol as argument");

	uv_rwlock_rdlock(&env->funcs_lock);
	ulver_symbol *func_symbol = ulver_symbolmap_get(env, env->funcs, argv->value, argv->len, 0);
	uv_rwlock_rdunlock(&env->funcs_lock);

        if (!func_symbol) return ulver_error_form(env, argv, "undefined function");
	if (!func_symbol->value) return ulver_error_form(env, argv, "undefined function");
	if (func_symbol->value->type != ULVER_FUNC) return ulver_error_form(env, argv, "is not a function");
        return func_symbol->value;
}

ulver_object *ulver_fun_gc(ulver_env *env, ulver_form *argv) {
	ulver_thread *ut = ulver_current_thread(env);
	ut->current_coro->trigger_gc = 1;
	return ulver_object_from_num(env, env->mem);
}

ulver_object *ulver_fun_call_with_lambda_list(ulver_env *env, ulver_form *argv) {
	// lambda and progn must be read before the lisp engine changes env->caller
	ulver_thread *ut = ulver_current_thread(env);
	ulver_form *uf = ut->current_coro->caller->lambda_list;
	ulver_form *progn = ut->current_coro->caller->form;

	if (!ut->current_coro->caller->no_lambda) {
		while(uf) {
			if (!argv) {
				return ulver_error(env, "missing argument %.*s for lambda_list", (int) uf->len, uf->value);
			}
			if (uf->len > 0 && uf->value[0] == '&') {
				//TODO manage keywords
			}
			else {
				ulver_symbol_set(env, uf->value, uf->len, ulver_eval(env, argv));
			}
			argv = argv->next;
			uf = uf->next;
		}
	}
	ulver_object *uo = NULL;
	while(progn) {
		uo = ulver_eval(env, progn);	
		progn = progn->next;
	}
	return uo;
}

ulver_object *ulver_fun_defun(ulver_env *env, ulver_form *argv) {
	if (!argv || !argv->next) return ulver_error(env, "defun requires two arguments");	
	ulver_symbol *us = NULL;
	// is the function commented ?
	if (argv->next->next && argv->next->next->type == ULVER_STRING) {
		us = ulver_register_fun2(env, argv->value, argv->len, ulver_fun_call_with_lambda_list, argv->next->list, argv->next->next->next);
	}
	else {
		us = ulver_register_fun2(env, argv->value, argv->len, ulver_fun_call_with_lambda_list, argv->next->list, argv->next->next);
	}
	return us->value;
}

ulver_object *ulver_fun_lambda(ulver_env *env, ulver_form *argv) {
        if (!argv || !argv->next) return ulver_error(env, "lambda requires two arguments");
	ulver_object *uo = ulver_object_new(env, ULVER_FUNC);
        uo->func = ulver_fun_call_with_lambda_list;
        uo->lambda_list = argv->list;
        uo->form = argv->next;
        return uo;
}

ulver_object *ulver_fun_car(ulver_env *env, ulver_form *argv) {
	if (!argv) return ulver_error(env, "car requires an argument");
	ulver_object *uo = ulver_eval(env, argv);
	if (!uo->list) {
		return env->nil;
	}
	return uo->list->o;
}

ulver_object *ulver_fun_atom(ulver_env *env, ulver_form *argv) {
	if (!argv) return ulver_error(env, "atom requires an argument");
	ulver_object *uo = ulver_eval(env, argv);
        if (!uo->list) {
                return env->t;
        }
        return env->nil;
}

ulver_object *ulver_fun_cdr(ulver_env *env, ulver_form *argv) {
	if (!argv) return ulver_error(env, "cdr requires an argument");
	ulver_object *uo = ulver_eval(env, argv);
	if (!uo->list) return env->nil;
	if (!uo->list->next) return env->nil;
	ulver_object *cdr = ulver_object_new(env, ULVER_LIST);
	ulver_object_item *item = uo->list->next;
	while(item) {
		ulver_object_push(env, cdr, ulver_object_copy(env, item->o));
		item = item->next;
	}
	return cdr;
}

ulver_object *ulver_fun_cons(ulver_env *env, ulver_form *argv) {
	if (!argv || !argv->next) return ulver_error(env, "cons requires two arguments");
        ulver_object *cons = ulver_object_new(env, ULVER_LIST);
        ulver_object_push(env, cons, ulver_eval(env, argv));
        ulver_object_push(env, cons, ulver_eval(env, argv->next));
        return cons;
}

ulver_object *ulver_fun_list(ulver_env *env, ulver_form *argv) {
	if (!argv) return ulver_error(env, "list requires an argument");
	ulver_object *list = ulver_object_new(env, ULVER_LIST);
	while(argv) {
		ulver_object *uo = ulver_eval(env, argv);
		if (!uo) return NULL;
		ulver_object_push(env, list, uo);
		argv = argv->next;
	}
	return list;
}

ulver_object *ulver_fun_if(ulver_env *env, ulver_form *argv) {
	if (!argv || !argv->next) return ulver_error(env, "if requires two arguments");
	ulver_object *uo = ulver_eval(env, argv);
	if (uo != env->nil) {
		return ulver_eval(env, argv->next);
	}
	if (argv->next->next) {
		return ulver_eval(env, argv->next->next);
	}
	else {
		return env->nil;
	}
}

ulver_object *ulver_fun_cond(ulver_env *env, ulver_form *argv) {
	while(argv) {
        	ulver_object *uo = ulver_eval(env, argv->list);
        	if (uo != env->nil) {
			if (argv->list->next) {
                		return ulver_eval(env, argv->list->next);
			}
			else {
				return uo;
			}
        	}
		argv = argv->next;
	}
        return env->nil;
}

// TODO check for symbol
ulver_object *ulver_fun_let(ulver_env *env, ulver_form *argv) {
	if (!argv) return ulver_error(env, "let requires an argument");
	ulver_form *vars = argv->list;
	while(vars) {
		ulver_form *var = vars->list;	
		if (!var) return ulver_error_form(env, vars, "not usable as local var");
		if (!var->next) return ulver_error_form(env, vars, "not usable as local var");
		ulver_object *value = ulver_eval(env, var->next);
		if (!value) return NULL;
		ulver_symbol_set(env, var->value, var->len, value);
		vars = vars->next;
	}
	if (!argv->next) return env->nil;
        return ulver_eval(env, argv->next);
}

ulver_object *ulver_fun_setq(ulver_env *env, ulver_form *argv) {
	if (!argv || !argv->next) return ulver_error(env, "setq requires two arguments");
	if (argv->type != ULVER_SYMBOL) return ulver_error_form(env, argv, "is not a symbol");
	ulver_object *uo = ulver_eval(env, argv->next);
	if (!uo) return NULL;
	// first of all check if the local variable is already defined
	ulver_thread *ut = ulver_current_thread(env);
        ulver_stackframe *stack = ut->current_coro->stack;
        while(stack) {
                ulver_symbol *us = ulver_symbolmap_get(env, stack->locals, argv->value, argv->len, 0);
                if (us) {
			us->value = uo;
			return uo;
		}
                stack = stack->prev;
        }
	uv_rwlock_wrlock(&env->globals_lock);
	ulver_symbol *us = ulver_symbolmap_set(env, env->globals, argv->value, argv->len, uo, 0);
	uv_rwlock_wrunlock(&env->globals_lock);
	if (!us) return NULL;
	return uo;
}

ulver_object *ulver_fun_progn(ulver_env *env, ulver_form *argv) {
	uint64_t i;
	ulver_object *uo = NULL;
	while(argv) {
                uo = ulver_eval(env, argv);
                argv = argv->next;
        }
        return uo;
}

ulver_object *ulver_fun_close(ulver_env *env, ulver_form *argv) {
	if (!argv) return ulver_error(env, "close requires an argument");
	ulver_object *stream = ulver_eval(env, argv);
        if (!stream) return NULL;
        if (stream->type != ULVER_STREAM) return ulver_error_form(env, argv, "is not a stream");
/*
	if (stream->closed) {
		return env->nil;
	}
*/
	//stream->closed = 1;
	return env->t;
}

ulver_object *ulver_fun_read_line(ulver_env *env, ulver_form *argv) {
	ulver_object *stream = env->_stdin;
	if (argv) {
		stream = ulver_eval(env, argv);
		if (!stream) return NULL;
		if (stream->type != ULVER_STREAM) return ulver_error_form(env, argv, "is not a stream");
	}
	//if (stream->closed) return ulver_error(env, "stream is closed");
	uint64_t slen = 0;
	char *buf = NULL;//ulver_utils_readline_from_fd(stream->fd, &slen);
	//if (!buf) return ulver_error(env, "error reading from fd %d", stream->fd);
	ulver_object *s = ulver_object_from_string(env, buf, slen);
	free(buf);
	return s;
}

ulver_object *ulver_fun_print(ulver_env *env, ulver_form *argv) {
	if (!argv) return ulver_error(env, "print requires an argument");
	ulver_object *uo = ulver_eval(env, argv);	
	if (!uo) return NULL;
	if (uo->type == ULVER_LIST) {
                printf("\n(");
		ulver_utils_print_list(env, uo);
                printf(") ");
	}
	else if (uo->type == ULVER_STRING) {
		printf("\n\"%.*s\" ", (int) uo->len, uo->str);
	}
	else if (uo->type == ULVER_KEYWORD) {
		printf("\n%.*s ", (int) uo->len, uo->str);
	}
	else if (uo->type == ULVER_FUNC) {
		if (uo->str) {
                	printf("\n#<FUNCTION %.*s ", (int) uo->len, uo->str);
                }
                else {
                	printf("\n#<FUNCTION :LAMBDA ");
                }
		if (uo->lambda_list) {
			printf("(");
                        ulver_utils_print_form(env, uo->lambda_list);
                	printf(") ");
                }
                if (uo->form) {
                	ulver_utils_print_form(env, uo->form);
                }
                else {
                	printf("<C FUNC>");
                }
                printf("> ");
	}
	else if (uo->type == ULVER_NUM) {
		printf("\n%lld ", (long long int) uo->n);
	}
	else if (uo->type == ULVER_FLOAT) {
		printf("\n%f ", uo->d);
	}
	else if (uo->type == ULVER_TRUE) {
		printf("\nT ");
	}
	else if (uo->type == ULVER_PACKAGE) {
		printf("\n#<PACKAGE %.*s> ", (int) uo->len, uo->str);
	}
	else if (uo->type == ULVER_STREAM) {
        	printf("#<STREAM %s>", uo->stream ? "-" : uo->file.path);
        }
	else if (uo->type == ULVER_THREAD) {
        	printf("#<THREAD %p>", uo->thread);
        }
	else if (uo->type == ULVER_CORO) {
        	printf("#<CORO %p>", uo->coro);
        }
	else if (uo->type == ULVER_HASHTABLE) {
        	printf("#<HASH-TABLE %p>", uo->map);
        }
	else if (uo->type == ULVER_FORM) {
		printf("\n");
		ulver_utils_print_form(env, uo->form);
		printf(" ");
	}
	else {
		printf("\n?%.*s? ", (int) uo->len, uo->str);
	}
        return uo;
}

// TODO find something better (and safer) than strtoll
ulver_object *ulver_fun_parse_integer(ulver_env *env, ulver_form *argv) {
	if (!argv) return ulver_error(env, "parse-integer requires an argument");
	ulver_object *uo = ulver_eval(env, argv);
	if (!uo) return NULL;
	if (uo->type != ULVER_STRING) return ulver_error_form(env, argv, "parse-integer expects a string");
	if (!ulver_utils_is_a_number(uo->str, uo->len)) return ulver_error_form(env, argv, "does not have integer syntax");
        return ulver_object_from_num(env, strtoll(uo->str, NULL, 10));
}

ulver_object *ulver_object_copy(ulver_env *env, ulver_object *uo) {
	ulver_object *new = ulver_object_new(env, uo->type);
	new->len = uo->len;
        new->str = uo->str;
        new->n = uo->n;
        new->d = uo->d;
	new->func = uo->func;
	new->lambda_list = uo->lambda_list;
	new->form = uo->form;
	if (uo->list) {
		ulver_object_item *item = uo->list;
		while(item) {
			ulver_object_push(env, new, ulver_object_copy(env, item->o));
			item = item->next;
		}
	}
	return new;
}

ulver_object *ulver_object_new(ulver_env *env, uint8_t type) {
	// get the current thread;
	ulver_object *uo = env->alloc(env, sizeof(ulver_object));
	uo->type = type;
	ulver_thread *ut = ulver_current_thread(env);


	// append the object to the stack-related ones
	ulver_object *latest_stack_object = ut->current_coro->stack->objects;
	ut->current_coro->stack->objects = uo;
	ut->current_coro->stack->objects->stack_next = latest_stack_object;

	uv_mutex_lock(&env->gc_root_lock);
	// put in the gc list
	if (!env->gc_root) {
		env->gc_root = uo;
		uv_mutex_unlock(&env->gc_root_lock);
		return uo;
	}

	ulver_object *current_root = env->gc_root;
	env->gc_root = uo;
	uo->gc_next = current_root;
	current_root->gc_prev = uo;

	uv_mutex_unlock(&env->gc_root_lock);

	return uo;
}

ulver_object *ulver_object_from_num(ulver_env *env, int64_t n) {
	ulver_object *uo = ulver_object_new(env, ULVER_NUM);
	uo->n = n;
	return uo;
}

ulver_object *ulver_object_from_float(ulver_env *env, double n) {
        ulver_object *uo = ulver_object_new(env, ULVER_FLOAT);
        uo->d = n;
        return uo;
}

ulver_object *ulver_object_from_string(ulver_env *env, char *value, uint64_t len) {
        ulver_object *uo = ulver_object_new(env, ULVER_STRING);
        uo->str = ulver_utils_strndup(env, value, len);
	uo->len = len;
        return uo;
}

ulver_object *ulver_object_from_keyword(ulver_env *env, char *value, uint64_t len) {
        ulver_object *uo = ulver_object_new(env, ULVER_KEYWORD);
        uo->str = ulver_utils_strndup(env, value, len);
	uo->len = len;
	ulver_utils_toupper(uo->str, uo->len);
        return uo;
}

ulver_object *ulver_package(ulver_env *env, char *value, uint64_t len) {
	ulver_object *uo = ulver_object_new(env, ULVER_PACKAGE);
        uo->str = ulver_utils_strndup(env, value, len);
        uo->len = len;
	// packages must be protected from gc
	uo->gc_protected = 1;
        ulver_utils_toupper(uo->str, uo->len);
	// this is the map of exported symbols
	uo->map = ulver_symbolmap_new(env);
	// package names do not take the current namespace into account

	uv_rwlock_wrlock(&env->packages_lock);
	ulver_symbolmap_set(env, env->packages, uo->str, uo->len, uo, 1);
	uv_rwlock_wrunlock(&env->packages_lock);
        return uo;
}


int ulver_symbol_delete(ulver_env *env, char *name, uint64_t len) {
	ulver_thread *ut = ulver_current_thread(env);
        // is it a global var ?
        if (len > 2 && name[0] == '*' && name[len-1] == '*') {
		uv_rwlock_wrlock(&env->globals_lock);
                int ret = ulver_symbolmap_delete(env, env->globals, name, len, 0);
		uv_rwlock_wrunlock(&env->globals_lock);
		return ret;
        }
        ulver_stackframe *stack = ut->current_coro->stack;
        while(stack) {
                if (!ulver_symbolmap_delete(env, stack->locals, name, len, 0)) {
			return 0;
		}
                stack = stack->prev;
        }
	uv_rwlock_wrlock(&env->globals_lock);
        int ret = ulver_symbolmap_delete(env, env->globals, name, len, 0);
	uv_rwlock_wrunlock(&env->globals_lock);
	return ret;
}

ulver_object *ulver_symbol_get(ulver_env *env, char *name, uint64_t len) {
	ulver_object *ret = NULL; 
	// is it a global var ?
	if (len > 2 && name[0] == '*' && name[len-1] == '*') {
		uv_rwlock_rdlock(&env->globals_lock);
		ulver_symbol *us = ulver_symbolmap_get(env, env->globals, name, len, 0);
		if (us) ret = us->value;
		uv_rwlock_rdunlock(&env->globals_lock);
		return ret;
	}	
	ulver_thread *ut = ulver_current_thread(env);
	ulver_stackframe *stack = ut->current_coro->stack;
	while(stack) {
		ulver_symbol *us = ulver_symbolmap_get(env, stack->locals, name, len, 0);
		if (us) return us->value;
		stack = stack->prev;
	}
	uv_rwlock_rdlock(&env->globals_lock);
	ulver_symbol *us = ulver_symbolmap_get(env, env->globals, name, len, 0);
	if (us) ret = us->value;
	uv_rwlock_rdunlock(&env->globals_lock);
	return ret;
}

ulver_object *ulver_object_from_symbol(ulver_env *env, ulver_form *uf) {
	ulver_object *uo = ulver_symbol_get(env, uf->value, uf->len);
	if (!uo) {
		return ulver_error_form(env, uf, "variable not bound"); 
	}
	return uo;
}

static ulver_object_item *new_list_item(ulver_env *env, ulver_object *uo) {
	ulver_object_item *item = env->alloc(env, sizeof(ulver_object_item));
	item->o = uo;	
	return item;
}

ulver_object *ulver_object_push(ulver_env *env, ulver_object *list, ulver_object *uo) {
	ulver_object_item *next = list->list;
	if (!next) {
		list->list = new_list_item(env, uo);
		return uo;
	}	
	while(next) {
		if (!next->next) {
			next->next = new_list_item(env, uo);
			break;
		}
		next = next->next;	
	}
	return uo;
}

ulver_object *ulver_call0(ulver_env *env, ulver_object *func) {
	if (!func || func->type != ULVER_FUNC) return ulver_error(env, "object is not a function");
	return call_do(env, func, NULL);
}

ulver_object *ulver_call(ulver_env *env, ulver_form *uf) {
	if (!uf) return env->nil;
	ulver_symbol *func_symbol = ulver_symbolmap_get(env, env->funcs, uf->value, uf->len, 0);
	if (!func_symbol) return ulver_error_form(env, uf, "undefined function");
	ulver_object *func = func_symbol->value;
	if (!func || func->type != ULVER_FUNC) return ulver_error_form(env, uf, "is not a function");

	return call_do(env, func, uf->next);
}

ulver_object *ulver_error_form(ulver_env *env, ulver_form *uf, char *msg) {
	return ulver_error(env, "[line: %llu pos: %lld] (%.*s) %s", uf->line, uf->line_pos, (int) uf->len, uf->value, msg);
}

ulver_object *ulver_error(ulver_env *env, char *fmt, ...) {
	ulver_thread *ut = ulver_current_thread(env);
	// when trying to set an error, check if a previous one is set
	if (ut->current_coro->error) {
		// if we are not setting a new error, we are attempting to free it...
		if (!fmt) {
			env->free(env, ut->current_coro->error, ut->current_coro->error_buf_len);
			ut->current_coro->error = NULL;
			ut->current_coro->error_len = 0;
			ut->current_coro->error_buf_len = 0;
		}
		return NULL;
	}
	if (!fmt) return NULL;
	uint64_t buf_size = 1024;
	char *buf = env->alloc(env, buf_size);
	va_list varg;
	va_start(varg, fmt);
	int ret = vsnprintf(buf, buf_size, fmt, varg);	
	va_end(varg);
	if (ret <= 0) goto fatal;
	if (ret+1 > buf_size) {
		int64_t new_size = ret;
		env->free(env, buf, buf_size);
		buf_size = new_size + 1;
		buf = env->alloc(env, buf_size);
		va_start(varg, fmt);
        	ret = vsnprintf(buf, buf_size, fmt, varg);
        	va_end(varg);
        	if (ret <= 0 || ret + 1 > buf_size) {
			goto fatal;
        	}
	} 
	ut->current_coro->error = buf;
	ut->current_coro->error_len = ret;
	ut->current_coro->error_buf_len = buf_size;
	return NULL;

fatal:
	env->free(env, buf, buf_size);
	ut->current_coro->error = ulver_utils_strdup(env, "FATAL ERROR");
        ut->current_coro->error_len = strlen(ut->current_coro->error);
	ut->current_coro->error_buf_len = ut->current_coro->error_len + 1;
        return NULL;
}

static ulver_object *eval_do(ulver_env *env, ulver_thread *ut, ulver_form *uf, uint8_t as_list) {
	ulver_object *ret = NULL;

	switch(uf->type) {
		// if it is a list, get the first member and eval it
		case ULVER_LIST:
			if (!as_list) {
				ret = ulver_call(env, uf->list);
				break;
			}
			ret = ulver_object_new(env, ULVER_LIST);
			ulver_form *list = uf->list;
			while(list) {
				ulver_object *uo = ulver_eval(env, list);
				if (!uo) {
					ret = NULL;
					break;
				}
				ulver_object_push(env, ret, uo);
				list = list->next;
			}
			break;
		// if it is a symbol, get its value
		case ULVER_SYMBOL:
			ret = ulver_object_from_symbol(env, uf);
			break;
		// if it is a number, build a new object
		case ULVER_NUM:
			ret = ulver_object_from_num(env, strtoll(uf->value, NULL, 10));
			break;
		// if it is a float, build a new object
		case ULVER_FLOAT:
			ret = ulver_object_from_float(env, strtod(uf->value, NULL));
			break;
		// if it is a string, build a new object
		case ULVER_STRING:
			ret = ulver_object_from_string(env, uf->value, uf->len);
			break;
		// if it is a keyword, build a new object
		case ULVER_KEYWORD:
			ret = ulver_object_from_keyword(env, uf->value, uf->len);
			break;
		default:
			printf("unknown object\n");
			break;
			
	}

	// allow the GC to run
	uv_rwlock_rdunlock(&env->gc_lock);

	if (ut->current_coro->error) {
		ret = NULL;
	}

	env->calls++;

        uint8_t run_gc = 0;

        if (ut->current_coro->trigger_gc) {
                run_gc = 1;
                ut->current_coro->trigger_gc = 0;
        }
        else {
                uv_mutex_lock(&env->mem_lock);
                // gc must be called only if the maximum amount of memory is reached
                if (env->gc_freq > 0 && env->mem > env->max_memory) {
                        if ((env->calls % env->gc_freq) == 0) {
                                run_gc = 1;
                        }
                }
                uv_mutex_unlock(&env->mem_lock);
        }

        if (run_gc) {
		// stop the world
		uv_rwlock_wrlock(&env->gc_lock);
		ulver_gc(env);
		// restart the world
		uv_rwlock_wrunlock(&env->gc_lock);
                ut->current_coro->trigger_gc = 0;
        }

end:
	uv_rwlock_rdlock(&env->gc_lock);
	return ret;
}

ulver_object *ulver_eval(ulver_env *env, ulver_form *uf) {
	return eval_do(env, ulver_current_thread(env), uf, 0);
	
}

ulver_object *ulver_eval_list(ulver_env *env, ulver_form *uf) {
	return eval_do(env, ulver_current_thread(env), uf, 1);
}

ulver_symbol *ulver_register_fun2(ulver_env *env, char *name, uint64_t len, ulver_object *(*func)(ulver_env *, ulver_form *), ulver_form *lambda_list, ulver_form *progn) {
	ulver_object *uo = ulver_object_new(env, ULVER_FUNC);
        uo->func = func;
	uo->lambda_list = lambda_list;
	uo->form = progn;
	uo->str = ulver_utils_strndup(env, name, len);
        uo->len = len;
	uo->gc_protected = 1;
        ulver_utils_toupper(uo->str, uo->len);
	uv_rwlock_wrlock(&env->funcs_lock);
        ulver_symbol *us = ulver_symbolmap_set(env, env->funcs, name, len, uo, 0);
	uv_rwlock_wrunlock(&env->funcs_lock);
	return us;
}

ulver_symbol *ulver_register_fun(ulver_env *env, char *name, ulver_object *(*func)(ulver_env *, ulver_form *)) {
	return ulver_register_fun2(env, name, strlen(name), func, NULL, NULL);
}

ulver_symbol *ulver_register_package_fun(ulver_env *env, ulver_object *package, char *name, ulver_object *(*func)(ulver_env *, ulver_form *)) {
	ulver_object *current_package = env->current_package;
	env->current_package = package;
	ulver_symbolmap_set(env, package->map, name, strlen(name), env->nil, 1);
        ulver_symbol *us = ulver_register_fun2(env, name, strlen(name), func, NULL, NULL);
	env->current_package = current_package;
	return us;
}

ulver_symbol *ulver_symbol_set(ulver_env *env, char *name, uint64_t len, ulver_object *uo) {
	ulver_thread *ut = ulver_current_thread(env);
	// is it a global var ?
	if ((len > 2 && name[0] == '*' && name[len-1] == '*') || ut->current_coro->stack->prev == NULL) {
		uv_rwlock_wrlock(&env->globals_lock);
		ulver_symbol *us = ulver_symbolmap_set(env, env->globals, name, len, uo, 0);
		uv_rwlock_wrunlock(&env->globals_lock);
		return us;
	}	
	return ulver_symbolmap_set(env, ut->current_coro->stack->locals, name, len, uo, 0);
}

ulver_object *ulver_load(ulver_env *env, char *filename) {
	char *entry_point = ulver_utils_is_library(env, filename);
	if (entry_point) {
#ifdef __WIN32__
		HINSTANCE module = LoadLibrary(TEXT(filename));
		if (!module) {
			env->free(env, entry_point, strlen(entry_point)+1);
			return ulver_error(env, "unable to load library %s: error code %d", filename, GetLastError());
		}
		int (*module_init)(ulver_env *) = (int (*)(ulver_env *)) GetProcAddress(module, entry_point);
#else
		void *module = dlopen(filename, RTLD_NOW | RTLD_GLOBAL);
		if (!module) {
			env->free(env, entry_point, strlen(entry_point)+1);
			return ulver_error(env, "unable to load library %s", dlerror());
		}
		int (*module_init)(ulver_env *) = (int (*)(ulver_env *)) dlsym(module, entry_point);
#endif
		env->free(env, entry_point, strlen(entry_point)+1);
		if (!module_init) {
			return ulver_error(env, "%s is not a ulver library", filename);
		}

		if (module_init(env)) {
			return ulver_error(env, "unable to initialize library");;
		}
		return env->t;
	}
	
	
	int fd = open(filename, O_RDONLY);
        if (fd < 0) {
		return ulver_error(env, "unable to open() file %s: %s", filename, strerror(errno));
        }

        struct stat st;
        if (fstat(fd, &st)) {
		close(fd);
		return ulver_error(env, "unable to stat() file %s: %s", filename, strerror(errno));
        }

	// we use low-level emory allocation, as it will be freed soon
        char *buf = env->alloc(env, st.st_size);

        ssize_t rlen = read(fd, buf, st.st_size);
	close(fd);
        if (rlen != st.st_size) {
		env->free(env, buf, st.st_size);
		return ulver_error(env, "unable to read() file %s: %s", filename, strerror(errno));
        }

        ulver_form *uf = ulver_parse(env, buf, rlen);
	env->free(env, buf, st.st_size);
	ulver_object *ret = NULL;
        while(uf) {
		ret = ulver_eval(env, uf);
		if (!ret) break;
                uf = uf->next;
        }
	return ret;
}

ulver_object *ulver_run(ulver_env *env, char *source) {
	ulver_form *uf = ulver_parse(env, source, strlen(source));
        ulver_object *ret = NULL;
        while(uf) {
		ret = ulver_eval(env, uf);
		if (!ret) break;
                uf = uf->next;
        }
        return ret;
}

void ulver_thread_destroy(ulver_env *, ulver_thread *, uint8_t);
uint64_t ulver_destroy(ulver_env *env) {

	if (uv_thread_self() != (unsigned long) env->creator_thread) {
		printf("[WARNING] only the creator thread can destroy a ulver environment\n");
		return env->mem;
	}

	// acquire the gc lock (destroy is a stop-the-world operation)
	uv_rwlock_rdunlock(&env->gc_lock);
	uv_rwlock_wrlock(&env->gc_lock);

	// destroy globals, functions and packages
	ulver_symbolmap_destroy(env, env->globals);
	env->globals = NULL;
	ulver_symbolmap_destroy(env, env->funcs);
	env->funcs = NULL;
	ulver_symbolmap_destroy(env, env->packages);
	env->packages = NULL;

	// mark all threads as dead
	ulver_thread *threads = env->threads;
	while(threads) {
		threads->dead = 1;
		threads = threads->next;
	}

	// unprotect and unmark objects
	ulver_object *uo = env->gc_root;
        while(uo) {
		uo->gc_protected = 0;
		uo->gc_mark = 0;
		uo = uo->gc_next;
	}

	// call the gc
	ulver_gc(env);

	// destroy sources list
	ulver_source *source = env->sources;
	while(source) {
		ulver_source *next = source->next;
		// destroy the form tree
		ulver_form *root = source->form_root;
		while(root) {
			ulver_form *next = root->next;
			ulver_form_destroy(env, root);
			root = next;
		}
		env->free(env, source->str, source->len+1);
		env->free(env, source, sizeof(ulver_source));
		source = next;
	}

	uint64_t mem = env->mem;

	uv_rwlock_wrunlock(&env->gc_lock);

	free(env);

	// return the leaked memory (MUST BE 0 !!!)
	return mem;
}

void ulver_report_error(ulver_env *env) {
	ulver_thread *ut = ulver_current_thread(env);
	if (ut->current_coro->error) {
		fprintf(env->_stderr, "\n*** ERROR: %.*s ***\n", (int) ut->current_coro->error_len, ut->current_coro->error);
                // clear error
                ulver_error(env, NULL);
	}
}

ulver_thread *ulver_current_thread(ulver_env *env) {
	ulver_thread *ut = (ulver_thread *) uv_key_get(&env->thread);
	if (ut) return ut;
	ut = env->alloc(env, sizeof(ulver_thread));
        uv_key_set(&env->thread, (void *) ut);

	ut->env = env;
	ut->t = (uv_thread_t) uv_thread_self();

	ut->main_coro = env->alloc(env, sizeof(ulver_coro));
	ut->main_coro->thread = ut;
	ut->main_coro->context = ulver_coro_alloc_context(env);

	ut->current_coro = ut->main_coro;

        ulver_stack_push(env, ut, ut->current_coro);

	uv_rwlock_wrlock(&env->threads_lock);
        ulver_thread *head = env->threads;
        if (head) head->prev = ut;
        env->threads = ut;
        ut->next = head;
        uv_rwlock_wrunlock(&env->threads_lock);

	uv_rwlock_rdlock(&env->gc_lock);

        return ut;
}

ulver_env *ulver_init() {

        ulver_env *env = calloc(1, sizeof(ulver_env));

	env->creator_thread = (uv_thread_t) uv_thread_self();
	if (uv_key_create(&env->thread)) {
		ulver_destroy(env);
		return NULL;
	}

        env->alloc = ulver_alloc;
        env->free = ulver_free;

	env->_stdout = stdout;
	env->_stderr = stderr;

	uv_mutex_init(&env->mem_lock);
	uv_mutex_init(&env->sources_lock);

	uv_rwlock_init(&env->threads_lock);
	uv_rwlock_init(&env->gc_lock);
	uv_mutex_init(&env->gc_root_lock);

	env->globals = ulver_symbolmap_new(env); 
	uv_rwlock_init(&env->globals_lock);
	env->funcs = ulver_symbolmap_new(env); 
	uv_rwlock_init(&env->funcs_lock);

        // create the packages map
        env->packages = ulver_symbolmap_new(env);
	uv_rwlock_init(&env->packages_lock);
        // create the CL-USER package (will be set as current)
        env->cl_user = ulver_package(env, "CL-USER", 7);
        env->current_package = env->cl_user;

	// the ulver special namespace
        ulver_object *ulver_ns = ulver_package(env, "ULVER", 5);

	ulver_symbol_set(env, "*package*", 9, env->cl_user);


        // the following objects must be protected from gc
        env->t = ulver_object_new(env, ULVER_TRUE);
        env->t->gc_protected = 1;
	ulver_symbol_set(env, "t", 1, env->t);

        env->nil = ulver_object_new(env, 0);
        env->nil->gc_protected = 1;
	ulver_symbol_set(env, "nil", 3, env->nil);

        //env->stdin = ulver_object_from_fd(env, 0);
        //env->stdin->gc_protected = 1;

	// 30 megs memory limit before triggering gc
	env->max_memory = 30 * 1024 * 1024;
	// invoke gc every 1000 calls
	env->gc_freq = 1000;

	// register functions
        ulver_register_fun(env, "+", ulver_fun_add);
        ulver_register_fun(env, "-", ulver_fun_sub);
        ulver_register_fun(env, "*", ulver_fun_mul);
        ulver_register_fun(env, ">", ulver_fun_higher);
        ulver_register_fun(env, "=", ulver_fun_equal);
        ulver_register_fun(env, "mod", ulver_fun_mod);
        ulver_register_fun(env, "print", ulver_fun_print);
        ulver_register_fun(env, "parse-integer", ulver_fun_parse_integer);
        ulver_register_fun(env, "setq", ulver_fun_setq);
        ulver_register_fun(env, "progn", ulver_fun_progn);
        ulver_register_fun(env, "read-line", ulver_fun_read_line);
        ulver_register_fun(env, "if", ulver_fun_if);
        ulver_register_fun(env, "cond", ulver_fun_cond);
        ulver_register_fun(env, "defun", ulver_fun_defun);
        ulver_register_fun(env, "list", ulver_fun_list);
        ulver_register_fun(env, "cons", ulver_fun_cons);
        ulver_register_fun(env, "car", ulver_fun_car);
        ulver_register_fun(env, "cdr", ulver_fun_cdr);
        ulver_register_fun(env, "atom", ulver_fun_atom);
        ulver_register_fun(env, "let", ulver_fun_let);
        ulver_register_fun(env, "gc", ulver_fun_gc);
        ulver_register_fun(env, "unintern", ulver_fun_unintern);
        ulver_register_fun(env, "error", ulver_fun_error);
        ulver_register_fun(env, "exit", ulver_fun_exit);
        ulver_register_fun(env, "load", ulver_fun_load);
        ulver_register_fun(env, "defpackage", ulver_fun_defpackage);
        ulver_register_fun(env, "in-package", ulver_fun_in_package);
        ulver_register_fun(env, "eq", ulver_fun_eq);
        ulver_register_fun(env, "getf", ulver_fun_getf);

        ulver_register_fun(env, "first", ulver_fun_first);
        ulver_register_fun(env, "second", ulver_fun_second);
        ulver_register_fun(env, "third", ulver_fun_third);
        ulver_register_fun(env, "fourth", ulver_fun_fourth);
        ulver_register_fun(env, "fifth", ulver_fun_fifth);
        ulver_register_fun(env, "sixth", ulver_fun_sixth);
        ulver_register_fun(env, "seventh", ulver_fun_seventh);
        ulver_register_fun(env, "eighth", ulver_fun_eighth);
        ulver_register_fun(env, "ninth", ulver_fun_ninth);
        ulver_register_fun(env, "tenth", ulver_fun_tenth);

        ulver_register_fun(env, "quote", ulver_fun_quote);
        ulver_register_fun(env, "eval", ulver_fun_eval);

        ulver_register_fun(env, "lambda", ulver_fun_lambda);
        ulver_register_fun(env, "funcall", ulver_fun_funcall);

        ulver_register_fun(env, "find", ulver_fun_find);
        ulver_register_fun(env, "position", ulver_fun_position);

        ulver_register_fun(env, "read-from-string", ulver_fun_read_from_string);
        ulver_register_fun(env, "subseq", ulver_fun_subseq);
        ulver_register_fun(env, "length", ulver_fun_length);

        ulver_register_fun(env, "read", ulver_fun_read);
        ulver_register_fun(env, "function", ulver_fun_function);

        ulver_register_fun(env, "values", ulver_fun_values);

        ulver_register_fun(env, "make-thread", ulver_fun_make_thread);
        ulver_register_fun(env, "join-thread", ulver_fun_join_thread);
        ulver_register_fun(env, "all-threads", ulver_fun_all_threads);

        ulver_register_fun(env, "sleep", ulver_fun_sleep);

        ulver_register_fun(env, "loop", ulver_fun_loop);

        ulver_register_fun(env, "return", ulver_fun_return);

        ulver_register_fun(env, "open", ulver_fun_open);
        ulver_register_fun(env, "close", ulver_fun_close);

        ulver_register_fun(env, "make-tcp-server", ulver_fun_make_tcp_server);

        ulver_register_fun(env, "make-coro", ulver_fun_make_coro);
        ulver_register_fun(env, "coro-switch", ulver_fun_coro_switch);
        ulver_register_fun(env, "coro-next", ulver_fun_coro_next);
        ulver_register_fun(env, "coro-yield", ulver_fun_coro_yield);

        ulver_register_fun(env, "write-string", ulver_fun_write_string);
        ulver_register_fun(env, "read-string", ulver_fun_read_string);

        ulver_register_fun(env, "make-hash-table", ulver_fun_make_hash_table);
        ulver_register_fun(env, "gethash", ulver_fun_gethash);
        ulver_register_fun(env, "sethash", ulver_fun_sethash);

        ulver_register_fun(env, "string-downcase", ulver_fun_string_downcase);
        ulver_register_fun(env, "string-upcase", ulver_fun_string_upcase);

        ulver_register_package_fun(env, ulver_ns, "hub", ulver_fun_hub);

        return env;
}

