#include <ulver.h>

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

	ulver_symbol *package = ulver_symbolmap_get(env, env->packages, name, len, 1);
	if (!package) {
		return ulver_error(env, "unable to find package %.*s", len, name);
	}
	env->current_package = package->value;
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
		if (uo->list && uo->list->type == ULVER_KEYWORD) {
			// :export ?
			if (uo->list->len == 7 && !ulver_utils_memicmp(uo->list->str, ":export", 7)) {
				// now start scanning for keywords and add them
				// to the package map
				ulver_object *exported = uo->list->next;
				while(exported) {
					if (exported->type == ULVER_KEYWORD) {
						// this cannot fail
						ulver_symbolmap_set(env, package->map, exported->str+1, exported->len-1, env->nil, 1);
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
	char *filename = strndup(uo->str, uo->len);
	ulver_object *ret = ulver_load(env, filename);
	free(filename);
	return ret;
}

ulver_object *ulver_fun_exit(ulver_env *env, ulver_form *argv) {
	ulver_destroy(env);
	exit(0);
}

ulver_object *ulver_fun_error(ulver_env *env, ulver_form *argv) {
	if (argv->type != ULVER_STRING) {
		return ulver_error_form(env, argv, "must be a string"); 
	}
	return ulver_error(env, argv->value, argv->len);
}

ulver_object *ulver_fun_unintern(ulver_env *env, ulver_form *argv) {
	if (!ulver_symbol_delete(env, argv->value, argv->len)) {
		return env->t;
	}
        return env->nil;
}

ulver_object *ulver_fun_gc(ulver_env *env, ulver_form *argv) {
	ulver_gc(env);
	return ulver_object_from_num(env, env->mem);
}

ulver_object *ulver_fun_add(ulver_env *env, ulver_form *argv) {
        int64_t n = 0;
	double d = 0.0;
	uint8_t is_float = 0;
	while(argv) {
		ulver_object *uo = ulver_eval(env, argv);
		if (!uo) return NULL;
		if (uo->type == ULVER_NUM) {
			n += uo->n;
		}
		else if (uo->type == ULVER_FLOAT) {
			is_float = 1;
			d += uo->d;
		}
		else {
			return ulver_error_form(env, argv, "argument is not a number"); 
		}
		argv = argv->next;
	}

	if (is_float) {
        	return ulver_object_from_float(env, d+n);
	}
        return ulver_object_from_num(env, n);
	
}

ulver_object *ulver_fun_sub(ulver_env *env, ulver_form *argv) {
        int64_t n = ulver_eval(env, argv)->n;
	argv = argv->next;
        while(argv) {
                ulver_object *uo = ulver_eval(env, argv);
                n -= uo->n;
                argv = argv->next;
        }
        return ulver_object_from_num(env, n);
}


ulver_object *ulver_fun_mul(ulver_env *env, ulver_form *argv) {
        int64_t n = 1;
	while(argv) {
                ulver_object *uo = ulver_eval(env, argv);
                n *= uo->n;
                argv = argv->next;
        }
        return ulver_object_from_num(env, n);
}

ulver_object *ulver_fun_higher(ulver_env *env, ulver_form *argv) {
	ulver_object *uo0 = ulver_eval(env, argv);
	ulver_object *uo1 = ulver_eval(env, argv->next);
	int64_t n0 = uo0->n;
	int64_t n1 = uo1->n;
	return n0 > n1 ? env->t : env->nil;
}

ulver_object *ulver_fun_equal(ulver_env *env, ulver_form *argv) {
	ulver_object *uo0 = ulver_eval(env, argv);
	ulver_object *uo1 = ulver_eval(env, argv->next);
	int64_t n0 = uo0->n;
	int64_t n1 = uo1->n;
	return n0 == n1 ? env->t : env->nil;
}

ulver_object *ulver_fun_call_with_lambda_list(ulver_env *env, ulver_form *argv) {
	// lambda and progn must be read before the lisp engine changes env->caller
	ulver_form *uf = env->caller->lambda_list;
	ulver_form *progn = env->caller->progn;

	while(uf) {
		ulver_symbol_set(env, uf->value, uf->len, ulver_eval(env, argv));
		argv = argv->next;
		uf = uf->next;
	}
	ulver_object *uo = NULL;
	while(progn) {
		uo = ulver_eval(env, progn);	
		progn = progn->next;
	}
	return uo;
}

ulver_object *ulver_fun_defun(ulver_env *env, ulver_form *argv) {
	ulver_symbol *us = ulver_register_fun2(env, argv->value, argv->len, ulver_fun_call_with_lambda_list, argv->next->list, argv->next->next);
	return us->value;
}

ulver_object *ulver_fun_car(ulver_env *env, ulver_form *argv) {
	ulver_object *uo = ulver_eval(env, argv);
	if (!uo->list) {
		return env->nil;
	}
	return uo->list;
}

ulver_object *ulver_fun_atom(ulver_env *env, ulver_form *argv) {
	ulver_object *uo = ulver_eval(env, argv);
        if (!uo->list) {
                return env->t;
        }
        return env->nil;
}

ulver_object *ulver_fun_cdr(ulver_env *env, ulver_form *argv) {
	ulver_object *uo = ulver_eval(env, argv);
	if (!uo->list) return env->nil;
	if (!uo->list->next) return env->nil;
	ulver_object *cdr = ulver_object_new(env, ULVER_LIST);
	ulver_object *item = uo->list->next;
	while(item) {
		ulver_object_push(env, cdr, ulver_object_copy(env, item));
		item = item->next;
	}
	return cdr;
}

ulver_object *ulver_fun_cons(ulver_env *env, ulver_form *argv) {
        ulver_object *cons = ulver_object_new(env, ULVER_LIST);
        ulver_object_push(env, cons, ulver_eval(env, argv));
        ulver_object_push(env, cons, ulver_eval(env, argv->next));
        return cons;
}

ulver_object *ulver_fun_list(ulver_env *env, ulver_form *argv) {
	ulver_object *list = ulver_object_new(env, ULVER_LIST);
	while(argv) {
		ulver_object_push(env, list, ulver_eval(env, argv));
		argv = argv->next;
	}
	return list;
}

ulver_object *ulver_fun_if(ulver_env *env, ulver_form *argv) {
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

ulver_object *ulver_fun_let(ulver_env *env, ulver_form *argv) {
	ulver_form *vars = argv->list;
	while(vars) {
		ulver_form *var = vars->list;	
		ulver_symbol_set(env, var->value, var->len, ulver_eval(env, var->next));
		vars = vars->next;
	}
	if (!argv->next) return env->nil;
        return ulver_eval(env, argv->next);
}

ulver_object *ulver_fun_setq(ulver_env *env, ulver_form *argv) {
	ulver_object *uo = ulver_eval(env, argv->next);
	ulver_symbol *us = ulver_symbolmap_set(env, env->global_stack->locals, argv->value, argv->len, uo, 0);
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

ulver_object *ulver_fun_read_line(ulver_env *env, ulver_form *argv) {
	char buf[8192];
	if (fgets(buf, 8192, stdin)) {
		size_t len = strlen(buf);
		if (buf[len-1] == '\n') len--;
		return ulver_object_from_string(env, buf, len);
	}
	return NULL;
}

ulver_object *ulver_fun_print(ulver_env *env, ulver_form *argv) {
	ulver_object *uo = ulver_eval(env, argv);	
	if (!uo) return NULL;
	if (uo->type == 0) {
                printf("\n(");
		ulver_utils_print_list(env, uo);
                printf(") ");
	}
	else if (uo->type == ULVER_STRING) {
		printf("\n\"%.*s\" ", (int) uo->len, uo->str);
	}
	else if (uo->type == ULVER_KEYWORD || uo->type == ULVER_FUNC) {
		printf("\n%.*s ", (int) uo->len, uo->str);
	}
	else if (uo->type == ULVER_NUM) {
		printf("\n%lld ", uo->n);
	}
	else if (uo->type == ULVER_FLOAT) {
		printf("\n%f ", uo->d);
	}
	else if (uo->type == ULVER_PACKAGE) {
		printf("\n#<PACKAGE %.*s> ", (int) uo->len, uo->str);
	}
	else {
		printf("\n?%.*s? ", (int) uo->len, uo->str);
	}
        return uo;
}

ulver_object *ulver_fun_parse_integer(ulver_env *env, ulver_form *argv) {
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
	new->progn = uo->progn;
	if (uo->list) {
		ulver_object *item = uo->list;
		while(item) {
			ulver_object_push(env, new, ulver_object_copy(env, item));
			item = item->next;
		}
	}
	return new;
}

ulver_object *ulver_object_new(ulver_env *env, uint8_t type) {
	ulver_object *uo = env->alloc(env, sizeof(ulver_object));
	uo->type = type;
	uo->size = sizeof(ulver_object);

	// put in the gc list
	if (!env->gc_root) {
		env->gc_root = uo;
		return uo;
	}

	ulver_object *current_root = env->gc_root;
	env->gc_root = uo;
	uo->gc_next = current_root;
	current_root->gc_prev = uo;
	return uo;
}

void ulver_object_destroy(ulver_env *env, ulver_object *uo) {
	ulver_object *prev = uo->gc_prev;
	ulver_object *next = uo->gc_next;

	if (next) {
		next->gc_prev = prev;
	}

	if (prev) {
		prev->gc_next = next;
	}

	if (uo == env->gc_root) {
		env->gc_root = next;
	}

	if (uo->str) {
		// strings memory area is zero-suffixed
		env->free(env, uo->str, (uo->len + 1));
		uo->size -= (uo->len + 1);
	}

	if (uo->map) {
		ulver_symbolmap_destroy(env, uo->map);
	}

	env->free(env, uo, uo->size);
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
	uo->size += (len + 1);
        return uo;
}

ulver_object *ulver_object_from_keyword(ulver_env *env, char *value, uint64_t len) {
        ulver_object *uo = ulver_object_new(env, ULVER_KEYWORD);
        uo->str = ulver_utils_strndup(env, value, len);
	uo->len = len;
	uo->size += (len + 1);
	ulver_utils_toupper(uo->str, uo->len);
        return uo;
}

ulver_object *ulver_package(ulver_env *env, char *value, uint64_t len) {
	ulver_object *uo = ulver_object_new(env, ULVER_PACKAGE);
        uo->str = ulver_utils_strndup(env, value, len);
        uo->len = len;
        uo->size += (len + 1);
	// packages must be protected from gc
	uo->gc_protected = 1;
        ulver_utils_toupper(uo->str, uo->len);
	// this is the map of exported symbols
	uo->map = ulver_symbolmap_new(env);
	// package names do not take the current namespace into account
	ulver_symbolmap_set(env, env->packages, uo->str, uo->len, uo, 1);
        return uo;
}


int ulver_symbol_delete(ulver_env *env, char *name, uint64_t len) {
        // is it a global var ?
        if (len > 2 && name[0] == '*' && name[len-1] == '*') {
                return ulver_symbolmap_delete(env, env->global_stack->locals, name, len, 0);
        }
        ulver_stackframe *stack = env->stack;
        while(stack) {
                if (!ulver_symbolmap_delete(env, stack->locals, name, len, 0)) {
			return 0;
		}
                stack = stack->prev;
        }
        return -1;
}

ulver_symbol *ulver_symbol_get(ulver_env *env, char *name, uint64_t len) {
	// is it a global var ?
	if (len > 2 && name[0] == '*' && name[len-1] == '*') {
		return ulver_symbolmap_get(env, env->global_stack->locals, name, len, 0);
	}	
	ulver_stackframe *stack = env->stack;
	while(stack) {
		ulver_symbol *us = ulver_symbolmap_get(env, stack->locals, name, len, 0);
		if (us) return us;
		stack = stack->prev;
	}
	return NULL;
}

ulver_object *ulver_object_from_symbol(ulver_env *env, ulver_form *uf) {
	ulver_symbol *us = ulver_symbol_get(env, uf->value, uf->len);
	if (!us) {
		return ulver_error_form(env, uf, "variable not bound"); 
	}
	return us->value;
}

ulver_object *ulver_object_push(ulver_env *env, ulver_object *list, ulver_object *uo) {
	ulver_object *next = list->list;
	if (!next) {
		list->list = uo;
		return uo;
	}	
	while(next) {
		if (!next->next) {
			next->next = uo;
			break;
		}
		next = next->next;	
	}
	return uo;
}

ulver_object *ulver_call(ulver_env *env, ulver_form *uf) {
	ulver_form *t_func = uf;
	if (!t_func) return env->nil;
	ulver_object *u_func = ulver_object_from_symbol(env, t_func);
	if (!u_func) return ulver_error_form(env, uf, "function not found");
	if (u_func->type != ULVER_FUNC) return ulver_error_form(env, uf, "is not a function");

	env->caller = u_func;
	
	ulver_stack_push(env);	
	env->calls++;
	ulver_object *ret = u_func->func(env, t_func->next);
	ulver_stack_pop(env);
	// this ensure the returned value is not garbage collected
	env->stack->ret = ret;
	return ret;
}

ulver_object *ulver_error_form(ulver_env *env, ulver_form *uf, char *msg) {
	return ulver_error(env, "[line: %llu pos: %lld] (%.*s) %s", uf->line, uf->line_pos, (int) uf->len, uf->value, msg);
}

ulver_object *ulver_error(ulver_env *env, char *fmt, ...) {
	// when trying to set an error, check if a previous one is set
	if (env->error) {
		// if we are not setting a new error, we are attempting to free it...
		if (!fmt) {
			env->free(env, env->error, env->error_buf_len);
			env->error = NULL;
			env->error_len = 0;
			env->error_buf_len = 0;
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
	env->error = buf;
	env->error_len = ret;
	env->error_buf_len = buf_size;
	return NULL;

fatal:
	env->free(env, buf, buf_size);
	env->error = ulver_utils_strdup(env, "FATAL ERROR");
        env->error_len = strlen(env->error);
	env->error_buf_len = env->error_len + 1;
        return NULL;
}

static ulver_object *eval_do(ulver_env *env, ulver_form *uf, uint8_t as_list) {
	ulver_object *ret = NULL;
	switch(uf->type) {
		// if it is a list, get the first member and eval it
		case ULVER_LIST:
			if (!as_list) return ulver_call(env, uf->list);
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
			return ulver_object_from_symbol(env, uf);
		// if it is a number, build a new object
		case ULVER_NUM:
			return ulver_object_from_num(env, strtoll(uf->value, NULL, 10));
		// if it is a float, build a new object
		case ULVER_FLOAT:
			return ulver_object_from_float(env, strtod(uf->value, NULL));
		// if it is a string, build a new object
		case ULVER_STRING:
			return ulver_object_from_string(env, uf->value, uf->len);
		// if it is a keyword, build a new object
		case ULVER_KEYWORD:
			return ulver_object_from_keyword(env, uf->value, uf->len);
		default:
			printf("unknown object\n");
			break;
			
	}

	if (env->error) {
		ret = NULL;
	}
	return ret;
}

ulver_object *ulver_eval(ulver_env *env, ulver_form *uf) {
	return eval_do(env, uf, 0);
}

ulver_object *ulver_eval_list(ulver_env *env, ulver_form *uf) {
	return eval_do(env, uf, 1);
}

ulver_symbol *ulver_register_fun2(ulver_env *env, char *name, uint64_t len, ulver_object *(*func)(ulver_env *, ulver_form *), ulver_form *lambda_list, ulver_form *progn) {
	ulver_object *uo = ulver_object_new(env, ULVER_FUNC);
        uo->func = func;
	uo->lambda_list = lambda_list;
	uo->progn = progn;
	uo->str = ulver_utils_strndup(env, name, len);
        uo->len = len;
        uo->size += (len + 1);
        ulver_utils_toupper(uo->str, uo->len);
        return ulver_symbolmap_set(env, env->global_stack->locals, name, len, uo, 0);
}

ulver_symbol *ulver_register_fun(ulver_env *env, char *name, ulver_object *(*func)(ulver_env *, ulver_form *)) {
	return ulver_register_fun2(env, name, strlen(name), func, NULL, NULL);
}

ulver_symbol *ulver_symbol_set(ulver_env *env, char *name, uint64_t len, ulver_object *uo) {
	// is it a global var ?
	if (len > 2 && name[0] == '*' && name[len-1] == '*') {
		return ulver_symbolmap_set(env, env->global_stack->locals, name, len, uo, 0);
	}	
	return ulver_symbolmap_set(env, env->stack->locals, name, len, uo, 0);
}

ulver_object *ulver_load(ulver_env *env, char *filename) {
	int fd = open(filename, O_RDONLY);
        if (fd < 0) {
		return ulver_error(env, "unable to open() file %s: %s", filename, strerror(errno));
        }

        struct stat st;
        if (fstat(fd, &st)) {
		return ulver_error(env, "unable to stat() file %s: %s", filename, strerror(errno));
        }

	// we use low-level emory allocation, as it will be freed soon
        char *buf = malloc(st.st_size);
        if (!buf) {
		return ulver_error(env, "unable to malloc() for file %s: %s", filename, strerror(errno));
        }

        ssize_t rlen = read(fd, buf, st.st_size);
        if (rlen != st.st_size) {
		free(buf);
		return ulver_error(env, "unable to read() file %s: %s", filename, strerror(errno));
        }

        ulver_form *uf = ulver_parse(env, buf, rlen);
	free(buf);
	ulver_object *ret = NULL;
        while(uf) {
		ret = ulver_eval(env, uf);
                if (ret == NULL) break;
                uf = uf->next;
        }
	return ret;
}

ulver_object *ulver_run(ulver_env *env, char *source) {
	ulver_form *uf = ulver_parse(env, source, strlen(source));
        ulver_object *ret = NULL;
        while(uf) {
                ret = ulver_eval(env, uf);
                if (ret == NULL) break;
                uf = uf->next;
        }
        return ret;
}

uint64_t ulver_destroy(ulver_env *env) {
	// clear errors (if any)
	ulver_error(env, NULL);
	// unprotect objects
	ulver_object *uo = env->gc_root;
	while(uo) {
		uo->gc_protected = 0;
		uo = uo->gc_next;
	}
	// consume the whole stack
	while(env->stack) {
		ulver_stack_pop(env);	
	}
	// call GC without stack
	ulver_gc(env);

	// destroy the packages map
	ulver_symbolmap_destroy(env, env->packages);

	// now destroy the form tree
	ulver_form *root = env->form_root;
	while(root) {
		ulver_form *next = root->next;
		ulver_form_destroy(env, root);
		root = next;
	}	

	// and the sources list
	ulver_source *source = env->sources;
	while(source) {
		ulver_source *next = source->next;
		env->free(env, source->str, source->len+1);
		env->free(env, source, sizeof(ulver_source));
		source = next;
	}

	uint64_t mem = env->mem;

	free(env);

	return mem;
}

void ulver_report_error(ulver_env *env) {
	if (env->error) {
		printf("\n*** ERROR: %.*s ***\n", (int) env->error_len, env->error);
                // clear error
                ulver_error(env, NULL);
	}
}

ulver_env *ulver_init() {

        ulver_env *env = calloc(1, sizeof(ulver_env));

        env->alloc = ulver_alloc;
        env->free = ulver_free;

        env->global_stack = ulver_stack_push(env);

        // create the packages map
        env->packages = ulver_symbolmap_new(env);
        // create the CL-USER package and set it as current
        env->cl_user = ulver_package(env, "CL-USER", 7);
        env->current_package = env->cl_user;

	ulver_symbol_set(env, "*package*", 9, env->cl_user);

        // the following objects must be protected from gc

        env->t = ulver_object_new(env, ULVER_TRUE);
        env->t->gc_protected = 1;

        env->nil = ulver_object_new(env, 0);
        env->nil->gc_protected = 1;

	// register functions

        ulver_register_fun(env, "+", ulver_fun_add);
        ulver_register_fun(env, "-", ulver_fun_sub);
        ulver_register_fun(env, "*", ulver_fun_mul);
        ulver_register_fun(env, ">", ulver_fun_higher);
        ulver_register_fun(env, "=", ulver_fun_equal);
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

        return env;
}

