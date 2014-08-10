#include <ulver_funcs.h>

ulver_object *ulver_fun_serialize(ulver_env *env, ulver_form *argv) {
	ulver_msgpack *um = ulver_form_serialize(env, argv, NULL);
	ulver_object *us = ulver_object_from_string(env, um->base, um->pos);
	env->free(env, um->base, um->len);
	env->free(env, um, sizeof(ulver_msgpack));
	return us;
}

ulver_object *ulver_fun_deserialize(ulver_env *env, ulver_form *argv) {
	if (!argv) return ulver_error(env, ULVER_ERR_ONE_ARG);
	ulver_object *msgpack = ulver_eval(env, argv);
	if (!msgpack) return NULL;
	if (msgpack->type != ULVER_STRING) return ulver_error(env, ULVER_ERR_NOT_STRING);
	char *str = msgpack->str;
	uint64_t len = msgpack->len;
	ulver_form *uf = ulver_form_deserialize(env, NULL, &str, &len);
	if (!uf) return NULL;
	ulver_object *uo = ulver_object_new(env, ULVER_FORM);
	uo->form = uf;
	return uo;
}

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
                if (stream->type != ULVER_STREAM) return ulver_error(env, ULVER_ERR_NOT_STREAM);
        }
	//if (stream->closed) return ulver_error(env, "stream is closed");
        uint64_t slen = 0;
        char *buf = NULL;//ulver_utils_readline_from_fd(stream->fd, &slen);
        //if (!buf) return ulver_error(env, "error reading from fd %d", stream->fd);
        ulver_form *uf = ulver_parse(env, buf, slen);
        //free(buf);
	if (!uf) return ulver_error(env, ULVER_ERR_PARSE);
        ulver_object *form = ulver_object_new(env, ULVER_FORM);
        form->form = uf;
       	return form;
}


static ulver_object *call_do(ulver_env *env, ulver_object *u_func, ulver_form *uf) {
	ulver_thread *ut = ulver_current_thread(env);

	// add a new stack frame
        ulver_stack_push(env, ut, ut->current_coro);

	// set caller and args
	ut->current_coro->stack->caller = u_func;
	ut->current_coro->stack->argv = uf;

	// call the function
        ulver_object *ret = u_func->func(env, uf);

	ulver_stack_pop(env, ut, ut->current_coro);

	// this ensure the returned value is not garbage collected
	ut->current_coro->stack->ret = ret;

        return ret;
}

ulver_object *ulver_fun_funcall(ulver_env *env, ulver_form *argv) {
        if (!argv) return ulver_error(env, ULVER_ERR_ONE_ARG);
        ulver_object *u_func = ulver_eval(env, argv);
        if (!u_func) return NULL;
        if (u_func->type != ULVER_FUNC) return ulver_error(env, ULVER_ERR_NOT_FUNC);

        return call_do(env, u_func, argv->next);
}

ulver_object *ulver_fun_eval(ulver_env *env, ulver_form *argv) {
        if (!argv) return ulver_error(env, ULVER_ERR_ONE_ARG);
	ulver_object *uo = ulver_eval(env, argv);
	if (!uo) return NULL;
	if (uo->type == ULVER_FORM || uo->type == ULVER_LIST) {
        	return ulver_eval(env, uo->form);
	}
	return ulver_error(env, ULVER_ERR_NOT_FORM);
}

ulver_object *ulver_fun_quote(ulver_env *env, ulver_form *argv) {
	if (!argv) return ulver_error(env, ULVER_ERR_ONE_ARG);
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
	if (!argv || !argv->next) return ulver_error(env, ULVER_ERR_TWO_ARG);
	ulver_object *uo1 = ulver_eval(env, argv);
	if (!uo1) return NULL;
	ulver_object *uo2 = ulver_eval(env, argv->next);
	if (!uo2) return NULL;
	if (ulver_utils_eq(uo1, uo2)) return env->t;
	return env->nil;
}

ulver_object *ulver_fun_in_package(ulver_env *env, ulver_form *argv) {
	if (!argv) return ulver_error(env, ULVER_ERR_ONE_ARG);
	ulver_object *package_name = ulver_eval(env, argv);
        if (!package_name || (package_name->type != ULVER_STRING && package_name->type != ULVER_KEYWORD))
                return ulver_error(env, ULVER_ERR_NOT_KEYSTRING);

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
		return ulver_error(env, ULVER_ERR_PKG_NOTFOUND);
	}
	uv_rwlock_wrlock(&env->packages_lock);
	env->current_package = package->value;
	uv_rwlock_wrunlock(&env->packages_lock);
	ulver_symbol_set(env, "*package*", 9, env->current_package);
	return package->value;
}

ulver_object *ulver_fun_defpackage(ulver_env *env, ulver_form *argv) {
        if (!argv) return ulver_error(env, ULVER_ERR_ONE_ARG);

	ulver_object *package_name = ulver_eval(env, argv);
	if (!package_name || (package_name->type != ULVER_STRING && package_name->type != ULVER_KEYWORD))
		return ulver_error(env, ULVER_ERR_NOT_KEYSTRING);

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
	if (!argv) return ulver_error(env, ULVER_ERR_ONE_ARG);
	ulver_object *uo = ulver_eval(env, argv);
	if (!uo) return NULL;
	if (uo->type != ULVER_STRING) return ulver_error(env, ULVER_ERR_NOT_STRING); 
	char *filename = ulver_utils_strndup(env, uo->str, uo->len);
	ulver_object *ret = ulver_load(env, filename);
	env->free(env, filename, uo->len+1);
	return ret;
}

ulver_object *ulver_fun_exit(ulver_env *env, ulver_form *argv) {
	exit(ulver_destroy(env));
}

// TODO fix it !!!
ulver_object *ulver_fun_error(ulver_env *env, ulver_form *argv) {
	if (!argv) return ulver_error(env, ULVER_ERR_ONE_ARG);
	ulver_object *us = ulver_eval(env, argv);
	if (!us) return NULL;
	if (us->type != ULVER_STRING) return ulver_error_form(env, ULVER_ERR_NOT_STRING, argv, NULL); 
	return ulver_error_form(env, ULVER_ERR_CUSTOM, NULL, us->str);
}

ulver_object *ulver_fun_unintern(ulver_env *env, ulver_form *argv) {
	if (!argv) return ulver_error(env, ULVER_ERR_ONE_ARG);
	if (argv->type != ULVER_SYMBOL) return ulver_error(env, ULVER_ERR_NOT_SYMBOL);
	if (!ulver_symbol_delete(env, argv->value, argv->len)) {
		return env->t;
	}
        return env->nil;
}

ulver_object *ulver_fun_function(ulver_env *env, ulver_form *argv) {
        if (!argv) return ulver_error(env, ULVER_ERR_ONE_ARG);
        if (argv->type != ULVER_SYMBOL) return ulver_error(env, ULVER_ERR_NOT_SYMBOL);

	uv_rwlock_rdlock(&env->funcs_lock);
	ulver_symbol *func_symbol = ulver_symbolmap_get(env, env->funcs, argv->value, argv->len, 0);
	uv_rwlock_rdunlock(&env->funcs_lock);

        if (!func_symbol) return ulver_error(env, ULVER_ERR_UNK_FUNC);
	if (!func_symbol->value) return ulver_error(env, ULVER_ERR_UNK_FUNC);
	if (func_symbol->value->type != ULVER_FUNC) return ulver_error(env, ULVER_ERR_NOT_FUNC);
        return func_symbol->value;
}

ulver_object *ulver_fun_gc(ulver_env *env, ulver_form *argv) {
	ulver_thread *ut = ulver_current_thread(env);
	ut->current_coro->trigger_gc = 1;
	return ulver_object_from_num(env, env->mem);
}

ulver_object *ulver_fun_call_with_lambda_list(ulver_env *env, ulver_form *argv) {
	ulver_thread *ut = ulver_current_thread(env);
	ulver_form *uf = ut->current_coro->stack->caller->lambda_list;
	ulver_form *progn = ut->current_coro->stack->caller->form;

	if (!ut->current_coro->stack->caller->no_lambda) {
		while(uf) {
			if (!argv) {
				return ulver_error_form(env, ULVER_ERR_LAMBDA, uf, "missing argument for lambda_list");
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
	if (!argv || !argv->next) return ulver_error(env, ULVER_ERR_TWO_ARG);	
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
        if (!argv || !argv->next) return ulver_error(env, ULVER_ERR_TWO_ARG);
	ulver_object *uo = ulver_object_new(env, ULVER_FUNC);
        uo->func = ulver_fun_call_with_lambda_list;
        uo->lambda_list = argv->list;
        uo->form = argv->next;
        return uo;
}

ulver_object *ulver_fun_car(ulver_env *env, ulver_form *argv) {
	if (!argv) return ulver_error(env, ULVER_ERR_ONE_ARG);
	ulver_object *uo = ulver_eval(env, argv);
	if (!uo->list) {
		return env->nil;
	}
	return uo->list->o;
}

ulver_object *ulver_fun_atom(ulver_env *env, ulver_form *argv) {
	if (!argv) return ulver_error(env, ULVER_ERR_ONE_ARG);
	ulver_object *uo = ulver_eval(env, argv);
        if (!uo->list) {
                return env->t;
        }
        return env->nil;
}

ulver_object *ulver_fun_cdr(ulver_env *env, ulver_form *argv) {
	if (!argv) return ulver_error(env, ULVER_ERR_ONE_ARG);
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
	if (!argv || !argv->next) return ulver_error(env, ULVER_ERR_TWO_ARG);
        ulver_object *cons = ulver_object_new(env, ULVER_LIST);
        ulver_object_push(env, cons, ulver_eval(env, argv));
        ulver_object_push(env, cons, ulver_eval(env, argv->next));
        return cons;
}

ulver_object *ulver_fun_list(ulver_env *env, ulver_form *argv) {
	if (!argv) return ulver_error(env, ULVER_ERR_ONE_ARG);
	ulver_object *list = ulver_object_new(env, ULVER_LIST);
	while(argv) {
		ulver_object *uo = ulver_eval(env, argv);
		if (!uo) return NULL;
		ulver_object_push(env, list, uo);
		argv = argv->next;
	}
	return list;
}

// TODO check for symbol
ulver_object *ulver_fun_let(ulver_env *env, ulver_form *argv) {
	if (!argv) return ulver_error(env, ULVER_ERR_ONE_ARG);
	ulver_form *vars = argv->list;
	while(vars) {
		ulver_form *var = vars->list;	
		if (!var) return ulver_error_form(env, ULVER_ERR_LAMBDA, argv, "not usable as local var");
		if (!var->next) return ulver_error_form(env, ULVER_ERR_LAMBDA, argv, "not usable as local var");
		ulver_object *value = ulver_eval(env, var->next);
		if (!value) return NULL;
		ulver_symbol_set(env, var->value, var->len, value);
		vars = vars->next;
	}
	if (!argv->next) return env->nil;
        return ulver_eval(env, argv->next);
}

ulver_object *ulver_fun_setq(ulver_env *env, ulver_form *argv) {
	if (!argv || !argv->next) return ulver_error(env, ULVER_ERR_TWO_ARG);
	if (argv->type != ULVER_SYMBOL) return ulver_error(env, ULVER_ERR_NOT_SYMBOL);

	ulver_object *uo = NULL;
	while(argv && argv->next) {
		uo = ulver_eval(env, argv->next);
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
		argv = argv->next->next;
	}
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
	if (!argv) return ulver_error(env, ULVER_ERR_ONE_ARG);
	ulver_object *stream = ulver_eval(env, argv);
        if (!stream) return NULL;
        if (stream->type != ULVER_STREAM) return ulver_error(env, ULVER_ERR_NOT_STREAM);
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
		if (stream->type != ULVER_STREAM) return ulver_error(env, ULVER_ERR_NOT_STREAM);
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
	if (!argv) return ulver_error(env, ULVER_ERR_ONE_ARG);
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
	if (!argv) return ulver_error(env, ULVER_ERR_ONE_ARG);
	ulver_object *uo = ulver_eval(env, argv);
	if (!uo) return NULL;
	if (uo->type != ULVER_STRING) return ulver_error(env, ULVER_ERR_NOT_STRING);
	if (!ulver_utils_is_a_number(uo->str, uo->len)) return ulver_error(env, ULVER_ERR_NOT_NUM);
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
		return ulver_error(env, ULVER_ERR_UNBOUND); 
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
	if (!func || func->type != ULVER_FUNC) return ulver_error(env, ULVER_ERR_NOT_FUNC);
	return call_do(env, func, NULL);
}

ulver_object *ulver_call(ulver_env *env, ulver_form *uf) {
	if (!uf) return env->nil;
	// is it a remote call ?
	char *at = memchr(uf->value, '@', uf->len);
	if (at) {
		printf("remote_call: %.*s -> %.*s\n", at - uf->value, uf->value, uf->len - (at - uf->value),  at+1);
		// does the node exists ?
		// symbolmap_get @server
		// prepare the blob to send
		ulver_msgpack *um = ulver_form_serialize(env, uf->parent, NULL);
		// connect to the node and switch to the hub
		//uv_connect
		// send the blob, switch to hub and free its memory
		// uv_write
        	env->free(env, um->base, um->len);
        	env->free(env, um, sizeof(ulver_msgpack));	
		// wait for the response (it will be an object)
		// uv_read
		// close the connection
		// deserialize
		//ulver_object *ret = ulver_object_deserialize();
		// return
		//return ret;
		return NULL;
	}
	ulver_symbol *func_symbol = ulver_symbolmap_get(env, env->funcs, uf->value, uf->len, 0);
	if (!func_symbol) return ulver_error_form(env, ULVER_ERR_UNK_FUNC, uf, NULL);
	ulver_object *func = func_symbol->value;
	if (!func || func->type != ULVER_FUNC) return ulver_error(env, ULVER_ERR_NOT_FUNC);

	return call_do(env, func, uf->next);
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

	if (ut->error) {
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
			return ulver_error_form(env, ULVER_ERR_IO, NULL, "unable to load library");
		}
		int (*module_init)(ulver_env *) = (int (*)(ulver_env *)) GetProcAddress(module, entry_point);
#else
		void *module = dlopen(filename, RTLD_NOW | RTLD_GLOBAL);
		if (!module) {
			env->free(env, entry_point, strlen(entry_point)+1);
			return ulver_error_form(env, ULVER_ERR_IO, NULL, "unable to load library");
		}
		int (*module_init)(ulver_env *) = (int (*)(ulver_env *)) dlsym(module, entry_point);
#endif
		env->free(env, entry_point, strlen(entry_point)+1);
		if (!module_init) {
			return ulver_error_form(env, ULVER_ERR_IO, NULL, "is not a ulver library");
		}

		if (module_init(env)) {
			return ulver_error_form(env, ULVER_ERR_IO, NULL, "unable to initialize library");
		}
		return env->t;
	}
	
	
	int fd = open(filename, O_RDONLY);
        if (fd < 0) {
		return ulver_error_form(env, ULVER_ERR_IO, NULL, NULL);
        }

        struct stat st;
        if (fstat(fd, &st)) {
		close(fd);
		return ulver_error_form(env, ULVER_ERR_IO, NULL, NULL);
        }

	// we use low-level emory allocation, as it will be freed soon
        char *buf = env->alloc(env, st.st_size);

        ssize_t rlen = read(fd, buf, st.st_size);
	close(fd);
        if (rlen != st.st_size) {
		env->free(env, buf, st.st_size);
		return ulver_error_form(env, ULVER_ERR_IO, NULL, "unable to read() file");
        }

	// check for shebang
	char *checked_buf = buf;
	if (rlen > 2) {
		if (buf[0] == '#' && buf[1] == '!') {
			checked_buf++;
			rlen--;
			while(rlen > 0) {
				checked_buf++;
				rlen--;
				if (*checked_buf == '\n') break;
			}
		}
	}
	if (rlen == 0) return NULL;

        ulver_form *uf = ulver_parse(env, checked_buf, rlen);
	env->free(env, buf, st.st_size);
	// set the filename for better debugging
	if (uf) {
		uf->source->filename = ulver_utils_strndup(env, filename, strlen(filename));
		uf->source->filename_len = strlen(filename);
	}
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
		threads->used = 0;
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
		if (source->filename) {
			env->free(env, source->filename, source->filename_len+1);
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

	// fill the errors table
	ulver_err_table_fill(env);

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
        ulver_register_fun(env, "sin", ulver_fun_sin);
        ulver_register_fun(env, "cos", ulver_fun_cos);
        ulver_register_fun(env, "1+", ulver_fun_1add);
        ulver_register_fun(env, "1-", ulver_fun_1sub);
        ulver_register_fun(env, "max", ulver_fun_max);
        ulver_register_fun(env, "min", ulver_fun_min);

        ulver_register_fun(env, "print", ulver_fun_print);
        ulver_register_fun(env, "parse-integer", ulver_fun_parse_integer);
        ulver_register_fun(env, "setq", ulver_fun_setq);
        ulver_register_fun(env, "progn", ulver_fun_progn);
        ulver_register_fun(env, "read-line", ulver_fun_read_line);

        ulver_register_fun(env, "if", ulver_fun_if);
        ulver_register_fun(env, "cond", ulver_fun_cond);
        ulver_register_fun(env, "and", ulver_fun_and);
        ulver_register_fun(env, "or", ulver_fun_or);
        ulver_register_fun(env, "not", ulver_fun_not);

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

        ulver_register_fun(env, "last", ulver_fun_last);
        ulver_register_fun(env, "union", ulver_fun_union);

        ulver_register_fun(env, "quote", ulver_fun_quote);
        ulver_register_fun(env, "eval", ulver_fun_eval);

        ulver_register_fun(env, "lambda", ulver_fun_lambda);
        ulver_register_fun(env, "funcall", ulver_fun_funcall);

        ulver_register_fun(env, "find", ulver_fun_find);
        ulver_register_fun(env, "position", ulver_fun_position);
        ulver_register_fun(env, "elt", ulver_fun_elt);

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
        ulver_register_fun(env, "string-split", ulver_fun_string_split);

        ulver_register_package_fun(env, ulver_ns, "hub", ulver_fun_hub);
        ulver_register_package_fun(env, ulver_ns, "serialize", ulver_fun_serialize);
        ulver_register_package_fun(env, ulver_ns, "deserialize", ulver_fun_deserialize);

        ulver_register_package_fun(env, ulver_ns, "cluster", ulver_fun_cluster);
        ulver_register_package_fun(env, ulver_ns, "defnode", ulver_fun_defnode);

        return env;
}

