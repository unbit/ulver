#include <ulver.h>

ulver_object *ulver_fun_hub(ulver_env *, ulver_form *);

ulver_object *ulver_fun_make_coro(ulver_env *, ulver_form *);
ulver_object *ulver_fun_sleep(ulver_env *, ulver_form *);
ulver_object *ulver_fun_make_tcp_server(ulver_env *, ulver_form *);

ulver_object *ulver_fun_coro_switch(ulver_env *, ulver_form *);
ulver_object *ulver_fun_coro_yield(ulver_env *, ulver_form *);
ulver_object *ulver_fun_coro_next(ulver_env *, ulver_form *);

ulver_object *ulver_fun_all_threads(ulver_env *env, ulver_form *argv) {
	ulver_object *threads = ulver_object_new(env, ULVER_LIST);
	pthread_rwlock_rdlock(&env->threads_lock);
        ulver_thread *ut = env->threads;
        while(ut) {
		ulver_object_push(env, threads, env->t);
                ut = ut->next;
        }
        pthread_rwlock_unlock(&env->threads_lock);
	return threads;
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

ulver_object *ulver_fun_length(ulver_env *env, ulver_form *argv) {
	if (!argv) return ulver_error(env, "read-from-string requires an argument");	

	ulver_object *sequence = ulver_eval(env, argv);
        if (!sequence) return NULL;

        if (sequence->type != ULVER_LIST && sequence->type != ULVER_STRING) {
                return ulver_error_form(env, argv, "is not a sequence");
        }

	return ulver_object_from_num(env, ulver_utils_length(sequence));
}

ulver_object *ulver_fun_subseq(ulver_env *env, ulver_form *argv) {
	if (!argv || !argv->next) return ulver_error(env, "subseq requires two arguments");

	ulver_object *sequence = ulver_eval(env, argv);
	if (!sequence) return NULL;

	if (sequence->type != ULVER_LIST && sequence->type != ULVER_STRING) {
		return ulver_error_form(env, argv, "is not a sequence");
	}

	ulver_object *index = ulver_eval(env, argv->next);
	if (!index) return NULL;
	if (index->type != ULVER_NUM) return ulver_error_form(env, argv->next, "must be an integer"); 
	if (index->n < 0) return ulver_error_form(env, argv->next, "must be an integer >=0");

	// check for the optional "end" argument
	uint64_t sequence_len = ulver_utils_length(sequence);
	uint64_t end_index = sequence_len;
	if (argv->next->next) {
		ulver_object *end = ulver_eval(env, argv->next->next);
		// eval error ?
		if (!end) return NULL;
		// is it nil or a num ??
		if (end != env->nil && end->type != ULVER_NUM) {
			return ulver_error_form(env, argv->next->next, "must be an integer or nil");
		}
		// if it is a num, set it !
		if (end->type == ULVER_NUM) {
			if (end->n < 0) {
				return ulver_error_form(env, argv->next->next, "must be an integer >=0");
			}
			end_index = end->n;
		}
	}

	if (index->n > sequence_len) return ulver_error_form(env, argv->next, "is out of range");
	if (end_index > sequence_len) return ulver_error_form(env, argv->next->next, "is out of range");

	// is it a string ?
	if (sequence->type == ULVER_STRING) {
		return ulver_object_from_string(env, sequence->str + index->n, end_index - index->n);
	}

	// is it a list ?
	if (sequence->type == ULVER_LIST) {
		uint64_t how_many_items = end_index - index->n;
		if (how_many_items > 0) how_many_items--;
		uint64_t start_item = index->n;
		uint64_t current_item = 0;
		uint64_t pushed_items = 0;
		ulver_object *new_list = ulver_object_new(env, ULVER_LIST);
		ulver_object_item *item = sequence->list;
		while(item) {
			if (pushed_items >= how_many_items) break;
			if (current_item >= start_item) {
				ulver_object_push(env, new_list, ulver_object_copy(env, item->o));
				pushed_items++;
			}
			current_item++;
			item = item->next;
		}
		return new_list;
	}

	return env->nil;
}

ulver_object *ulver_fun_read_from_string(ulver_env *env, ulver_form *argv) {
	if (!argv) return ulver_error(env, "read-from-string requires an argument");
	ulver_object *source = ulver_eval(env, argv);
	if (!source) return NULL;
	if (source->type != ULVER_STRING) return ulver_error(env, "read-from-string expects a string");
	ulver_form *uf = ulver_parse(env, source->str, source->len);
	if (!uf) return ulver_error(env, "unable to parse \"%.*s\"", (int) source->len, source->str);
	ulver_object *form = ulver_object_new(env, ULVER_FORM);
	form->form = uf;
	return form;
}

ulver_object *ulver_fun_read(ulver_env *env, ulver_form *argv) {
	ulver_object *stream = env->stdin;
        if (argv) {
                stream = ulver_eval(env, argv);
                if (!stream) return NULL;
                if (stream->type != ULVER_STREAM) return ulver_error_form(env, argv, "is not a stream");
        }
	if (stream->closed) return ulver_error(env, "stream is closed");
        uint64_t slen = 0;
        char *buf = ulver_utils_readline_from_fd(stream->fd, &slen);
        if (!buf) return ulver_error(env, "error reading from fd %d", stream->fd);
        ulver_form *uf = ulver_parse(env, buf, slen);
        free(buf);
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
        ulver_stack_push(env, ut);

/*
	pthread_rwlock_unlock(&env->unsafe_lock);

	// some function is "unsafe", it means it is able to modify objects
	// at runtime. This requires heavy locking of the specific operation
	if (!u_func->unsafe) {
		pthread_rwlock_rdlock(&env->unsafe_lock);
	}
	else {
		pthread_rwlock_wrlock(&env->unsafe_lock);
	}
*/

	// call the function
        ulver_object *ret = u_func->func(env, uf);

	//pthread_rwlock_unlock(&env->unsafe_lock);

	ulver_stack_pop(env, ut);

	// this ensure the returned value is not garbage collected
	ut->current_coro->stack->ret = ret;

	//pthread_rwlock_unlock(&env->unsafe_lock);

	// unlock the thread, now the gc can run again
	//pthread_mutex_unlock(&ut->lock);

	pthread_rwlock_unlock(&env->unsafe_lock);

	uint8_t run_gc = 0;

	pthread_mutex_lock(&env->mem_lock);
        env->calls++;
	// gc must be called only if the maximum amount of memory is reached
	if (ut->current_coro->trigger_gc || (env->gc_freq > 0 && env->mem > env->max_memory)) {
		if (ut->current_coro->trigger_gc || ((env->calls % env->gc_freq) == 0)) {
			run_gc = 1;
		}
	}
	pthread_mutex_unlock(&env->mem_lock);

	if (run_gc) {
		// get the gc lock
		pthread_rwlock_wrlock(&env->unsafe_lock);
		//pthread_mutex_lock(&env->gc_lock);
		ulver_gc(env);
		pthread_rwlock_unlock(&env->unsafe_lock);
		//pthread_mutex_unlock(&ut->lock);
		//pthread_mutex_unlock(&env->gc_lock);
		ut->current_coro->trigger_gc = 0;
	}

	// and now lock again the thread, and pop the stack
	//pthread_mutex_lock(&ut->lock);

	//pthread_rwlock_rdlock(&env->unsafe_lock);

	pthread_rwlock_rdlock(&env->unsafe_lock);

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

ulver_object *ulver_fun_first(ulver_env *env, ulver_form *argv) {
	if (!argv) return ulver_error(env, "first requires an argument");	
	ulver_object *list = ulver_eval(env, argv);
	if (!list) return NULL;
	if (list->type != ULVER_LIST) return ulver_error_form(env, argv, "is not a list");
	ulver_object *ret = ulver_utils_nth(list, 1);
	if (ret) return ret;
	return env->nil;
}

ulver_object *ulver_fun_second(ulver_env *env, ulver_form *argv) {
	if (!argv) return ulver_error(env, "second requires an argument");	
	ulver_object *list = ulver_eval(env, argv);
	if (!list) return NULL;
	if (list->type != ULVER_LIST) return ulver_error_form(env, argv, "is not a list");
	ulver_object *ret = ulver_utils_nth(list, 2);
	if (ret) return ret;
	return env->nil;
}

ulver_object *ulver_fun_third(ulver_env *env, ulver_form *argv) {
	if (!argv) return ulver_error(env, "third requires an argument");	
	ulver_object *list = ulver_eval(env, argv);
	if (!list) return NULL;
	if (list->type != ULVER_LIST) return ulver_error_form(env, argv, "is not a list");
	ulver_object *ret = ulver_utils_nth(list, 3);
	if (ret) return ret;
	return env->nil;
}

ulver_object *ulver_fun_fourth(ulver_env *env, ulver_form *argv) {
	if (!argv) return ulver_error(env, "fourth requires an argument");	
	ulver_object *list = ulver_eval(env, argv);
	if (!list) return NULL;
	if (list->type != ULVER_LIST) return ulver_error_form(env, argv, "is not a list");
	ulver_object *ret = ulver_utils_nth(list, 4);
	if (ret) return ret;
	return env->nil;
}

ulver_object *ulver_fun_fifth(ulver_env *env, ulver_form *argv) {
	if (!argv) return ulver_error(env, "fifth requires an argument");	
	ulver_object *list = ulver_eval(env, argv);
	if (!list) return NULL;
	if (list->type != ULVER_LIST) return ulver_error_form(env, argv, "is not a list");
	ulver_object *ret = ulver_utils_nth(list, 5);
	if (ret) return ret;
	return env->nil;
}

ulver_object *ulver_fun_sixth(ulver_env *env, ulver_form *argv) {
	if (!argv) return ulver_error(env, "sixth requires an argument");	
	ulver_object *list = ulver_eval(env, argv);
	if (!list) return NULL;
	if (list->type != ULVER_LIST) return ulver_error_form(env, argv, "is not a list");
	ulver_object *ret = ulver_utils_nth(list, 6);
	if (ret) return ret;
	return env->nil;
}

ulver_object *ulver_fun_seventh(ulver_env *env, ulver_form *argv) {
	if (!argv) return ulver_error(env, "seventh requires an argument");	
	ulver_object *list = ulver_eval(env, argv);
	if (!list) return NULL;
	if (list->type != ULVER_LIST) return ulver_error_form(env, argv, "is not a list");
	ulver_object *ret = ulver_utils_nth(list, 7);
	if (ret) return ret;
	return env->nil;
}

ulver_object *ulver_fun_eighth(ulver_env *env, ulver_form *argv) {
	if (!argv) return ulver_error(env, "eighth requires an argument");	
	ulver_object *list = ulver_eval(env, argv);
	if (!list) return NULL;
	if (list->type != ULVER_LIST) return ulver_error_form(env, argv, "is not a list");
	ulver_object *ret = ulver_utils_nth(list, 8);
	if (ret) return ret;
	return env->nil;
}

ulver_object *ulver_fun_ninth(ulver_env *env, ulver_form *argv) {
	if (!argv) return ulver_error(env, "ninth requires an argument");	
	ulver_object *list = ulver_eval(env, argv);
	if (!list) return NULL;
	if (list->type != ULVER_LIST) return ulver_error_form(env, argv, "is not a list");
	ulver_object *ret = ulver_utils_nth(list, 9);
	if (ret) return ret;
	return env->nil;
}

ulver_object *ulver_fun_tenth(ulver_env *env, ulver_form *argv) {
	if (!argv) return ulver_error(env, "tenth requires an argument");	
	ulver_object *list = ulver_eval(env, argv);
	if (!list) return NULL;
	if (list->type != ULVER_LIST) return ulver_error_form(env, argv, "is not a list");
	ulver_object *ret = ulver_utils_nth(list, 10);
	if (ret) return ret;
	return env->nil;
}



ulver_object *ulver_fun_getf(ulver_env *env, ulver_form *argv) {
        if (!argv || !argv->next) return ulver_error(env, "getf requires two arguments");
        ulver_object *plist = ulver_eval(env, argv);
        if (!plist) return NULL;
	if (plist->type != ULVER_LIST) return ulver_error(env, "getf expect a property list as the first argument");
        ulver_object *place = ulver_eval(env, argv->next);
        if (!place) return NULL;

	ulver_object_item *key = plist->list;
	while(key) {
		if (ulver_utils_eq(key->o, place)) {
			if (key->next) return key->next->o;
			return ulver_error(env, "the property list has an odd length");
		}
		ulver_object_item *value = key->next;	
		if (!value) return ulver_error(env, "the property list has an odd length");
		key = value->next;
	}
        return env->nil;
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

	pthread_rwlock_rdlock(&env->packages_lock);
	ulver_symbol *package = ulver_symbolmap_get(env, env->packages, name, len, 1);
	pthread_rwlock_unlock(&env->packages_lock);
	if (!package) {
		return ulver_error(env, "unable to find package %.*s", len, name);
	}
	pthread_rwlock_wrlock(&env->packages_lock);
	env->current_package = package->value;
	pthread_rwlock_unlock(&env->packages_lock);
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

ulver_object *ulver_fun_open(ulver_env *env, ulver_form *argv) {
        if (!argv) return ulver_error(env, "open requires an argument");
        ulver_object *uo = ulver_eval(env, argv);
        if (!uo) return NULL;
        if (uo->type != ULVER_STRING) return ulver_error_form(env, argv, "must be a string");
	int mode = O_RDONLY;
	if (argv->next) {
		ulver_form *direction = argv->next;
		if (direction->type != ULVER_KEYWORD) return ulver_error_form(env, direction, "must be a keyword");
		if (direction->len == 6 && !ulver_utils_memicmp(direction->value, ":input", 6)) {
			mode = O_RDONLY;
		}
		else if (direction->len == 7 && !ulver_utils_memicmp(direction->value, ":output", 7)) {
			mode = O_WRONLY;
		}
		else if (direction->len == 3 && !ulver_utils_memicmp(direction->value, ":io", 3)) {
			mode = O_WRONLY;
		}
	}
	int fd = open(uo->str, mode);
	if (fd < 0) {
		return ulver_error(env, "unable to open file");
	}
        return ulver_object_from_fd(env, fd);
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
	ulver_destroy(env);
	exit(0);
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

	pthread_rwlock_rdlock(&env->funcs_lock);
	ulver_symbol *func_symbol = ulver_symbolmap_get(env, env->funcs, argv->value, argv->len, 0);
	pthread_rwlock_unlock(&env->funcs_lock);

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

ulver_object *ulver_fun_mod(ulver_env *env, ulver_form *argv) {
	if (!argv || !argv->next) return ulver_error(env, "mod requires two arguments");
	ulver_object *uo1 = ulver_eval(env, argv);
	if (!uo1) return NULL;
	int64_t n1 = 0;
	if (uo1->type == ULVER_NUM) n1 = uo1->n;
	else if (uo1->type == ULVER_FLOAT) n1 = uo1->d;	
	else return ulver_error_form(env, argv, "is not a number");

	ulver_object *uo2 = ulver_eval(env, argv->next);
	if (!uo2) return NULL;
	int64_t n2 = 0;
        if (uo2->type == ULVER_NUM) n2 = uo2->n;
        else if (uo2->type == ULVER_FLOAT) n2 = uo2->d;
        else return ulver_error_form(env, argv->next, "is not a number");

	if (n2 == 0) return ulver_error(env, "division by zero");
	
        return ulver_object_from_num(env, n1 % n2);
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
			return ulver_error_form(env, argv, "argument is not a number or a float"); 
		}
		argv = argv->next;
	}

	if (is_float) {
        	return ulver_object_from_float(env, d+n);
	}
        return ulver_object_from_num(env, n);
}

ulver_object *ulver_fun_sub(ulver_env *env, ulver_form *argv) {
	if (!argv) return ulver_error(env, "- requires an argument");
	int64_t n = 0;
        double d = 0.0;
        uint8_t is_float = 0;
	ulver_form *item = argv->next;
        while(item) {
                ulver_object *uo = ulver_eval(env, item);
                if (!uo) return NULL;
                if (uo->type == ULVER_NUM) {
                        n += uo->n;
                }
                else if (uo->type == ULVER_FLOAT) {
                        is_float = 1;
                        d += uo->d;
                }
                else {
                        return ulver_error_form(env, item, "argument is not a number or a float");
                }
                item = item->next;
        }

        if (is_float) {
		ulver_object *uo = ulver_eval(env, argv);
		if (!uo) return NULL;
		if (uo->type == ULVER_NUM) {
                	return ulver_object_from_float(env, uo->n - (d+n));
		}
		else if (uo->type == ULVER_FLOAT) {
                	return ulver_object_from_float(env, uo->d - (d+n));
		}
		return ulver_error_form(env, argv, "argument is not a number or a float");
        }

	ulver_object *uo = ulver_eval(env, argv);
        if (!uo) return NULL;
        if (uo->type == ULVER_NUM) {
        	return ulver_object_from_num(env, argv->next ? uo->n - n :  - uo->n);
        }
        else if (uo->type == ULVER_FLOAT) {
        	return ulver_object_from_float(env, argv->next ? uo->d - n: - uo->d);
        }
        return ulver_error_form(env, argv, "argument is not a number or a float");
}


ulver_object *ulver_fun_mul(ulver_env *env, ulver_form *argv) {
	int64_t n = 1;
        double d = 1.0;
        uint8_t is_float = 0;
        while(argv) {
                ulver_object *uo = ulver_eval(env, argv);
                if (!uo) return NULL;
                if (uo->type == ULVER_NUM) {
                        n *= uo->n;
                }
                else if (uo->type == ULVER_FLOAT) {
                        is_float = 1;
                        d *= uo->d;
                }
                else {
                        return ulver_error_form(env, argv, "argument is not a number or a float");
                }
                argv = argv->next;
        }

        if (is_float) {
                return ulver_object_from_float(env, d*n);
        }
        return ulver_object_from_num(env, n);
}

// TODO fix it
ulver_object *ulver_fun_higher(ulver_env *env, ulver_form *argv) {
	if (!argv) return ulver_error(env, "higher requires an argument");	
	ulver_object *uo0 = ulver_eval(env, argv);
	if (!uo0) return NULL;
	ulver_object *uo1 = ulver_eval(env, argv->next);
	if (!uo1) return NULL;
	int64_t n0 = uo0->n;
	int64_t n1 = uo1->n;
	return n0 > n1 ? env->t : env->nil;
}

// TODO fix it
ulver_object *ulver_fun_equal(ulver_env *env, ulver_form *argv) {
	if (!argv) return ulver_error(env, "higher requires an argument");	
	ulver_object *uo0 = ulver_eval(env, argv);
	if (!uo0) return NULL;
	ulver_object *uo1 = ulver_eval(env, argv->next);
	if (!uo1) return NULL;
	int64_t n0 = uo0->n;
	int64_t n1 = uo1->n;
	return n0 == n1 ? env->t : env->nil;
}

ulver_object *ulver_fun_call_with_lambda_list(ulver_env *env, ulver_form *argv) {
	// lambda and progn must be read before the lisp engine changes env->caller
	ulver_thread *ut = ulver_current_thread(env);
	ulver_form *uf = ut->current_coro->caller->lambda_list;
	ulver_form *progn = ut->current_coro->caller->form;

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
	pthread_rwlock_wrlock(&env->globals_lock);
	ulver_symbol *us = ulver_symbolmap_set(env, env->globals, argv->value, argv->len, uo, 0);
	pthread_rwlock_unlock(&env->globals_lock);
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

ulver_object *ulver_fun_socket_listen(ulver_env *env, ulver_form *argv) {
	if (!argv) return ulver_error(env, "socket-listen requires an argument");
	ulver_object *host = ulver_eval(env, argv);
	if (!host) return NULL;
	if (host->type != ULVER_STRING) return ulver_error_form(env, argv, "is not a string");
	struct hostent *he = gethostbyname(host->str);
	if (!he) return ulver_error_form(env, argv, "unable to resolve");
	int s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0) return ulver_error(env, "unable to create socket");
	struct sockaddr_in sin;
	memset(&sin, 0, sizeof(struct sockaddr_in));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(9191);
	memcpy(&sin.sin_addr, he->h_addr_list[0], 4);
	if (bind(s, (struct sockaddr *) &sin, sizeof(struct sockaddr_in))) {
		close(s);
		return ulver_error(env, "unable to bind socket");
	}
	if (listen(s, 100)) {
		close(s);
		return ulver_error(env, "unable to put socket in listen state");
	}
	return ulver_object_from_fd(env, s);	
}

ulver_object *ulver_fun_socket_accept(ulver_env *env, ulver_form *argv) {
        if (!argv) return ulver_error(env, "socket-accept requires an argument");
        ulver_object *stream = ulver_eval(env, argv);
        if (!stream) return NULL;
        if (stream->type != ULVER_STREAM) return ulver_error_form(env, argv, "is not a stream");
        if (stream->closed) return ulver_error(env, "stream is closed");
	int fd = accept(stream->fd, NULL, NULL);
        if (fd < 0) {
                return ulver_error(env, "accept() error");
        }
        return ulver_object_from_fd(env, fd);
}

ulver_object *ulver_fun_close(ulver_env *env, ulver_form *argv) {
	if (!argv) return ulver_error(env, "close requires an argument");
	ulver_object *stream = ulver_eval(env, argv);
        if (!stream) return NULL;
        if (stream->type != ULVER_STREAM) return ulver_error_form(env, argv, "is not a stream");
	if (stream->closed) {
		return env->nil;
	}
	stream->closed = 1;
	return env->t;
}

ulver_object *ulver_fun_read_line(ulver_env *env, ulver_form *argv) {
	ulver_object *stream = env->stdin;
	if (argv) {
		stream = ulver_eval(env, argv);
		if (!stream) return NULL;
		if (stream->type != ULVER_STREAM) return ulver_error_form(env, argv, "is not a stream");
	}
	if (stream->closed) return ulver_error(env, "stream is closed");
	uint64_t slen = 0;
	char *buf = ulver_utils_readline_from_fd(stream->fd, &slen);
	if (!buf) return ulver_error(env, "error reading from fd %d", stream->fd);
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
        	printf("#<STREAM fd:%d>", uo->fd);
        }
	else if (uo->type == ULVER_THREAD) {
        	printf("#<THREAD %p>", uo->thread);
        }
	else if (uo->type == ULVER_CORO) {
        	printf("#<CORO %p>", uo->coro);
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
	ulver_object *uo = env->alloc(env, sizeof(ulver_object));
	uo->env = env;
	uo->type = type;

	// get the current thread;
	ulver_thread *ut = ulver_current_thread(env);	
	uo->current_coro = ut->current_coro;

	// append the object to the stack-related ones
	ulver_object *latest_stack_object = ut->current_coro->stack->objects;
	ut->current_coro->stack->objects = uo;
	ut->current_coro->stack->objects->stack_next = latest_stack_object;

	pthread_mutex_lock(&env->gc_root_lock);

	// put in the gc list
	if (!env->gc_root) {
		env->gc_root = uo;
		pthread_mutex_unlock(&env->gc_root_lock);
		return uo;
	}

	ulver_object *current_root = env->gc_root;
	env->gc_root = uo;
	uo->gc_next = current_root;
	current_root->gc_prev = uo;

	pthread_mutex_unlock(&env->gc_root_lock);

	return uo;
}

// this must be called with gc_root_lock mutex acquired !!!
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
		env->free(env, uo->str, uo->len + 1);
	}

	if (uo->map) {
		ulver_symbolmap_destroy(env, uo->map);
	}

	if (uo->type == ULVER_STREAM) {
		if (!uo->closed) {
			close(uo->fd);
		}
	}

	// free items
	ulver_object_item *item = uo->list;
	while(item) {
		ulver_object_item *next = item->next;
		env->free(env, item, sizeof(ulver_object_item));
		item = next;
	}

	env->free(env, uo, sizeof(ulver_object));
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

ulver_object *ulver_object_from_fd(ulver_env *env, int fd) {
        ulver_object *uo = ulver_object_new(env, ULVER_STREAM);
	uo->fd = fd;
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

	pthread_rwlock_wrlock(&env->packages_lock);
	ulver_symbolmap_set(env, env->packages, uo->str, uo->len, uo, 1);
	pthread_rwlock_unlock(&env->packages_lock);
        return uo;
}


int ulver_symbol_delete(ulver_env *env, char *name, uint64_t len) {
	ulver_thread *ut = ulver_current_thread(env);
        // is it a global var ?
        if (len > 2 && name[0] == '*' && name[len-1] == '*') {
		pthread_rwlock_wrlock(&env->globals_lock);
                int ret = ulver_symbolmap_delete(env, env->globals, name, len, 0);
		pthread_rwlock_unlock(&env->globals_lock);
		return ret;
        }
        ulver_stackframe *stack = ut->current_coro->stack;
        while(stack) {
                if (!ulver_symbolmap_delete(env, stack->locals, name, len, 0)) {
			return 0;
		}
                stack = stack->prev;
        }
	pthread_rwlock_wrlock(&env->globals_lock);
        int ret = ulver_symbolmap_delete(env, env->globals, name, len, 0);
	pthread_rwlock_unlock(&env->globals_lock);
	return ret;
}

ulver_object *ulver_symbol_get(ulver_env *env, char *name, uint64_t len) {
	ulver_object *ret = NULL; 
	// is it a global var ?
	if (len > 2 && name[0] == '*' && name[len-1] == '*') {
		pthread_rwlock_rdlock(&env->globals_lock);
		ulver_symbol *us = ulver_symbolmap_get(env, env->globals, name, len, 0);
		if (us) ret = us->value;
		pthread_rwlock_unlock(&env->globals_lock);
		return ret;
	}	
	ulver_thread *ut = ulver_current_thread(env);
	ulver_stackframe *stack = ut->current_coro->stack;
	while(stack) {
		ulver_symbol *us = ulver_symbolmap_get(env, stack->locals, name, len, 0);
		if (us) return us->value;
		stack = stack->prev;
	}
	pthread_rwlock_rdlock(&env->globals_lock);
	ulver_symbol *us = ulver_symbolmap_get(env, env->globals, name, len, 0);
	if (us) ret = us->value;
	pthread_rwlock_unlock(&env->globals_lock);
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
	//pthread_rwlock_unlock(&env->unsafe_lock);

	//pthread_rwlock_rdlock(&env->unsafe_lock);
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

	if (ut->current_coro->error) {
		ret = NULL;
	}

	/*
	// switch back to main
	printf("back to hub\n");
	__splitstack_getcontext(ut->current_coro->ss_contexts);
	swapcontext(&ut->current_coro->context, &ut->hub_context);
	// run uv tasks
	uv_run(ut->loop, UV_RUN_NOWAIT);
	printf("ready to return %p\n", ret);
	*/
	return ret;
}

ulver_object *ulver_eval(ulver_env *env, ulver_form *uf) {
/*
	ulver_thread *ut = ulver_current_thread(env);
	if (ut->current_coro == NULL) {
		printf("ready to switch\n");
		ut->current_coro = ut->coros;
		ut->current_coro->argv = uf;
		__splitstack_setcontext(ut->current_coro->ss_contexts);
        	swapcontext(&ut->hub_context, &ut->current_coro->context);
		printf("back from coro !!! %p\n", ut->current_coro->stack->ret);
		return ut->current_coro->stack->ret;
	}
	printf("NORMAL EVAL\n");
*/
	return eval_do(env, ulver_current_thread(env), uf, 0);
	
}

ulver_object *ulver_eval_list(ulver_env *env, ulver_form *uf) {
	return eval_do(env, ulver_current_thread(env), uf, 1);
}

struct ulver_thread_args {
	ulver_env *env;
	ulver_form *argv;
};

void *run_new_thread(void *arg) {
	struct ulver_thread_args *uta = (struct ulver_thread_args *) arg;
	ulver_env *env = uta->env;
	ulver_form *argv = uta->argv;
	free(uta);
        // this will create the memory structures
        // for the new thread
        ulver_thread *ut = ulver_current_thread(env);
	ut->t = pthread_self();
        ulver_object *ret = eval_do(env, ut, argv, 0);

        // the function ended
        // as the error status is per-thread, we need to make
        // something with the return value
        // ...
        // we can safely mark the thread as dead
        // (as the lock is still acquired);
	ulver_report_error(env);
        ut->dead = 1;
	printf("thread ended\n");
        // return something ?
        return NULL;
}

ulver_object *ulver_fun_make_thread(ulver_env *env, ulver_form *argv) {
	if (!argv) return ulver_error(env, "make-thread requires an argument");	
        pthread_t t;
	struct ulver_thread_args *uta = malloc(sizeof(struct ulver_thread_args));;
	if (!uta) {
		perror("malloc()");
		exit(1);
	}
	uta->env = env;
	uta->argv = argv;
        if (pthread_create(&t, NULL, run_new_thread, uta)) {
		free(uta);
	}
        return env->nil;
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
	pthread_rwlock_wrlock(&env->funcs_lock);
        ulver_symbol *us = ulver_symbolmap_set(env, env->funcs, name, len, uo, 0);
	pthread_rwlock_unlock(&env->funcs_lock);
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
		pthread_rwlock_wrlock(&env->globals_lock);
		ulver_symbol *us = ulver_symbolmap_set(env, env->globals, name, len, uo, 0);
		pthread_rwlock_wrlock(&env->globals_lock);
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
        char *buf = malloc(st.st_size);
        if (!buf) {
		close(fd);
		return ulver_error(env, "unable to malloc() for file %s: %s", filename, strerror(errno));
        }

        ssize_t rlen = read(fd, buf, st.st_size);
	close(fd);
        if (rlen != st.st_size) {
		free(buf);
		return ulver_error(env, "unable to read() file %s: %s", filename, strerror(errno));
        }

        ulver_form *uf = ulver_parse(env, buf, rlen);
	free(buf);
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

uint64_t ulver_destroy(ulver_env *env) {
	if (!pthread_equal(pthread_self(), env->creator_thread)) {
		fprintf(env->stderr, "only the creator thread can call ulver_destroy()\n");
		return env->mem;
	}

	// mark myself as dead
	ulver_thread *me = ulver_current_thread(env);
	me->dead = 1;

	// wait for all registered threads...
	pthread_rwlock_rdlock(&env->threads_lock);
	ulver_thread *ut = env->threads;
	while(ut) {
		if (!pthread_equal(pthread_self(), ut->t)) {
			pthread_join(ut->t, NULL);
		} 
		// mark all the coros as dead
		ulver_coro *coros = ut->coros;
		while(coros) {
			coros->dead = 1;
			coros = coros->next;
		}
		if (ut->hub) {
			uv_loop_delete(ut->hub_loop);
			ulver_coro_free_context(env, ut->hub->context);
        		env->free(env, ut->hub, sizeof(ulver_coro));
		}
		ut = ut->next;
	}
	pthread_rwlock_unlock(&env->threads_lock);
	// and 

	// clear errors (if any)
	ulver_error(env, NULL);
	// unprotect objects
	ulver_object *uo = env->gc_root;
	while(uo) {
		uo->gc_protected = 0;
		uo = uo->gc_next;
	}

	// destroy globals, functions and packages
	ulver_symbolmap_destroy(env, env->globals);
	env->globals = NULL;
	ulver_symbolmap_destroy(env, env->funcs);
	env->funcs = NULL;
	ulver_symbolmap_destroy(env, env->packages);
	env->packages = NULL;

	// call GC (no locking needed)
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

	free(env);

	// return the leaked memory (MUST BE 0 !!!)
	return mem;
}

void ulver_report_error(ulver_env *env) {
	ulver_thread *ut = ulver_current_thread(env);
	if (ut->current_coro->error) {
		fprintf(env->stderr, "\n*** ERROR: %.*s ***\n", (int) ut->current_coro->error_len, ut->current_coro->error);
                // clear error
                ulver_error(env, NULL);
	}
}

static void fake() {
	printf("the fake function\n");
}

ulver_thread *ulver_current_thread(ulver_env *env) {
	ulver_thread *ut = (ulver_thread *) pthread_getspecific(env->thread);
	if (ut) return ut;
	ut = env->alloc(env, sizeof(ulver_thread));
	ut->env = env;
	ut->t = pthread_self();

	ut->main_coro = env->alloc(env, sizeof(ulver_coro));
	// uv callbacks only get ulver_coro pointer as data
	// so we need an additional pointer to env
	ut->main_coro->env = env;
	ut->main_coro->context = ulver_coro_alloc_context(env);
	ut->current_coro = ut->main_coro;

	//ut->current_coro = ut->coros;
	//pthread_mutex_init(&ut->lock, NULL);
	//pthread_mutex_lock(&ut->lock);
        pthread_setspecific(env->thread, (void *) ut);
        ulver_stack_push(env, ut);
	pthread_rwlock_wrlock(&env->threads_lock);
	ulver_thread *head = env->threads;
	if (head) head->prev = ut;
	env->threads = ut;
	ut->next = head;
	pthread_rwlock_unlock(&env->threads_lock);

	pthread_rwlock_rdlock(&env->unsafe_lock);

/*
	//we can now switch to the new coro
	__splitstack_setcontext(ut->coros->ss_contexts);
        swapcontext(&ut->hub_context, &ut->coros->context);
	printf("back\n");	
*/
        return ut;
}

ulver_env *ulver_init() {

        ulver_env *env = calloc(1, sizeof(ulver_env));

	env->creator_thread = pthread_self();
	if (pthread_key_create(&env->thread, NULL)) {
		ulver_destroy(env);
		return NULL;
	}

        env->alloc = ulver_alloc;
        env->free = ulver_free;

	env->stdout = stdout;
	env->stderr = stderr;

	pthread_mutex_init(&env->gc_lock, NULL);

	pthread_mutex_init(&env->gc_root_lock, NULL);

	pthread_mutex_init(&env->mem_lock, NULL);

	pthread_rwlock_init(&env->threads_lock, NULL);

	pthread_rwlock_init(&env->unsafe_lock, NULL);


	env->globals = ulver_symbolmap_new(env); 
	pthread_rwlock_init(&env->globals_lock, NULL);
	env->funcs = ulver_symbolmap_new(env); 
	pthread_rwlock_init(&env->funcs_lock, NULL);

        // create the packages map
        env->packages = ulver_symbolmap_new(env);
	pthread_rwlock_init(&env->packages_lock, NULL);
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

        env->stdin = ulver_object_from_fd(env, 0);
        env->stdin->gc_protected = 1;

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
        ulver_register_fun(env, "all-threads", ulver_fun_all_threads);

        ulver_register_fun(env, "sleep", ulver_fun_sleep);

        ulver_register_fun(env, "loop", ulver_fun_loop);

        ulver_register_fun(env, "return", ulver_fun_return);

        ulver_register_fun(env, "open", ulver_fun_open);
        ulver_register_fun(env, "close", ulver_fun_close);

        ulver_register_fun(env, "socket-listen", ulver_fun_socket_listen);
        ulver_register_fun(env, "socket-accept", ulver_fun_socket_accept);

        ulver_register_fun(env, "make-tcp-server", ulver_fun_make_tcp_server);

        ulver_register_fun(env, "make-coro", ulver_fun_make_coro);
        ulver_register_fun(env, "coro-switch", ulver_fun_coro_switch);
        ulver_register_fun(env, "coro-next", ulver_fun_coro_next);
        ulver_register_fun(env, "coro-yield", ulver_fun_coro_yield);

        ulver_register_package_fun(env, ulver_ns, "hub", ulver_fun_hub);

        return env;
}

